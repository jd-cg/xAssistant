// AI助手窗口实现文件
#include "AIAssistantWindow.h"
#include "AIService.h"
#include "SmartUEAssistantSettings.h" // 新增：读取配置
#include "SceneContextProvider.h"     // 新增：生成场景摘要
#include "SmartUEAssistantStyle.h"    // 新增：样式集
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
// 移除显式复选框UI包含，保持最小依赖
//#include "Widgets/Input/SCheckBox.h"
#include "Framework/Application/SlateApplication.h"
#include "Async/Async.h"
#include "Styling/CoreStyle.h" // 新增：使用默认复合字体，支持中文
#include "Widgets/Input/SEditableTextBox.h"
#include "Framework/Application/SlateApplication.h"



// 新增：高级布局与样式
#include "SWebBrowser.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Framework/Application/SlateApplication.h"



#if WITH_EDITOR
#include "Editor.h" // 为 GEditor 提供声明
#endif

#define LOCTEXT_NAMESPACE "SAIAssistantWindow"
#define LOCTEXT_NAMESPACE "AIAssistant"
void SAIAssistantWindow::Construct(const FArguments& InArgs)
{
    // 创建聊天记录滚动框
    ChatScrollBox = SNew(SScrollBox);
    
    // 创建输入文本框 - 使用现代化样式
    InputTextBox = SNew(SMultiLineEditableTextBox)
        .HintText(LOCTEXT("InputHint", "输入您的问题或指令..."))
        .AllowMultiLine(true)
        .AutoWrapText(true)
        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 12)) // 指定支持中文的复合字体
        .OnKeyDownHandler(this, &SAIAssistantWindow::OnInputKeyDown) // 新增：回车发送，Shift+Enter换行
        .Style(&FSmartUEAssistantStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(FSmartUEAssistantStyle::NAME_ModernInputBox));

    // 创建发送按钮 - 使用现代化样式
    SendButton = SNew(SButton)
        .Text(LOCTEXT("SendButton", "发送"))
        .OnClicked(this, &SAIAssistantWindow::OnSendMessage)
        .ButtonStyle(&FSmartUEAssistantStyle::Get().GetWidgetStyle<FButtonStyle>(FSmartUEAssistantStyle::NAME_ModernButton));

    // 创建头部操作按钮 - 使用现代化样式
    SettingsButton = SNew(SButton)
        .Text(LOCTEXT("SettingsBtn", "设置"))
        .OnClicked(this, &SAIAssistantWindow::OnSettingsClicked)
        .ToolTipText(LOCTEXT("SettingsTip", "打开设置面板"))
        .ButtonStyle(&FSmartUEAssistantStyle::Get().GetWidgetStyle<FButtonStyle>(FSmartUEAssistantStyle::NAME_ModernIconButton));

    DocumentationButton = SNew(SButton)
        .Text(LOCTEXT("DocsBtn", "文档"))
        .OnClicked(this, &SAIAssistantWindow::OnDocumentationClicked)
        .ToolTipText(LOCTEXT("DocsTip", "查看使用文档"))
        .ButtonStyle(&FSmartUEAssistantStyle::Get().GetWidgetStyle<FButtonStyle>(FSmartUEAssistantStyle::NAME_ModernIconButton));

    FeedbackButton = SNew(SButton)
        .Text(LOCTEXT("FeedbackBtn", "反馈"))
        .OnClicked(this, &SAIAssistantWindow::OnFeedbackClicked)
        .ToolTipText(LOCTEXT("FeedbackTip", "提交反馈意见"))
        .ButtonStyle(&FSmartUEAssistantStyle::Get().GetWidgetStyle<FButtonStyle>(FSmartUEAssistantStyle::NAME_ModernIconButton));

    // 创建输入区快捷操作按钮 - 使用现代化样式
    AttachFileButton = SNew(SButton)
        .Text(LOCTEXT("AttachBtn", "📎"))
        .OnClicked(this, &SAIAssistantWindow::OnAttachFileClicked)
        .ToolTipText(LOCTEXT("AttachTip", "附加文件"))
        .ButtonStyle(&FSmartUEAssistantStyle::Get().GetWidgetStyle<FButtonStyle>(FSmartUEAssistantStyle::NAME_ModernIconButton));

    ScreenshotButton = SNew(SButton)
        .Text(LOCTEXT("ScreenshotBtn", "📷"))
        .OnClicked(this, &SAIAssistantWindow::OnScreenshotClicked)
        .ToolTipText(LOCTEXT("ScreenshotTip", "截图"))
        .ButtonStyle(&FSmartUEAssistantStyle::Get().GetWidgetStyle<FButtonStyle>(FSmartUEAssistantStyle::NAME_ModernIconButton));

    CodeBlockButton = SNew(SButton)
        .Text(LOCTEXT("CodeBtn", "💻"))
        .OnClicked(this, &SAIAssistantWindow::OnCodeBlockClicked)
        .ToolTipText(LOCTEXT("CodeTip", "插入代码块"))
        .ButtonStyle(&FSmartUEAssistantStyle::Get().GetWidgetStyle<FButtonStyle>(FSmartUEAssistantStyle::NAME_ModernIconButton));

    ClearInputButton = SNew(SButton)
        .Text(LOCTEXT("ClearBtn", "🗑"))
        .OnClicked(this, &SAIAssistantWindow::OnClearInputClicked)
        .ToolTipText(LOCTEXT("ClearTip", "清空输入"))
        .ButtonStyle(&FSmartUEAssistantStyle::Get().GetWidgetStyle<FButtonStyle>(FSmartUEAssistantStyle::NAME_ModernIconButton));

    SwitchModelButton = SNew(SButton)
        .OnClicked(this, &SAIAssistantWindow::OnSwitchModelClicked)
        .ToolTipText(LOCTEXT("ModelTip", "切换模型"))
        .ButtonStyle(&FSmartUEAssistantStyle::Get().GetWidgetStyle<FButtonStyle>(FSmartUEAssistantStyle::NAME_ModernButton))
        [
            SAssignNew(SwitchModelLabel, STextBlock)
            .Text(FText::FromString(GetCurrentModelLabel()))
        ];

    // 新布局：顶部标题栏 + 内容区（滚动）+ 输入区
    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FSmartUEAssistantStyle::Get().GetBrush(FSmartUEAssistantStyle::NAME_PanelBackground))
        .Padding(0)
        [
            SNew(SVerticalBox)
            // 顶部栏：标题 + 头部操作按钮组
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SBorder)
                .BorderImage(FSmartUEAssistantStyle::Get().GetBrush(FSmartUEAssistantStyle::NAME_PanelBackgroundDark))
                .Padding(6)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("HeaderTitle", "xAssistant"))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
                        .ColorAndOpacity(FSlateColor::UseForeground())
                    ]
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    [
                        SNew(SSpacer)
                    ]
                    // 头部按钮组：设置 | 文档 | 反馈
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(FMargin(4, 0))
                    [
                        SettingsButton.ToSharedRef()
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(FMargin(4, 0))
                    [
                        DocumentationButton.ToSharedRef()
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(FMargin(4, 0))
                    [
                        FeedbackButton.ToSharedRef()
                    ]
                ]
            ]
            // 聊天内容区
            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            .Padding(FMargin(8, 6, 8, 6))
            [
                SNew(SBorder)
                .BorderImage(FSmartUEAssistantStyle::Get().GetBrush(FSmartUEAssistantStyle::NAME_PanelBackgroundDark))
                .Padding(6)
                [
                    ChatScrollBox.ToSharedRef()
                ]
            ]
            // 输入区：快捷操作工具栏 + 输入框 + 发送按钮
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FMargin(8, 0, 8, 8))
            [
                SNew(SBorder)
                .BorderImage(FSmartUEAssistantStyle::Get().GetBrush(FSmartUEAssistantStyle::NAME_PanelBackground))
                .Padding(FMargin(8))
                [
                    SNew(SVerticalBox)
                    // 快捷操作工具栏
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(FMargin(0, 0, 0, 6))
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(FMargin(2, 0))
                        [
                            AttachFileButton.ToSharedRef()
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(FMargin(2, 0))
                        [
                            ScreenshotButton.ToSharedRef()
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(FMargin(2, 0))
                        [
                            CodeBlockButton.ToSharedRef()
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(FMargin(2, 0))
                        [
                            ClearInputButton.ToSharedRef()
                        ]
                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        [
                            SNew(SSpacer)
                        ]
                        // 新增：状态文本 + 取消按钮
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(FMargin(6, 0))
                        [
                            SAssignNew(StatusLabel, STextBlock)
                            .Text(FText::GetEmpty())
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                            .ColorAndOpacity(FSmartUEAssistantStyle::TextSecondary)
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(FMargin(6, 0))
                        [
                            SAssignNew(CancelButton, SButton)
                            .Text(LOCTEXT("CancelBtn", "取消"))
                            .OnClicked(this, &SAIAssistantWindow::OnCancelClicked)
                            .IsEnabled(false)
                            .ButtonStyle(&FSmartUEAssistantStyle::Get().GetWidgetStyle<FButtonStyle>(FSmartUEAssistantStyle::NAME_ModernButton))
                        ]
                    ]
                
                    // 输入框 + 发送
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        [
                            SAssignNew(InputTextBox, SMultiLineEditableTextBox)
                            .Style(&FSmartUEAssistantStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(FSmartUEAssistantStyle::NAME_ModernInputBox))
                            .Text(FText::GetEmpty())
                            .AutoWrapText(true)
                            .AllowContextMenu(true)
                            .OnKeyDownHandler(this, &SAIAssistantWindow::OnInputKeyDown)
                            .OnTextCommitted(this, &SAIAssistantWindow::OnTextCommitted)
                            .OnTextChanged(this, &SAIAssistantWindow::OnTextChanged)
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(FMargin(6, 0))
                        [
                            SNew(SButton)
                            .Text(LOCTEXT("SendBtn", "发送"))
                            .OnClicked(this, &SAIAssistantWindow::OnSendClicked)
                            .ButtonStyle(&FSmartUEAssistantStyle::Get().GetWidgetStyle<FButtonStyle>(FSmartUEAssistantStyle::NAME_ModernButton))
                        ]
                    ]
                ]
            ]
        ]
    ];

    // 初始化请求UI
    UpdateRequestUI();
}

FReply SAIAssistantWindow::OnSendMessage()
{
    if (!InputTextBox.IsValid())
    {
        return FReply::Handled();
    }

    const FString UserInput = InputTextBox->GetText().ToString();

    // 清理自动重试状态
    CancelRetrySchedule();
    bRetryScheduled = false;
    RetryAttempt = 0;
    LastErrorCode.Empty();
    LastErrorText.Empty();

    // 发送/回车后：立即清空输入框，并将焦点留在输入框，便于继续输入
    if (InputTextBox.IsValid())
    {
        InputTextBox->SetText(FText::GetEmpty());
        FSlateApplication::Get().SetKeyboardFocus(InputTextBox, EFocusCause::SetDirectly);
    }

    // 进入发送流程前：若存在待确认计划，根据设置处理“空回车即确认/关键词确认/取消”
    {
        USmartUEAssistantSettings* Settings = GetMutableDefault<USmartUEAssistantSettings>();
        const bool bAcceptEnabled = Settings ? Settings->bEnterAcceptsPending : true;
        const bool bServicePending = FAIService::Get().HasPendingConfirmation();
        if (bServicePending && bAcceptEnabled)
        {
            auto ParseCSV = [](const FString& Csv) {
                TArray<FString> Out; Csv.ParseIntoArray(Out, TEXT(","), true);
                for (FString& S : Out) { S.TrimStartAndEndInline(); }
                return Out;
            };
            const TArray<FString> Accepts = Settings ? ParseCSV(Settings->ConfirmAcceptKeywords) : TArray<FString>{ TEXT("确认"), TEXT("执行"), TEXT("同意"), TEXT("继续"), TEXT("ok"), TEXT("yes") };
            const TArray<FString> Cancels = Settings ? ParseCSV(Settings->ConfirmCancelKeywords) : TArray<FString>{ TEXT("取消"), TEXT("放弃"), TEXT("中止"), TEXT("no") };

            FString Trimmed = UserInput; Trimmed.TrimStartAndEndInline();
            const auto InList = [](const FString& S, const TArray<FString>& L){ for (const FString& X : L) { if (S.Equals(X, ESearchCase::IgnoreCase)) return true; } return false; };

            if (Trimmed.IsEmpty() || InList(Trimmed, Accepts))
            {
                // 用户确认执行（空输入或同意关键词）
                AddMessageToChat(Trimmed.IsEmpty() ? TEXT("你: （确认执行）") : FString::Printf(TEXT("你: %s"), *Trimmed), true);
                CancelStreaming();
                FAIService::Get().ConfirmPendingPlan(FOnAIMessageReceived::CreateSP(SharedThis(this), &SAIAssistantWindow::OnAIResponseReceived));
                bHasPendingConfirm = false;
                bRequestInFlight = true;
                UpdateRequestUI();
                return FReply::Handled();
            }
            if (InList(Trimmed, Cancels))
            {
                AddMessageToChat(FString::Printf(TEXT("你: %s"), *Trimmed), true);
                FAIService::Get().CancelPendingPlan();
                OnAIResponseReceived(TEXT("已取消待执行的操作。"));
                bHasPendingConfirm = false;
                return FReply::Handled();
            }
        }
    }

    FString Error;
    if (!ValidateUserInput(UserInput, Error))
    {
        AddMessageToChat(FString::Printf(TEXT("输入无效：%s"), *Error), false);
        return FReply::Handled();
    }

    // 发送新消息前：停止上一次流式显示，并显式取消上一个HTTP请求（同时压制下一条取消提示）
    CancelStreaming(); // 仅UI清理
    bSuppressCancelNotice = true;
    FAIService::Get().CancelCurrentRequest();

    // 记录用户消息
    AddMessageToChat(FString::Printf(TEXT("你: %s"), *UserInput), true);
    LastPendingUserMessage = UserInput; // 缓存以便预览后可知来源
    LastSentUserMessage = UserInput;    // 自动重试复用
    bHasPendingConfirm = false; // 进入新一轮会话，清除本地待确认标记，具体由AI响应时更新

    // 加载设置（保留），但根据需求默认直接执行工具（不经过确认，不预览）
    USmartUEAssistantSettings* Settings = GetMutableDefault<USmartUEAssistantSettings>();
    const bool bAutoExecuteSafeTools = Settings ? Settings->bAutoExecuteSafeTools : true;


    
    // 调用服务
    const bool bEnableTools = ShouldEnableTools(UserInput);//hys 根据用户输入关键词判断是否需要工具
    const bool bDryRun = !bAutoExecuteSafeTools; // 关闭自动执行时，先预览
    const bool bUserConfirmed = false;           // 首次发送不视为已确认

    // 标记进行中并刷新UI
    bRequestInFlight = true;
    UpdateRequestUI();

    //hys 根据判断选择调用哪个函数
    if (bEnableTools)
    {
        FAIService::Get().SendMessageWithTools(
            UserInput,
            FOnAIMessageReceived::CreateSP(SharedThis(this), &SAIAssistantWindow::OnAIResponseReceived),
            /*bEnableTools*/ true,
            /*bDryRun*/ bDryRun,
            /*bUserConfirmed*/ bUserConfirmed
        );
    }
    else
    {
        // 无工具，轻量版
        FAIService::Get().SendMessage(
            UserInput,
            FOnAIMessageReceived::CreateSP(SharedThis(this), &SAIAssistantWindow::OnAIResponseReceived)
        );
    }

    return FReply::Handled();
}

//hys 关键词筛选
bool SAIAssistantWindow::ShouldEnableTools(const FString& UserInput) const
{
    if (UserInput.IsEmpty()) return false;

    FString Lower = UserInput.ToLower();

    // 操作意图关键词（可根据你的插件常用场景增减）
    static const TArray<FString> OperationKeywords = {
        TEXT("生成"), TEXT("创建"), TEXT("建"), TEXT("加"), TEXT("新增"),
        TEXT("修改"), TEXT("改"), TEXT("设置"), TEXT("调"), TEXT("改变"),
        TEXT("移动"), TEXT("移到"), TEXT("挪"), TEXT("定位"),
        TEXT("旋转"), TEXT("转"), TEXT("朝向"),
        TEXT("缩放"), TEXT("放大"), TEXT("缩小"),
        TEXT("隐藏"), TEXT("显示"), TEXT("可见"),
        TEXT("删除"), TEXT("移除"), TEXT("删"),
        TEXT("批量"), TEXT("batch"), TEXT("所有"),
        TEXT("灯光"), TEXT("light"), TEXT("亮度"), TEXT("颜色"),
        TEXT("选择"), TEXT("选中"), TEXT("聚焦"), TEXT("focus"),
        TEXT("重命名"), TEXT("rename"), TEXT("标签"), TEXT("tag"),
        TEXT("对齐"), TEXT("分布"), TEXT("保存"), TEXT("执行")
    };

    for (const FString& Keyword : OperationKeywords)
    {
        if (Lower.Contains(Keyword))
        {
            return true;
        }
    }
    
    // 默认：纯聊天不启用工具
    return false;
}
// 新增：实现文本回车提交与内容变化处理
void SAIAssistantWindow::OnTextCommitted(const FText& InText, ETextCommit::Type CommitMethod)
{
    if (CommitMethod == ETextCommit::OnEnter)
    {
        OnSendMessage();
    }
}

void SAIAssistantWindow::OnTextChanged(const FText& InText)
{
    const bool bHasText = !InText.IsEmptyOrWhitespace();
    if (SendButton.IsValid())
    {
        SendButton->SetEnabled(bHasText);
    }
}

FReply SAIAssistantWindow::OnSendClicked()
{
    return OnSendMessage();
}

FReply SAIAssistantWindow::OnInputKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
    const FKey Key = InKeyEvent.GetKey();
    if (Key == EKeys::Enter || Key == EKeys::Virtual_Accept)
    {
        // Shift+Enter：插入换行，交由控件默认行为处理
        if (InKeyEvent.IsShiftDown())
        {
            return FReply::Unhandled();
        }
        return OnSendMessage();
    }
    return FReply::Unhandled();
}

void SAIAssistantWindow::OnAIResponseReceived(const FString& Response)
{
    const FString Safe = SanitizeResponse(Response);

    // 若这是我们主动触发的取消反馈且设置了抑制，则吞掉一次（兼容旧提示与新前缀）
    if (bSuppressCancelNotice && (Safe.Contains(TEXT("已取消：用户主动取消请求")) || Safe.Contains(TEXT("[ERR:CANCEL]"))))
    {
        bSuppressCancelNotice = false; // 只抑制一次
        bRequestInFlight = false;
        UpdateRequestUI();
        return;
    }
    // 无论收到何种正常反馈，都清除抑制标志，避免误吞后续消息
    bSuppressCancelNotice = false;

    // 使用流式显示（或一次性显示），避免与普通AddMessageToChat重复
    AsyncTask(ENamedThreads::GameThread, [this, Safe]()
    {
        // 请求已完成，切换至非请求中状态
        bRequestInFlight = false;
        CancelStreaming();
        StartStreamingDisplay(Safe);
        // 根据服务状态更新本地待确认标志（用于UI交互）
        bHasPendingConfirm = FAIService::Get().HasPendingConfirmation();

        // 解析错误并根据策略安排自动重试
        FString DisplayText; bool bRetryable = false; FString Code;
        const bool bIsError = ParseErrorAndDecideRetry(Safe, DisplayText, bRetryable, Code);
        if (bIsError)
        {
            LastErrorCode = Code;
            LastErrorText = DisplayText;
            if (bRetryable && RetryAttempt < MaxRetryAttempts)
            {
                ScheduleRetry();
            }
            else
            {
                CancelRetrySchedule();
            }
        }
        else
        {
            CancelRetrySchedule();
            LastErrorCode.Empty();
            LastErrorText.Empty();
        }

        UpdateRequestUI();
    });
}

bool SAIAssistantWindow::ValidateUserInput(const FString& In, FString& OutError) const
{
    const int32 N = In.Len();
    for (int32 i = 0; i < N; ++i)
    {
        const uint32 Ch = static_cast<uint32>(In[i]);
        // 控制字符（除换行与制表符）
        if (Ch < 0x20 && Ch != '\n' && Ch != '\t')
        {
            OutError = TEXT("包含不可见控制字符");
            return false;
        }
        // 单独出现的低代理项
        if (Ch >= 0xDC00 && Ch <= 0xDFFF)
        {
            OutError = TEXT("存在不成对的低代理项");
            return false;
        }
        // 高代理项必须与后续低代理项成对
        if (Ch >= 0xD800 && Ch <= 0xDBFF)
        {
            if (i + 1 >= N)
            {
                OutError = TEXT("末尾存在不完整的Unicode代理对");
                return false;
            }
            const uint32 Next = static_cast<uint32>(In[i + 1]);
            if (!(Next >= 0xDC00 && Next <= 0xDFFF))
            {
                OutError = TEXT("Unicode代理项未正确配对");
                return false;
            }
            // 成对有效，跳过下一个
            ++i;
            continue;
        }
        // 非字符码位（U+FDD0..U+FDEF、U+FFFE、U+FFFF 等）
        if ((Ch >= 0xFDD0 && Ch <= 0xFDEF) || Ch == 0xFFFE || Ch == 0xFFFF)
        {
            OutError = TEXT("包含保留的非字符码位");
            return false;
        }
    }
    return true;
}

void SAIAssistantWindow::StartStreamingDisplay(const FString& FullText)
{
    // 短文本直接一次性显示，避免无谓的打字机动画
    const FString Trimmed = FullText; // Sanitize 已做修整
    if (Trimmed.Len() <= 120)
    {
        AddMessageToChat(FString::Printf(TEXT("AI: %s"), *Trimmed), false);
        UpdateRequestUI();
        return;
    }

    // 创建一个初始"AI: "消息气泡并逐步填充（使用可编辑文本框以支持复制）
    // 注意：我们直接创建可编辑文本框，不使用CreateMessageBubble的OutTextBlock参数
    // 这样可以确保文本可选择
    TSharedPtr<SMultiLineEditableTextBox> InnerEditableText;
    
    // 手动创建消息气泡（包含可编辑文本框）
    const FLinearColor TimeColor = FSmartUEAssistantStyle::TextSecondary.CopyWithNewOpacity(0.85f);
    const FDateTime Now = FDateTime::Now();
    const FString TimeStr = FString::Printf(TEXT("%s.%03d"), *Now.ToString(TEXT("%Y-%m-%d %H:%M:%S")), Now.GetMillisecond());
    
    TSharedRef<SMultiLineEditableTextBox> TextWidget = SNew(SMultiLineEditableTextBox)
        .Text(FText::FromString(TEXT("AI: ")))
        .IsReadOnly(true)
        .AutoWrapText(true)
        .WrapTextAt(this, &SAIAssistantWindow::GetWrapAt)
        .Justification(ETextJustify::Left)
        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 15))
        .ForegroundColor(FSlateColor::UseForeground())
        .BackgroundColor(FLinearColor::Transparent)
        .Padding(FMargin(0))
        .Margin(FMargin(0))
        .Style(FAppStyle::Get(), "Log.TextBox")
        .AllowContextMenu(true)
        .SelectAllTextWhenFocused(false)
        .AlwaysShowScrollbars(false);
    
    InnerEditableText = TextWidget;
    
    TSharedRef<STextBlock> TimeWidget = SNew(STextBlock)
        .Text(FText::FromString(TimeStr))
        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
        .ColorAndOpacity(TimeColor);
    
    const FSlateBrush* BubbleBrush = FSmartUEAssistantStyle::Get().GetBrush(FSmartUEAssistantStyle::NAME_AIMessageBubble);
    
    TSharedRef<SWidget> Bubble = SNew(SBorder)
        .BorderImage(BubbleBrush)
        .ForegroundColor(FSmartUEAssistantStyle::TextPrimary)
        .Padding(FMargin(14, 10))
        .HAlign(HAlign_Left)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SBox)
                .WidthOverride(this, &SAIAssistantWindow::GetDynamicWrapWidth)
                [
                    TextWidget
                ]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Right)
            .Padding(FMargin(0, 6, 0, 0))
            [
                TimeWidget
            ]
        ];
    
    TSharedRef<SWidget> Row = CreateMessageRow(Bubble, /*bIsUser*/ false);

    ChatScrollBox->AddSlot()
        .Padding(FMargin(0, 6))
        [
            Row
        ];
    ChatScrollBox->ScrollToEnd();

    CurrentAIStreamingEditableBlock = InnerEditableText;
    StreamingTarget = FullText;
    StreamingIndex = 0;

    // 注册 ActiveTimer 以逐步追加文本
    if (TSharedPtr<SWidget> ThisWidget = AsShared())
    {
        StreamingTimerHandle = RegisterActiveTimer(StreamingInterval, FWidgetActiveTimerDelegate::CreateSP(this, &SAIAssistantWindow::HandleStreamTick));
    }

    UpdateRequestUI(); // 切换为“显示中…”
}

void SAIAssistantWindow::CancelStreaming()
{
    // 仅UI层清理：不再取消HTTP请求，避免服务层状态混乱
    if (StreamingTimerHandle.IsValid())
    {
        UnRegisterActiveTimer(StreamingTimerHandle.ToSharedRef());
        StreamingTimerHandle.Reset();
    }
    StreamingTarget.Reset();
    StreamingIndex = 0;
    CurrentAIStreamingBlock.Reset();
    CurrentAIStreamingEditableBlock.Reset();  // 新增：清理可编辑文本框引用

    UpdateRequestUI();
}

EActiveTimerReturnType SAIAssistantWindow::HandleStreamTick(double CurrentTime, float DeltaTime)
{
    // 检查是否有有效的流式显示文本框
    if (!CurrentAIStreamingEditableBlock.IsValid() && !CurrentAIStreamingBlock.IsValid())
    {
        return EActiveTimerReturnType::Stop;
    }
    
    const int32 Chunk = FMath::Clamp(30, 1, 200); // 每次追加字符数
    const int32 Remaining = StreamingTarget.Len() - StreamingIndex;
    const int32 Take = FMath::Min(Chunk, Remaining);

    const FString Prefix = TEXT("AI: ");
    const FString SoFar = StreamingTarget.Left(StreamingIndex + Take);

    // 优先使用可编辑文本框（新方案）
    if (TSharedPtr<SMultiLineEditableTextBox> EditableBlock = CurrentAIStreamingEditableBlock.Pin())
    {
        EditableBlock->SetText(FText::FromString(Prefix + SoFar));
    }
    // 兼容旧的STextBlock方案
    else if (TSharedPtr<STextBlock> Block = CurrentAIStreamingBlock.Pin())
    {
        Block->SetText(FText::FromString(Prefix + SoFar));
    }
    
    ChatScrollBox->ScrollToEnd();

    StreamingIndex += Take;
    if (StreamingIndex >= StreamingTarget.Len())
    {
        // 结束
        StreamingTarget.Reset();
        StreamingIndex = 0;
        CurrentAIStreamingBlock.Reset();
        CurrentAIStreamingEditableBlock.Reset();
        UpdateRequestUI();
        return EActiveTimerReturnType::Stop;
    }

    return EActiveTimerReturnType::Continue;
}

void SAIAssistantWindow::AddMessageToChat(const FString& Message, bool bIsUser)
{
    TSharedRef<SWidget> Bubble = CreateMessageBubble(Message, bIsUser, nullptr);
    TSharedRef<SWidget> Row = CreateMessageRow(Bubble, bIsUser);

    // 添加到聊天记录（内容区居中，左右对齐）
    ChatScrollBox->AddSlot()
        .Padding(FMargin(0, 6))
        [
            Row
        ];
    
    // 滚动到底部
    ChatScrollBox->ScrollToEnd();
}

FReply SAIAssistantWindow::OnConfirmExecute()
{
    if (FAIService::Get().HasPendingConfirmation())
    {
        FAIService::Get().ConfirmPendingPlan(FOnAIMessageReceived::CreateSP(SharedThis(this), &SAIAssistantWindow::OnAIResponseReceived));
        // 确认执行后进入请求中
        bRequestInFlight = true;
        UpdateRequestUI();
    }
    return FReply::Handled();
}

FReply SAIAssistantWindow::OnInsertSceneSummaryClicked()
{
    // 该按钮当前不在UI中呈现，仅保留接口以兼容旧逻辑
    return FReply::Handled();
}

bool SAIAssistantWindow::ShouldAttachSceneContext(const FString& In) const
{
    // 当前版本默认不自动附带场景上下文，仅保留占位逻辑，防止链接错误
    // 后续可按关键词和编辑器选择态启用
    return false;
}

TSharedRef<SWidget> SAIAssistantWindow::CreateMessageBubble(const FString& InText, bool bIsUser, TSharedPtr<STextBlock>* OutTextBlock)
{
    // 由容器传递前景色，保证深/浅主题下对比度一致
    const FLinearColor TimeColor = FSmartUEAssistantStyle::TextSecondary.CopyWithNewOpacity(0.85f);

    // 统一使用可选择的只读文本框，支持用户复制内容
    // 即使是流式显示，也使用此控件（SetText性能足够）
    TSharedPtr<SWidget> TextContentWidget;
    
    // 创建可选择的只读文本框
    TextContentWidget = SNew(SMultiLineEditableTextBox)
        .Text(FText::FromString(InText))
        .IsReadOnly(true)  // 只读模式，允许选择但不允许编辑
        .AutoWrapText(true)
        .WrapTextAt(this, &SAIAssistantWindow::GetWrapAt)
        .Justification(ETextJustify::Left)
        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 15))
        .ForegroundColor(FSlateColor::UseForeground())
        .BackgroundColor(FLinearColor::Transparent)  // 透明背景，与气泡融合
        .Padding(FMargin(0))  // 移除内边距
        .Margin(FMargin(0))
        .Style(FAppStyle::Get(), "Log.TextBox")  // 使用日志风格的只读文本框
        .AllowContextMenu(true)  // 允许右键菜单（复制等）
        .SelectAllTextWhenFocused(false)  // 不要在获得焦点时全选
        .AlwaysShowScrollbars(false);  // 不显示滚动条
    
    // 如果需要流式更新，我们需要保留STextBlock（因为性能更好）
    // 但由于用户需要能复制，我们暂时牺牲一点性能，所有消息都用可编辑文本框
    if (OutTextBlock)
    {
        // 为了兼容性，仍然创建一个STextBlock引用（但实际上我们会用可编辑文本框）
        // 注意：这里存在类型不匹配，需要另一种方案
        // 暂时保留流式显示用STextBlock，但添加注释说明这是临时方案
        TSharedRef<STextBlock> TextWidget = SNew(STextBlock)
            .Text(FText::FromString(InText))
            .AutoWrapText(true)
            .WrapTextAt(this, &SAIAssistantWindow::GetWrapAt)
            .WrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping)
            .Justification(ETextJustify::Left)
            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 15))
            .ColorAndOpacity(FSlateColor::UseForeground());
        
        *OutTextBlock = TextWidget; // 返回内部文本以便流式更新（临时方案，后续应改为可编辑文本框）
        TextContentWidget = TextWidget;
    }

    // 时间戳（现代化对齐在右下角）
    const FDateTime Now = FDateTime::Now();
    // 目标格式：YYYY-MM-DD HH:mm:ss.SSS（毫秒3位，左侧补零）
    const FString TimeStr = FString::Printf(TEXT("%s.%03d"), *Now.ToString(TEXT("%Y-%m-%d %H:%M:%S")), Now.GetMillisecond());
    TSharedRef<STextBlock> TimeWidget = SNew(STextBlock)
        .Text(FText::FromString(TimeStr))
        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
        .ColorAndOpacity(TimeColor);

    // 使用StyleSet的圆角画刷
    const FSlateBrush* BubbleBrush = bIsUser ? 
        FSmartUEAssistantStyle::Get().GetBrush(FSmartUEAssistantStyle::NAME_UserMessageBubble) :
        FSmartUEAssistantStyle::Get().GetBrush(FSmartUEAssistantStyle::NAME_AIMessageBubble);

    // 移除内部固定 640px 上限，交由外层 CreateMessageRow 的动态宽度控制
    return SNew(SBorder)
        .BorderImage(BubbleBrush)
        .ForegroundColor(FSmartUEAssistantStyle::TextPrimary)
        .Padding(FMargin(14, 10))
        .HAlign(bIsUser ? HAlign_Right : HAlign_Left)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SBox)
                .WidthOverride(this, &SAIAssistantWindow::GetDynamicWrapWidth)
                [
                    TextContentWidget.ToSharedRef()
                ]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Right)
            .Padding(FMargin(0, 6, 0, 0))
            [
                TimeWidget
            ]
        ];
}

TSharedRef<SWidget> SAIAssistantWindow::CreateMessageRow(const TSharedRef<SWidget>& Bubble, bool bIsUser)
{
    const FLinearColor AvatarText = FSmartUEAssistantStyle::TextPrimary;

    // 新需求：AI用机器人图标「🤖」，用户头像用首字母
    FString AvatarLabel = bIsUser ? GetUserInitial() : TEXT("🤖");

    // 使用样式集的圆角画刷作为头像底板
    const FSlateBrush* AvatarBrush = bIsUser
        ? FSmartUEAssistantStyle::Get().GetBrush(FSmartUEAssistantStyle::NAME_UserMessageBubble)
        : FSmartUEAssistantStyle::Get().GetBrush(FSmartUEAssistantStyle::NAME_AIMessageBubble);

    TSharedRef<SWidget> Avatar =
        SNew(SBorder)
        .BorderImage(AvatarBrush)
        .ForegroundColor(AvatarText)
        .Padding(FMargin(6, 4))
        [
            SNew(STextBlock)
            .Text(FText::FromString(AvatarLabel))
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
            .ColorAndOpacity(FSlateColor::UseForeground())
        ];

    TSharedRef<SHorizontalBox> Pair = SNew(SHorizontalBox);
    if (!bIsUser)
    {
        Pair->AddSlot().AutoWidth().VAlign(VAlign_Bottom).Padding(FMargin(0, 0, 8, 0))[ Avatar ];
        Pair->AddSlot().AutoWidth()[
            SNew(SBox)
            .WidthOverride(this, &SAIAssistantWindow::GetDynamicBubbleWidth)
            [
                Bubble
            ]
        ];
    }
    else
    {
        Pair->AddSlot().AutoWidth()[
            SNew(SBox)
            .WidthOverride(this, &SAIAssistantWindow::GetDynamicBubbleWidth)
            [
                Bubble
            ]
        ];
        Pair->AddSlot().AutoWidth().VAlign(VAlign_Bottom).Padding(FMargin(8, 0, 0, 0))[ Avatar ];
    }

    return SNew(SBox)
        .HAlign(HAlign_Fill)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .FillWidth(bIsUser ? 1.0f : 0.0f)
            [
                SNew(SSpacer)
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                Pair
            ]
            + SHorizontalBox::Slot()
            .FillWidth(bIsUser ? 0.0f : 1.0f)
            [
                SNew(SSpacer)
            ]
        ];
}

// 新增：头部操作按钮事件处理

FReply SAIAssistantWindow::OnSettingsClicked()
{
    // 临时实现：显示设置面板或配置文件
    AddMessageToChat(TEXT("AI: 设置面板功能正在开发中，敬请期待。"), false);
    return FReply::Handled();
}

FReply SAIAssistantWindow::OnDocumentationClicked()
{

    // 文档的 URL
    FString URL = TEXT("https://github.com/jd-cg/xAssistant");

    // 创建 WebBrowser 控件
    TSharedPtr<SWebBrowser> WebBrowserWidget = SNew(SWebBrowser)
        .InitialURL(URL)
        .SupportsTransparency(true)
        .ShowControls(false) ; // 显示地址栏、刷新等按钮


    // 创建一个新窗口显示浏览器
    TSharedRef<SWindow> WebWindow = SNew(SWindow)
        .Title(LOCTEXT("WebWindowTitle", "说明文档")) // 使用 LOCTEXT 包装
        .ClientSize(FVector2D(1280, 800))
        .ScreenPosition(FVector2D(200, 200))
        .SupportsMaximize(true)
        .SupportsMinimize(true);



    WebWindow->SetContent(WebBrowserWidget.ToSharedRef());

    // 打开窗口
    FSlateApplication::Get().AddWindow(WebWindow);

    AddMessageToChat(TEXT("AI: 已为您打开内置文档"), false);

    return FReply::Handled();
}

FReply SAIAssistantWindow::OnFeedbackClicked()
{

    // 反馈通道的 URL
    FString URL = TEXT("http://www.jd-cg.com/");

    // 创建 WebBrowser 控件
    TSharedPtr<SWebBrowser> WebBrowserWidget = SNew(SWebBrowser)
        .InitialURL(URL)
        .SupportsTransparency(true)
        .ShowControls(false) ; // 显示地址栏、刷新等按钮


    // 创建一个新窗口显示浏览器
    TSharedRef<SWindow> WebWindow = SNew(SWindow)
        .Title(LOCTEXT("WebWindowTitle", "反馈通道")) // 使用 LOCTEXT 包装
        .ClientSize(FVector2D(1280, 800))
        .ScreenPosition(FVector2D(200, 200))
        .SupportsMaximize(true)
        .SupportsMinimize(true);



    WebWindow->SetContent(WebBrowserWidget.ToSharedRef());

    // 打开窗口
    FSlateApplication::Get().AddWindow(WebWindow);

    AddMessageToChat(TEXT("AI: 已为您打开反馈通道"), false);
    return FReply::Handled();
}

FReply SAIAssistantWindow::OnAttachFileClicked()
{
    AddMessageToChat(TEXT("AI: 附件功能正在开发中。"), false);
    return FReply::Handled();
}
//hys,执行虚幻内置高分辨率截图命令
FReply SAIAssistantWindow::OnScreenshotClicked()
{
    // 先给反馈（
    AddMessageToChat(TEXT("AI: 正在生成高分辨率截图..."), false);
    // 4 表示 4 倍分辨率（视口 1080p → 输出约 4K），可改成 2、8、16 等
    FString Command = TEXT("HighResShot 4");
    if (GEditor)
    {
        GEditor->Exec(GEditor->GetEditorWorldContext().World(), *Command);
    
    }
    else
    {
        AddMessageToChat(TEXT("AI: 截图失败，编辑器环境异常"), false);
    }

    return FReply::Handled();

}

FReply SAIAssistantWindow::OnCodeBlockClicked()
{
    AddMessageToChat(TEXT("AI: 代码块插入功能正在开发中。"), false);
    return FReply::Handled();
}

FReply SAIAssistantWindow::OnClearInputClicked()
{
    if (InputTextBox.IsValid())
    {
        InputTextBox->SetText(FText::GetEmpty());
        FSlateApplication::Get().SetKeyboardFocus(InputTextBox, EFocusCause::SetDirectly);
    }
    return FReply::Handled();
}

FReply SAIAssistantWindow::OnSwitchModelClicked()
{
    if (ModelOptions.Num() > 0)
    {
        CurrentModelIndex = (CurrentModelIndex + 1) % ModelOptions.Num();
        if (SwitchModelLabel.IsValid())
        {
            SwitchModelLabel->SetText(FText::FromString(GetCurrentModelLabel()));
        }
    }
    return FReply::Handled();
}

FReply SAIAssistantWindow::OnCancelClicked()
{
    const bool bStreaming = IsStreamingActive();

    // 若处于自动重试倒计时，允许中断
    if (bRetryScheduled)
    {
        CancelRetrySchedule();
        UpdateRequestUI();
        AddMessageToChat(TEXT("AI: 已取消自动重试。"), false);
        return FReply::Handled();
    }

    if (bRequestInFlight)
    {
        // 取消网络请求，并抑制服务层的取消提示
        bSuppressCancelNotice = true;
        FAIService::Get().CancelCurrentRequest();
        bRequestInFlight = false;
        CancelStreaming();
        UpdateRequestUI();
        AddMessageToChat(TEXT("AI: 已取消当前请求。"), false);
        return FReply::Handled();
    }

    if (bStreaming)
    {
        CancelStreaming();
        UpdateRequestUI();
        AddMessageToChat(TEXT("AI: 已停止显示。"), false);
        return FReply::Handled();
    }

    return FReply::Handled();
}

FOptionalSize SAIAssistantWindow::GetDynamicBubbleWidth() const
{
    const float MaxWidth = 800.0f; // 可根据窗口大小动态调整，示例固定值
    return FOptionalSize(MaxWidth);
}

FOptionalSize SAIAssistantWindow::GetDynamicWrapWidth() const
{
    const float MaxWidth = 720.0f; // 文字区域比气泡略小
    return FOptionalSize(MaxWidth);
}

float SAIAssistantWindow::GetWrapAt() const
{
    return 700.0f;
}

FString SAIAssistantWindow::GetUserInitial() const
{
    FString User = FPlatformProcess::UserName();
    if (User.Len() > 0)
    {
        return User.Mid(0, 1).ToUpper();
    }
    return TEXT("U");
}

TSharedRef<SWidget> SAIAssistantWindow::CreateIconButton(const FString& IconContent, const FString& TooltipText, TDelegate<FReply()> OnClicked)
{
    return SNew(SButton)
        .Text(FText::FromString(IconContent))
        .ToolTipText(FText::FromString(TooltipText))
        .OnClicked(OnClicked)
        .ButtonStyle(&FSmartUEAssistantStyle::Get().GetWidgetStyle<FButtonStyle>(FSmartUEAssistantStyle::NAME_ModernIconButton));
}

void SAIAssistantWindow::UpdateRequestUI()
{
    if (!StatusLabel.IsValid() || !CancelButton.IsValid())
    {
        return;
    }

    const bool bStreaming = IsStreamingActive();
    FText StatusText = FText::GetEmpty();

    if (bRequestInFlight)
    {
        StatusText = LOCTEXT("StatusRequesting", "请求中…");
    }
    else if (bRetryScheduled)
    {
        const int32 Secs = FMath::Max(0, FMath::CeilToInt(RetryCountdownRemaining));
        const int32 NextAttempt = RetryAttempt + 1;
        const FString Tip = FString::Printf(TEXT("错误(%s)：%s，将在 %ds 后重试（%d/%d）…"), *LastErrorCode, *LastErrorText, Secs, NextAttempt, MaxRetryAttempts);
        StatusText = FText::FromString(Tip);
    }
    else if (bStreaming)
    {
        StatusText = LOCTEXT("StatusDisplaying", "显示中…");
    }
    else if (FAIService::Get().HasPendingConfirmation())
    {
        StatusText = LOCTEXT("StatusPendingConfirm", "待确认…");
    }

    StatusLabel->SetText(StatusText);

    const bool bCanCancel = bRequestInFlight || bStreaming || bRetryScheduled;
    CancelButton->SetEnabled(bCanCancel);
}

bool SAIAssistantWindow::ParseErrorAndDecideRetry(const FString& In, FString& OutDisplayText, bool& bOutRetryable, FString& OutCode) const
{
    bOutRetryable = false;
    OutCode.Empty();
    OutDisplayText = In;

    // 标准前缀：[ERR:CODE] 文本
    int32 L, R;
    if (In.FindChar('[', L) && In.FindChar(']', R) && L == 0 && R > L)
    {
        const FString Prefix = In.Mid(L + 1, R - L - 1); // ERR:CODE
        if (Prefix.StartsWith(TEXT("ERR:")))
        {
            OutCode = Prefix.Mid(4);
            OutDisplayText = In.Mid(R + 1).TrimStart();
        }
    }
    else
    {
        // 兼容旧错误文案的简单映射
        if (In.Contains(TEXT("请求失败，请检查网络连接和API配置")))
        {
            OutCode = TEXT("NET");
        }
        else if (In.Contains(TEXT("超时")))
        {
            OutCode = TEXT("TIMEOUT");
        }
        else if (In.Contains(TEXT("已取消")))
        {
            OutCode = TEXT("CANCEL");
        }
    }

    const FString CodeUpper = OutCode.ToUpper();
    if (CodeUpper == TEXT("RATE") || CodeUpper == TEXT("NET") || CodeUpper == TEXT("SERVER") || CodeUpper == TEXT("TIMEOUT"))
    {
        bOutRetryable = true;
    }
    else
    {
        bOutRetryable = false; // AUTH / API / CANCEL / UNKNOWN 均不自动重试
    }

    return !OutCode.IsEmpty();
}

void SAIAssistantWindow::ScheduleRetry()
{
    // 计算指数退避秒数：Base * 2^Attempt
    const float Delay = RetryBaseDelay * FMath::Pow(2.0f, static_cast<float>(RetryAttempt));
    RetryCountdownRemaining = Delay;
    bRetryScheduled = true;

    // 重置现有定时器
    if (RetryTimerHandle.IsValid())
    {
        UnRegisterActiveTimer(RetryTimerHandle.ToSharedRef());
        RetryTimerHandle.Reset();
    }

    if (TSharedPtr<SWidget> ThisWidget = AsShared())
    {
        RetryTimerHandle = RegisterActiveTimer(0.2f, FWidgetActiveTimerDelegate::CreateSP(this, &SAIAssistantWindow::HandleRetryTick));
    }
}

void SAIAssistantWindow::CancelRetrySchedule()
{
    if (RetryTimerHandle.IsValid())
    {
        UnRegisterActiveTimer(RetryTimerHandle.ToSharedRef());
        RetryTimerHandle.Reset();
    }
    bRetryScheduled = false;
    RetryCountdownRemaining = 0.0f;
}

void SAIAssistantWindow::TriggerRetry()
{
    CancelRetrySchedule();

    if (LastSentUserMessage.IsEmpty())
    {
        UpdateRequestUI();
        return;
    }

    // 增加尝试计数
    RetryAttempt = RetryAttempt + 1;

    // 读取设置决定是否DryRun
    USmartUEAssistantSettings* Settings = GetMutableDefault<USmartUEAssistantSettings>();
    const bool bAutoExecuteSafeTools = Settings ? Settings->bAutoExecuteSafeTools : true;

    const bool bEnableTools = true;
    const bool bDryRun = !bAutoExecuteSafeTools;
    const bool bUserConfirmed = false;

    bRequestInFlight = true;
    UpdateRequestUI();

    FAIService::Get().SendMessageWithTools(
        LastSentUserMessage,
        FOnAIMessageReceived::CreateSP(SharedThis(this), &SAIAssistantWindow::OnAIResponseReceived),
        /*bEnableTools*/ bEnableTools,
        /*bDryRun*/ bDryRun,
        /*bUserConfirmed*/ bUserConfirmed
    );
}

EActiveTimerReturnType SAIAssistantWindow::HandleRetryTick(double CurrentTime, float DeltaTime)
{
    RetryCountdownRemaining = FMath::Max(0.0f, RetryCountdownRemaining - DeltaTime);
    if (RetryCountdownRemaining <= 0.0f)
    {
        // 触发重试
        TriggerRetry();
        UpdateRequestUI();
        return EActiveTimerReturnType::Stop;
    }

    // 更新状态栏文案
    UpdateRequestUI();
    return EActiveTimerReturnType::Continue;
}

#undef LOCTEXT_NAMESPACE