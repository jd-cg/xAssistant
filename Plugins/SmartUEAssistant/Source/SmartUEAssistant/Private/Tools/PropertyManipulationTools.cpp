// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/PropertyManipulationTools.h"
#include "PropertyModificationHelper.h"
#include "SmartUEAssistantLog.h"
#include "Editor.h"
#include "Selection.h"
#include "Engine/Selection.h"
#include "GameFramework/Actor.h"
#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ActorComponent.h"
#include "UObject/UnrealType.h"
#include "EngineUtils.h"

// Old PropertyToolsHelper namespace removed - now using FPropertyModificationHelper

// ==================== Set Actor Property Tool ====================

FSetActorPropertyTool::FSetActorPropertyTool()
{
	Spec.Name = TEXT("set_actor_property");
	Spec.Description = TEXT("Set property value for selected actors using reflection system. Supports various property types including colors, vectors, numbers, and booleans.");
	Spec.Permission = EToolPermission::Modify;
	Spec.bRequireConfirm = false;
	
	Spec.Params.Add({TEXT("PropertyPath"), TEXT("string"), false, TEXT("Property path (e.g., 'LightComponent.Intensity', 'LightColor')")});
	Spec.Params.Add({TEXT("Value"), TEXT("object"), false, TEXT("Property value (type depends on property)")});
	Spec.Params.Add({TEXT("ApplyToComponents"), TEXT("boolean"), true, TEXT("Search in components too")});
}

bool FSetActorPropertyTool::SetPropertyValue(UObject* Object, const FString& PropertyPath, const TSharedPtr<FJsonValue>& Value, FString& OutError)
{
	// Try direct property path first
	bool bSuccess = FPropertyModificationHelper::SetPropertyValue(
		Object,
		PropertyPath,
		Value,
		false, // No transaction for batch operations (transaction handled at tool level)
		TEXT("Set Property"),
		OutError
	);

	if (bSuccess)
	{
		return true;
	}

	// If direct path failed, try searching in common components
	if (AActor* Actor = Cast<AActor>(Object))
	{
		// Try common component patterns
		TArray<FString> ComponentPaths;
		
		// For lights
		if (PropertyPath.Contains(TEXT("Color")) || PropertyPath.Contains(TEXT("Intensity")) || 
			PropertyPath.Contains(TEXT("Brightness")) || PropertyPath.Contains(TEXT("Light")))
		{
			ComponentPaths.Add(FString::Printf(TEXT("LightComponent.%s"), *PropertyPath));
			ComponentPaths.Add(FString::Printf(TEXT("PointLightComponent.%s"), *PropertyPath));
			ComponentPaths.Add(FString::Printf(TEXT("SpotLightComponent.%s"), *PropertyPath));
		}

		// Try each component path
		for (const FString& CompPath : ComponentPaths)
		{
			FString CompError;
			if (FPropertyModificationHelper::SetPropertyValue(Actor, CompPath, Value, false, TEXT("Set Property"), CompError))
			{
				OutError.Empty();
				return true;
			}
		}
	}

	return false;
}

FAIToolResult FSetActorPropertyTool::Execute(const TSharedPtr<FJsonObject>& Args)
{
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("╔═══════════════════════════════════════════"));
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("║ SET_ACTOR_PROPERTY Tool Executing"));
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("╚═══════════════════════════════════════════"));

	if (!GEditor)
	{
		UE_LOG(LogSmartUEAssistantTools, Error, TEXT("ERROR: Editor not available"));
		return {false, TEXT("Editor not available"), nullptr};
	}

	USelection* SelectedActors = GEditor->GetSelectedActors();
	if (!SelectedActors || SelectedActors->Num() == 0)
	{
		UE_LOG(LogSmartUEAssistantTools, Warning, TEXT("WARNING: No actors selected"));
		return {false, TEXT("No actors selected"), nullptr};
	}

	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("Selected actors count: %d"), SelectedActors->Num());

	FString PropertyPath = Args->GetStringField(TEXT("PropertyPath"));
	TSharedPtr<FJsonValue> Value = Args->Values.FindRef(TEXT("Value"));

	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("PropertyPath: %s"), *PropertyPath);

	int32 ModifiedCount = 0;
	TArray<FString> Errors;

	for (FSelectionIterator It(*SelectedActors); It; ++It)
	{
		if (AActor* Actor = Cast<AActor>(*It))
		{
			UE_LOG(LogSmartUEAssistantTools, Log, TEXT("───────────────────────────────────────────"));
			UE_LOG(LogSmartUEAssistantTools, Log, TEXT("Processing Actor: %s"), *Actor->GetName());
			
			FString Error;
			if (SetPropertyValue(Actor, PropertyPath, Value, Error))
			{
				ModifiedCount++;
				UE_LOG(LogSmartUEAssistantTools, Log, TEXT("✓ Successfully modified: %s"), *Actor->GetName());
			}
			else
			{
				UE_LOG(LogSmartUEAssistantTools, Error, TEXT("✗ Failed to modify: %s - %s"), *Actor->GetName(), *Error);
				if (!Error.IsEmpty())
				{
					Errors.AddUnique(Error);
				}
			}
		}
	}

	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("───────────────────────────────────────────"));
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("Modified count: %d / %d"), ModifiedCount, SelectedActors->Num());

	// Force viewport refresh (PostEditChangeProperty should handle most updates, but ensure viewport is redrawn)
	if (ModifiedCount > 0 && GEditor)
	{
		UE_LOG(LogSmartUEAssistantTools, Log, TEXT("Calling GEditor->RedrawAllViewports()..."));
		GEditor->RedrawAllViewports();
		UE_LOG(LogSmartUEAssistantTools, Log, TEXT("Calling GEditor->NoteSelectionChange()..."));
		GEditor->NoteSelectionChange(); // Notify that selection properties changed
		UE_LOG(LogSmartUEAssistantTools, Log, TEXT("✓ Viewport refresh completed"));
	}

	FString Message;
	if (ModifiedCount > 0)
	{
		Message = FString::Printf(TEXT("Set property '%s' for %d actors"), *PropertyPath, ModifiedCount);
		if (Errors.Num() > 0)
		{
			Message += FString::Printf(TEXT(" (with %d errors)"), Errors.Num());
		}
		UE_LOG(LogSmartUEAssistantTools, Log, TEXT("╔═══════════════════════════════════════════"));
		UE_LOG(LogSmartUEAssistantTools, Log, TEXT("║ RESULT: SUCCESS"));
		UE_LOG(LogSmartUEAssistantTools, Log, TEXT("║ %s"), *Message);
		UE_LOG(LogSmartUEAssistantTools, Log, TEXT("╚═══════════════════════════════════════════"));
		return {true, Message, nullptr};
	}
	else
	{
		Message = FString::Printf(TEXT("Failed to set property. Errors: %s"), *FString::Join(Errors, TEXT("; ")));
		UE_LOG(LogSmartUEAssistantTools, Error, TEXT("╔═══════════════════════════════════════════"));
		UE_LOG(LogSmartUEAssistantTools, Error, TEXT("║ RESULT: FAILED"));
		UE_LOG(LogSmartUEAssistantTools, Error, TEXT("║ %s"), *Message);
		UE_LOG(LogSmartUEAssistantTools, Error, TEXT("╚═══════════════════════════════════════════"));
		return {false, Message, nullptr};
	}
}

// ==================== Batch Set Properties Tool ====================

FBatchSetPropertiesTool::FBatchSetPropertiesTool()
{
	Spec.Name = TEXT("batch_set_properties");
	Spec.Description = TEXT("Set multiple properties at once for selected actors. Efficient for setting many properties in one call.");
	Spec.Permission = EToolPermission::Modify;
	Spec.bRequireConfirm = false;
	
	Spec.Params.Add({TEXT("Properties"), TEXT("object"), false, TEXT("Map of property paths to values")});
}

FAIToolResult FBatchSetPropertiesTool::Execute(const TSharedPtr<FJsonObject>& Args)
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

	const TSharedPtr<FJsonObject>* PropertiesObj;
	if (!Args->TryGetObjectField(TEXT("Properties"), PropertiesObj))
	{
		return {false, TEXT("Properties parameter must be an object"), nullptr};
	}

	int32 TotalModified = 0;
	TArray<FString> SuccessfulProperties;

	for (const auto& Pair : (*PropertiesObj)->Values)
	{
		FString PropertyPath = Pair.Key;
		TSharedPtr<FJsonValue> Value = Pair.Value;

		int32 ModifiedForProperty = 0;
		for (FSelectionIterator It(*SelectedActors); It; ++It)
		{
			if (AActor* Actor = Cast<AActor>(*It))
			{
				FString Error;
				FSetActorPropertyTool TempTool;
				if (TempTool.SetPropertyValue(Actor, PropertyPath, Value, Error))
				{
					ModifiedForProperty++;
				}
			}
		}

		if (ModifiedForProperty > 0)
		{
			TotalModified += ModifiedForProperty;
			SuccessfulProperties.Add(PropertyPath);
		}
	}

	// Force viewport refresh (PostEditChangeProperty should handle most updates, but ensure viewport is redrawn)
	if (TotalModified > 0 && GEditor)
	{
		GEditor->RedrawAllViewports();
		GEditor->NoteSelectionChange();
	}

	FString Message = FString::Printf(TEXT("Set %d properties for selected actors: %s"), 
		SuccessfulProperties.Num(), *FString::Join(SuccessfulProperties, TEXT(", ")));
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("%s"), *Message);

	return {true, Message, nullptr};
}

// ==================== Get Available Properties Tool ====================

FGetAvailablePropertiesTool::FGetAvailablePropertiesTool()
{
	Spec.Name = TEXT("get_available_properties");
	Spec.Description = TEXT("Discover what properties can be modified on selected actors. Useful for understanding what can be changed.");
	Spec.Permission = EToolPermission::Safe;
	Spec.bRequireConfirm = false;
	
	Spec.Params.Add({TEXT("ActorName"), TEXT("string"), true, TEXT("Specific actor name, or use first selected")});
	Spec.Params.Add({TEXT("IncludeComponents"), TEXT("boolean"), true, TEXT("Include component properties")});
}

void FGetAvailablePropertiesTool::DiscoverProperties(UObject* Object, const FString& Prefix, TArray<FString>& OutProperties)
{
	for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
	{
		FProperty* Property = *It;
		
		// Skip non-editable properties
		if (!Property->HasAnyPropertyFlags(CPF_Edit))
		{
			continue;
		}

		FString PropertyName = Prefix.IsEmpty() ? Property->GetName() : Prefix + TEXT(".") + Property->GetName();
		FString PropertyType = Property->GetClass()->GetName();

		OutProperties.Add(FString::Printf(TEXT("%s (%s)"), *PropertyName, *PropertyType));
	}
}

FAIToolResult FGetAvailablePropertiesTool::Execute(const TSharedPtr<FJsonObject>& Args)
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

	AActor* TargetActor = nullptr;
	for (FSelectionIterator It(*SelectedActors); It; ++It)
	{
		TargetActor = Cast<AActor>(*It);
		if (TargetActor)
		{
			break;
		}
	}

	if (!TargetActor)
	{
		return {false, TEXT("No valid actor found"), nullptr};
	}

	TArray<FString> Properties;
	DiscoverProperties(TargetActor, TEXT(""), Properties);

	// Include components if requested
	bool bIncludeComponents = Args->HasField(TEXT("IncludeComponents")) && Args->GetBoolField(TEXT("IncludeComponents"));
	if (bIncludeComponents)
	{
		TArray<UActorComponent*> Components;
		TargetActor->GetComponents(UActorComponent::StaticClass(), Components);
		for (UActorComponent* Component : Components)
		{
			if (Component)
			{
				FString ComponentName = Component->GetClass()->GetName();
				DiscoverProperties(Component, ComponentName, Properties);
			}
		}
	}

	FString Message = FString::Printf(TEXT("Found %d editable properties for '%s':\n%s"),
		Properties.Num(), *TargetActor->GetActorLabel(), *FString::Join(Properties, TEXT("\n")));

	TSharedPtr<FJsonObject> ResultData = MakeShareable(new FJsonObject);
	TArray<TSharedPtr<FJsonValue>> PropertiesArray;
	for (const FString& Prop : Properties)
	{
		PropertiesArray.Add(MakeShareable(new FJsonValueString(Prop)));
	}
	ResultData->SetArrayField(TEXT("Properties"), PropertiesArray);

	return {true, Message, ResultData};
}

// ==================== Smart Set Property Tool ====================

FSmartSetPropertyTool::FSmartSetPropertyTool()
{
	Spec.Name = TEXT("smart_set_property");
	Spec.Description = TEXT("Intelligently set properties based on natural language hints. Searches for matching property names semantically.");
	Spec.Permission = EToolPermission::Modify;
	Spec.bRequireConfirm = false;
	
	Spec.Params.Add({TEXT("PropertyHint"), TEXT("string"), false, TEXT("Natural language property hint (e.g., 'color', 'brightness', 'size')")});
	Spec.Params.Add({TEXT("Value"), TEXT("object"), false, TEXT("Property value")});
}

TArray<FString> FSmartSetPropertyTool::FindMatchingProperties(UObject* Object, const FString& Hint)
{
	TArray<FString> Matches;
	FString LowerHint = Hint.ToLower();

	// Define semantic mappings
	TMap<FString, TArray<FString>> SemanticMap;
	SemanticMap.Add(TEXT("color"), {TEXT("Color"), TEXT("LightColor"), TEXT("Tint"), TEXT("BaseColor")});
	SemanticMap.Add(TEXT("颜色"), {TEXT("Color"), TEXT("LightColor"), TEXT("Tint"), TEXT("BaseColor")});
	SemanticMap.Add(TEXT("brightness"), {TEXT("Intensity"), TEXT("Brightness"), TEXT("Strength")});
	SemanticMap.Add(TEXT("亮度"), {TEXT("Intensity"), TEXT("Brightness"), TEXT("Strength")});
	SemanticMap.Add(TEXT("size"), {TEXT("Scale"), TEXT("Radius"), TEXT("Size"), TEXT("Extent")});
	SemanticMap.Add(TEXT("大小"), {TEXT("Scale"), TEXT("Radius"), TEXT("Size"), TEXT("Extent")});

	// Find matching keywords
	for (const auto& Pair : SemanticMap)
	{
		if (LowerHint.Contains(Pair.Key))
		{
			Matches.Append(Pair.Value);
		}
	}

	return Matches;
}

FString FSmartSetPropertyTool::GetPropertyPath(UObject* Object, const FString& PropertyName)
{
	// Try direct property
	if (FindFProperty<FProperty>(Object->GetClass(), *PropertyName))
	{
		return PropertyName;
	}

	// Try in components
	if (AActor* Actor = Cast<AActor>(Object))
	{
		TArray<UActorComponent*> Components;
		Actor->GetComponents(UActorComponent::StaticClass(), Components);
		for (UActorComponent* Component : Components)
		{
			if (Component && FindFProperty<FProperty>(Component->GetClass(), *PropertyName))
			{
				return Component->GetClass()->GetName() + TEXT(".") + PropertyName;
			}
		}
	}

	return TEXT("");
}

FAIToolResult FSmartSetPropertyTool::Execute(const TSharedPtr<FJsonObject>& Args)
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

	FString PropertyHint = Args->GetStringField(TEXT("PropertyHint"));
	TSharedPtr<FJsonValue> Value = Args->Values.FindRef(TEXT("Value"));

	// Find matching properties
	AActor* FirstActor = nullptr;
	for (FSelectionIterator It(*SelectedActors); It; ++It)
	{
		FirstActor = Cast<AActor>(*It);
		if (FirstActor) break;
	}

	if (!FirstActor)
	{
		return {false, TEXT("No valid actor found"), nullptr};
	}

	TArray<FString> CandidateProperties = FindMatchingProperties(FirstActor, PropertyHint);
	if (CandidateProperties.Num() == 0)
	{
		return {false, FString::Printf(TEXT("No matching properties found for hint: %s"), *PropertyHint), nullptr};
	}

	// Try each candidate property
	FSetActorPropertyTool PropertySetter;
	int32 SuccessCount = 0;
	FString UsedProperty;

	for (const FString& PropName : CandidateProperties)
	{
		FString PropertyPath = GetPropertyPath(FirstActor, PropName);
		if (PropertyPath.IsEmpty())
		{
			continue;
		}

		int32 ModifiedForThis = 0;
		for (FSelectionIterator It(*SelectedActors); It; ++It)
		{
			if (AActor* Actor = Cast<AActor>(*It))
			{
				FString Error;
				if (PropertySetter.SetPropertyValue(Actor, PropertyPath, Value, Error))
				{
					ModifiedForThis++;
				}
			}
		}

		if (ModifiedForThis > 0)
		{
			SuccessCount = ModifiedForThis;
			UsedProperty = PropertyPath;
			break;
		}
	}

	// Force viewport refresh (PostEditChangeProperty should handle most updates, but ensure viewport is redrawn)
	if (SuccessCount > 0 && GEditor)
	{
		GEditor->RedrawAllViewports();
		GEditor->NoteSelectionChange();
	}

	if (SuccessCount > 0)
	{
		FString Message = FString::Printf(TEXT("Smart set property '%s' (matched '%s') for %d actors"),
			*PropertyHint, *UsedProperty, SuccessCount);
		UE_LOG(LogSmartUEAssistantTools, Log, TEXT("%s"), *Message);
		return {true, Message, nullptr};
	}

	return {false, FString::Printf(TEXT("Could not set any matching property for hint: %s"), *PropertyHint), nullptr};
}

// ==================== Adjust Property Tool ====================

FAdjustPropertyTool::FAdjustPropertyTool()
{
	Spec.Name = TEXT("adjust_property");
	Spec.Description = TEXT("Adjust numeric property by offset (relative change). Useful for 'increase by' or 'decrease by' operations.");
	Spec.Permission = EToolPermission::Modify;
	Spec.bRequireConfirm = false;
	
	Spec.Params.Add({TEXT("PropertyPath"), TEXT("string"), false, TEXT("Property path")});
	Spec.Params.Add({TEXT("Offset"), TEXT("number"), false, TEXT("Amount to add (can be negative)")});
	Spec.Params.Add({TEXT("Multiply"), TEXT("boolean"), true, TEXT("If true, multiply instead of add")});
}

FAIToolResult FAdjustPropertyTool::Execute(const TSharedPtr<FJsonObject>& Args)
{
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("╔═══════════════════════════════════════════"));
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("║ ADJUST_PROPERTY Tool Executing"));
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("╚═══════════════════════════════════════════"));

	if (!GEditor)
	{
		return {false, TEXT("Editor not available"), nullptr};
	}

	USelection* SelectedActors = GEditor->GetSelectedActors();
	if (!SelectedActors || SelectedActors->Num() == 0)
	{
		return {false, TEXT("No actors selected"), nullptr};
	}

	FString PropertyPath = Args->GetStringField(TEXT("PropertyPath"));
	double OffsetValue = Args->GetNumberField(TEXT("Offset"));
	bool bMultiply = Args->HasField(TEXT("Multiply")) ? Args->GetBoolField(TEXT("Multiply")) : false;

	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("PropertyPath: %s"), *PropertyPath);
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("Offset: %f, Multiply: %s"), OffsetValue, bMultiply ? TEXT("true") : TEXT("false"));

	int32 ModifiedCount = 0;
	TArray<FString> Errors;

	for (FSelectionIterator It(*SelectedActors); It; ++It)
	{
		if (AActor* Actor = Cast<AActor>(*It))
		{
			UE_LOG(LogSmartUEAssistantTools, Log, TEXT("Processing Actor: %s"), *Actor->GetName());

			// Step 1: Get current value
			UObject* Container = nullptr;
			FProperty* Property = FPropertyModificationHelper::FindPropertyByPath(Actor, PropertyPath, Container);

			if (!Property || !Container)
			{
				FString Error = FString::Printf(TEXT("Property not found: %s on %s"), *PropertyPath, *Actor->GetName());
				Errors.AddUnique(Error);
				UE_LOG(LogSmartUEAssistantTools, Warning, TEXT("  %s"), *Error);
				continue;
			}

			void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);

			// Step 2: Adjust the value based on property type
			bool bSuccess = false;

			// Float
			if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
			{
				float CurrentValue = FloatProp->GetPropertyValue(ValuePtr);
				float NewValue = bMultiply ? (CurrentValue * OffsetValue) : (CurrentValue + OffsetValue);
				FloatProp->SetPropertyValue(ValuePtr, NewValue);
				bSuccess = true;
				UE_LOG(LogSmartUEAssistantTools, Log, TEXT("  Float: %f -> %f"), CurrentValue, NewValue);
			}
			// Double
			else if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
			{
				double CurrentValue = DoubleProp->GetPropertyValue(ValuePtr);
				double NewValue = bMultiply ? (CurrentValue * OffsetValue) : (CurrentValue + OffsetValue);
				DoubleProp->SetPropertyValue(ValuePtr, NewValue);
				bSuccess = true;
				UE_LOG(LogSmartUEAssistantTools, Log, TEXT("  Double: %f -> %f"), CurrentValue, NewValue);
			}
			// Int
			else if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
			{
				int32 CurrentValue = IntProp->GetPropertyValue(ValuePtr);
				int32 NewValue = bMultiply ? FMath::RoundToInt(CurrentValue * OffsetValue) : FMath::RoundToInt(CurrentValue + OffsetValue);
				IntProp->SetPropertyValue(ValuePtr, NewValue);
				bSuccess = true;
				UE_LOG(LogSmartUEAssistantTools, Log, TEXT("  Int: %d -> %d"), CurrentValue, NewValue);
			}
			// Byte
			else if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
			{
				uint8 CurrentValue = ByteProp->GetPropertyValue(ValuePtr);
				int32 NewValue = bMultiply ? FMath::RoundToInt(CurrentValue * OffsetValue) : FMath::RoundToInt(CurrentValue + OffsetValue);
				ByteProp->SetPropertyValue(ValuePtr, FMath::Clamp(NewValue, 0, 255));
				bSuccess = true;
				UE_LOG(LogSmartUEAssistantTools, Log, TEXT("  Byte: %d -> %d"), CurrentValue, NewValue);
			}
			else
			{
				FString Error = FString::Printf(TEXT("Property '%s' is not a numeric type"), *PropertyPath);
				Errors.AddUnique(Error);
				UE_LOG(LogSmartUEAssistantTools, Error, TEXT("  %s"), *Error);
				continue;
			}

			// Step 3: Apply proper UE notifications
			if (bSuccess)
			{
				Container->Modify();
				FPropertyChangedEvent PropertyChangedEvent(Property, EPropertyChangeType::ValueSet);
				Container->PostEditChangeProperty(PropertyChangedEvent);

				if (UActorComponent* Component = Cast<UActorComponent>(Container))
				{
					Component->MarkRenderStateDirty();
				}

				ModifiedCount++;
				UE_LOG(LogSmartUEAssistantTools, Log, TEXT("  ✓ Adjusted successfully"));
			}
		}
	}

	// Refresh viewports
	if (ModifiedCount > 0 && GEditor)
	{
		GEditor->RedrawAllViewports();
		GEditor->NoteSelectionChange();
	}

	FString Message;
	if (ModifiedCount > 0)
	{
		Message = FString::Printf(TEXT("Adjusted property '%s' for %d actors (%s: %f)"),
			*PropertyPath, ModifiedCount, bMultiply ? TEXT("Multiply" ) : TEXT("Offset"), OffsetValue);
		if (Errors.Num() > 0)
		{
			Message += FString::Printf(TEXT(" (%d errors)"), Errors.Num());
		}
		UE_LOG(LogSmartUEAssistantTools, Log, TEXT("╔═══════════════════════════════════════════"));
		UE_LOG(LogSmartUEAssistantTools, Log, TEXT("║ RESULT: SUCCESS"));
		UE_LOG(LogSmartUEAssistantTools, Log, TEXT("║ %s"), *Message);
		UE_LOG(LogSmartUEAssistantTools, Log, TEXT("╚═══════════════════════════════════════════"));
		return {true, Message, nullptr};
	}

	Message = FString::Printf(TEXT("Failed to adjust property. Errors: %s"), *FString::Join(Errors, TEXT("; ")));
	UE_LOG(LogSmartUEAssistantTools, Error, TEXT("╔═══════════════════════════════════════════"));
	UE_LOG(LogSmartUEAssistantTools, Error, TEXT("║ RESULT: FAILED"));
	UE_LOG(LogSmartUEAssistantTools, Error, TEXT("║ %s"), *Message);
	UE_LOG(LogSmartUEAssistantTools, Error, TEXT("╚═══════════════════════════════════════════"));
	return {false, Message, nullptr};
}

// ==================== Copy Properties Tool ====================

FCopyPropertiesTool::FCopyPropertiesTool()
{
	Spec.Name = TEXT("copy_properties");
	Spec.Description = TEXT("Copy property values from one actor to selected actors.");
	Spec.Permission = EToolPermission::Modify;
	Spec.bRequireConfirm = false;
	
	Spec.Params.Add({TEXT("SourceActor"), TEXT("string"), false, TEXT("Source actor name")});
	Spec.Params.Add({TEXT("PropertyPaths"), TEXT("array"), false, TEXT("Properties to copy")});
	Spec.Params.Add({TEXT("ApplyToSelected"), TEXT("boolean"), true, TEXT("Apply to all selected actors")});
}

FAIToolResult FCopyPropertiesTool::Execute(const TSharedPtr<FJsonObject>& Args)
{
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("╔═══════════════════════════════════════════"));
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("║ COPY_PROPERTIES Tool Executing"));
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("╚═══════════════════════════════════════════"));

	if (!GEditor)
	{
		return {false, TEXT("Editor not available"), nullptr};
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return {false, TEXT("World not available"), nullptr};
	}

	// Get source actor name
	FString SourceActorName = Args->GetStringField(TEXT("SourceActor"));
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("Source Actor: %s"), *SourceActorName);

	// Find source actor
	AActor* SourceActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor->GetName().Equals(SourceActorName, ESearchCase::IgnoreCase) ||
			Actor->GetActorLabel().Equals(SourceActorName, ESearchCase::IgnoreCase))
		{
			SourceActor = Actor;
			break;
		}
	}

	if (!SourceActor)
	{
		return {false, FString::Printf(TEXT("Source actor not found: %s"), *SourceActorName), nullptr};
	}

	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("Found source actor: %s (Label: %s)"), *SourceActor->GetName(), *SourceActor->GetActorLabel());

	// Get property paths to copy
	const TArray<TSharedPtr<FJsonValue>>* PropertyPathsArray;
	if (!Args->TryGetArrayField(TEXT("PropertyPaths"), PropertyPathsArray))
	{
		return {false, TEXT("PropertyPaths must be an array of strings"), nullptr};
	}

	TArray<FString> PropertyPaths;
	for (const TSharedPtr<FJsonValue>& PathValue : *PropertyPathsArray)
	{
		FString Path;
		if (PathValue->TryGetString(Path))
		{
			PropertyPaths.Add(Path);
		}
	}

	if (PropertyPaths.Num() == 0)
	{
		return {false, TEXT("No property paths provided"), nullptr};
	}

	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("Properties to copy: %s"), *FString::Join(PropertyPaths, TEXT(", ")));

	// Get target actors
	bool bApplyToSelected = Args->HasField(TEXT("ApplyToSelected")) ? Args->GetBoolField(TEXT("ApplyToSelected")) : true;
	TArray<AActor*> TargetActors;

	if (bApplyToSelected)
	{
		USelection* SelectedActors = GEditor->GetSelectedActors();
		if (SelectedActors && SelectedActors->Num() > 0)
		{
			for (FSelectionIterator It(*SelectedActors); It; ++It)
			{
				if (AActor* Actor = Cast<AActor>(*It))
				{
					// Skip source actor
					if (Actor != SourceActor)
					{
						TargetActors.Add(Actor);
					}
				}
			}
		}
	}

	if (TargetActors.Num() == 0)
	{
		return {false, TEXT("No target actors selected (or only source actor is selected)"), nullptr};
	}

	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("Target actors count: %d"), TargetActors.Num());

	// Copy properties
	int32 TotalCopied = 0;
	TArray<FString> FailedProperties;

	for (const FString& PropertyPath : PropertyPaths)
	{
		// Get value from source
		UObject* SourceContainer = nullptr;
		FProperty* SourceProperty = FPropertyModificationHelper::FindPropertyByPath(SourceActor, PropertyPath, SourceContainer);

		if (!SourceProperty || !SourceContainer)
		{
			FailedProperties.Add(PropertyPath);
			UE_LOG(LogSmartUEAssistantTools, Warning, TEXT("  Source property not found: %s"), *PropertyPath);
			continue;
		}

		// Get source value as JSON
		TSharedPtr<FJsonValue> SourceValue = FPropertyModificationHelper::GetPropertyValueAsJson(SourceProperty, SourceContainer);
		if (!SourceValue.IsValid())
		{
			FailedProperties.Add(PropertyPath);
			UE_LOG(LogSmartUEAssistantTools, Warning, TEXT("  Failed to get source value: %s"), *PropertyPath);
			continue;
		}

		UE_LOG(LogSmartUEAssistantTools, Log, TEXT("  Copying property: %s"), *PropertyPath);

		// Apply to all target actors
		for (AActor* TargetActor : TargetActors)
		{
			FString Error;
			if (FPropertyModificationHelper::SetPropertyValue(TargetActor, PropertyPath, SourceValue, false, TEXT("Copy Property"), Error))
			{
				TotalCopied++;
			}
			else
			{
				UE_LOG(LogSmartUEAssistantTools, Warning, TEXT("    Failed on %s: %s"), *TargetActor->GetName(), *Error);
			}
		}
	}

	// Refresh viewports
	if (TotalCopied > 0 && GEditor)
	{
		GEditor->RedrawAllViewports();
		GEditor->NoteSelectionChange();
	}

	// Build result
	FString Message;
	if (TotalCopied > 0)
	{
		Message = FString::Printf(TEXT("Copied %d properties from '%s' to %d actors"),
			TotalCopied, *SourceActor->GetActorLabel(), TargetActors.Num());
		if (FailedProperties.Num() > 0)
		{
			Message += FString::Printf(TEXT(" (%d properties failed)"), FailedProperties.Num());
		}
		UE_LOG(LogSmartUEAssistantTools, Log, TEXT("╔═══════════════════════════════════════════"));
		UE_LOG(LogSmartUEAssistantTools, Log, TEXT("║ RESULT: SUCCESS"));
		UE_LOG(LogSmartUEAssistantTools, Log, TEXT("║ %s"), *Message);
		UE_LOG(LogSmartUEAssistantTools, Log, TEXT("╚═══════════════════════════════════════════"));

		TSharedPtr<FJsonObject> ResultData = MakeShareable(new FJsonObject);
		ResultData->SetNumberField(TEXT("total_copied"), TotalCopied);
		ResultData->SetNumberField(TEXT("target_count"), TargetActors.Num());
		ResultData->SetStringField(TEXT("source_actor"), SourceActor->GetActorLabel());

		return {true, Message, ResultData};
	}

	Message = FString::Printf(TEXT("Failed to copy any properties. Failed: %s"), *FString::Join(FailedProperties, TEXT(", ")));
	UE_LOG(LogSmartUEAssistantTools, Error, TEXT("╔═══════════════════════════════════════════"));
	UE_LOG(LogSmartUEAssistantTools, Error, TEXT("║ RESULT: FAILED"));
	UE_LOG(LogSmartUEAssistantTools, Error, TEXT("║ %s"), *Message);
	UE_LOG(LogSmartUEAssistantTools, Error, TEXT("╚═══════════════════════════════════════════"));
	return {false, Message, nullptr};
}


// �?自动注册工具
#include "ToolAutoRegister.h"

REGISTER_EDITOR_TOOL(FSetActorPropertyTool)
REGISTER_EDITOR_TOOL(FBatchSetPropertiesTool)
REGISTER_EDITOR_TOOL(FGetAvailablePropertiesTool)
REGISTER_EDITOR_TOOL(FSmartSetPropertyTool)
REGISTER_EDITOR_TOOL(FAdjustPropertyTool)
REGISTER_EDITOR_TOOL(FCopyPropertiesTool)

