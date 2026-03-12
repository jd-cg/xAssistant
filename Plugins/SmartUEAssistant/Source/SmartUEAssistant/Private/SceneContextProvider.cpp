// 场景上下文采集提供者实现（编辑器专用）
#include "SceneContextProvider.h"
#include "SmartUEAssistantSettings.h"
#include "FSceneContext.h" // 统一的数据模型

#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "UnrealClient.h"              // FViewport 定义
#include "EditorViewportClient.h"      // FEditorViewportClient 定义
#include "LevelEditor.h"
#include "Engine/Selection.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopedSlowTask.h"
#include "JsonObjectConverter.h"

static void CollectViewLocation(FVector& OutLoc, FRotator& OutRot)
{
    OutLoc = FVector::ZeroVector; OutRot = FRotator::ZeroRotator;
    if (!GEditor) return;
    FViewport* ActiveViewport = GEditor->GetActiveViewport();
    if (!ActiveViewport) return;
    FEditorViewportClient* ViewClient = static_cast<FEditorViewportClient*>(ActiveViewport->GetClient());
    if (!ViewClient) return;
    OutLoc = ViewClient->GetViewLocation();
    OutRot = ViewClient->GetViewRotation();
}

FString FSceneContextProvider::BuildSceneSummaryJson(const USmartUEAssistantSettings* Settings)
{
    if (!GEditor) return FString();

    UWorld* World = nullptr;
    for (const FWorldContext& Ctx : GEditor->GetWorldContexts())
    {
        if (Ctx.WorldType == EWorldType::Editor)
        {
            World = Ctx.World();
            break;
        }
    }
    if (!World) return FString();

    // 相机位置（用于排序和上下文）
    FVector CamLoc = FVector::ZeroVector;
    FRotator CamRot = FRotator::ZeroRotator;
    CollectViewLocation(CamLoc, CamRot);

    // 结构定义（精简字段）
    struct FActorBrief
    {
        FString n;       // name
        FString c;       // class
        FString l;       // location compact
        FString r;       // rotation compact
        FString s;       // scale compact
    };

    struct FLevelBrief
    {
        FString level;
        TArray<FActorBrief> actors;
    };

    TArray<FLevelBrief> Levels;
    TArray<FActorBrief> Selected;

    const int32 MaxActorsPerLevel = Settings ? FMath::Clamp(Settings->MaxActorsInSummary, 1, 500) : 100;

    // 选中对象（优先）
    if (GEditor->GetSelectedActors())
    {
        for (FSelectionIterator It(*GEditor->GetSelectedActors()); It; ++It)
        {
            if (AActor* A = Cast<AActor>(*It))
            {
                FActorBrief B;
                B.n = A->GetName();
                B.c = A->GetClass()->GetName();
                B.l = A->GetActorLocation().ToCompactString();
                B.r = A->GetActorRotation().ToCompactString();
                B.s = A->GetActorScale3D().ToCompactString();
                Selected.Add(MoveTemp(B));
            }
        }
    }

    // 关卡演员（限制数量，按相机距离排序）
    for (ULevel* Level : World->GetLevels())
    {
        if (!Level) continue;

        FLevelBrief LB;
        LB.level = Level->GetOuter()->GetName();

        int32 Count = 0;
        TArray<FActorBrief> LevelActors;

        for (AActor* A : Level->Actors)
        {
            if (!A || A->IsA<AWorldSettings>()) continue;

            FActorBrief B;
            B.n = A->GetName();
            B.c = A->GetClass()->GetName();
            B.l = A->GetActorLocation().ToCompactString();
            B.r = A->GetActorRotation().ToCompactString();
            B.s = A->GetActorScale3D().ToCompactString();
            LevelActors.Add(MoveTemp(B));

            if (++Count >= MaxActorsPerLevel) break;
        }

        // 按相机距离排序（最近优先）
        LevelActors.Sort([&CamLoc](const FActorBrief& A, const FActorBrief& B)
        {
            return FVector::DistSquared(A.l == B.l ? FVector::ZeroVector : FVector(), CamLoc) <
                   FVector::DistSquared(B.l == B.l ? FVector::ZeroVector : FVector(), CamLoc);
        });

        LB.actors = MoveTemp(LevelActors);
        Levels.Add(MoveTemp(LB));
    }

    // 构建 JSON（极简键名）
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();

    // levels
    TArray<TSharedPtr<FJsonValue>> LevelsArray;
    for (const FLevelBrief& LB : Levels)
    {
        TSharedRef<FJsonObject> LObj = MakeShared<FJsonObject>();
        LObj->SetStringField(TEXT("level"), LB.level);

        TArray<TSharedPtr<FJsonValue>> ActorsArray;
        for (const FActorBrief& A : LB.actors)
        {
            TSharedRef<FJsonObject> AObj = MakeShared<FJsonObject>();
            AObj->SetStringField(TEXT("n"), A.n);
            AObj->SetStringField(TEXT("c"), A.c);
            AObj->SetStringField(TEXT("l"), A.l);
            AObj->SetStringField(TEXT("r"), A.r);
            AObj->SetStringField(TEXT("s"), A.s);
            ActorsArray.Add(MakeShared<FJsonValueObject>(AObj));
        }
        LObj->SetArrayField(TEXT("a"), ActorsArray);  // actors → a
        LevelsArray.Add(MakeShared<FJsonValueObject>(LObj));
    }
    Root->SetArrayField(TEXT("levels"), LevelsArray);

    // selected
    if (!Selected.IsEmpty())
    {
        TArray<TSharedPtr<FJsonValue>> SelArray;
        for (const FActorBrief& A : Selected)
        {
            TSharedRef<FJsonObject> AObj = MakeShared<FJsonObject>();
            AObj->SetStringField(TEXT("n"), A.n);
            AObj->SetStringField(TEXT("c"), A.c);
            AObj->SetStringField(TEXT("l"), A.l);
            AObj->SetStringField(TEXT("r"), A.r);
            AObj->SetStringField(TEXT("s"), A.s);
            SelArray.Add(MakeShared<FJsonValueObject>(AObj));
        }
        Root->SetArrayField(TEXT("sel"), SelArray);  // selected → sel
    }

    // view camera（强烈推荐保留）
    if (Settings && Settings->bIncludeViewportCamera)
    {
        TSharedRef<FJsonObject> View = MakeShared<FJsonObject>();
        View->SetStringField(TEXT("l"), CamLoc.ToCompactString());
        View->SetStringField(TEXT("r"), CamRot.ToCompactString());
        Root->SetObjectField(TEXT("view"), View);
    }

    FString JsonStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
    FJsonSerializer::Serialize(Root, Writer);
    return JsonStr;
}
//
// FString FSceneContextProvider::BuildSceneSummaryJson(const USmartUEAssistantSettings* Settings)
// {
//     // 容错：无编辑器或无世界
//     if (!GEditor) return FString();
//
//     UWorld* EditorWorld = nullptr;
//     for (const FWorldContext& Ctx : GEditor->GetWorldContexts())
//     {
//         if (Ctx.WorldType == EWorldType::Editor)
//         {
//             EditorWorld = Ctx.World();
//             break;
//         }
//     }
//     if (!EditorWorld) return FString();
//
//     // 使用统一数据模型（后续便于与参考项目保持一致）
//     FSceneContext Brief;
//     Brief.WorldType = TEXT("Editor");
//
//     // 预取视口相机信息（用于排序与可选输出）
//     FVector CamLoc = FVector::ZeroVector; FRotator CamRot = FRotator::ZeroRotator;
//     CollectViewLocation(CamLoc, CamRot);
//
//     // 选中集
//     if (GEditor->GetSelectedActors())
//     {
//         for (FSelectionIterator It(*GEditor->GetSelectedActors()); It; ++It)
//         {
//             if (AActor* A = Cast<AActor>(*It))
//             {
//                 FSceneActorBrief B;
//                 B.Name = A->GetName();
//                 B.Class = A->GetClass()->GetName();
//                 B.Location = A->GetActorLocation();
//                 B.Rotation = A->GetActorRotation();
//                 B.Scale = A->GetActorScale3D();
//                 B.ComponentCount = A->GetComponents().Num();
//                 Brief.Selected.Add(MoveTemp(B));
//             }
//         }
//     }
//
//     // 关卡与Actor（限制数量，避免过大）
//     const int32 MaxActors = Settings ? FMath::Clamp(Settings->MaxActorsInSummary, 1, 500) : 100;
//     int32 TotalActorCount = 0;
//     TMap<FString, int32> ClassCounts;
//     for (ULevel* Level : EditorWorld->GetLevels())
//     {
//         if (!Level) continue;
//         FSceneLevelBrief LB;
//         LB.LevelName = Level->GetOuter()->GetName();
//         int32 Count = 0;
//         for (AActor* A : Level->Actors)
//         {
//             if (!A || A->IsA<AWorldSettings>()) continue;
//             FSceneActorBrief B;
//             B.Name = A->GetName();
//             B.Class = A->GetClass()->GetName();
//             B.Location = A->GetActorLocation();
//             B.Rotation = A->GetActorRotation();
//             B.Scale = A->GetActorScale3D();
//             B.ComponentCount = A->GetComponents().Num();
//             LB.Actors.Add(MoveTemp(B));
//
//             ++TotalActorCount;
//             int32& C = ClassCounts.FindOrAdd(LB.Actors.Last().Class);
//             ++C;
//
//             if (++Count >= MaxActors) break;
//         }
//
//         // 按与视口相机的距离升序排序（最近的优先）
//         LB.Actors.Sort([&CamLoc](const FSceneActorBrief& L, const FSceneActorBrief& R)
//         {
//             const double DL = FVector::DistSquared(L.Location, CamLoc);
//             const double DR = FVector::DistSquared(R.Location, CamLoc);
//             return DL < DR;
//         });
//
//         Brief.Levels.Add(MoveTemp(LB));
//     }
//
//     // 保存视口信息到摘要
//     Brief.ViewLocation = CamLoc;
//     Brief.ViewRotation = CamRot;
//
//     // 转JSON（轻量）
//     TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
//     Root->SetStringField(TEXT("world_type"), Brief.WorldType);
//
//     // levels
//     TArray<TSharedPtr<FJsonValue>> LevelsArray;
//     for (const FSceneLevelBrief& LB : Brief.Levels)
//     {
//         TSharedRef<FJsonObject> LObj = MakeShared<FJsonObject>();
//         LObj->SetStringField(TEXT("level"), LB.LevelName);
//         TArray<TSharedPtr<FJsonValue>> AArray;
//         for (const FSceneActorBrief& B : LB.Actors)
//         {
//             TSharedRef<FJsonObject> AObj = MakeShared<FJsonObject>();
//             AObj->SetStringField(TEXT("name"), B.Name);
//             AObj->SetStringField(TEXT("class"), B.Class);
//             AObj->SetStringField(TEXT("loc"), B.Location.ToCompactString());
//             AObj->SetStringField(TEXT("rot"), B.Rotation.ToCompactString());
//             AObj->SetStringField(TEXT("scale"), B.Scale.ToString());
//             if (!Settings || Settings->bIncludeComponentSummary)
//             {
//                 AObj->SetNumberField(TEXT("components"), B.ComponentCount);
//             }
//             AArray.Add(MakeShared<FJsonValueObject>(AObj));
//         }
//         LObj->SetArrayField(TEXT("actors"), AArray);
//         LevelsArray.Add(MakeShared<FJsonValueObject>(LObj));
//     }
//     Root->SetArrayField(TEXT("levels"), LevelsArray);
//
//     // selected
//     TArray<TSharedPtr<FJsonValue>> SelArray;
//     for (const FSceneActorBrief& B : Brief.Selected)
//     {
//         TSharedRef<FJsonObject> AObj = MakeShared<FJsonObject>();
//         AObj->SetStringField(TEXT("name"), B.Name);
//         AObj->SetStringField(TEXT("class"), B.Class);
//         AObj->SetStringField(TEXT("loc"), B.Location.ToCompactString());
//         AObj->SetStringField(TEXT("rot"), B.Rotation.ToCompactString());
//         AObj->SetStringField(TEXT("scale"), B.Scale.ToString());
//         if (!Settings || Settings->bIncludeComponentSummary)
//         {
//             AObj->SetNumberField(TEXT("components"), B.ComponentCount);
//         }
//         SelArray.Add(MakeShared<FJsonValueObject>(AObj));
//     }
//     Root->SetArrayField(TEXT("selected"), SelArray);
//
//     // view（可选）
//     if (Settings && Settings->bIncludeViewportCamera)
//     {
//         TSharedRef<FJsonObject> View = MakeShared<FJsonObject>();
//         View->SetStringField(TEXT("location"), Brief.ViewLocation.ToCompactString());
//         View->SetStringField(TEXT("rotation"), Brief.ViewRotation.ToCompactString());
//         Root->SetObjectField(TEXT("view"), View);
//     }
//
//     // 统计信息（类型分布、关卡数、Actor数等）
//     {
//         TSharedRef<FJsonObject> Stats = MakeShared<FJsonObject>();
//         Stats->SetNumberField(TEXT("levels"), Brief.Levels.Num());
//         Stats->SetNumberField(TEXT("selected_count"), Brief.Selected.Num());
//         Stats->SetNumberField(TEXT("actor_count"), TotalActorCount);
//
//         // Top-N 类别计数
//         TArray<TPair<FString,int32>> ClassArray;
//         ClassArray.Reserve(ClassCounts.Num());
//         for (const TPair<FString,int32>& KVP : ClassCounts)
//         {
//             ClassArray.Add(TPair<FString,int32>(KVP.Key, KVP.Value));
//         }
//         ClassArray.Sort([](const TPair<FString,int32>& A, const TPair<FString,int32>& B){ return A.Value > B.Value; });
//
//         TSharedRef<FJsonObject> CCObj = MakeShared<FJsonObject>();
//         const int32 TopN = FMath::Min(8, ClassArray.Num());
//         for (int32 i=0; i<TopN; ++i)
//         {
//             CCObj->SetNumberField(ClassArray[i].Key, ClassArray[i].Value);
//         }
//         Stats->SetObjectField(TEXT("class_counts"), CCObj);
//
//         Root->SetObjectField(TEXT("stats"), Stats);
//     }
//
//     FString Out;
//     const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
//     FJsonSerializer::Serialize(Root, Writer);
//     return Out;
// }
