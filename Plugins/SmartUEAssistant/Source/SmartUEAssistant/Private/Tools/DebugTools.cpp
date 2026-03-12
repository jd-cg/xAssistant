// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/DebugTools.h"
#include "SmartUEAssistantLog.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/Light.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Engine/Selection.h"

// ==================== Debug Set Light Color Tool ====================

FDebugSetLightColorTool::FDebugSetLightColorTool()
{
	Spec.Name = TEXT("debug_set_light_color");
	Spec.Description = TEXT("DEBUG: Directly set all lights in scene to a color. For testing only.");
	Spec.Permission = EToolPermission::Modify;
	Spec.bRequireConfirm = false;
	
	Spec.Params.Add({TEXT("Color"), TEXT("string"), false, TEXT("Color name: red, green, blue, white, yellow")});
}

FAIToolResult FDebugSetLightColorTool::Execute(const TSharedPtr<FJsonObject>& Args)
{
	UE_LOG(LogSmartUEAssistantTools, Warning, TEXT("=== DEBUG TOOL CALLED: debug_set_light_color ==="));
	
	if (!GEditor)
	{
		return {false, TEXT("Editor not available"), nullptr};
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return {false, TEXT("World not available"), nullptr};
	}

	FString ColorName = Args->GetStringField(TEXT("Color")).ToLower();
	UE_LOG(LogSmartUEAssistantTools, Warning, TEXT("[DEBUG] 要求颜色: %s"), *ColorName);

	FLinearColor TargetColor = FLinearColor::Red;
	if (ColorName == TEXT("red") || ColorName.Contains(TEXT("红")))
	{
		TargetColor = FLinearColor::Red;
	}
	else if (ColorName == TEXT("green") || ColorName.Contains(TEXT("绿")))
	{
		TargetColor = FLinearColor::Green;
	}
	else if (ColorName == TEXT("blue") || ColorName.Contains(TEXT("蓝")))
	{
		TargetColor = FLinearColor::Blue;
	}
	else if (ColorName == TEXT("white") || ColorName.Contains(TEXT("白")))
	{
		TargetColor = FLinearColor::White;
	}
	else if (ColorName == TEXT("yellow") || ColorName.Contains(TEXT("黄")))
	{
		TargetColor = FLinearColor::Yellow;
	}

	UE_LOG(LogSmartUEAssistantTools, Warning, TEXT("[DEBUG] 目标颜色: R=%f G=%f B=%f"), 
		TargetColor.R, TargetColor.G, TargetColor.B);

	int32 ModifiedCount = 0;
	
	// Method 1: Iterate all actors
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}

		// Check if it's a light actor
		if (Actor->IsA(ALight::StaticClass()))
		{
			UE_LOG(LogSmartUEAssistantTools, Warning, TEXT("[DEBUG] 找到光源对象: %s"), *Actor->GetName());
			
			// Try to get light component
			ULightComponent* LightComp = Actor->FindComponentByClass<ULightComponent>();
			if (LightComp)
			{
				UE_LOG(LogSmartUEAssistantTools, Warning, TEXT("[DEBUG] 找到光源组件，设置颜色..."));
				LightComp->SetLightColor(TargetColor);
				LightComp->MarkRenderStateDirty();
				ModifiedCount++;
				UE_LOG(LogSmartUEAssistantTools, Warning, TEXT("[DEBUG] 颜色设置！当前颜色: R=%f G=%f B=%f"),
					LightComp->GetLightColor().R, LightComp->GetLightColor().G, LightComp->GetLightColor().B);
			}
			else
			{
				UE_LOG(LogSmartUEAssistantTools, Error, TEXT("[DEBUG] 找到光源对象，但没有光源组件！"));
			}
		}
	}

	FString Message = FString::Printf(TEXT("[DEBUG] 将 %d 盏灯光修改为 %s 颜色"), ModifiedCount, *ColorName);
	UE_LOG(LogSmartUEAssistantTools, Warning, TEXT("%s"), *Message);
	
	return {true, Message, nullptr};
}

// ==================== Debug List Lights Tool ====================

FDebugListLightsTool::FDebugListLightsTool()
{
	Spec.Name = TEXT("debug_list_lights");
	Spec.Description = TEXT("DEBUG: List all light actors in the scene with their properties.");
	Spec.Permission = EToolPermission::Safe;
	Spec.bRequireConfirm = false;
}

FAIToolResult FDebugListLightsTool::Execute(const TSharedPtr<FJsonObject>& Args)
{
	UE_LOG(LogSmartUEAssistantTools, Warning, TEXT("=== DEBUG TOOL CALLED: debug_list_lights ==="));
	
	if (!GEditor)
	{
		return {false, TEXT("Editor not available"), nullptr};
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return {false, TEXT("World not available"), nullptr};
	}

	TArray<FString> LightInfo;
	int32 LightCount = 0;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || !Actor->IsA(ALight::StaticClass()))
		{
			continue;
		}

		LightCount++;
		FString ActorName = Actor->GetName();
		FString ActorLabel = Actor->GetActorLabel();
		FString ActorClass = Actor->GetClass()->GetName();

		ULightComponent* LightComp = Actor->FindComponentByClass<ULightComponent>();
		if (LightComp)
		{
			FLinearColor Color = LightComp->GetLightColor();
			float Intensity = LightComp->Intensity;
			
			FString Info = FString::Printf(TEXT("%s (%s, %s): Color(%.2f,%.2f,%.2f) Intensity=%.0f"),
				*ActorLabel, *ActorName, *ActorClass, Color.R, Color.G, Color.B, Intensity);
			LightInfo.Add(Info);
			
			UE_LOG(LogSmartUEAssistantTools, Warning, TEXT("[DEBUG] %s"), *Info);
		}
		else
		{
			FString Info = FString::Printf(TEXT("%s (%s, %s): 没有光源组件！"),
				*ActorLabel, *ActorName, *ActorClass);
			LightInfo.Add(Info);
			UE_LOG(LogSmartUEAssistantTools, Error, TEXT("[DEBUG] %s"), *Info);
		}
	}

	FString Message = FString::Printf(TEXT("[DEBUG] 找到 %d 个光源:\n%s"), 
		LightCount, *FString::Join(LightInfo, TEXT("\n")));
	
	TSharedPtr<FJsonObject> ResultData = MakeShareable(new FJsonObject);
	ResultData->SetNumberField(TEXT("LightCount"), LightCount);
	TArray<TSharedPtr<FJsonValue>> InfoArray;
	for (const FString& Info : LightInfo)
	{
		InfoArray.Add(MakeShareable(new FJsonValueString(Info)));
	}
	ResultData->SetArrayField(TEXT("Lights"), InfoArray);

	return {true, Message, ResultData};
}

// ==================== Debug Show Properties Tool ====================

FDebugShowPropertiesTool::FDebugShowPropertiesTool()
{
	Spec.Name = TEXT("debug_show_properties");
	Spec.Description = TEXT("DEBUG: Show all properties and components of selected actors.");
	Spec.Permission = EToolPermission::Safe;
	Spec.bRequireConfirm = false;
}

FAIToolResult FDebugShowPropertiesTool::Execute(const TSharedPtr<FJsonObject>& Args)
{
	UE_LOG(LogSmartUEAssistantTools, Warning, TEXT("=== DEBUG TOOL CALLED: debug_show_properties ==="));
	
	if (!GEditor)
	{
		return {false, TEXT("Editor not available"), nullptr};
	}

	USelection* SelectedActors = GEditor->GetSelectedActors();
	if (!SelectedActors || SelectedActors->Num() == 0)
	{
		return {false, TEXT("No actors selected"), nullptr};
	}

	TArray<FString> ActorInfoList;

	for (FSelectionIterator It(*SelectedActors); It; ++It)
	{
		AActor* Actor = Cast<AActor>(*It);
		if (!Actor)
		{
			continue;
		}

		FString ActorInfo = FString::Printf(TEXT("\n=== %s (%s) ==="), 
			*Actor->GetActorLabel(), *Actor->GetClass()->GetName());
		ActorInfoList.Add(ActorInfo);

		// List components
		TArray<UActorComponent*> Components;
		Actor->GetComponents(UActorComponent::StaticClass(), Components);
		
		ActorInfoList.Add(FString::Printf(TEXT("组件: %d"), Components.Num()));
		
		for (UActorComponent* Component : Components)
		{
			if (Component)
			{
				FString CompInfo = FString::Printf(TEXT("  - %s (%s)"), 
					*Component->GetName(), *Component->GetClass()->GetName());
				ActorInfoList.Add(CompInfo);
				UE_LOG(LogSmartUEAssistantTools, Warning, TEXT("[DEBUG] %s"), *CompInfo);

				// If it's a light component, show its properties
				if (ULightComponent* LC = Cast<ULightComponent>(Component))
				{
					FLinearColor Color = LC->GetLightColor();
					FString LightInfo = FString::Printf(TEXT("    颜色: (%.2f, %.2f, %.2f), 强度: %.0f"),
						Color.R, Color.G, Color.B, LC->Intensity);
					ActorInfoList.Add(LightInfo);
					UE_LOG(LogSmartUEAssistantTools, Warning, TEXT("[DEBUG] %s"), *LightInfo);
				}
			}
		}
	}

	FString Message = FString::Join(ActorInfoList, TEXT("\n"));
	return {true, Message, nullptr};
}

// ✅ 自动注册 Debug 工具（仅在开发/调试版本）
#include "ToolAutoRegister.h"

REGISTER_EDITOR_TOOL_DEBUG(FDebugSetLightColorTool)
REGISTER_EDITOR_TOOL_DEBUG(FDebugListLightsTool)
REGISTER_EDITOR_TOOL_DEBUG(FDebugShowPropertiesTool)
