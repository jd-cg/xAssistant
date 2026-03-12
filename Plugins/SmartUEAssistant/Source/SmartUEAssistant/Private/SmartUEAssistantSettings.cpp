// 插件设置类实现文件
#include "SmartUEAssistantSettings.h"

USmartUEAssistantSettings::USmartUEAssistantSettings()
{
	// 设置默认值
	APIEndpoint = TEXT("https://api.openai.com/v1/chat/completions");
	APIKey = TEXT("sk-a49098a1905344b5bac282d234a953cd");
	SupportedModels = TEXT("gpt-3.5-turbo,gpt-4");
	RequestTimeout = 30;
	MaxTokens = 1000;
	Temperature = 0.7f;

	// 场景上下文默认值
	bAutoAttachSceneContext = true;
	MaxActorsInSummary = 100;
	bIncludeViewportCamera = true;
	bIncludeComponentSummary = true;

	// 自动执行安全工具：默认启用
	bAutoExecuteSafeTools = true;

	// 新增默认：空回车确认启用与关键词
	bEnterAcceptsPending = true;
	ConfirmAcceptKeywords = TEXT("确认,执行,同意,继续,ok,yes");
	ConfirmCancelKeywords = TEXT("取消,放弃,中止,no");

	// 会话记忆默认：启用，最大轮次6
	bEnableConversationMemory = true;
	MaxConversationRounds = 6;
}