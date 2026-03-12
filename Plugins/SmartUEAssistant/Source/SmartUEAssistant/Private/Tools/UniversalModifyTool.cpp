// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/UniversalModifyTool.h"
#include "PropertyModificationHelper.h"
#include "SmartUEAssistantLog.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/LightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Light.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Selection.h"

FUniversalModifyTool::FUniversalModifyTool()
{
	Spec.Name = TEXT("modify");
	Spec.Description = TEXT("Universal tool to modify any property on any object. Automatically finds targets and applies changes. Use this for all property modifications.");
	Spec.Permission = EToolPermission::Modify;
	Spec.bRequireConfirm = false;
	
	Spec.Params.Add({TEXT("Target"), TEXT("string"), false, TEXT("Target description: 'Light', 'Selected', 'Cube', etc.")});
	Spec.Params.Add({TEXT("PropertyName"), TEXT("string"), false, TEXT("EXACT UE property name: 'LightColor', 'Intensity', 'RelativeLocation', 'RelativeRotation', 'RelativeScale3D', 'bHidden', etc.")});
	Spec.Params.Add({TEXT("Value"), TEXT("string|number|object"), false, TEXT("New value: color name (string), number, or object like {\"X\":1,\"Y\":2,\"Z\":3} or {\"R\":1,\"G\":0,\"B\":0}")});
}

TArray<AActor*> FUniversalModifyTool::FindTargetActors(const FString& TargetDescription)
{
	TArray<AActor*> Results;
	
	if (!GEditor)
	{
		return Results;
	}

	FString Target = TargetDescription.ToLower();
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("Finding targets for: %s"), *TargetDescription);

	// Case 1: Selected objects
	if (Target.Contains(TEXT("选中")) || Target.Contains(TEXT("selected")))
	{
		UE_LOG(LogSmartUEAssistantTools, Log, TEXT("  Using selected actors"));
		USelection* SelectedActors = GEditor->GetSelectedActors();
		if (SelectedActors && SelectedActors->Num() > 0)
		{
			for (FSelectionIterator It(*SelectedActors); It; ++It)
			{
				if (AActor* Actor = Cast<AActor>(*It))
				{
					Results.Add(Actor);
				}
			}
		}
		UE_LOG(LogSmartUEAssistantTools, Log, TEXT("  Found %d selected actors"), Results.Num());
		return Results;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return Results;
	}

	// Case 2: All lights / 所有灯光 / 灯光
	if (Target.Contains(TEXT("灯光")) || Target.Contains(TEXT("light")))
	{
		UE_LOG(LogSmartUEAssistantTools, Log, TEXT("  Searching for lights"));
		for (TActorIterator<ALight> It(World); It; ++It)
		{
			Results.Add(*It);
		}
		UE_LOG(LogSmartUEAssistantTools, Log, TEXT("  Found %d lights"), Results.Num());
		return Results;
	}

	// Case 3: Static mesh actors
	if (Target.Contains(TEXT("cube")) || Target.Contains(TEXT("sphere")) || Target.Contains(TEXT("mesh")))
	{
		UE_LOG(LogSmartUEAssistantTools, Log, TEXT("  Searching for static mesh actors"));
		for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			FString ActorName = Actor->GetName().ToLower();
			
			// Check if actor name matches target
			if (ActorName.Contains(Target) || Target.Contains(ActorName))
			{
				Results.Add(Actor);
			}
		}
		UE_LOG(LogSmartUEAssistantTools, Log, TEXT("  Found %d matching static mesh actors"), Results.Num());
		return Results;
	}

	// Case 4: By name (general search)
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("  Searching for actors by name"));
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		FString ActorName = Actor->GetName().ToLower();
		FString ActorLabel = Actor->GetActorLabel().ToLower();
		
		if (ActorName.Contains(Target) || ActorLabel.Contains(Target) || Target.Contains(ActorName))
		{
			Results.Add(Actor);
		}
	}
	
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("  Found %d actors by name"), Results.Num());
	return Results;
}

FString FUniversalModifyTool::FindPropertyPath(UObject* Object, const FString& PropertyName)
{
	// AI已经提供了精确的属性名，直接使用
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("  Using property name provided by AI: %s"), *PropertyName);
	return PropertyName;
}

bool FUniversalModifyTool::ModifyActorProperty(AActor* Actor, const FString& PropertyPath, const TSharedPtr<FJsonValue>& Value, FString& OutError)
{
	
	// For light-specific properties, use direct API (more reliable)
	if (Actor->IsA(ALight::StaticClass()))
	{
		ULightComponent* LightComp = Actor->FindComponentByClass<ULightComponent>();
		if (LightComp)
		{
			// Handle LightColor
			if (PropertyPath.Equals(TEXT("LightColor"), ESearchCase::IgnoreCase) || 
			    PropertyPath.Contains(TEXT("Color")))
			{
				FLinearColor Color;
				if (FPropertyModificationHelper::ParseColor(Value, Color))
				{
					UE_LOG(LogSmartUEAssistantTools, Log, TEXT("  Setting light color via direct API"));
					LightComp->Modify();
					LightComp->SetLightColor(Color);
					LightComp->MarkRenderStateDirty();
					
					// Also call PostEditChangeProperty
					FProperty* ColorProperty = FindFProperty<FProperty>(LightComp->GetClass(), TEXT("LightColor"));
					if (ColorProperty)
					{
						FPropertyChangedEvent PropertyChangedEvent(ColorProperty, EPropertyChangeType::ValueSet);
						LightComp->PostEditChangeProperty(PropertyChangedEvent);
					}
					return true;
				}
			}
			// Handle Intensity
			else if (PropertyPath.Equals(TEXT("Intensity"), ESearchCase::IgnoreCase))
			{
				if (Value->Type == EJson::Number)
				{
					float Intensity = Value->AsNumber();
					UE_LOG(LogSmartUEAssistantTools, Log, TEXT("  Setting light intensity via direct API"));
					LightComp->Modify();
					LightComp->SetIntensity(Intensity);
					LightComp->MarkRenderStateDirty();
					
					FProperty* IntensityProperty = FindFProperty<FProperty>(LightComp->GetClass(), TEXT("Intensity"));
					if (IntensityProperty)
					{
						FPropertyChangedEvent PropertyChangedEvent(IntensityProperty, EPropertyChangeType::ValueSet);
						LightComp->PostEditChangeProperty(PropertyChangedEvent);
					}
					return true;
				}
			}
		}
	}

	// For other properties, use reflection system
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("  Using reflection system for property: %s"), *PropertyPath);
	return FPropertyModificationHelper::SetPropertyValue(Actor, PropertyPath, Value, false, TEXT("Modify Property"), OutError);
}

FAIToolResult FUniversalModifyTool::Execute(const TSharedPtr<FJsonObject>& Args)
{
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("╔════════════════════════════════════════════════"));
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("║ UNIVERSAL MODIFY TOOL (AI Semantic Mode)"));
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("╚════════════════════════════════════════════════"));

	if (!GEditor)
	{
		return {false, TEXT("Editor not available"), nullptr};
	}

	FString Target = Args->GetStringField(TEXT("Target"));
	FString PropertyName = Args->GetStringField(TEXT("PropertyName"));
	TSharedPtr<FJsonValue> Value = Args->Values.FindRef(TEXT("Value"));

	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("Target: %s"), *Target);
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("PropertyName (from AI): %s"), *PropertyName);
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("────────────────────────────────────────────────"));

	// Step 1: Find target actors
	TArray<AActor*> TargetActors = FindTargetActors(Target);
	
	if (TargetActors.Num() == 0)
	{
		UE_LOG(LogSmartUEAssistantTools, Error, TEXT("ERROR: No actors found for target: %s"), *Target);
		return {false, FString::Printf(TEXT("未找到目标对象: %s"), *Target), nullptr};
	}

	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("Found %d target actors"), TargetActors.Num());
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("────────────────────────────────────────────────"));

	// Step 2: Modify each actor
	int32 SuccessCount = 0;
	TArray<FString> Errors;

	for (AActor* Actor : TargetActors)
	{
		UE_LOG(LogSmartUEAssistantTools, Log, TEXT("Processing: %s"), *Actor->GetName());
		
		// Use property name directly (AI provides exact name)
		FString PropertyPath = FindPropertyPath(Actor, PropertyName);
		
		// Modify property
		FString Error;
		if (ModifyActorProperty(Actor, PropertyPath, Value, Error))
		{
			SuccessCount++;
			UE_LOG(LogSmartUEAssistantTools, Log, TEXT("  ✓ Success"));
		}
		else
		{
			UE_LOG(LogSmartUEAssistantTools, Error, TEXT("  ✗ Failed: %s"), *Error);
			Errors.AddUnique(Error);
		}
	}

	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("────────────────────────────────────────────────"));
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("Success: %d / %d"), SuccessCount, TargetActors.Num());

	// Step 3: Refresh viewports
	if (SuccessCount > 0 && GEditor)
	{
		UE_LOG(LogSmartUEAssistantTools, Log, TEXT("Refreshing viewports..."));
		GEditor->RedrawAllViewports();
		GEditor->NoteSelectionChange();
	}

	// Step 4: Return result
	if (SuccessCount > 0)
	{
		FString Message = FString::Printf(TEXT("成功修改 %d 个对象"), SuccessCount);
		if (Errors.Num() > 0)
		{
			Message += FString::Printf(TEXT(" (%d 错误)"), Errors.Num());
		}
		
		UE_LOG(LogSmartUEAssistantTools, Log, TEXT("╔════════════════════════════════════════════════"));
		UE_LOG(LogSmartUEAssistantTools, Log, TEXT("║ RESULT: SUCCESS"));
		UE_LOG(LogSmartUEAssistantTools, Log, TEXT("║ %s"), *Message);
		UE_LOG(LogSmartUEAssistantTools, Log, TEXT("╚════════════════════════════════════════════════"));
		return {true, Message, nullptr};
	}
	else
	{
		FString Message = FString::Printf(TEXT("未能修改对象参与者。错误: %s"), 
			*FString::Join(Errors, TEXT("; ")));
		UE_LOG(LogSmartUEAssistantTools, Error, TEXT("╔════════════════════════════════════════════════"));
		UE_LOG(LogSmartUEAssistantTools, Error, TEXT("║ RESULT: FAILED"));
		UE_LOG(LogSmartUEAssistantTools, Error, TEXT("║ %s"), *Message);
		UE_LOG(LogSmartUEAssistantTools, Error, TEXT("╚════════════════════════════════════════════════"));
		return {false, Message, nullptr};
	}
}


// �?自动注册工具
#include "ToolAutoRegister.h"

REGISTER_EDITOR_TOOL(FUniversalModifyTool)

