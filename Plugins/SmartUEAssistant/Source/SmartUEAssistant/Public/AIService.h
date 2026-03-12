// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"

// Forward declarations to avoid including heavy headers
class FJsonObject;
class FJsonValue;
class IHttpRequest;

/**
 * 接收 AI 消息响应的委托
 * Delegate for receiving AI message responses
 * 
 * @param Response AI 响应文本 / The AI response text
 */
DECLARE_DELEGATE_OneParam(FOnAIMessageReceived, const FString& /*Response*/);

/**
 * 工具执行结果委托
 * Delegate for tool execution results
 * 
 * @param bSuccess 工具执行是否成功 / Whether the tool execution succeeded
 * @param Message 结果消息或错误描述 / Result message or error description
 */
DECLARE_DELEGATE_TwoParams(FOnToolExecutionResult, bool /*bSuccess*/, const FString& /*Message*/);

/**
 * AI 服务类 - 处理与 AI API 端点的通信
 * AI Service class that handles communication with AI API endpoints
 * 
 * 此服务管理：
 * This service manages:
 * - 向 AI API 发送 HTTP 请求（OpenAI 兼容端点）/ HTTP requests to AI API (OpenAI-compatible endpoints)
 * - 对话历史和上下文管理 / Conversation history and context management
 * - 工具调用和执行，支持试运行 / Tool calling and execution with dry-run support
 * - 请求超时和取消 / Request timeout and cancellation
 * 
 * 用法 / Usage:
 *   FAIService::Get().SendMessage(TEXT("Your query"), 
 *       FOnAIMessageReceived::CreateLambda([](const FString& Response) {
 *           UE_LOG(LogTemp, Log, TEXT("Response: %s"), *Response);
 *       }));
 */
class FAIService
{
public:
	/** 获取 AI 服务的单例实例 / Get the singleton instance of the AI Service */
	static FAIService& Get();

	/** 析构函数 - 在 cpp 中定义以确保 IHttpRequest 完整类型可见 / Destructor - defined in cpp to ensure IHttpRequest complete type visibility */
	~FAIService();

	/**
	 * 向 AI 服务发送消息（不支持工具）
	 * Send a message to the AI service without tool support
	 * 
	 * @param Message 要发送的消息 / The message to send
	 * @param Callback 接收 AI 响应的回调 / Callback to receive the AI response
	 */
	void SendMessage(const FString& Message, const FOnAIMessageReceived& Callback);

	/**
	 * 发送消息并支持工具执行
	 * Send a message with tool execution support
	 * 
	 * @param Message 要发送的消息 / The message to send
	 * @param Callback 接收 AI 响应的回调 / Callback to receive the AI response
	 * @param bEnableTools 是否启用工具调用 / Whether to enable tool calling
	 * @param bDryRun 如果为 true，预览工具执行而不实际执行 / If true, preview tool execution without actually executing
	 * @param bUserConfirmed 如果为 true，跳过危险操作的确认 / If true, skip confirmation for dangerous operations
	 */
	void SendMessageWithTools(const FString& Message, const FOnAIMessageReceived& Callback, bool bEnableTools = true, bool bDryRun = true, bool bUserConfirmed = false);

	/**
	 * 为可用工具生成 JSON 模式
	 * Generate JSON schema for available tools
	 * 
	 * @return 描述所有已注册工具的 JSON 字符串 / JSON string describing all registered tools
	 */
	FString GenerateToolsSchema() const;

	/**
	 * 解析并执行 AI 响应中的工具调用
	 * Parse and execute tool calls from AI response
	 * 
	 * @param ResponseObject 包含工具调用的 JSON 响应 / The JSON response containing tool calls
	 * @param Callback 接收执行结果的回调 / Callback to receive execution results
	 * @param bDryRun 如果为 true，预览而不执行 / If true, preview without executing
	 * @param bUserConfirmed 如果为 true，跳过确认检查 / If true, skip confirmation checks
	 */
	void ExecuteToolCalls(const TSharedPtr<FJsonObject>& ResponseObject, const FOnAIMessageReceived& Callback, bool bDryRun = true, bool bUserConfirmed = false);

	/** 取消任何活动的 HTTP 请求 / Cancel any active HTTP request */
	void CancelCurrentRequest();

	/** 检查是否有活动的 HTTP 请求 / Check if there is an active HTTP request */
	bool HasActiveRequest() const { return ActiveRequest.IsValid(); }

	/** 检查是否有等待确认的挂起执行计划 / Check if there is a pending execution plan awaiting confirmation */
	bool HasPendingConfirmation() const { return bHasPendingPlan && PendingResponseObject.IsValid(); }

	/**
	 * 确认并执行挂起的计划
	 * Confirm and execute the pending plan
	 * 
	 * @param Callback 接收执行结果的回调 / Callback to receive execution results
	 */
	void ConfirmPendingPlan(const FOnAIMessageReceived& Callback);

	/** 取消挂起的执行计划 / Cancel the pending execution plan */
	void CancelPendingPlan();

	/** 清除对话历史 / Clear conversation history */
	void ClearConversationMemory();

private:

	bool bHasSentFullToolsSchema = false;  //hys 是否已发送过完整 tools schema
	
	/** 私有构造函数（单例模式）/ Private constructor for singleton pattern */
	FAIService();

	/** 获取不带工具的系统提示词 / Get system prompt without tools */
	FString GetSystemPrompt() const;

	/** 获取带工具描述的系统提示词 / Get system prompt with tool descriptions */
	FString GetSystemPromptWithTools() const;

	/**
	 * 检查工具执行是否需要用户确认
	 * Check if a tool execution requires user confirmation
	 * 
	 * @param ToolName 要检查的工具名称 / Name of the tool to check
	 * @param bOutIsDangerous 输出参数，指示工具是否危险 / Output parameter indicating if the tool is dangerous
	 * @return 如果需要确认则返回 true / True if confirmation is required
	 */
	bool ShouldConfirmToolExecution(const FString& ToolName, bool& bOutIsDangerous) const;

	/** 插件设置引用 / Plugin settings reference */
	class USmartUEAssistantSettings* Settings;

	/** 用于超时控制的活动 HTTP 请求 / Active HTTP request for timeout control */
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> ActiveRequest;

	/** 请求超时的 Ticker 句柄 / Ticker handle for request timeout */
	FTSTicker::FDelegateHandle TimeoutHandle;

	/** 指示是否请求取消的标志 / Flag indicating if cancellation was requested */
	bool bCancelRequested;

	/** 指示超时是否已触发的标志 / Flag indicating if timeout has fired */
	bool bTimeoutFired;

	/** 指示是否有挂起执行计划的标志 / Flag indicating if there is a pending execution plan */
	bool bHasPendingPlan;

	/** 上次试运行或确认请求的缓存响应对象 / Cached response object from last dry-run or confirmation request */
	TSharedPtr<FJsonObject> PendingResponseObject;

	/** 对话消息结构 / Conversation message structure */
	struct FConvMsg
	{
		FString Role;     // 角色 / Role
		FString Content;  // 内容 / Content
	};

	/** 对话历史数组（修剪到最近的轮次）/ Conversation history array (trimmed to most recent rounds) */
	TArray<FConvMsg> Conversation;

	/** 将对话历史追加到消息数组 / Append conversation history to messages array */
	void AppendConversationHistory(TArray<TSharedPtr<FJsonValue>>& InOutMessages) const;

	/** 将用户消息推送到对话历史 / Push a user message to conversation history */
	void PushUserMessage(const FString& Content);

	/** 将助手消息推送到对话历史 / Push an assistant message to conversation history */
	void PushAssistantMessage(const FString& Content);

	/** 将对话历史修剪到 MaxConversationRounds 设置 / Trim conversation history to MaxConversationRounds setting */
	void TrimConversation();
};

/**
 * 获取默认设置对象的辅助函数
 * Helper function to get default settings object
 * 
 * @return 指定类型的可变默认对象 / Mutable default object of specified type
 */
template<typename T>
T* GetDefaultObject()
{
	return GetMutableDefault<T>();
}