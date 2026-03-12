// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/BatchOperationTools.h"
#include "SmartUEAssistantLog.h"
#include "Editor.h"
#include "Selection.h"
#include "Engine/Selection.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "EditorLevelUtils.h"
#include "LevelEditorViewport.h"
#include "Components/SceneComponent.h"
#include "Engine/CollisionProfile.h"

// ==================== Batch Rename Tool ====================

FBatchRenameActorsTool::FBatchRenameActorsTool()
{
	Spec.Name = TEXT("batch_rename_actors");
	Spec.Description = TEXT("Batch rename selected actors with prefix, suffix, and numbering");
	Spec.Permission = EToolPermission::Modify;
	Spec.bRequireConfirm = false;
	
	Spec.Params.Add({TEXT("Prefix"), TEXT("string"), true, TEXT("Prefix to add to actor names")});
	Spec.Params.Add({TEXT("Suffix"), TEXT("string"), true, TEXT("Suffix to add to actor names")});
	Spec.Params.Add({TEXT("StartIndex"), TEXT("number"), true, TEXT("Starting number for sequential numbering")});
	Spec.Params.Add({TEXT("RemovePrefix"), TEXT("string"), true, TEXT("Prefix to remove from existing names")});
}

FAIToolResult FBatchRenameActorsTool::Execute(const TSharedPtr<FJsonObject>& Args)
{
	if (!GEditor)
	{
		return {false, TEXT("Editor not available"), nullptr};
	}

	USelection* SelectedActors = GEditor->GetSelectedActors();
	if (!SelectedActors || SelectedActors->Num() == 0)
	{
		return {false, TEXT("No actors selected"), nullptr};
	}

	FString Prefix = Args->HasField(TEXT("Prefix")) ? Args->GetStringField(TEXT("Prefix")) : TEXT("");
	FString Suffix = Args->HasField(TEXT("Suffix")) ? Args->GetStringField(TEXT("Suffix")) : TEXT("");
	int32 StartIndex = Args->HasField(TEXT("StartIndex")) ? static_cast<int32>(Args->GetNumberField(TEXT("StartIndex"))) : 0;
	FString RemovePrefix = Args->HasField(TEXT("RemovePrefix")) ? Args->GetStringField(TEXT("RemovePrefix")) : TEXT("");

	TArray<AActor*> ActorsToRename;
	for (FSelectionIterator It(*SelectedActors); It; ++It)
	{
		if (AActor* Actor = Cast<AActor>(*It))
		{
			ActorsToRename.Add(Actor);
		}
	}

	int32 RenamedCount = 0;
	int32 CurrentIndex = StartIndex;

	for (AActor* Actor : ActorsToRename)
	{
		FString CurrentName = Actor->GetActorLabel();
		FString NewName = CurrentName;

		// Remove prefix if specified
		if (!RemovePrefix.IsEmpty() && CurrentName.StartsWith(RemovePrefix))
		{
			NewName = CurrentName.RightChop(RemovePrefix.Len());
		}

		// Build new name
		FString FinalName;
		if (!Prefix.IsEmpty())
		{
			FinalName += Prefix;
		}
		
		if (StartIndex >= 0)
		{
			FinalName += FString::Printf(TEXT("%s%d"), !Prefix.IsEmpty() ? TEXT("_") : TEXT(""), CurrentIndex++);
			if (!NewName.IsEmpty())
			{
				FinalName += TEXT("_") + NewName;
			}
		}
		else
		{
			FinalName += NewName;
		}

		if (!Suffix.IsEmpty())
		{
			FinalName += Suffix;
		}

		Actor->SetActorLabel(FinalName);
		RenamedCount++;
	}

	FString Message = FString::Printf(TEXT("Successfully renamed %d actors"), RenamedCount);
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("%s"), *Message);
	
	return {true, Message, nullptr};
}

// ==================== Batch Set Visibility Tool ====================

FBatchSetVisibilityTool::FBatchSetVisibilityTool()
{
	Spec.Name = TEXT("batch_set_visibility");
	Spec.Description = TEXT("Batch set visibility for selected actors");
	Spec.Permission = EToolPermission::Modify;
	Spec.bRequireConfirm = false;
	
	Spec.Params.Add({TEXT("Visible"), TEXT("boolean"), false, TEXT("Whether actors should be visible")});
	Spec.Params.Add({TEXT("ApplyToChildren"), TEXT("boolean"), true, TEXT("Apply to all children recursively")});
}

FAIToolResult FBatchSetVisibilityTool::Execute(const TSharedPtr<FJsonObject>& Args)
{
	if (!GEditor)
	{
		return {false, TEXT("Editor not available"), nullptr};
	}

	USelection* SelectedActors = GEditor->GetSelectedActors();
	if (!SelectedActors || SelectedActors->Num() == 0)
	{
		return {false, TEXT("No actors selected"), nullptr};
	}

	bool bVisible = Args->GetBoolField(TEXT("Visible"));
	bool bApplyToChildren = Args->HasField(TEXT("ApplyToChildren")) ? Args->GetBoolField(TEXT("ApplyToChildren")) : true;

	int32 ModifiedCount = 0;

	for (FSelectionIterator It(*SelectedActors); It; ++It)
	{
		if (AActor* Actor = Cast<AActor>(*It))
		{
			Actor->SetActorHiddenInGame(!bVisible);
			Actor->SetIsTemporarilyHiddenInEditor(!bVisible);
			Actor->bHiddenEd=!bVisible;
			if (bApplyToChildren)
			{
				TArray<AActor*> ChildActors;
				Actor->GetAllChildActors(ChildActors, true);
				for (AActor* ChildActor : ChildActors)
				{
					ChildActor->SetActorHiddenInGame(!bVisible);
					ChildActor->SetIsTemporarilyHiddenInEditor(!bVisible);
					ChildActor->bHiddenEd=!bVisible;
				}
				ModifiedCount += ChildActors.Num();
			}
			ModifiedCount++;
		}
	}

	FString Message = FString::Printf(TEXT("将 %d 个对象的可见性设置为 %s"), 
		ModifiedCount, bVisible ? TEXT("可见") : TEXT("隐藏"));
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("%s"), *Message);
	
	return {true, Message, nullptr};
}

// ==================== Batch Set Mobility Tool ====================

FBatchSetMobilityTool::FBatchSetMobilityTool()
{
	Spec.Name = TEXT("batch_set_mobility");
	Spec.Description = TEXT("Batch set mobility for selected actors");
	Spec.Permission = EToolPermission::Modify;
	Spec.bRequireConfirm = false;
	
	Spec.Params.Add({TEXT("Mobility"), TEXT("string"), false, TEXT("Mobility type: Static, Stationary, or Movable")});
}

FAIToolResult FBatchSetMobilityTool::Execute(const TSharedPtr<FJsonObject>& Args)
{
	if (!GEditor)
	{
		return {false, TEXT("Editor not available"), nullptr};
	}

	USelection* SelectedActors = GEditor->GetSelectedActors();
	if (!SelectedActors || SelectedActors->Num() == 0)
	{
		return {false, TEXT("No actors selected"), nullptr};
	}

	FString MobilityStr = Args->GetStringField(TEXT("Mobility"));
	EComponentMobility::Type Mobility;

	if (MobilityStr.Equals(TEXT("Static"), ESearchCase::IgnoreCase))
	{
		Mobility = EComponentMobility::Static;
	}
	else if (MobilityStr.Equals(TEXT("Stationary"), ESearchCase::IgnoreCase))
	{
		Mobility = EComponentMobility::Stationary;
	}
	else if (MobilityStr.Equals(TEXT("Movable"), ESearchCase::IgnoreCase))
	{
		Mobility = EComponentMobility::Movable;
	}
	else
	{
		return {false, FString::Printf(TEXT("无效的移动类型: %s. 请使用静态、固定或可移动类型的对象"), *MobilityStr), nullptr};
	}

	int32 ModifiedCount = 0;

	for (FSelectionIterator It(*SelectedActors); It; ++It)
	{
		if (AActor* Actor = Cast<AActor>(*It))
		{
			if (USceneComponent* RootComp = Actor->GetRootComponent())
			{
				RootComp->SetMobility(Mobility);
				ModifiedCount++;
			}
		}
	}

	FString Message = FString::Printf(TEXT("为 %s 名参与者将移动性设置为 %d "), *MobilityStr, ModifiedCount);
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("%s"), *Message);
	
	return {true, Message, nullptr};
}

// ==================== Batch Move To Level Tool ====================

FBatchMoveToLevelTool::FBatchMoveToLevelTool()
{
	Spec.Name = TEXT("batch_move_to_level");
	Spec.Description = TEXT("Move selected actors to a specific level");
	Spec.Permission = EToolPermission::Modify;
	Spec.bRequireConfirm = true;
	
	Spec.Params.Add({TEXT("LevelName"), TEXT("string"), false, TEXT("Name of the target level")});
}

FAIToolResult FBatchMoveToLevelTool::Execute(const TSharedPtr<FJsonObject>& Args)
{
	if (!GEditor)
	{
		return {false, TEXT("Editor not available"), nullptr};
	}

	USelection* SelectedActors = GEditor->GetSelectedActors();
	if (!SelectedActors || SelectedActors->Num() == 0)
	{
		return {false, TEXT("No actors selected"), nullptr};
	}

	FString LevelName = Args->GetStringField(TEXT("LevelName"));
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return {false, TEXT("No world available"), nullptr};
	}

	// Find target level
	ULevel* TargetLevel = nullptr;
	for (ULevel* Level : World->GetLevels())
	{
		if (Level && Level->GetOuter()->GetName().Contains(LevelName))
		{
			TargetLevel = Level;
			break;
		}
	}

	if (!TargetLevel)
	{
		return {false, FString::Printf(TEXT("Level not found: %s"), *LevelName), nullptr};
	}

	TArray<AActor*> ActorsToMove;
	for (FSelectionIterator It(*SelectedActors); It; ++It)
	{
		if (AActor* Actor = Cast<AActor>(*It))
		{
			ActorsToMove.Add(Actor);
		}
	}

	// Use UEditorLevelUtils::MoveActorsToLevel for batch operation
	if (ActorsToMove.Num() > 0)
	{
		UEditorLevelUtils::MoveActorsToLevel(ActorsToMove, TargetLevel);
	}

	FString Message = FString::Printf(TEXT("移动 %d 个对象到关卡: %s"), ActorsToMove.Num(), *LevelName);
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("%s"), *Message);
	
	return {true, Message, nullptr};
}

// ==================== Batch Set Tags Tool ====================

FBatchSetTagsTool::FBatchSetTagsTool()
{
	Spec.Name = TEXT("batch_set_tags");
	Spec.Description = TEXT("Batch set, add, or remove tags for selected actors");
	Spec.Permission = EToolPermission::Modify;
	Spec.bRequireConfirm = false;
	
	Spec.Params.Add({TEXT("Tags"), TEXT("array"), false, TEXT("Array of tag strings")});
	Spec.Params.Add({TEXT("Mode"), TEXT("string"), false, TEXT("Mode: Set, Add, or Remove")});
}

FAIToolResult FBatchSetTagsTool::Execute(const TSharedPtr<FJsonObject>& Args)
{
	if (!GEditor)
	{
		return {false, TEXT("Editor not available"), nullptr};
	}

	USelection* SelectedActors = GEditor->GetSelectedActors();
	if (!SelectedActors || SelectedActors->Num() == 0)
	{
		return {false, TEXT("No actors selected"), nullptr};
	}

	FString Mode = Args->GetStringField(TEXT("Mode"));
	const TArray<TSharedPtr<FJsonValue>>* TagsArray;
	if (!Args->TryGetArrayField(TEXT("Tags"), TagsArray))
	{
		return {false, TEXT("Tags parameter must be an array"), nullptr};
	}

	TArray<FName> Tags;
	for (const TSharedPtr<FJsonValue>& TagValue : *TagsArray)
	{
		Tags.Add(FName(*TagValue->AsString()));
	}

	int32 ModifiedCount = 0;

	for (FSelectionIterator It(*SelectedActors); It; ++It)
	{
		if (AActor* Actor = Cast<AActor>(*It))
		{
			if (Mode.Equals(TEXT("Set"), ESearchCase::IgnoreCase))
			{
				Actor->Tags.Empty();
				Actor->Tags.Append(Tags);
			}
			else if (Mode.Equals(TEXT("Add"), ESearchCase::IgnoreCase))
			{
				for (FName Tag : Tags)
				{
					Actor->Tags.AddUnique(Tag);
				}
			}
			else if (Mode.Equals(TEXT("Remove"), ESearchCase::IgnoreCase))
			{
				for (FName Tag : Tags)
				{
					Actor->Tags.Remove(Tag);
				}
			}
			else
			{
				return {false, FString::Printf(TEXT("无效模式: %s. 请使用“设置”、“添加”或“删除”"), *Mode), nullptr};
			}
			ModifiedCount++;
		}
	}

	FString Message = FString::Printf(TEXT("%s 个标签的 %d 个对象"), *Mode, ModifiedCount);
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("%s"), *Message);
	
	return {true, Message, nullptr};
}

// ==================== Align To Ground Tool ====================

FAlignToGroundTool::FAlignToGroundTool()
{
	Spec.Name = TEXT("align_to_ground");
	Spec.Description = TEXT("Align selected actors to ground/surface with optional rotation alignment");
	Spec.Permission = EToolPermission::Modify;
	Spec.bRequireConfirm = false;
	
	Spec.Params.Add({TEXT("AlignRotation"), TEXT("boolean"), true, TEXT("Align rotation to surface normal")});
	Spec.Params.Add({TEXT("Offset"), TEXT("number"), true, TEXT("Z-axis offset from surface")});
}

FAIToolResult FAlignToGroundTool::Execute(const TSharedPtr<FJsonObject>& Args)
{
	if (!GEditor)
	{
		return {false, TEXT("Editor not available"), nullptr};
	}

	USelection* SelectedActors = GEditor->GetSelectedActors();
	if (!SelectedActors || SelectedActors->Num() == 0)
	{
		return {false, TEXT("No actors selected"), nullptr};
	}

	bool bAlignRotation = Args->HasField(TEXT("AlignRotation")) ? Args->GetBoolField(TEXT("AlignRotation")) : false;
	float Offset = Args->HasField(TEXT("Offset")) ? static_cast<float>(Args->GetNumberField(TEXT("Offset"))) : 0.0f;

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return {false, TEXT("No world available"), nullptr};
	}

	int32 AlignedCount = 0;

	for (FSelectionIterator It(*SelectedActors); It; ++It)
	{
		if (AActor* Actor = Cast<AActor>(*It))
		{
			FVector ActorLocation = Actor->GetActorLocation();
			FVector TraceStart = ActorLocation + FVector(0, 0, 1000);
			FVector TraceEnd = ActorLocation - FVector(0, 0, 10000);

			FHitResult HitResult;
			FCollisionQueryParams QueryParams;
			QueryParams.AddIgnoredActor(Actor);

			if (World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams))
			{
				FVector NewLocation = HitResult.Location + FVector(0, 0, Offset);
				Actor->SetActorLocation(NewLocation);

				if (bAlignRotation && HitResult.Normal.SizeSquared() > 0.01f)
				{
					FRotator NewRotation = HitResult.Normal.Rotation();
					NewRotation.Pitch -= 90.0f; // Adjust for standing upright
					Actor->SetActorRotation(NewRotation);
				}

				AlignedCount++;
			}
		}
	}

	FString Message = FString::Printf(TEXT("Aligned %d actors to ground"), AlignedCount);
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("%s"), *Message);
	
	return {true, Message, nullptr};
}

// ==================== Distribute Actors Tool ====================

FDistributeActorsTool::FDistributeActorsTool()
{
	Spec.Name = TEXT("distribute_actors");
	Spec.Description = TEXT("Distribute selected actors in various patterns (Line, Grid, Circle, Random)");
	Spec.Permission = EToolPermission::Modify;
	Spec.bRequireConfirm = false;
	
	Spec.Params.Add({TEXT("Pattern"), TEXT("string"), false, TEXT("Pattern: Line, Grid, Circle, or Random")});
	Spec.Params.Add({TEXT("Spacing"), TEXT("number"), false, TEXT("Distance between actors")});
	Spec.Params.Add({TEXT("Rows"), TEXT("number"), true, TEXT("Number of rows (for Grid pattern)")});
	Spec.Params.Add({TEXT("Columns"), TEXT("number"), true, TEXT("Number of columns (for Grid pattern)")});
	Spec.Params.Add({TEXT("Radius"), TEXT("number"), true, TEXT("Radius (for Circle pattern)")});
}

FAIToolResult FDistributeActorsTool::Execute(const TSharedPtr<FJsonObject>& Args)
{
	if (!GEditor)
	{
		return {false, TEXT("Editor not available"), nullptr};
	}

	USelection* SelectedActors = GEditor->GetSelectedActors();
	if (!SelectedActors || SelectedActors->Num() == 0)
	{
		return {false, TEXT("No actors selected"), nullptr};
	}

	FString Pattern = Args->GetStringField(TEXT("Pattern"));
	float Spacing = static_cast<float>(Args->GetNumberField(TEXT("Spacing")));

	TArray<AActor*> ActorsArray;
	for (FSelectionIterator It(*SelectedActors); It; ++It)
	{
		if (AActor* Actor = Cast<AActor>(*It))
		{
			ActorsArray.Add(Actor);
		}
	}

	if (ActorsArray.Num() == 0)
	{
		return {false, TEXT("No valid actors to distribute"), nullptr};
	}

	// Get center point from first actor
	FVector CenterPoint = ActorsArray[0]->GetActorLocation();

	if (Pattern.Equals(TEXT("Line"), ESearchCase::IgnoreCase))
	{
		for (int32 i = 0; i < ActorsArray.Num(); ++i)
		{
			FVector NewLocation = CenterPoint + FVector(i * Spacing, 0, 0);
			ActorsArray[i]->SetActorLocation(NewLocation);
		}
	}
	else if (Pattern.Equals(TEXT("Grid"), ESearchCase::IgnoreCase))
	{
		int32 Columns = Args->HasField(TEXT("Columns")) ? static_cast<int32>(Args->GetNumberField(TEXT("Columns"))) : 5;
		
		for (int32 i = 0; i < ActorsArray.Num(); ++i)
		{
			int32 Row = i / Columns;
			int32 Col = i % Columns;
			FVector NewLocation = CenterPoint + FVector(Col * Spacing, Row * Spacing, 0);
			ActorsArray[i]->SetActorLocation(NewLocation);
		}
	}
	else if (Pattern.Equals(TEXT("Circle"), ESearchCase::IgnoreCase))
	{
		float Radius = Args->HasField(TEXT("Radius")) ? static_cast<float>(Args->GetNumberField(TEXT("Radius"))) : Spacing * ActorsArray.Num() / (2 * PI);
		
		for (int32 i = 0; i < ActorsArray.Num(); ++i)
		{
			float Angle = (2 * PI * i) / ActorsArray.Num();
			FVector Offset = FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0);
			ActorsArray[i]->SetActorLocation(CenterPoint + Offset);
		}
	}
	else if (Pattern.Equals(TEXT("Random"), ESearchCase::IgnoreCase))
	{
		for (int32 i = 1; i < ActorsArray.Num(); ++i) // Skip first actor at center
		{
			FVector RandomOffset = FVector(
				FMath::FRandRange(-Spacing, Spacing),
				FMath::FRandRange(-Spacing, Spacing),
				0
			);
			ActorsArray[i]->SetActorLocation(CenterPoint + RandomOffset);
		}
	}
	else
	{
		return {false, FString::Printf(TEXT("无效模式: %s. 请使用直线、网格、圆或随机"), *Pattern), nullptr};
	}

	FString Message = FString::Printf(TEXT("已经分布 %d 个对象进行 %s 模式分布"), ActorsArray.Num(), *Pattern);
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("%s"), *Message);
	
	return {true, Message, nullptr};
}

// ✅ 自动注册所有批量操作工具
#include "ToolAutoRegister.h"

REGISTER_EDITOR_TOOL(FBatchRenameActorsTool)
REGISTER_EDITOR_TOOL(FBatchSetVisibilityTool)
REGISTER_EDITOR_TOOL(FBatchSetMobilityTool)
REGISTER_EDITOR_TOOL(FBatchMoveToLevelTool)
REGISTER_EDITOR_TOOL(FBatchSetTagsTool)
REGISTER_EDITOR_TOOL(FAlignToGroundTool)
REGISTER_EDITOR_TOOL(FDistributeActorsTool)
