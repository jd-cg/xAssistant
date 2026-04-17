// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SmartUEAssistantSettings.generated.h"

/**
 * xAssistant 插件设置
 * Plugin settings for xAssistant
 * 
 * 配置 AI 服务连接、行为和功能开关。
 * 设置存储在 Config/DefaultSmartUEAssistant.ini
 * 
 * Configures AI service connection, behavior, and feature toggles.
 * Settings are stored in Config/DefaultSmartUEAssistant.ini
 */
UCLASS(Config=SmartUEAssistant, DefaultConfig)
class SMARTUEASSISTANT_API USmartUEAssistantSettings : public UObject
{
	GENERATED_BODY()
	
public:
	USmartUEAssistantSettings();
	
	/** API endpoint URL (deprecated, use BaseURL instead) */
	UPROPERTY(Config, EditAnywhere, Category="AI Settings", meta=(Tooltip="Legacy API endpoint configuration"))
	FString APIEndpoint;

	/** Base URL for OpenAI-compatible API endpoints (e.g., DeepSeek) */
	UPROPERTY(Config, EditAnywhere, Category="AI Settings", meta=(Tooltip="Base URL for API requests"))
	FString BaseURL;
	
	/** API authentication key */
	UPROPERTY(Config, EditAnywhere, Category="AI Settings", meta=(Tooltip="Your API key for authentication"))
	FString APIKey;
	
	/** Comma-separated list of supported model names */
	UPROPERTY(Config, EditAnywhere, Category="AI Settings", meta=(Tooltip="Available AI models"))
	FString SupportedModels;
	
	/** Request timeout in seconds */
	UPROPERTY(Config, EditAnywhere, Category="AI Settings", meta=(ClampMin="1", ClampMax="300", Tooltip="Timeout for API requests in seconds"))
	int32 RequestTimeout;
	
	/** Maximum number of tokens in AI response */
	UPROPERTY(Config, EditAnywhere, Category="AI Settings", meta=(ClampMin="1", ClampMax="4000", Tooltip="Max tokens in response"))
	int32 MaxTokens;
	
	/** Temperature parameter for AI response randomness (0.0 = deterministic, 2.0 = very random) */
	UPROPERTY(Config, EditAnywhere, Category="AI Settings", meta=(ClampMin="0.0", ClampMax="2.0", Tooltip="Response randomness (0=deterministic, 2=creative)"))
	float Temperature;

	/** Automatically attach scene summary when sending messages in editor */
	UPROPERTY(EditAnywhere, Config, Category="AI", meta=(Tooltip="Include scene context in AI queries"))
	bool bAutoAttachSceneContext = true;

	/** Maximum number of actors to include in scene summary per level */
	UPROPERTY(EditAnywhere, Config, Category="AI", meta=(ClampMin="1", ClampMax="500", Tooltip="Max actors in scene context"))
	int32 MaxActorsInSummary = 100;

	/** Include viewport camera position and rotation in scene context */
	UPROPERTY(EditAnywhere, Config, Category="AI", meta=(Tooltip="Include camera position in context"))
	bool bIncludeViewportCamera = true;

	/** Include component count information in scene summary */
	UPROPERTY(EditAnywhere, Config, Category="AI", meta=(Tooltip="Include component summary"))
	bool bIncludeComponentSummary = true;

	/** Automatically execute safe tools without confirmation (Permission=Safe && !bRequireConfirm) */
	UPROPERTY(EditAnywhere, Config, Category="AI", meta=(Tooltip="Auto-execute safe operations"))
	bool bAutoExecuteSafeTools = true;

	/** Allow empty Enter key to confirm pending dangerous operations */
	UPROPERTY(EditAnywhere, Config, Category="Confirm", meta=(Tooltip="Press Enter to confirm pending operations"))
	bool bEnterAcceptsPending = true;

	/** Comma-separated keywords that confirm pending operations */
	UPROPERTY(EditAnywhere, Config, Category="Confirm", meta=(Tooltip="Keywords to confirm (comma-separated)"))
	FString ConfirmAcceptKeywords = TEXT("确认,执行,同意,继续,ok,yes");

	/** Comma-separated keywords that cancel pending operations */
	UPROPERTY(EditAnywhere, Config, Category="Confirm", meta=(Tooltip="Keywords to cancel (comma-separated)"))
	FString ConfirmCancelKeywords = TEXT("取消,放弃,中止,no");

	/** Skip confirmation for console commands (HIGH RISK - enable with caution) */
	UPROPERTY(EditAnywhere, Config, Category="Confirm", meta=(Tooltip="WARNING: Skips confirmation for console commands"))
	bool bSkipConfirmForConsoleCommands = false;

	/** Enable conversation memory to maintain context across messages */
	UPROPERTY(EditAnywhere, Config, Category="AI|Memory", meta=(Tooltip="Remember previous conversation"))
	bool bEnableConversationMemory = true;

	/** Maximum number of conversation rounds to keep in memory (0 = no history) */
	UPROPERTY(EditAnywhere, Config, Category="AI|Memory", meta=(ClampMin="0", ClampMax="50", Tooltip="Max conversation rounds to remember"))
	int32 MaxConversationRounds = 6;

	/** High resolution screenshot resolution multiplier (1-10)-截图分辨率倍率  */
	UPROPERTY(Config, EditAnywhere, Category = "Editor Features", meta = (ClampMin = "1", ClampMax = "10", Tooltip = "Multiplier for HighResShot command"))
	int32 ScreenshotResolutionMultiplier = 4;
};