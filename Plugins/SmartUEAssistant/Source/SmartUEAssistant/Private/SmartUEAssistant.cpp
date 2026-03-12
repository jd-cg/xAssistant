// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartUEAssistant.h"
#include "SmartUEAssistantLog.h"
#include "AIAssistantWindow.h"
#include "SmartUEAssistantStyle.h"
#include "ToolMenus.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "EditorAIToolRegistry.h"
// Include all built-in tools
#include "BuiltInEditorTools.h"
#include "ISettingsModule.h"
#include "SmartUEAssistantSettings.h"
#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"

#define LOCTEXT_NAMESPACE "FSmartUEAssistantModule"

// 定义静态TabName
FName FSmartUEAssistantModule::TabName = TEXT("SmartUEAssistantTab");
// 文件级静态标志：是否已注册Nomad Tab的Spawner
static bool GSmartUEAssistantTabRegistered = false;

void FSmartUEAssistantModule::StartupModule()
{
    // 初始化样式系统
    FSmartUEAssistantStyle::Initialize();

    // 注册插件命令（TCommands）
    FSmartUEAssistantCommands::Register();

    // 注册插件命令列表
    PluginCommands = MakeShareable(new FUICommandList);

    // 绑定命令到打开窗口动作
    PluginCommands->MapAction(
        FSmartUEAssistantCommands::Get().OpenCommandPanel,
        FExecuteAction::CreateRaw(this, &FSmartUEAssistantModule::OpenAIAssistantWindow),
        FCanExecuteAction()
    );

    // 将命令列表追加到 LevelEditor 的全局动作，使快捷键全局可用
#if WITH_EDITOR
    {
        FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
        LevelEditorModule.GetGlobalLevelEditorActions()->Append(PluginCommands.ToSharedRef());
    }
#endif

    // 在 ToolMenus 启动后注册菜单，避免过早访问
    UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FSmartUEAssistantModule::RegisterMenus));

	// ✅ 工具已通过 REGISTER_EDITOR_TOOL 宏自动注册（见各工具 .cpp 文件末尾）
	// 无需手动注册！

#if WITH_EDITOR
    // 新增：注册 Project Settings 面板
    if (FModuleManager::Get().IsModuleLoaded("Settings"))
    {
        if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
        {
            SettingsModule->RegisterSettings(
                TEXT("Project"),
                TEXT("Plugins"),
                TEXT("SmartUEAssistant"),
                LOCTEXT("SmartUEAssistantSettingsName", "xAssistant"),
                LOCTEXT("SmartUEAssistantSettingsDesc", "Configuration for xAssistant plugin"),
                GetMutableDefault<USmartUEAssistantSettings>()
            );
        }
    }
#endif

    // 新增：注册可停靠Nomad Tab
    {
        FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TabName,
            FOnSpawnTab::CreateRaw(this, &FSmartUEAssistantModule::OnSpawnPluginTab))
            .SetDisplayName(LOCTEXT("SmartUEAssistantTabTitle", "AI Assistant"))
            .SetMenuType(ETabSpawnerMenuType::Hidden);
        GSmartUEAssistantTabRegistered = true;
    }

	UE_LOG(LogSmartUEAssistant, Log, TEXT("SmartUEAssistant Plugin Started"));
}

void FSmartUEAssistantModule::ShutdownModule()
{
    // 取消 ToolMenus 相关注册
    UToolMenus::UnRegisterStartupCallback(this);
    UToolMenus::UnregisterOwner(this);

    // 反注册可停靠Tab（依据内部标志）
    if (GSmartUEAssistantTabRegistered)
    {
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TabName);
        GSmartUEAssistantTabRegistered = false;
    }

	// ✅ 工具会自动清理，无需手动注销
	// （注册表在模块卸载时自动清空）

#if WITH_EDITOR
    // 新增：注销 Project Settings 面板
    if (FModuleManager::Get().IsModuleLoaded("Settings"))
    {
        if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
        {
            SettingsModule->UnregisterSettings(TEXT("Project"), TEXT("Plugins"), TEXT("SmartUEAssistant"));
        }
    }
#endif

    // 注销插件命令（TCommands）
    FSmartUEAssistantCommands::Unregister();

    // 关闭样式系统
    FSmartUEAssistantStyle::Shutdown();

	UE_LOG(LogSmartUEAssistant, Log, TEXT("SmartUEAssistant Plugin Shutdown"));
}

void FSmartUEAssistantModule::RegisterMenus()
{
	// 注册到编辑器工具菜单
	FToolMenuOwnerScoped OwnerScoped(this);
	{
	    //hys
		//UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
	    UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
		{
	        //hys
	    	// 基于命令的菜单项，自动显示快捷键
			//FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
	        //Section.AddEntry(FToolMenuEntry::InitMenuEntry(FSmartUEAssistantCommands::Get().OpenCommandPanel));
	        FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("PluginOperations");
			
	        //hys
	        FToolMenuEntry Entry = FToolMenuEntry::InitToolBarButton(FSmartUEAssistantCommands::Get().OpenCommandPanel);
	        Entry.Label = LOCTEXT("Toolbar_Label", "AI助手");
	        Entry.ToolTip = LOCTEXT("Toolbar_Tooltip", "打开 AI 辅助面板");
	        Section.AddEntry(Entry);
		}
	}
}

void FSmartUEAssistantModule::OpenAIAssistantWindow()
{
    // 打开（或激活）可停靠的Nomad Tab
    FGlobalTabmanager::Get()->TryInvokeTab(TabName);
}

TSharedRef<SDockTab> FSmartUEAssistantModule::OnSpawnPluginTab(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        [
            SNew(SAIAssistantWindow)
        ];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSmartUEAssistantModule, SmartUEAssistant)