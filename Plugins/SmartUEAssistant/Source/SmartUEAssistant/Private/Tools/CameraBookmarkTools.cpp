// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/CameraBookmarkTools.h"
#include "SmartUEAssistantLog.h"
#include "Editor.h"
#include "LevelEditorViewport.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace CameraBookmarkUtils
{
	FString GetBookmarkFilePath()
	{
		return FPaths::ProjectSavedDir() / TEXT("SmartUEAssistant/CameraBookmarks.json");
	}

	bool LoadBookmarks(TSharedPtr<FJsonObject>& OutBookmarks)
	{
		const FString Path = GetBookmarkFilePath();
		if (!FPaths::FileExists(Path))
		{
			OutBookmarks = MakeShareable(new FJsonObject);
			OutBookmarks->SetObjectField(TEXT("bookmarks"), MakeShareable(new FJsonObject));
			return true;
		}

		FString JsonString;
		if (!FFileHelper::LoadFileToString(JsonString, *Path))
		{
			return false;
		}

		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
		return FJsonSerializer::Deserialize(Reader, OutBookmarks) && OutBookmarks.IsValid();
	}

	bool SaveBookmarks(const TSharedPtr<FJsonObject>& Bookmarks)
	{
		FString JsonString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
		
		if (!FJsonSerializer::Serialize(Bookmarks.ToSharedRef(), Writer))
		{
			return false;
		}

		const FString Path = GetBookmarkFilePath();
		FString Dir = FPaths::GetPath(Path);
		if (!FPaths::DirectoryExists(Dir))
		{
			IFileManager::Get().MakeDirectory(*Dir, true);
		}

		return FFileHelper::SaveStringToFile(JsonString, *Path);
	}

	FLevelEditorViewportClient* GetActiveViewportClient()
	{
		if (GEditor && GEditor->GetActiveViewport())
		{
			return static_cast<FLevelEditorViewportClient*>(GEditor->GetActiveViewport()->GetClient());
		}
		return nullptr;
	}
}

// ==================== Save Camera Bookmark Tool ====================

FSaveCameraBookmarkTool::FSaveCameraBookmarkTool()
{
	Spec.Name = TEXT("save_camera_bookmark");
	Spec.Description = TEXT("Save current viewport camera position and rotation as a bookmark");
	Spec.Permission = EToolPermission::Safe;
	Spec.bRequireConfirm = false;
	
	Spec.Params.Add({TEXT("Name"), TEXT("string"), false, TEXT("Bookmark name")});
	Spec.Params.Add({TEXT("Description"), TEXT("string"), true, TEXT("Bookmark description")});
}

FAIToolResult FSaveCameraBookmarkTool::Execute(const TSharedPtr<FJsonObject>& Args)
{
	FString Name = Args->GetStringField(TEXT("Name"));
	FString Description = Args->HasField(TEXT("Description")) ? Args->GetStringField(TEXT("Description")) : TEXT("");

	FLevelEditorViewportClient* ViewportClient = CameraBookmarkUtils::GetActiveViewportClient();
	if (!ViewportClient)
	{
		return {false, TEXT("No active viewport available"), nullptr};
	}

	FVector Location = ViewportClient->GetViewLocation();
	FRotator Rotation = ViewportClient->GetViewRotation();

	TSharedPtr<FJsonObject> Bookmarks;
	if (!CameraBookmarkUtils::LoadBookmarks(Bookmarks))
	{
		return {false, TEXT("Failed to load bookmarks"), nullptr};
	}

	TSharedPtr<FJsonObject> BookmarksData = Bookmarks->GetObjectField(TEXT("bookmarks"));
	
	TSharedPtr<FJsonObject> BookmarkData = MakeShareable(new FJsonObject);
	BookmarkData->SetStringField(TEXT("Description"), Description);
	BookmarkData->SetNumberField(TEXT("LocationX"), Location.X);
	BookmarkData->SetNumberField(TEXT("LocationY"), Location.Y);
	BookmarkData->SetNumberField(TEXT("LocationZ"), Location.Z);
	BookmarkData->SetNumberField(TEXT("RotationPitch"), Rotation.Pitch);
	BookmarkData->SetNumberField(TEXT("RotationYaw"), Rotation.Yaw);
	BookmarkData->SetNumberField(TEXT("RotationRoll"), Rotation.Roll);
	BookmarkData->SetStringField(TEXT("Timestamp"), FDateTime::Now().ToString());

	BookmarksData->SetObjectField(Name, BookmarkData);

	if (!CameraBookmarkUtils::SaveBookmarks(Bookmarks))
	{
		return {false, TEXT("Failed to save bookmarks"), nullptr};
	}

	FString Message = FString::Printf(TEXT("Saved camera bookmark: %s"), *Name);
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("%s"), *Message);
	
	return {true, Message, nullptr};
}

// ==================== Jump To Camera Bookmark Tool ====================

FJumpToCameraBookmarkTool::FJumpToCameraBookmarkTool()
{
	Spec.Name = TEXT("jump_to_camera_bookmark");
	Spec.Description = TEXT("Jump viewport camera to a saved bookmark");
	Spec.Permission = EToolPermission::Safe;
	Spec.bRequireConfirm = false;
	
	Spec.Params.Add({TEXT("Name"), TEXT("string"), false, TEXT("Bookmark name")});
}

FAIToolResult FJumpToCameraBookmarkTool::Execute(const TSharedPtr<FJsonObject>& Args)
{
	FString Name = Args->GetStringField(TEXT("Name"));

	FLevelEditorViewportClient* ViewportClient = CameraBookmarkUtils::GetActiveViewportClient();
	if (!ViewportClient)
	{
		return {false, TEXT("No active viewport available"), nullptr};
	}

	TSharedPtr<FJsonObject> Bookmarks;
	if (!CameraBookmarkUtils::LoadBookmarks(Bookmarks))
	{
		return {false, TEXT("Failed to load bookmarks"), nullptr};
	}

	TSharedPtr<FJsonObject> BookmarksData = Bookmarks->GetObjectField(TEXT("bookmarks"));
	const TSharedPtr<FJsonObject>* BookmarkPtr = nullptr;
	if (!BookmarksData->TryGetObjectField(Name, BookmarkPtr) || !BookmarkPtr)
	{
		return {false, FString::Printf(TEXT("Bookmark not found: %s"), *Name), nullptr};
	}

	const TSharedPtr<FJsonObject>& Bookmark = *BookmarkPtr;

	FVector Location(
		Bookmark->GetNumberField(TEXT("LocationX")),
		Bookmark->GetNumberField(TEXT("LocationY")),
		Bookmark->GetNumberField(TEXT("LocationZ"))
	);

	FRotator Rotation(
		Bookmark->GetNumberField(TEXT("RotationPitch")),
		Bookmark->GetNumberField(TEXT("RotationYaw")),
		Bookmark->GetNumberField(TEXT("RotationRoll"))
	);

	ViewportClient->SetViewLocation(Location);
	ViewportClient->SetViewRotation(Rotation);

	FString Message = FString::Printf(TEXT("跳转到相机书签: %s"), *Name);
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("%s"), *Message);
	
	return {true, Message, nullptr};
}

// ==================== List Camera Bookmarks Tool ====================

FListCameraBookmarksTool::FListCameraBookmarksTool()
{
	Spec.Name = TEXT("list_camera_bookmarks");
	Spec.Description = TEXT("List all saved camera bookmarks");
	Spec.Permission = EToolPermission::Safe;
	Spec.bRequireConfirm = false;
}

FAIToolResult FListCameraBookmarksTool::Execute(const TSharedPtr<FJsonObject>& Args)
{
	TSharedPtr<FJsonObject> Bookmarks;
	if (!CameraBookmarkUtils::LoadBookmarks(Bookmarks))
	{
		return {false, TEXT("Failed to load bookmarks"), nullptr};
	}

	TSharedPtr<FJsonObject> BookmarksData = Bookmarks->GetObjectField(TEXT("bookmarks"));
	
	TArray<FString> BookmarkList;
	for (const auto& Pair : BookmarksData->Values)
	{
		const TSharedPtr<FJsonObject>* BookmarkPtr = nullptr;
		if (Pair.Value->TryGetObject(BookmarkPtr) && BookmarkPtr)
		{
			FString Description = (*BookmarkPtr)->HasField(TEXT("Description")) ? 
				(*BookmarkPtr)->GetStringField(TEXT("Description")) : TEXT("");
			FString Timestamp = (*BookmarkPtr)->HasField(TEXT("Timestamp")) ? 
				(*BookmarkPtr)->GetStringField(TEXT("Timestamp")) : TEXT("");
			
			FString Entry = FString::Printf(TEXT("- %s: %s (Saved: %s)"), 
				*Pair.Key, *Description, *Timestamp);
			BookmarkList.Add(Entry);
		}
	}

	FString Message;
	if (BookmarkList.Num() > 0)
	{
		Message = FString::Printf(TEXT("Found %d camera bookmarks:\n%s"), 
			BookmarkList.Num(), *FString::Join(BookmarkList, TEXT("\n")));
	}
	else
	{
		Message = TEXT("No camera bookmarks saved");
	}

	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("%s"), *Message);
	
	// Return bookmarks in structured format
	TSharedPtr<FJsonObject> ResultData = MakeShareable(new FJsonObject);
	ResultData->SetObjectField(TEXT("Bookmarks"), BookmarksData);
	
	return {true, Message, ResultData};
}

// ==================== Delete Camera Bookmark Tool ====================

FDeleteCameraBookmarkTool::FDeleteCameraBookmarkTool()
{
	Spec.Name = TEXT("delete_camera_bookmark");
	Spec.Description = TEXT("Delete a saved camera bookmark");
	Spec.Permission = EToolPermission::Modify;
	Spec.bRequireConfirm = false;
	
	Spec.Params.Add({TEXT("Name"), TEXT("string"), false, TEXT("Bookmark name to delete")});
}

FAIToolResult FDeleteCameraBookmarkTool::Execute(const TSharedPtr<FJsonObject>& Args)
{
	FString Name = Args->GetStringField(TEXT("Name"));

	TSharedPtr<FJsonObject> Bookmarks;
	if (!CameraBookmarkUtils::LoadBookmarks(Bookmarks))
	{
		return {false, TEXT("Failed to load bookmarks"), nullptr};
	}

	TSharedPtr<FJsonObject> BookmarksData = Bookmarks->GetObjectField(TEXT("bookmarks"));
	
	if (!BookmarksData->HasField(Name))
	{
		return {false, FString::Printf(TEXT("Bookmark not found: %s"), *Name), nullptr};
	}

	BookmarksData->RemoveField(Name);

	if (!CameraBookmarkUtils::SaveBookmarks(Bookmarks))
	{
		return {false, TEXT("Failed to save bookmarks"), nullptr};
	}

	FString Message = FString::Printf(TEXT("Deleted camera bookmark: %s"), *Name);
	UE_LOG(LogSmartUEAssistantTools, Log, TEXT("%s"), *Message);
	
	return {true, Message, nullptr};
}


// 鉁?鑷姩娉ㄥ唽宸ュ叿
#include "ToolAutoRegister.h"

REGISTER_EDITOR_TOOL(FSaveCameraBookmarkTool)
REGISTER_EDITOR_TOOL(FJumpToCameraBookmarkTool)
REGISTER_EDITOR_TOOL(FListCameraBookmarksTool)
REGISTER_EDITOR_TOOL(FDeleteCameraBookmarkTool)

