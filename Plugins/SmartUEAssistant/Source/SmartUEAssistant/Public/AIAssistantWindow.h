/**
 * AI 助手窗口头文件
 * AI Assistant Window header file
 */
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Delegates/Delegate.h"
#include "Widgets/SWidget.h"

class SScrollBox;
class SButton;
class SMultiLineEditableTextBox;
class STextBlock;
//class SCheckBox; // 移除：复选框 / Removed: checkbox
class FActiveTimerHandle; // 修正：与引擎声明一致使用 class / Fixed: use class consistent with engine declaration

/**
 * AI 助手窗口 Slate 组件
 * AI Assistant Window Slate widget
 */
class SAIAssistantWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAIAssistantWindow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	/** UI 控件 / UI Widgets */
	TSharedPtr<SScrollBox> ChatScrollBox;                  // 聊天滚动框 / Chat scroll box
	TSharedPtr<SMultiLineEditableTextBox> InputTextBox;    // 输入文本框 / Input text box
	TSharedPtr<SButton> SendButton;                        // 发送按钮 / Send button
	
	/** 头部操作按钮组 / Header action buttons */
	TSharedPtr<SButton> SettingsButton;                    // 设置按钮 / Settings button
	TSharedPtr<SButton> DocumentationButton;               // 文档按钮 / Documentation button
	TSharedPtr<SButton> FeedbackButton;                    // 反馈按钮 / Feedback button
	
	/** 输入区快捷操作按钮 / Input area quick action buttons */
	TSharedPtr<SButton> AttachFileButton;                  // 附件按钮 / Attach file button
	TSharedPtr<SButton> ScreenshotButton;                  // 截图按钮 / Screenshot button
	TSharedPtr<SButton> CodeBlockButton;                   // 代码块按钮 / Code block button
	TSharedPtr<SButton> ClearInputButton;                  // 清除输入按钮 / Clear input button
	TSharedPtr<SButton> SwitchModelButton;                 // 切换模型按钮 / Switch model button
	TSharedPtr<STextBlock> SwitchModelLabel;               // 可动态更新的按钮文本 / Dynamically updatable button text
	//TSharedPtr<SCheckBox> AttachSceneContextCheck;       // 移除：是否附带场景上下文 / Removed: attach scene context checkbox
	TSharedPtr<SButton> InsertSceneSummaryButton;          // 逻辑保留但UI不呈现 / Logic retained but UI hidden

	/** 请求进度与取消 / Request progress and cancellation */
	TSharedPtr<STextBlock> StatusLabel;                    // 显示"请求中…/显示中…" / Display "Requesting.../Showing..."
	TSharedPtr<SButton> CancelButton;                      // 取消按钮 / Cancel button
	bool bRequestInFlight = false;                         // 服务层是否有请求在进行中 / Whether service layer has request in progress

	/** 事件处理 / Event handling */
	FReply OnSendMessage();
	// 新增：头部操作事件
	FReply OnSettingsClicked();
	FReply OnDocumentationClicked();
	FReply OnFeedbackClicked();
	// 新增：输入区快捷操作事件
	FReply OnAttachFileClicked();
	FReply OnScreenshotClicked();
	FReply OnCodeBlockClicked();
	FReply OnClearInputClicked();
	FReply OnSwitchModelClicked();
	FReply OnInputKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent);
	FReply OnInsertSceneSummaryClicked(); // 不再呈现按钮
	FReply OnConfirmExecute();            // 新增：补齐声明
	FReply OnSendClicked();               // 新增：发送按钮点击
	FReply OnCancelClicked();             // 新增：取消按钮点击

	//hys 关键词验证
	bool ShouldEnableTools(const FString& UserInput) const;
	
	// 新增：输入框回调与发送按钮点击
	void OnTextCommitted(const FText& InText, ETextCommit::Type CommitMethod);
	void OnTextChanged(const FText& InText);
	// 自动判定是否需要附带场景上下文
	bool ShouldAttachSceneContext(const FString& In) const;

	// 聊天逻辑
	void AddMessageToChat(const FString& Message, bool bIsUser);
	void OnAIResponseReceived(const FString& Response);

	// 输入校验
	bool ValidateUserInput(const FString& In, FString& OutError) const;
	// 新增：对AI响应进行清洗，移除非法/不可见字符，统一换行（类内联实现）
	FString SanitizeResponse(const FString& In) const {
		if (In.IsEmpty()) return FString();
		FString Out; Out.Reserve(In.Len());
		for (int32 i = 0; i < In.Len(); ++i) {
			const TCHAR C = In[i];
			if (C == TEXT('\r')) { continue; } // 统一换行到\n
			const uint32 Code = static_cast<uint32>(C);
			// 过滤控制字符（保留\n 和 \t）
			if (Code < 0x20u && C != TEXT('\n') && C != TEXT('\t')) { continue; }
			// 过滤零宽字符与BOM
			if (C == 0x200B || C == 0x200C || C == 0x200D || C == 0xFEFF) { continue; }
			// 过滤非字符码位
			if ((Code >= 0xFDD0u && Code <= 0xFDEFu) || Code == 0xFFFEu || Code == 0xFFFFu) { continue; }
			Out.AppendChar(C);
		}
		// 去除首尾空白
		Out.TrimStartAndEndInline();
		return Out;
	}

	// UI流式显示
	void StartStreamingDisplay(const FString& FullText);
	void CancelStreaming();
	EActiveTimerReturnType HandleStreamTick(double CurrentTime, float DeltaTime);

	// 辅助：创建消息气泡与消息行
	TSharedRef<SWidget> CreateMessageBubble(const FString& InText, bool bIsUser, TSharedPtr<STextBlock>* OutTextBlock);
	TSharedRef<SWidget> CreateMessageRow(const TSharedRef<SWidget>& Bubble, bool bIsUser);

	// 其他辅助
	FOptionalSize GetDynamicBubbleWidth() const; // 基于窗口宽度动态计算气泡最大宽度
	FOptionalSize GetDynamicWrapWidth() const;           // 文本换行宽度，考虑边距扣减
	float GetWrapAt() const;                              // 供STextBlock::WrapTextAt绑定的float版本
	FString GetUserInitial() const;      // 读取系统用户名首字母
	TSharedRef<SWidget> CreateIconButton(const FString& IconContent, const FString& TooltipText, TDelegate<FReply()> OnClicked);

	// 新增：请求状态更新与查询
	void UpdateRequestUI();              // 根据 bRequestInFlight 与流式状态更新UI
	bool IsStreamingActive() const {     // 仅查询，不做修改
		return CurrentAIStreamingBlock.IsValid() || CurrentAIStreamingEditableBlock.IsValid();
	}

	// 流式显示状态
	TWeakPtr<STextBlock> CurrentAIStreamingBlock;  // 旧的，用于STextBlock
	TWeakPtr<SMultiLineEditableTextBox> CurrentAIStreamingEditableBlock;  // 新的，用于可编辑文本框
	FString StreamingTarget;
	int32 StreamingIndex = 0;
	TSharedPtr<FActiveTimerHandle> StreamingTimerHandle; // 修正：改为 TSharedPtr 以支持 ToSharedRef()
	float StreamingInterval = 0.03f;

	// 动画平滑算法所需
	double StreamingStartTime = 0.0;
	float TotalDuration = 2.0f;

	// 窗口引用缓存
	TWeakPtr<SWindow> CachedDocWindow;
	TWeakPtr<SWindow> CachedFeedbackWindow;

	// 模型切换
	TArray<FString> ModelOptions { TEXT("GPT-4.1"), TEXT("GPT-4o"), TEXT("GPT-5") };
	int32 CurrentModelIndex = 2; // 默认 GPT-5
	FString GetCurrentModelLabel() const { return ModelOptions.IsValidIndex(CurrentModelIndex) ? ModelOptions[CurrentModelIndex] : TEXT("默认模型"); }

	// 新增：抑制内部触发的取消/超时提示（例如发送新消息前主动取消上一个请求）
	bool bSuppressCancelNotice = false;

	// 新增：待确认状态缓存
	bool bHasPendingConfirm = false;         // 当前是否存在待确认操作
	FString LastPendingUserMessage;          // 触发预览/确认的原始用户消息

	// 新增：自动重试状态
	bool bRetryScheduled = false;            // 是否已安排自动重试倒计时
	int32 RetryAttempt = 0;                  // 已重试次数
	int32 MaxRetryAttempts = 3;              // 最大重试次数（指数退避）
	float RetryCountdownRemaining = 0.0f;    // 距离下一次重试的剩余秒数
	float RetryBaseDelay = 1.0f;             // 基础退避秒数，实际=Base*2^Attempt
	TSharedPtr<FActiveTimerHandle> RetryTimerHandle; // 重试倒计时计时器句柄
	FString LastErrorCode;                   // 最近错误码（如 RATE/NET/SERVER/…）
	FString LastErrorText;                   // 最近错误描述（无前缀，便于状态栏显示）
	FString LastSentUserMessage;             // 最近一次发送的用户消息（用于自动重试复用）

	// 自动重试：解析错误并决定是否重试
	bool ParseErrorAndDecideRetry(const FString& In, FString& OutDisplayText, bool& bOutRetryable, FString& OutCode) const;
	void ScheduleRetry();                    // 安排下一次重试（注册ActiveTimer）
	void CancelRetrySchedule();              // 取消已安排的重试
	void TriggerRetry();                     // 立即触发重试发送
	EActiveTimerReturnType HandleRetryTick(double CurrentTime, float DeltaTime); // 倒计时回调


};