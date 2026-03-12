// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/QuickLightTools.h"
#include "PropertyModificationHelper.h"
#include "SmartUEAssistantLog.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/LightComponent.h"
#include "Engine/Light.h"
#include "Engine/Selection.h"

// ==================== Quick Set Light Color Tool ====================

FQuickSetLightColorTool::FQuickSetLightColorTool()
{
	Spec.Name = TEXT("quick_set_light_color");
	Spec.Description = TEXT("Quickly set light color for selected lights (or all lights if none selected). Supports color names and RGB.");
	Spec.Permission = EToolPermission::Modify;
	Spec.bRequireConfirm = false;
	
	Spec.Params.Add({TEXT("Color"), TEXT("string"), false, TEXT("Color name (red/红色, green/绿色, blue/蓝色, etc) or RGB object {\"R\":1,\"G\":0,\"B\":0}")});
	Spec.Params.Add({TEXT("UseAllLights"), TEXT("boolean"), true, TEXT("If true and no selection, apply to all lights")});
}

FAIToolResult FQuickSetLightColorTool::Execute(const TSharedPtr<FJsonObject>& Args)
{
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("╔═══════════════════════════════════════════"));
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("║ QUICK_SET_LIGHT_COLOR Tool Executing"));
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("╚═══════════════════════════════════════════"));

	if (!GEditor)
	{
		UE_LOG(LogSmartUEAssistantTools, Error, TEXT("ERROR: Editor not available"));
		return {false, TEXT("Editor not available"), nullptr};
	}

	// Parse color
	TSharedPtr<FJsonValue> ColorValue = Args->Values.FindRef(TEXT("Color"));
	FLinearColor TargetColor;
	
	if (!FPropertyModificationHelper::ParseColor(ColorValue, TargetColor))
	{
		UE_LOG(LogSmartUEAssistantTools, Error, TEXT("ERROR: Failed to parse color"));
		return {false, TEXT("Invalid color value"), nullptr};
	}

	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("Target Color: R=%f G=%f B=%f"), 
		(float)TargetColor.R, (float)TargetColor.G, (float)TargetColor.B);

	// Get target lights
	TArray<AActor*> TargetLights;
	
	// First try selected actors
	USelection* SelectedActors = GEditor->GetSelectedActors();
	if (SelectedActors && SelectedActors->Num() > 0)
	{
		UE_LOG(LogSmartUEAssistantTools, Log, TEXT("Using selected actors: %d"), SelectedActors->Num());
		
		for (FSelectionIterator It(*SelectedActors); It; ++It)
		{
			if (AActor* Actor = Cast<AActor>(*It))
			{
				if (Actor->IsA(ALight::StaticClass()))
				{
					TargetLights.Add(Actor);
					UE_LOG(LogSmartUEAssistantTools, Log, TEXT("  - Selected light: %s"), *Actor->GetName());
				}
			}
		}
	}
	
	// If no selected lights, use all lights in scene (if allowed)
	bool bUseAllLights = Args->HasField(TEXT("UseAllLights")) ? Args->GetBoolField(TEXT("UseAllLights")) : true;
	if (TargetLights.Num() == 0 && bUseAllLights)
	{
		UE_LOG(LogSmartUEAssistantTools, Log, TEXT("No selected lights, using all lights in scene"));
		
		UWorld* World = GEditor->GetEditorWorldContext().World();
		if (World)
		{
			for (TActorIterator<ALight> It(World); It; ++It)
			{
				TargetLights.Add(*It);
				UE_LOG(LogSmartUEAssistantTools, Log, TEXT("  - Scene light: %s"), *It->GetName());
			}
		}
	}

	if (TargetLights.Num() == 0)
	{
		UE_LOG(LogSmartUEAssistantTools, Warning, TEXT("WARNING: No lights found"));
		return {false, TEXT("No lights found to modify"), nullptr};
	}

	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("Total lights to modify: %d"), TargetLights.Num());
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("───────────────────────────────────────────"));

	// Modify each light
	int32 ModifiedCount = 0;
	
	for (AActor* LightActor : TargetLights)
	{
		UE_LOG(LogSmartUEAssistantTools, Log, TEXT("Processing: %s"), *LightActor->GetName());
		
		ULightComponent* LightComp = LightActor->FindComponentByClass<ULightComponent>();
		if (!LightComp)
		{
			UE_LOG(LogSmartUEAssistantTools, Warning, TEXT("  WARNING: No LightComponent found"));
			continue;
		}

		// Mark for undo/redo
		LightComp->Modify();
		
		// Set color (direct method, bypasses reflection)
		LightComp->SetLightColor(TargetColor);
		
		// Trigger updates
		LightComp->MarkRenderStateDirty();
		
		// Also call PostEditChangeProperty for full editor integration
		FProperty* ColorProperty = FindFProperty<FProperty>(LightComp->GetClass(), TEXT("LightColor"));
		if (ColorProperty)
		{
			FPropertyChangedEvent PropertyChangedEvent(ColorProperty, EPropertyChangeType::ValueSet);
			LightComp->PostEditChangeProperty(PropertyChangedEvent);
		}
		
		ModifiedCount++;
		
		UE_LOG(LogSmartUEAssistantTools, Log, TEXT("  ✓ Color set to R=%f G=%f B=%f"), 
			(float)LightComp->GetLightColor().R, 
			(float)LightComp->GetLightColor().G, 
			(float)LightComp->GetLightColor().B);
	}

	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("───────────────────────────────────────────"));
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("Modified: %d / %d lights"), ModifiedCount, TargetLights.Num());

	// Force viewport refresh
	if (ModifiedCount > 0 && GEditor)
	{
		UE_LOG(LogSmartUEAssistantTools, Log, TEXT("Refreshing viewports..."));
		GEditor->RedrawAllViewports();
		GEditor->NoteSelectionChange();
		UE_LOG(LogSmartUEAssistantTools, Log, TEXT("✓ Viewports refreshed"));
	}

	FString Message = FString::Printf(TEXT("已经成功设置 %d 个光源对象的颜色"), ModifiedCount);
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("╔═══════════════════════════════════════════"));
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("║ RESULT: SUCCESS"));
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("║ %s"), *Message);
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("╚═══════════════════════════════════════════"));

	return {true, Message, nullptr};
}

// ==================== Quick Set Light Intensity Tool ====================

FQuickSetLightIntensityTool::FQuickSetLightIntensityTool()
{
	Spec.Name = TEXT("quick_set_light_intensity");
	Spec.Description = TEXT("Quickly set light intensity for selected lights (or all lights if none selected).");
	Spec.Permission = EToolPermission::Modify;
	Spec.bRequireConfirm = false;
	
	Spec.Params.Add({TEXT("Intensity"), TEXT("number"), false, TEXT("Light intensity value")});
	Spec.Params.Add({TEXT("UseAllLights"), TEXT("boolean"), true, TEXT("If true and no selection, apply to all lights")});
}

FAIToolResult FQuickSetLightIntensityTool::Execute(const TSharedPtr<FJsonObject>& Args)
{
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("╔═══════════════════════════════════════════"));
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("║ QUICK_SET_LIGHT_INTENSITY Tool Executing"));
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("╚═══════════════════════════════════════════"));

	if (!GEditor)
	{
		return {false, TEXT("Editor not available"), nullptr};
	}

	float Intensity = Args->GetNumberField(TEXT("Intensity"));
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("Target Intensity: %f"), Intensity);

	// Get target lights (same logic as color tool)
	TArray<AActor*> TargetLights;
	
	USelection* SelectedActors = GEditor->GetSelectedActors();
	if (SelectedActors && SelectedActors->Num() > 0)
	{
		for (FSelectionIterator It(*SelectedActors); It; ++It)
		{
			if (AActor* Actor = Cast<AActor>(*It))
			{
				if (Actor->IsA(ALight::StaticClass()))
				{
					TargetLights.Add(Actor);
				}
			}
		}
	}
	
	bool bUseAllLights = Args->HasField(TEXT("UseAllLights")) ? Args->GetBoolField(TEXT("UseAllLights")) : true;
	if (TargetLights.Num() == 0 && bUseAllLights)
	{
		UWorld* World = GEditor->GetEditorWorldContext().World();
		if (World)
		{
			for (TActorIterator<ALight> It(World); It; ++It)
			{
				TargetLights.Add(*It);
			}
		}
	}

	if (TargetLights.Num() == 0)
	{
		return {false, TEXT("No lights found to modify"), nullptr};
	}

	// Modify each light
	int32 ModifiedCount = 0;
	
	for (AActor* LightActor : TargetLights)
	{
		ULightComponent* LightComp = LightActor->FindComponentByClass<ULightComponent>();
		if (!LightComp)
		{
			continue;
		}

		LightComp->Modify();
		LightComp->SetIntensity(Intensity);
		LightComp->MarkRenderStateDirty();
		
		FProperty* IntensityProperty = FindFProperty<FProperty>(LightComp->GetClass(), TEXT("Intensity"));
		if (IntensityProperty)
		{
			FPropertyChangedEvent PropertyChangedEvent(IntensityProperty, EPropertyChangeType::ValueSet);
			LightComp->PostEditChangeProperty(PropertyChangedEvent);
		}
		
		ModifiedCount++;
	}

	if (ModifiedCount > 0 && GEditor)
	{
		GEditor->RedrawAllViewports();
		GEditor->NoteSelectionChange();
	}

	FString Message = FString::Printf(TEXT("Successfully set intensity to %f for %d lights"), Intensity, ModifiedCount);
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("╔═══════════════════════════════════════════"));
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("║ RESULT: SUCCESS"));
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("║ %s"), *Message);
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("╚═══════════════════════════════════════════"));

	return {true, Message, nullptr};
}


// �?自动注册工具
#include "ToolAutoRegister.h"

REGISTER_EDITOR_TOOL(FQuickSetLightColorTool)
REGISTER_EDITOR_TOOL(FQuickSetLightIntensityTool)

