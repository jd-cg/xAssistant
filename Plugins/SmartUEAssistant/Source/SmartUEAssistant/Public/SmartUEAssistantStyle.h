/**
 * xAssistant 样式集 - 全局样式管理
 * xAssistant Style Set - Global style management
 */
#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateTypes.h"

/**
 * xAssistant 的 Slate 样式管理器
 * Slate style manager for xAssistant
 */
class FSmartUEAssistantStyle
{
public:

    static const FName NAME_ToolbarIcon;// hys新增工具栏图标名称
    
    static TSharedRef<class FSlateStyleSet> Create();
    static void Initialize();
    static void Shutdown();
    static const class ISlateStyle& Get();
    static FName GetStyleSetName();

    /** 样式名称常量 / Style name constants */
    static const FName NAME_UserMessageBubble;
    static const FName NAME_AIMessageBubble;
    static const FName NAME_ModernButton;
    static const FName NAME_ModernIconButton;
    static const FName NAME_ModernInputBox;
    static const FName NAME_RobotIcon;
    static const FName NAME_SettingsIcon;
    static const FName NAME_DocumentIcon;
    static const FName NAME_FeedbackIcon;
    static const FName NAME_AttachIcon;
    static const FName NAME_ScreenshotIcon;
    static const FName NAME_CodeIcon;
    static const FName NAME_ClearIcon;
    // 新增：面板背景（圆角）样式名称
    static const FName NAME_PanelBackground;
    static const FName NAME_PanelBackgroundDark;

public:
    // 颜色主题
    static const FLinearColor PrimaryBlue;
    static const FLinearColor SecondaryGray;
    static const FLinearColor BackgroundDark;
    static const FLinearColor TextPrimary;
    static const FLinearColor TextSecondary;
    static const FLinearColor BorderColor;
    static const FLinearColor HoverColor;
    static const FLinearColor PressedColor;

private:
    static TSharedPtr<class FSlateStyleSet> StyleInstance;
};