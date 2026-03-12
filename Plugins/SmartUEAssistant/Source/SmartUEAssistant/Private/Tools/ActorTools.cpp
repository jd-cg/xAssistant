#include "Tools/ActorTools.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "JsonObjectConverter.h"

// Editor-only includes
#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "ScopedTransaction.h"
#include "SmartUEAssistantLog.h"
#include "Engine/Selection.h"
#include "Engine/PointLight.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMeshActor.h"
#include "Tools/SUEAEditorHelpers.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Subsystems/EditorActorSubsystem.h"
static AActor* FindActorByNameFuzzy(UWorld* World, const FString& Query, bool bExact)
{
    if (!World || Query.IsEmpty()) 
    {
        return nullptr;
    }
    FString LowerQuery = Query.ToLower().TrimStartAndEnd();
    int32 Checked = 0;
    AActor* FirstMatch = nullptr;

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        Checked++;
        AActor* Actor = *It;
        FString ActorName = Actor->GetName().ToLower();
        FString ActorLabel = Actor->GetActorLabel().ToLower();

        // 打印每个 Actor（调试用，）
        // UE_LOG(LogSmartUEAssistantAI, Verbose, TEXT("  [%d] Name='%s' | Label='%s'"), Checked, *ActorName, *ActorLabel);

        bool bMatched = false;

        if (bExact)
        {
            if (ActorLabel == LowerQuery || ActorName == LowerQuery)
            {
                bMatched = true;
            }
        }
        else
        {
            // hys 模糊匹配
            if (ActorLabel.Contains(LowerQuery) || ActorName.Contains(LowerQuery))
            {
                bMatched = true;
            }
        }

        if (bMatched)
        {
            if (!FirstMatch) FirstMatch = Actor;  // 返回第一个匹配的
            return Actor;
        }
    }
    return nullptr;
}


// ---------------- FSelectAndFocusActorTool ----------------
FSelectAndFocusActorTool::FSelectAndFocusActorTool()
{
    Spec.Name = TEXT("select_focus_actor");
    Spec.Description = TEXT("选择并聚焦指定Actor（按名称模糊匹配）");
    Spec.Params = {
        {TEXT("query"), TEXT("string"), false, TEXT("待查找的Actor名称关键词或完整名")},
        {TEXT("exact"), TEXT("boolean"), true, TEXT("是否精确匹配，默认false")}
    };
    Spec.Permission = EToolPermission::Safe;
}

FAIToolResult FSelectAndFocusActorTool::Execute(const TSharedPtr<FJsonObject>& Args)
{
    if (!Args.IsValid() || !Args->HasTypedField<EJson::String>(TEXT("query")))
    {
        return {false, TEXT("缺少参数：query"), nullptr};
    }
    const FString Query = Args->GetStringField(TEXT("query"));
    const bool bExact = Args->HasField(TEXT("exact")) ? Args->GetBoolField(TEXT("exact")) : false;

    UWorld* World = SUEA::GetEditorWorld();
    if (!World) return {false, TEXT("未获取到编辑器世界"), nullptr};

    AActor* Target = FindActorByNameFuzzy(World, Query, bExact);
    if (!Target)
    {
        return {false, FString::Printf(TEXT("未找到匹配的Actor：%s"), *Query), nullptr};
    }

    if (GEditor)
    {
        GEditor->SelectNone(true, true, false);
        GEditor->SelectActor(Target, true, true, true);

        TArray<AActor*> Actors; Actors.Add(Target);
        GEditor->MoveViewportCamerasToActor(Actors, false);
        GEditor->NoteSelectionChange();
    }

    TSharedPtr<FJsonObject> Data = MakeShareable(new FJsonObject);
    Data->SetStringField(TEXT("actor"), Target->GetName());
    Data->SetStringField(TEXT("class"), Target->GetClass()->GetName());
    Data->SetStringField(TEXT("location"), Target->GetActorLocation().ToString());

    return {true, TEXT("已选择并聚焦目标Actor"), Data};
}

// ---------------- FSetActorTransformTool ----------------
FSetActorTransformTool::FSetActorTransformTool()
{
    Spec.Name = TEXT("set_actor_transform");
    Spec.Description = TEXT("设置Actor的位置/旋转/缩放（支持部分字段）");
    Spec.Params = {
        {TEXT("name"), TEXT("string"), false, TEXT("Actor名称（默认精确匹配）")},
        {TEXT("location"), TEXT("object"), true, TEXT("{x,y,z}，单位cm")},
        {TEXT("rotation"), TEXT("object"), true, TEXT("{pitch,yaw,roll}，单位度")},
        {TEXT("scale"), TEXT("object"), true, TEXT("{x,y,z}")}
    };
    Spec.Permission = EToolPermission::Modify;
}

FAIToolResult FSetActorTransformTool::Execute(const TSharedPtr<FJsonObject>& Args)
{
    if (!Args.IsValid() || !Args->HasTypedField<EJson::String>(TEXT("name")))
    {
        return {false, TEXT("缺少参数：name"), nullptr};
    }
    UWorld* World = SUEA::GetEditorWorld();
    if (!World) return {false, TEXT("未获取到编辑器世界"), nullptr};

    const FString Name = Args->GetStringField(TEXT("name"));
    AActor* Target = FindActorByNameFuzzy(World, Name, /*bExact*/false);
    if (!Target)
    {
        // 如果找不到，尝试去掉后缀再找一次（针对 BP_Actor_C 这种类名干扰）
        FString CleanName = Name.Replace(TEXT("_C"), TEXT(""));
        Target = FindActorByNameFuzzy(World, CleanName, false);
    }

    // 2. 找不到时打印当前场景前 3 个 Actor 的 Label 到日志，方便你调试
    if (!Target)
    {
        UE_LOG(LogSmartUEAssistantAI, Warning, TEXT("找不到 Actor: %s. 正在扫描场景..."), *Name);
        int32 Count = 0;
        for (TActorIterator<AActor> It(World); It && Count < 3; ++It, ++Count)
        {
            UE_LOG(LogSmartUEAssistantAI, Log, TEXT("场景中有: %s"), *It->GetActorLabel());
        }
    }
    if (!Target) return {false, FString::Printf(TEXT("未找到Actor：%s"), *Name), nullptr};

    const FScopedTransaction Tx(NSLOCTEXT("SmartUE", "SetActorTransformTx", "AI: Set Actor Transform"));
    Target->Modify();

    // location
    if (Args->HasTypedField<EJson::Object>(TEXT("location")))
    {
        const TSharedPtr<FJsonObject> L = Args->GetObjectField(TEXT("location"));
        FVector Loc = Target->GetActorLocation();
        if (L->HasTypedField<EJson::Number>(TEXT("x"))) Loc.X = L->GetNumberField(TEXT("x"));
        if (L->HasTypedField<EJson::Number>(TEXT("y"))) Loc.Y = L->GetNumberField(TEXT("y"));
        if (L->HasTypedField<EJson::Number>(TEXT("z"))) Loc.Z = L->GetNumberField(TEXT("z"));
        Target->SetActorLocation(Loc, false, nullptr, ETeleportType::None);
    }

    // rotation
    if (Args->HasTypedField<EJson::Object>(TEXT("rotation")))
    {
        const TSharedPtr<FJsonObject> R = Args->GetObjectField(TEXT("rotation"));
        FRotator Rot = Target->GetActorRotation();
        if (R->HasTypedField<EJson::Number>(TEXT("pitch"))) Rot.Pitch = R->GetNumberField(TEXT("pitch"));
        if (R->HasTypedField<EJson::Number>(TEXT("yaw"))) Rot.Yaw = R->GetNumberField(TEXT("yaw"));
        if (R->HasTypedField<EJson::Number>(TEXT("roll"))) Rot.Roll = R->GetNumberField(TEXT("roll"));
        Target->SetActorRotation(Rot, ETeleportType::None);
    }

    // scale
    if (Args->HasTypedField<EJson::Object>(TEXT("scale")))
    {
        const TSharedPtr<FJsonObject> S = Args->GetObjectField(TEXT("scale"));
        FVector Scale = Target->GetActorScale3D();
        if (S->HasTypedField<EJson::Number>(TEXT("x"))) Scale.X = S->GetNumberField(TEXT("x"));
        if (S->HasTypedField<EJson::Number>(TEXT("y"))) Scale.Y = S->GetNumberField(TEXT("y"));
        if (S->HasTypedField<EJson::Number>(TEXT("z"))) Scale.Z = S->GetNumberField(TEXT("z"));
        Target->SetActorScale3D(Scale);
    }

    Target->PostEditChange();
    Target->MarkPackageDirty();
    if (GEditor) GEditor->RedrawLevelEditingViewports(true);

    TSharedPtr<FJsonObject> Data = MakeShareable(new FJsonObject);
    Data->SetStringField(TEXT("actor"), Target->GetName());
    Data->SetStringField(TEXT("location"), Target->GetActorLocation().ToString());
    Data->SetStringField(TEXT("rotation"), Target->GetActorRotation().ToString());
    Data->SetStringField(TEXT("scale"), Target->GetActorScale3D().ToString());

    return {true, TEXT("已更新Actor变换"), Data};
}

// ---------------- FCreateActorBasicTool (升级版) ----------------
FCreateActorBasicTool::FCreateActorBasicTool()
{
    Spec.Name = TEXT("create_actor_basic");
    Spec.Description = TEXT("创建任意 Actor，支持内置类型、自定义类名、批量创建、复制现有对象。");

    Spec.Params = {
        // 核心参数
        {TEXT("type"), TEXT("string"), true, TEXT("内置类型（empty/point_light/directional_light/cube/sphere/cylinder/plane）或完整类名（如 BP_MyActor_C）")},
        {TEXT("count"), TEXT("number"), true, TEXT("创建数量，默认 1")},
        {TEXT("name_prefix"), TEXT("string"), true, TEXT("名称前缀，批量时自动加编号，默认空")},
        {TEXT("location"), TEXT("object"), true, TEXT("单个位置 {x,y,z}，所有对象共用")},
        {TEXT("locations"), TEXT("array"), true, TEXT("位置数组 [{x,y,z}, ...]，优先级高于 location")},

        // 复制功能
        {TEXT("copy_from"), TEXT("string"), true, TEXT("要复制的现有 Actor 名称/Label，支持模糊匹配")},
        {TEXT("copy_count"), TEXT("number"), true, TEXT("复制数量，默认 1，与 count 互斥")},

        // 可选高级参数
        {TEXT("rotation"), TEXT("object"), true, TEXT("旋转 {pitch,yaw,roll}")},
        {TEXT("scale"), TEXT("object"), true, TEXT("缩放 {x,y,z}")},
        {TEXT("attach_to"), TEXT("string"), true, TEXT("附加到现有 Actor 的名称")},
    };

    Spec.Permission = EToolPermission::Modify;
}

FAIToolResult FCreateActorBasicTool::Execute(const TSharedPtr<FJsonObject>& Args)
{
    UE_LOG(LogSmartUEAssistantAI, Log, TEXT("【create_actor_basic 执行开始】"));

    UWorld* World = SUEA::GetEditorWorld();
    if (!World) return {false, TEXT("未获取到编辑器世界"), nullptr};

    // ──────────────────────── 参数解析 ────────────────────────
    FString TypeStr = Args->HasField(TEXT("type")) ? Args->GetStringField(TEXT("type")).TrimStartAndEnd() : TEXT("");
    int32 Count = Args->HasField(TEXT("count")) ? FMath::Max(1, (int32)Args->GetNumberField(TEXT("count"))) : 1;
    FString NamePrefix = Args->HasField(TEXT("name_prefix")) ? Args->GetStringField(TEXT("name_prefix")).TrimStartAndEnd() : TEXT("");
    FString CopyFrom = Args->HasField(TEXT("copy_from")) ? Args->GetStringField(TEXT("copy_from")).TrimStartAndEnd() : TEXT("");
    int32 CopyCount = Args->HasField(TEXT("copy_count")) ? FMath::Max(1, (int32)Args->GetNumberField(TEXT("copy_count"))) : 0;

    // 如果有 copy_from，优先使用复制模式
    bool bIsCopyMode = !CopyFrom.IsEmpty() && CopyCount > 0;
    if (bIsCopyMode) Count = CopyCount;

    // 位置数组优先
    TArray<FVector> Locations;
    if (Args->HasTypedField<EJson::Array>(TEXT("locations")))
    {
        const TArray<TSharedPtr<FJsonValue>>& LocArr = Args->GetArrayField(TEXT("locations"));
        for (const TSharedPtr<FJsonValue>& LocVal : LocArr)
        {
            if (LocVal->Type == EJson::Object)
            {
                auto LocObj = LocVal->AsObject();
                FVector Loc;
                if (LocObj->HasField(TEXT("x"))) Loc.X = LocObj->GetNumberField(TEXT("x"));
                if (LocObj->HasField(TEXT("y"))) Loc.Y = LocObj->GetNumberField(TEXT("y"));
                if (LocObj->HasField(TEXT("z"))) Loc.Z = LocObj->GetNumberField(TEXT("z"));
                Locations.Add(Loc);
            }
        }
    }
    // 如果有单个 location，填充到数组
    else if (Args->HasTypedField<EJson::Object>(TEXT("location")))
    {
        auto LocObj = Args->GetObjectField(TEXT("location"));
        FVector Loc;
        if (LocObj->HasField(TEXT("x"))) Loc.X = LocObj->GetNumberField(TEXT("x"));
        if (LocObj->HasField(TEXT("y"))) Loc.Y = LocObj->GetNumberField(TEXT("y"));
        if (LocObj->HasField(TEXT("z"))) Loc.Z = LocObj->GetNumberField(TEXT("z"));
        Locations.Add(Loc);
    }

    // 如果位置数组不足 Count，补默认位置（递增偏移）
    while (Locations.Num() < Count)
    {
        FVector LastLoc = Locations.Num() > 0 ? Locations.Last() : FVector::ZeroVector;
        FVector Offset = FVector(200.0f * Locations.Num(), 0, 0); // 每多一个向 X 轴偏移 200cm
        Locations.Add(LastLoc + Offset);
    }

    // ──────────────────────── 确定要创建的类 ────────────────────────
    UClass* ActorClass = nullptr;

    if (!TypeStr.IsEmpty())
    {
        // 内置类型映射
        if (TypeStr.Equals(TEXT("empty"), ESearchCase::IgnoreCase))
            ActorClass = AActor::StaticClass();
        else if (TypeStr.Equals(TEXT("point_light"), ESearchCase::IgnoreCase))
            ActorClass = APointLight::StaticClass();
        else if (TypeStr.Equals(TEXT("directional_light"), ESearchCase::IgnoreCase))
            ActorClass = ADirectionalLight::StaticClass();
        else if (TypeStr.Equals(TEXT("cube"), ESearchCase::IgnoreCase) ||
                 TypeStr.Equals(TEXT("sphere"), ESearchCase::IgnoreCase) ||
                 TypeStr.Equals(TEXT("cylinder"), ESearchCase::IgnoreCase) ||
                 TypeStr.Equals(TEXT("plane"), ESearchCase::IgnoreCase))
        {
            // mesh 类型保持原有逻辑
            FString MeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");
            if (TypeStr == TEXT("sphere")) MeshPath = TEXT("/Engine/BasicShapes/Sphere.Sphere");
            else if (TypeStr == TEXT("cylinder")) MeshPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
            else if (TypeStr == TEXT("plane")) MeshPath = TEXT("/Engine/BasicShapes/Plane.Plane");

            UStaticMesh* Mesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *MeshPath));
            if (!Mesh) return {false, TEXT("无法加载网格资源"), nullptr};

            // 这里可以直接创建并返回，不走下面的通用创建
            TArray<TSharedPtr<FJsonValue>> CreatedActors;
            for (int32 i = 0; i < Count; ++i)
            {
                FString FinalName = NamePrefix.IsEmpty() ? TEXT("") : NamePrefix + TEXT("_") + FString::FromInt(i+1);
                FActorSpawnParameters SpawnParams;
                if (!FinalName.IsEmpty()) SpawnParams.Name = FName(*FinalName);

                AStaticMeshActor* MeshActor = World->SpawnActorDeferred<AStaticMeshActor>(
                    AStaticMeshActor::StaticClass(),
                    FTransform(FRotator::ZeroRotator, Locations[i]),
                    nullptr, nullptr,
                    ESpawnActorCollisionHandlingMethod::AlwaysSpawn
                );

                if (MeshActor)
                {
                    if (!FinalName.IsEmpty()) MeshActor->SetActorLabel(FinalName);
                    UStaticMeshComponent* MeshComp = MeshActor->GetStaticMeshComponent();
                    if (MeshComp) MeshComp->SetStaticMesh(Mesh);
                    MeshActor->FinishSpawning(FTransform(FRotator::ZeroRotator, Locations[i]));

                    TSharedPtr<FJsonObject> Data = MakeShareable(new FJsonObject);
                    Data->SetStringField(TEXT("name"), MeshActor->GetName());
                    Data->SetStringField(TEXT("label"), MeshActor->GetActorLabel());
                    CreatedActors.Add(MakeShared<FJsonValueObject>(Data));
                }
            }

            if (CreatedActors.Num() == 0) return {false, TEXT("StaticMeshActor 创建失败"), nullptr};

            TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
            Result->SetArrayField(TEXT("created_actors"), CreatedActors);
            return {true, FString::Printf(TEXT("已创建 %d 个 %s"), CreatedActors.Num(), *TypeStr), Result};
        }
        else
        {
            // 尝试作为类名加载
            ActorClass = FindObject<UClass>(ANY_PACKAGE, *TypeStr);
            if (!ActorClass)
            {
                FString WithC = TypeStr + TEXT("_C");
                ActorClass = FindObject<UClass>(ANY_PACKAGE, *WithC);
            }
            if (!ActorClass)
            {
                UE_LOG(LogSmartUEAssistantAI, Error, TEXT("未找到类或不支持类型: %s"), *TypeStr);
                return {false, FString::Printf(TEXT("未找到类或不支持类型: %s"), *TypeStr), nullptr};
            }
        }
    }
    else if (!bIsCopyMode)
    {
        return {false, TEXT("缺少 type 参数或 copy_from"), nullptr};
    }

    // ──────────────────────── 复制模式 ────────────────────────
    TArray<AActor*> SourceActors;
    if (bIsCopyMode)
    {
        AActor* Source = FindActorByNameFuzzy(World, CopyFrom, false);
        if (!Source)
        {
            return {false, FString::Printf(TEXT("copy_from 未找到 Actor: %s"), *CopyFrom), nullptr};
        }
        SourceActors.Add(Source);
    }

    // ──────────────────────── 开始创建 ────────────────────────
    TArray<TSharedPtr<FJsonValue>> CreatedActors;

    for (int32 i = 0; i < Count; ++i)
    {
        FString FinalName = NamePrefix;
        if (Count > 1) FinalName += FString::Printf(TEXT("_%d"), i+1);

        FActorSpawnParameters SpawnParams;
        if (!FinalName.IsEmpty()) SpawnParams.Name = FName(*FinalName);

        FTransform SpawnTransform(FRotator::ZeroRotator, Locations[i]);

        AActor* NewActor = nullptr;

        if (bIsCopyMode)
        {
            // 获取编辑器子系统（这是处理克隆最标准、最稳定的方式）
            UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    
            if (EditorActorSubsystem)
            {
                // 这一步会处理：内存克隆、大纲注册、组件注册、Undo记录
                NewActor = EditorActorSubsystem->DuplicateActor(SourceActors[0], World);
        
                if (NewActor)
                {
                    // 1. 设置变换
                    NewActor->SetActorTransform(SpawnTransform);
                    if (!FinalName.IsEmpty()) NewActor->SetActorLabel(FinalName);

                    // 2. 核心补救：强制注册所有组件（解决看不见的问题）
                    NewActor->RegisterAllComponents();

                    // 3. 强制通知编辑器：Actor 已经移动并需要重新渲染
                    NewActor->PostEditMove(true);

                    // 4. 确保在场景大纲中显示
                    NewActor->MarkComponentsRenderStateDirty();
                }
            }
            else
            {
                return {false, TEXT("复制失败"), nullptr};
            }
        }
        else
        {
            // 正常创建
            NewActor = World->SpawnActor<AActor>(ActorClass, SpawnTransform, SpawnParams);
        }

        if (NewActor && IsValid(NewActor))
        {
            if (!FinalName.IsEmpty()) NewActor->SetActorLabel(FinalName);

            TSharedPtr<FJsonObject> Data = MakeShareable(new FJsonObject);
            Data->SetStringField(TEXT("name"), NewActor->GetName());
            Data->SetStringField(TEXT("label"), NewActor->GetActorLabel());
            Data->SetStringField(TEXT("class"), NewActor->GetClass()->GetName());
            Data->SetStringField(TEXT("location"), NewActor->GetActorLocation().ToString());
            CreatedActors.Add(MakeShared<FJsonValueObject>(Data));
        }
        else
        {
            UE_LOG(LogSmartUEAssistantAI, Error, TEXT("第 %d 个 Actor 创建/复制失败"), i+1);
        }
    }

    if (CreatedActors.Num() == 0)
    {
        return {false, TEXT("所有 Actor 创建失败"), nullptr};
    }

    // 刷新编辑器
    if (GEditor)
    {
        GEditor->RedrawLevelEditingViewports(true);
        GEditor->NoteSelectionChange();
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetArrayField(TEXT("created_actors"), CreatedActors);
    Result->SetNumberField(TEXT("count"), CreatedActors.Num());

    return {true, FString::Printf(TEXT("已创建 %d 个 Actor"), CreatedActors.Num()), Result};
}

// ---------------- FDeleteActorTool ----------------
FDeleteActorTool::FDeleteActorTool()
{
    Spec.Name = TEXT("delete_actor");
    Spec.Description = TEXT("删除指定名称的Actor（精确匹配）");
    Spec.Params = {
        {TEXT("name"), TEXT("string"), false, TEXT("Actor名称（精确匹配）")}
    };
    Spec.Permission = EToolPermission::Modify;
    Spec.bRequireConfirm = true;
}

FAIToolResult FDeleteActorTool::Execute(const TSharedPtr<FJsonObject>& Args)
{
    if (!Args.IsValid() || !Args->HasTypedField<EJson::String>(TEXT("name")))
        return {false, TEXT("缺少参数：name"), nullptr};

    UWorld* World = SUEA::GetEditorWorld();
    if (!World) return {false, TEXT("未获取到编辑器世界"), nullptr};

    const FString Name = Args->GetStringField(TEXT("name"));
    AActor* Target = FindActorByNameFuzzy(World, Name, /*bExact*/true);
    if (!Target) return {false, FString::Printf(TEXT("未找到Actor：%s"), *Name), nullptr};

    const FScopedTransaction Tx(NSLOCTEXT("SmartUE", "DeleteActorTx", "AI: Delete Actor"));
    Target->Modify();
    bool bDestroyed = Target->Destroy();

    return {bDestroyed, bDestroyed ? TEXT("已删除Actor") : TEXT("删除失败"), nullptr};
}

static bool CollectTargetActors(UWorld* World, const TSharedPtr<FJsonObject>& Args, TArray<AActor*>& OutTargets)
{
    if (!World) return false;

    // 1) 显式 targets
    if (Args.IsValid() && Args->HasTypedField<EJson::Array>(TEXT("targets")))
    {
        const TArray<TSharedPtr<FJsonValue>>& Arr = Args->GetArrayField(TEXT("targets"));
        for (const auto& V : Arr)
        {
            FString Name;
            if (V.IsValid() && V->TryGetString(Name))
            {
                for (TActorIterator<AActor> It(World); It; ++It)
                {
                    if ((*It)->GetName().Equals(Name, ESearchCase::IgnoreCase))
                    {
                        OutTargets.Add(*It);
                        break;
                    }
                }
            }
        }
    }

    // 2) 若无 targets，使用当前选择集
    if (OutTargets.Num() == 0 && GEditor)
    {
        USelection* Sel = GEditor->GetSelectedActors();
        if (Sel)
        {
            for (FSelectionIterator It(*Sel); It; ++It)
            {
                if (AActor* A = Cast<AActor>(*It)) OutTargets.Add(A);
            }
        }
    }

    // 3) 若仍为空，按 name_contains / class 过滤所有Actor
    if (OutTargets.Num() == 0 && Args.IsValid())
    {
        const FString NameFilter = Args->HasTypedField<EJson::String>(TEXT("name_contains")) ? Args->GetStringField(TEXT("name_contains")) : TEXT("");
        const FString ClassFilter = Args->HasTypedField<EJson::String>(TEXT("class")) ? Args->GetStringField(TEXT("class")) : TEXT("");
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* A = *It;
            if (!NameFilter.IsEmpty() && !A->GetName().Contains(NameFilter, ESearchCase::IgnoreCase)) continue;
            if (!ClassFilter.IsEmpty() && !A->GetClass()->GetName().Contains(ClassFilter, ESearchCase::IgnoreCase)) continue;
            OutTargets.Add(A);
        }
    }

    // 去重
    {
        TSet<AActor*> Set;
        for (AActor* A : OutTargets)
        {
            if (IsValid(A)) Set.Add(A);
        }
        OutTargets.Reset(Set.Num());
        for (AActor* A : Set)
        {
            OutTargets.Add(A);
        }
        OutTargets.Sort([](const AActor& L, const AActor& R)
        {
            return L.GetName() < R.GetName();
        });
    }
 
     return OutTargets.Num() > 0;
}

// ---------------- FTransformActorsDeltaTool ----------------
FTransformActorsDeltaTool::FTransformActorsDeltaTool()
{
    Spec.Name = TEXT("transform_actors_delta");
    Spec.Description = TEXT("对多个Actor进行相对变换：位移/旋转/缩放增量，支持world/local空间，支持作用于选择集或按名称过滤");
    Spec.Params = {
        {TEXT("targets"), TEXT("array"), true, TEXT("可选：显式Actor名称数组，若省略则使用当前选择集；也可配合 name_contains/class 过滤")},
        {TEXT("name_contains"), TEXT("string"), true, TEXT("按名称包含过滤（当未提供targets时生效）")},
        {TEXT("class"), TEXT("string"), true, TEXT("按类名过滤（当未提供targets时生效）")},
        {TEXT("space"), TEXT("string"), true, TEXT("world 或 local，默认world")},
        {TEXT("translate"), TEXT("object"), true, TEXT("{x,y,z} cm 增量")},
        {TEXT("rotate"), TEXT("object"), true, TEXT("{pitch,yaw,roll} 度增量")},
        {TEXT("scale"), TEXT("object"), true, TEXT("{x,y,z} 缩放比例增量，如 0.1 表示 +10%")}
    };
    Spec.Permission = EToolPermission::Modify;
    Spec.bRequireConfirm = false;
}

FAIToolResult FTransformActorsDeltaTool::Execute(const TSharedPtr<FJsonObject>& Args)
{
    UWorld* World = SUEA::GetEditorWorld();
    if (!World) return {false, TEXT("未获取到编辑器世界"), nullptr};

    TArray<AActor*> Targets; if (!CollectTargetActors(World, Args, Targets))
    {
        return {false, TEXT("未找到目标Actor（请提供 targets，或选择一些Actor，或使用 name_contains/class 过滤）"), nullptr};
    }

    const FString Space = (Args.IsValid() && Args->HasTypedField<EJson::String>(TEXT("space"))) ? Args->GetStringField(TEXT("space")) : TEXT("world");

    FVector T(0); FRotator R(0); FVector S(0);
    if (Args.IsValid() && Args->HasTypedField<EJson::Object>(TEXT("translate")))
    {
        auto O = Args->GetObjectField(TEXT("translate"));
        if (O->HasTypedField<EJson::Number>(TEXT("x"))) T.X = O->GetNumberField(TEXT("x"));
        if (O->HasTypedField<EJson::Number>(TEXT("y"))) T.Y = O->GetNumberField(TEXT("y"));
        if (O->HasTypedField<EJson::Number>(TEXT("z"))) T.Z = O->GetNumberField(TEXT("z"));
    }
    if (Args.IsValid() && Args->HasTypedField<EJson::Object>(TEXT("rotate")))
    {
        auto O = Args->GetObjectField(TEXT("rotate"));
        if (O->HasTypedField<EJson::Number>(TEXT("pitch"))) R.Pitch = O->GetNumberField(TEXT("pitch"));
        if (O->HasTypedField<EJson::Number>(TEXT("yaw"))) R.Yaw = O->GetNumberField(TEXT("yaw"));
        if (O->HasTypedField<EJson::Number>(TEXT("roll"))) R.Roll = O->GetNumberField(TEXT("roll"));
    }
    if (Args.IsValid() && Args->HasTypedField<EJson::Object>(TEXT("scale")))
    {
        auto O = Args->GetObjectField(TEXT("scale"));
        if (O->HasTypedField<EJson::Number>(TEXT("x"))) S.X = O->GetNumberField(TEXT("x"));
        if (O->HasTypedField<EJson::Number>(TEXT("y"))) S.Y = O->GetNumberField(TEXT("y"));
        if (O->HasTypedField<EJson::Number>(TEXT("z"))) S.Z = O->GetNumberField(TEXT("z"));
    }

    const FScopedTransaction Tx(NSLOCTEXT("SmartUE", "TransformActorsDeltaTx", "AI: Transform Actors Delta"));
    int32 Affected = 0;

    for (AActor* A : Targets)
    {
        if (!IsValid(A)) continue;
        A->Modify();

        if (!T.IsNearlyZero())
        {
            if (Space.Equals(TEXT("local"), ESearchCase::IgnoreCase))
            {
                FVector NewLoc = A->GetActorLocation() + A->GetActorRotation().RotateVector(T);
                A->SetActorLocation(NewLoc, false, nullptr, ETeleportType::None);
            }
            else
            {
                A->AddActorWorldOffset(T, false, nullptr, ETeleportType::None);
            }
        }

        if (!R.IsNearlyZero())
        {
            if (Space.Equals(TEXT("local"), ESearchCase::IgnoreCase))
            {
                FRotator NewRot = (A->GetActorRotation().Quaternion() * R.Quaternion()).Rotator();
                A->SetActorRotation(NewRot, ETeleportType::None);
            }
            else
            {
                A->AddActorWorldRotation(R, false, nullptr, ETeleportType::None);
            }
        }

        if (!S.IsNearlyZero())
        {
            FVector Cur = A->GetActorScale3D();
            FVector NewS = Cur + S;
            A->SetActorScale3D(NewS);
        }

        ++Affected;
    }

    TSharedPtr<FJsonObject> Data = MakeShareable(new FJsonObject);
    Data->SetNumberField(TEXT("affected"), Affected);
    Data->SetStringField(TEXT("space"), Space);
    return {true, FString::Printf(TEXT("已对 %d 个Actor应用相对变换"), Affected), Data};
}

// ✅ 自动注册所有 Actor 工具
#include "ToolAutoRegister.h"

REGISTER_EDITOR_TOOL(FSelectAndFocusActorTool)
REGISTER_EDITOR_TOOL(FSetActorTransformTool)
REGISTER_EDITOR_TOOL(FCreateActorBasicTool)
REGISTER_EDITOR_TOOL(FDeleteActorTool)
REGISTER_EDITOR_TOOL(FTransformActorsDeltaTool)