#include "Tools/ViewportTools.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "JsonObjectConverter.h"
#include "SmartUEAssistantLog.h"//hys
#include "Engine/Selection.h"
#include "Tools/SUEAEditorHelpers.h"


FFocusViewportTool::FFocusViewportTool()
{
    Spec.Name = TEXT("focus_viewport");
    Spec.Description = TEXT("将视口聚焦到目标Actor或当前选择");
    Spec.Params = {
        {TEXT("name"), TEXT("string"), true, TEXT("目标Actor名称（精确匹配）。若省略则使用当前选择")}
    };
    Spec.Permission = EToolPermission::Safe;
    Spec.bRequireConfirm = false;
}

FAIToolResult FFocusViewportTool::Execute(const TSharedPtr<FJsonObject>& Args)
{
    UE_LOG(LogSmartUEAssistantAI, Log, TEXT("【Focus Viewport Tool 执行开始】"));

    if (!GEditor) 
    {
        UE_LOG(LogSmartUEAssistantAI, Error, TEXT("  GEditor 不可用"));
        return {false, TEXT("GEditor 不可用"), nullptr};
    }

    UWorld* World = SUEA::GetEditorWorld();
    if (!World) 
    {
        UE_LOG(LogSmartUEAssistantAI, Error, TEXT("  未获取到编辑器世界"));
        return {false, TEXT("未获取到编辑器世界"), nullptr};
    }

    UE_LOG(LogSmartUEAssistantAI, Log, TEXT("  当前世界: %s"), *World->GetName());

    // 收集目标Actor
    TArray<AActor*> Targets;

    // 获取 name 参数并打印
    FString NameParam;
    bool bHasName = Args.IsValid() && Args->HasTypedField<EJson::String>(TEXT("name"));
    if (bHasName)
    {
        NameParam = Args->GetStringField(TEXT("name"));
        UE_LOG(LogSmartUEAssistantAI, Log, TEXT("  收到参数 name = '%s'"), *NameParam);
    }
    else
    {
        UE_LOG(LogSmartUEAssistantAI, Log, TEXT("  未收到 name 参数，将使用当前选中集"));
    }

    if (bHasName && !NameParam.IsEmpty())
    {
        UE_LOG(LogSmartUEAssistantAI, Log, TEXT("  开始按名称精确查找 Actor: '%s'"), *NameParam);

        int32 CheckedCount = 0;
        bool bFound = false;

        for (TActorIterator<AActor> It(World); It; ++It)
        {
            CheckedCount++;
            AActor* Actor = *It;
            FString ActorName = Actor->GetName();
            FString ActorLabel = Actor->GetActorLabel();

            UE_LOG(LogSmartUEAssistantAI, Verbose, TEXT("    检查 Actor [%d]: Name='%s', Label='%s'"), CheckedCount, *ActorName, *ActorLabel);

            if (ActorName.Equals(NameParam, ESearchCase::IgnoreCase) ||
                ActorLabel.Equals(NameParam, ESearchCase::IgnoreCase))
            {
                Targets.Add(Actor);
                UE_LOG(LogSmartUEAssistantAI, Log, TEXT("    ★ 匹配成功！添加 Actor: Name='%s', Label='%s'"), *ActorName, *ActorLabel);
                bFound = true;
                break;  // 找到一个就停止（或去掉 break 找所有匹配）
            }
        }

        UE_LOG(LogSmartUEAssistantAI, Log, TEXT("  遍历结束，共检查 %d 个 Actor"), CheckedCount);

        if (!bFound)
        {
            UE_LOG(LogSmartUEAssistantAI, Warning, TEXT("  未找到匹配的 Actor: '%s'"), *NameParam);
            return {false, FString::Printf(TEXT("未找到Actor：%s (已检查 %d 个对象)"), *NameParam, CheckedCount), nullptr};
        }
    }
    else
    {
        // 无 name 或 name 为空 → 使用当前选中集
        UE_LOG(LogSmartUEAssistantAI, Log, TEXT("  使用当前选中集"));

        USelection* Sel = GEditor->GetSelectedActors();
        if (Sel && Sel->Num() > 0)
        {
            UE_LOG(LogSmartUEAssistantAI, Log, TEXT("  当前选中数量: %d"), Sel->Num());

            for (FSelectionIterator It(*Sel); It; ++It)
            {
                if (AActor* A = Cast<AActor>(*It))
                {
                    Targets.Add(A);
                    UE_LOG(LogSmartUEAssistantAI, Log, TEXT("    添加选中 Actor: Name='%s', Label='%s'"), *A->GetName(), *A->GetActorLabel());
                }
            }
        }
        else
        {
            UE_LOG(LogSmartUEAssistantAI, Warning, TEXT("  当前没有选中任何对象"));
            return {false, TEXT("未提供 name 且当前无选择"), nullptr};
        }
    }

    // 执行聚焦
    if (Targets.Num() > 0)
    {
        UE_LOG(LogSmartUEAssistantAI, Log, TEXT("  找到 %d 个目标 Actor，开始聚焦视口"), Targets.Num());

        GEditor->MoveViewportCamerasToActor(Targets, /*bActiveViewportOnly*/ false);

        FString Msg = TEXT("视口已聚焦到目标");
        if (Targets.Num() == 1)
        {
            Msg += FString::Printf(TEXT("：%s (Label: %s)"), *Targets[0]->GetName(), *Targets[0]->GetActorLabel());
        }
        else
        {
            Msg += FString::Printf(TEXT("（共 %d 个对象）"), Targets.Num());
        }

        UE_LOG(LogSmartUEAssistantAI, Log, TEXT("【Focus Viewport Tool 执行成功】 %s"), *Msg);
        return {true, Msg, nullptr};
    }

    UE_LOG(LogSmartUEAssistantAI, Error, TEXT("【Focus Viewport Tool 执行失败】 无有效目标"));
    return {false, TEXT("无有效目标"), nullptr};
}
//
// FAIToolResult FFocusViewportTool::Execute(const TSharedPtr<FJsonObject>& Args)
// {
//     if (!GEditor) return {false, TEXT("GEditor 不可用"), nullptr};
//     UWorld* World = SUEA::GetEditorWorld();
//     if (!World) return {false, TEXT("未获取到编辑器世界"), nullptr};
//
//     // 收集目标Actor：name 精确匹配 或 当前选择集
//     TArray<AActor*> Targets;
//     if (Args.IsValid() && Args->HasTypedField<EJson::String>(TEXT("name")))
//     {
//         const FString Name = Args->GetStringField(TEXT("name"));
//         for (TActorIterator<AActor> It(World); It; ++It)
//         {
//             if ((*It)->GetName().Equals(Name, ESearchCase::IgnoreCase)) { Targets.Add(*It); break; }
//         }
//         if (Targets.Num() == 0) return {false, FString::Printf(TEXT("未找到Actor：%s"), *Name), nullptr};
//     }
//     else
//     {
//         if (USelection* Sel = GEditor->GetSelectedActors())
//         {
//             for (FSelectionIterator It(*Sel); It; ++It)
//             {
//                 if (AActor* A = Cast<AActor>(*It)) Targets.Add(A);
//             }
//         }
//         if (Targets.Num() == 0) return {false, TEXT("未提供 name 且当前无选择"), nullptr};
//     }
//
//     GEditor->MoveViewportCamerasToActor(Targets, /*bActiveViewportOnly*/ false);
//     return {true, TEXT("视口已聚焦到目标"), nullptr};
// }
// �?自动注册工具
#include "ToolAutoRegister.h"

REGISTER_EDITOR_TOOL(FFocusViewportTool)

