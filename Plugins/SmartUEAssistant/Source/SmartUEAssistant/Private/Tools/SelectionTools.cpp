#include "Tools/SelectionTools.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "JsonObjectConverter.h"

// Editor-only includes
#include "Editor.h"
#include "Engine/Selection.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Internationalization/Regex.h"
#include "HAL/FileManager.h"
#include "Tools/SUEAEditorHelpers.h"



// 预设存取辅助改用公共助手 SUEA::GetSelectionPresetPath()

// 使用公共助手的加载实现
// static bool SUEA_LoadSelectionPresets(...) 移除

static bool SUEA_SaveSelectionPresets(const TSharedPtr<FJsonObject>& InObj)
{
    if (!InObj.IsValid()) return false;
    FString Dir = FPaths::GetPath(SUEA::GetSelectionPresetPath());
    IFileManager::Get().MakeDirectory(*Dir, true);
    FString OutStr; TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutStr);
    if (!FJsonSerializer::Serialize(InObj.ToSharedRef(), Writer)) return false;
    return FFileHelper::SaveStringToFile(OutStr, *SUEA::GetSelectionPresetPath());
}

static bool ReadStringOrArray(const TSharedPtr<FJsonObject>& Obj, const FString& Key, TArray<FString>& Out)
{
    if (!Obj.IsValid() || !Obj->HasField(Key)) return false;
    if (Obj->HasTypedField<EJson::String>(*Key)) { Out.Add(Obj->GetStringField(*Key)); return true; }
    if (Obj->HasTypedField<EJson::Array>(*Key))
    {
        const TArray<TSharedPtr<FJsonValue>> Arr = Obj->GetArrayField(*Key);
        for (const auto& V : Arr)
        {
            FString S; if (V->TryGetString(S)) Out.Add(S);
        }
        return Out.Num() > 0;
    }
    return false;
}

// ---------------- FSelectActorsByRuleTool ----------------
FSelectActorsByRuleTool::FSelectActorsByRuleTool()
{
    Spec.Name = TEXT("select_actors_by_rule");
    Spec.Description = TEXT("按规则选择Actor（类/名称/标签/组件/正则），支持操作：set/add/remove/invert；支持保存/加载预设");
    Spec.Params = {
        {TEXT("rule"), TEXT("object"), true, TEXT("筛选规则：{ class/name/name_contains/name_regex/tag/component_class } 均可为字符串或字符串数组")},
        {TEXT("op"), TEXT("string"), true, TEXT("选择操作：set | add | remove | invert，默认 set")},
        {TEXT("preset_save"), TEXT("string"), true, TEXT("将本次 rule 保存为预设名")},
        {TEXT("preset_load"), TEXT("string"), true, TEXT("从该预设名加载 rule")},
        {TEXT("limit"), TEXT("number"), true, TEXT("匹配数量上限，默认不限制")}
    };
    Spec.Permission = EToolPermission::Safe;
}

FAIToolResult FSelectActorsByRuleTool::Execute(const TSharedPtr<FJsonObject>& Args)
{
    if (!GEditor) return {false, TEXT("GEditor 不可用"), nullptr};
    UWorld* World = SUEA::GetEditorWorld();
    if (!World) return {false, TEXT("未获取到编辑器世界"), nullptr};

    // 1) 解析规则：来自 rule 对象或 preset_load
    TSharedPtr<FJsonObject> Rule;
    if (Args.IsValid() && Args->HasTypedField<EJson::String>(TEXT("preset_load")))
    {
        const FString PresetName = Args->GetStringField(TEXT("preset_load"));
        TSharedPtr<FJsonObject> Root; if (!SUEA::LoadSelectionPresets(Root)) return {false, TEXT("读取预设失败"), nullptr};
        TSharedPtr<FJsonObject> Presets = Root->GetObjectField(TEXT("presets"));
        if (!Presets.IsValid() || !Presets->HasTypedField<EJson::Object>(*PresetName))
            return {false, FString::Printf(TEXT("未找到预设：%s"), *PresetName), nullptr};
        Rule = Presets->GetObjectField(*PresetName);
    }
    else if (Args.IsValid() && Args->HasTypedField<EJson::Object>(TEXT("rule")))
    {
        Rule = Args->GetObjectField(TEXT("rule"));
    }
    else
    {
        return {false, TEXT("缺少筛选规则：请提供 rule 或 preset_load"), nullptr};
    }

    // 2) 解析op
    FString Op = TEXT("set");
    if (Args.IsValid() && Args->HasTypedField<EJson::String>(TEXT("op"))) Op = Args->GetStringField(TEXT("op"));

    // 3) 从规则提取字段
    TArray<FString> ClassKeys, NameExactKeys, NameContainsKeys, NameRegexKeys, TagKeys, CompClassKeys;
    ReadStringOrArray(Rule, TEXT("class"), ClassKeys);
    ReadStringOrArray(Rule, TEXT("name"), NameExactKeys);
    ReadStringOrArray(Rule, TEXT("name_contains"), NameContainsKeys);
    ReadStringOrArray(Rule, TEXT("name_regex"), NameRegexKeys);
    ReadStringOrArray(Rule, TEXT("tag"), TagKeys);
    ReadStringOrArray(Rule, TEXT("component_class"), CompClassKeys);

    const int32 Limit = (Args.IsValid() && Args->HasTypedField<EJson::Number>(TEXT("limit"))) ? (int32)Args->GetNumberField(TEXT("limit")) : -1;

    // 4) 遍历匹配
    TArray<AActor*> Matched;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* A = *It; bool bOk = true;
        // class
        if (bOk && ClassKeys.Num() > 0)
        {
            const FString C = A->GetClass()->GetName();
            bool Any = false; for (const FString& K : ClassKeys) { if (C.Contains(K, ESearchCase::IgnoreCase)) { Any = true; break; } }
            bOk = Any;
        }
        // name exact
        if (bOk && NameExactKeys.Num() > 0)
        {
            const FString N = A->GetName();
            bool Any = false; for (const FString& K : NameExactKeys) { if (N.Equals(K, ESearchCase::IgnoreCase)) { Any = true; break; } }
            bOk = Any;
        }
        // name contains
        if (bOk && NameContainsKeys.Num() > 0)
        {
            const FString N = A->GetName();
            bool Any = false; for (const FString& K : NameContainsKeys) { if (N.Contains(K, ESearchCase::IgnoreCase)) { Any = true; break; } }
            bOk = Any;
        }
        // name regex
        if (bOk && NameRegexKeys.Num() > 0)
        {
            const FString N = A->GetName();
            bool Any = false; for (const FString& K : NameRegexKeys) { FRegexPattern P(K); FRegexMatcher M(P, N); if (M.FindNext()) { Any = true; break; } }
            bOk = Any;
        }
        // tag
        if (bOk && TagKeys.Num() > 0)
        {
            bool Any = false; for (const FString& K : TagKeys) { if (A->ActorHasTag(FName(*K))) { Any = true; break; } }
            bOk = Any;
        }
        // component class
        if (bOk && CompClassKeys.Num() > 0)
        {
            bool Any = false;
            TArray<UActorComponent*> Comps; A->GetComponents(Comps);
            for (UActorComponent* Cmp : Comps)
            {
                if (!Cmp) continue; const FString CN = Cmp->GetClass()->GetName();
                for (const FString& K : CompClassKeys) { if (CN.Contains(K, ESearchCase::IgnoreCase)) { Any = true; break; } }
                if (Any) break;
            }
            bOk = Any;
        }

        if (bOk)
        {
            Matched.Add(A);
            if (Limit > 0 && Matched.Num() >= Limit) break;
        }
    }

    // 5) 应用选择
    int32 Affected = 0;
    if (Op.Equals(TEXT("set"), ESearchCase::IgnoreCase))
    {
        GEditor->SelectNone(true, true, false);
        for (AActor* A : Matched) { GEditor->SelectActor(A, true, true, true); ++Affected; }
    }
    else if (Op.Equals(TEXT("add"), ESearchCase::IgnoreCase))
    {
        for (AActor* A : Matched) { GEditor->SelectActor(A, true, true, true); ++Affected; }
    }
    else if (Op.Equals(TEXT("remove"), ESearchCase::IgnoreCase))
    {
        for (AActor* A : Matched) { GEditor->SelectActor(A, false, true, true); ++Affected; }
    }
    else if (Op.Equals(TEXT("invert"), ESearchCase::IgnoreCase))
    {
        USelection* Sel = GEditor->GetSelectedActors();
        for (AActor* A : Matched)
        {
            const bool bIsSel = Sel && Sel->IsSelected(A);
            GEditor->SelectActor(A, !bIsSel, true, true);
            ++Affected;
        }
    }
    else
    {
        return {false, TEXT("不支持的op，允许：set/add/remove/invert"), nullptr};
    }
    GEditor->NoteSelectionChange();

    // 6) 可选保存预设
    FString SavedPreset;
    if (Args.IsValid() && Args->HasTypedField<EJson::String>(TEXT("preset_save")))
    {
        SavedPreset = Args->GetStringField(TEXT("preset_save"));
        TSharedPtr<FJsonObject> Root; if (!SUEA::LoadSelectionPresets(Root)) return {false, TEXT("读取预设失败（保存）"), nullptr};
        TSharedPtr<FJsonObject> Presets = Root->HasTypedField<EJson::Object>(TEXT("presets")) ? Root->GetObjectField(TEXT("presets")) : MakeShareable(new FJsonObject);
        Presets->SetObjectField(*SavedPreset, Rule.ToSharedRef());
        Root->SetObjectField(TEXT("presets"), Presets);
        if (!SUEA_SaveSelectionPresets(Root))
        {
            return {false, TEXT("保存预设失败"), nullptr};
        }
    }

    // 7) 返回结果
    TArray<TSharedPtr<FJsonValue>> Names;
    for (int32 i=0;i<FMath::Min(100, Matched.Num());++i)
    {
        Names.Add(MakeShareable(new FJsonValueString(Matched[i]->GetName())));
    }
    TSharedPtr<FJsonObject> Data = MakeShareable(new FJsonObject);
    Data->SetArrayField(TEXT("matched"), Names);
    Data->SetNumberField(TEXT("matched_count"), Matched.Num());
    Data->SetNumberField(TEXT("affected"), Affected);
    Data->SetStringField(TEXT("op"), Op);
    if (!SavedPreset.IsEmpty()) Data->SetStringField(TEXT("preset_saved"), SavedPreset);
    return {true, FString::Printf(TEXT("匹配对象： %d，操作行为： %s 成功修改 %d"), Matched.Num(), *Op, Affected), Data};
}
// �?自动注册工具
#include "ToolAutoRegister.h"

REGISTER_EDITOR_TOOL(FSelectActorsByRuleTool)

