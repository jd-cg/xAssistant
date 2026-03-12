// xAssistant Style Set - 全局样式管理实现
#include "SmartUEAssistantStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateBorderBrush.h"
#include "Interfaces/IPluginManager.h"
#include "HAL/PlatformFilemanager.h"
#include "Styling/AppStyle.h"          // 新增：AppStyle 父样式
#include "Styling/StyleColors.h"       // 新增：系统色表
#include "Styling/SlateStyleMacros.h"


#define RootToContentDir Style->RootToContentDir
TSharedPtr<FSlateStyleSet> FSmartUEAssistantStyle::StyleInstance = nullptr;

//hys初始化向量
const FName FSmartUEAssistantStyle::NAME_ToolbarIcon(TEXT("SmartUEAssistant.OpenCommandPanel"));

// 样式名称常量定义
const FName FSmartUEAssistantStyle::NAME_UserMessageBubble(TEXT("SmartUEAssistant.UserMessageBubble"));
const FName FSmartUEAssistantStyle::NAME_AIMessageBubble(TEXT("SmartUEAssistant.AIMessageBubble"));
const FName FSmartUEAssistantStyle::NAME_ModernButton(TEXT("SmartUEAssistant.ModernButton"));
const FName FSmartUEAssistantStyle::NAME_ModernIconButton(TEXT("SmartUEAssistant.ModernIconButton"));
const FName FSmartUEAssistantStyle::NAME_ModernInputBox(TEXT("SmartUEAssistant.ModernInputBox"));
const FName FSmartUEAssistantStyle::NAME_RobotIcon(TEXT("SmartUEAssistant.RobotIcon"));
const FName FSmartUEAssistantStyle::NAME_SettingsIcon(TEXT("SmartUEAssistant.SettingsIcon"));
const FName FSmartUEAssistantStyle::NAME_DocumentIcon(TEXT("SmartUEAssistant.DocumentIcon"));
const FName FSmartUEAssistantStyle::NAME_FeedbackIcon(TEXT("SmartUEAssistant.FeedbackIcon"));
const FName FSmartUEAssistantStyle::NAME_AttachIcon(TEXT("SmartUEAssistant.AttachIcon"));
const FName FSmartUEAssistantStyle::NAME_ScreenshotIcon(TEXT("SmartUEAssistant.ScreenshotIcon"));
const FName FSmartUEAssistantStyle::NAME_CodeIcon(TEXT("SmartUEAssistant.CodeIcon"));
const FName FSmartUEAssistantStyle::NAME_ClearIcon(TEXT("SmartUEAssistant.ClearIcon"));
// 新增：面板背景样式名称定义
const FName FSmartUEAssistantStyle::NAME_PanelBackground(TEXT("SmartUEAssistant.PanelBackground"));
const FName FSmartUEAssistantStyle::NAME_PanelBackgroundDark(TEXT("SmartUEAssistant.PanelBackgroundDark"));

// 颜色主题定义（改为引用AppStyle系统色）
const FLinearColor FSmartUEAssistantStyle::PrimaryBlue = FStyleColors::AccentBlue.GetSpecifiedColor();
const FLinearColor FSmartUEAssistantStyle::SecondaryGray = FStyleColors::Recessed.GetSpecifiedColor();
const FLinearColor FSmartUEAssistantStyle::BackgroundDark = FStyleColors::Panel.GetSpecifiedColor();
const FLinearColor FSmartUEAssistantStyle::TextPrimary = FStyleColors::Foreground.GetSpecifiedColor();
const FLinearColor FSmartUEAssistantStyle::TextSecondary = FStyleColors::ForegroundHover.GetSpecifiedColor();
const FLinearColor FSmartUEAssistantStyle::BorderColor = FStyleColors::Input.GetSpecifiedColor();
const FLinearColor FSmartUEAssistantStyle::HoverColor = FStyleColors::Hover.
    GetSpecifiedColor();
const FLinearColor FSmartUEAssistantStyle::PressedColor = FStyleColors::Primary.
    GetSpecifiedColor();

TSharedRef<FSlateStyleSet> FSmartUEAssistantStyle::Create()
{
    TSharedRef<FSlateStyleSet> Style = MakeShared<FSlateStyleSet>(GetStyleSetName());

    // 继承 AppStyle，自动应用引擎统一字体与基础控件样式
    Style->SetParentStyleName(FAppStyle::GetAppStyleSetName());
    
    // 设置资源根路径
    TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin("SmartUEAssistant");
    if (Plugin.IsValid())
    {
        Style->SetContentRoot(Plugin->GetBaseDir() / TEXT("Resources"));
    }

    const FVector2D Icon20x20(20.0f, 20.0f);
    
    // 创建圆角气泡画刷（使用系统色）
    {
        // 用户消息气泡使用更柔和的蓝色（在深色主题下更易读）
        const FLinearColor UserBubble = FLinearColor(
            PrimaryBlue.R, PrimaryBlue.G, PrimaryBlue.B, 0.8f
        ) * 0.9f; // 轻微降亮度
        Style->Set(NAME_UserMessageBubble, new FSlateRoundedBoxBrush(
            UserBubble, 8.0f
        ));
 
        Style->Set(NAME_AIMessageBubble, new FSlateRoundedBoxBrush(
            SecondaryGray, 8.0f
        ));
    }

    // 面板背景（外层/聊天区）圆角画刷
    {
        Style->Set(NAME_PanelBackground, new FSlateRoundedBoxBrush(
            BackgroundDark, 6.0f, BorderColor, 1.0f
        ));
        Style->Set(NAME_PanelBackgroundDark, new FSlateRoundedBoxBrush(
            FLinearColor::LerpUsingHSV(BackgroundDark, SecondaryGray, 0.35f), 6.0f, BorderColor, 1.0f
        ));
    }

    // 现代化按钮样式（基于系统色）
    {
        FButtonStyle ModernButton = FButtonStyle()
            .SetNormal(FSlateRoundedBoxBrush(BackgroundDark, 6.0f))
            .SetHovered(FSlateRoundedBoxBrush(HoverColor, 6.0f))
            .SetPressed(FSlateRoundedBoxBrush(PressedColor, 6.0f))
            .SetNormalForeground(TextSecondary)
            .SetHoveredForeground(TextPrimary)
            .SetPressedForeground(TextPrimary)
            .SetDisabledForeground(FLinearColor(0.4f, 0.4f, 0.4f, 1.0f))
            .SetNormalPadding(FMargin(12, 8))
            .SetPressedPadding(FMargin(12, 9, 12, 7));

        Style->Set(NAME_ModernButton, ModernButton);

        // 图标按钮：更紧凑的内边距
        FButtonStyle ModernIconButton = ModernButton;
        ModernIconButton.SetNormalPadding(FMargin(8, 6))
                        .SetPressedPadding(FMargin(8, 7, 8, 5));
        Style->Set(NAME_ModernIconButton, ModernIconButton);
    }

    // 现代化输入框样式（基于系统色）
    {
        FEditableTextBoxStyle ModernInputBox = FEditableTextBoxStyle()
            .SetBackgroundImageNormal(FSlateRoundedBoxBrush(BackgroundDark, 6.0f, BorderColor, 1.0f))
            .SetBackgroundImageHovered(FSlateRoundedBoxBrush(BackgroundDark, 6.0f, HoverColor, 1.0f))
            .SetBackgroundImageFocused(FSlateRoundedBoxBrush(BackgroundDark, 6.0f, PrimaryBlue, 2.0f))
            .SetBackgroundImageReadOnly(FSlateRoundedBoxBrush(SecondaryGray, 6.0f, BorderColor, 1.0f))
            .SetPadding(FMargin(12, 8))
            .SetForegroundColor(TextPrimary)
            .SetBackgroundColor(BackgroundDark)
            .SetReadOnlyForegroundColor(TextSecondary);

        Style->Set(NAME_ModernInputBox, ModernInputBox);
    }

    // SVG 图标资源（占位，沿用白色，保证在深色背景可见）
    {
        // hys 绑定工具栏按钮图标 
        Style->Set(NAME_ToolbarIcon, new IMAGE_BRUSH(RootToContentDir(TEXT("PlaceholderButtonIcon")), Icon20x20));
        
        Style->Set(NAME_RobotIcon, new FSlateColorBrush(FLinearColor::White));
        Style->Set(NAME_SettingsIcon, new FSlateColorBrush(FLinearColor::White));
        Style->Set(NAME_DocumentIcon, new FSlateColorBrush(FLinearColor::White));
        Style->Set(NAME_FeedbackIcon, new FSlateColorBrush(FLinearColor::White));
        Style->Set(NAME_AttachIcon, new FSlateColorBrush(FLinearColor::White));
        Style->Set(NAME_ScreenshotIcon, new FSlateColorBrush(FLinearColor::White));
        Style->Set(NAME_CodeIcon, new FSlateColorBrush(FLinearColor::White));
        Style->Set(NAME_ClearIcon, new FSlateColorBrush(FLinearColor::White));
    }

    return Style;
}

void FSmartUEAssistantStyle::Initialize()
{
    if (!StyleInstance.IsValid())
    {
        StyleInstance = Create();
        FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
    }
}

void FSmartUEAssistantStyle::Shutdown()
{
    if (StyleInstance.IsValid())
    {
        FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
        StyleInstance.Reset();
    }
}

const ISlateStyle& FSmartUEAssistantStyle::Get()
{
    if (!StyleInstance.IsValid())
    {
        Initialize();
    }
    return *StyleInstance;
}

FName FSmartUEAssistantStyle::GetStyleSetName()
{
    static FName StyleSetName(TEXT("SmartUEAssistantStyle"));
    return StyleSetName;
}