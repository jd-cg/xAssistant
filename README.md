# xAssistant - UE智能助手插件

> AI驱动的Unreal Engine智能助手，通过自然语言控制编辑器操作

[![版本](https://img.shields.io/badge/版本-1.0.8-blue.svg)](https://github.com/jd-cg/xAssistant)
[![UE版本](https://img.shields.io/badge/UE-5.0+-orange.svg)](https://www.unrealengine.com)
[![协议](https://img.shields.io/badge/协议-Apache--2.0-green.svg)](LICENSE)
---

## ✨ 功能特性

- 🤖 **AI对话控制** - 自然语言操作UE编辑器
- 🎯 **智能工具执行** - 30+编辑器工具自动调用
- ⚡ **属性智能修改** - AI理解语义，精准修改对象属性
- 🎨 **现代化UI** - Slate界面，流式对话体验
- 🔐 **安全可控** - 危险操作确认，权限分级管理
- 🌐 **多模型支持** - OpenAI、DeepSeek等兼容API

---

## 📦 快速开始

### 安装

1. 将插件复制到项目的 `Plugins/` 目录
2. 启用插件：编辑器 → 插件管理器 → 搜索 "Smart UE Assistant" → 启用
3. 重启编辑器

### 配置API密钥

> **安全提示**: 配置文件 `Plugins/SmartUEAssistant/Config/DefaultSmartUEAssistant.ini` 和 `Config/DefaultSmartUEAssistant.ini` 已加入 `.gitignore`，**不会**被提交到版本库，请勿将含有真实密钥的文件手动添加到 git。

1. 复制示例模板：

   ```text
   Plugins/SmartUEAssistant/Config/DefaultSmartUEAssistant.ini.example
   → Plugins/SmartUEAssistant/Config/DefaultSmartUEAssistant.ini
   ```

2. 填写您的 API 密钥：

   ```ini
   [/Script/SmartUEAssistant.SmartUEAssistantSettings]
   APIKey=your-api-key-here
   BaseURL=https://api.deepseek.com/v1
   SupportedModels=deepseek-chat
   ```

#### 获取API密钥

- **OpenAI**: [platform.openai.com/api-keys](https://platform.openai.com/api-keys)
- **DeepSeek**: [platform.deepseek.com/api_keys](https://platform.deepseek.com/api_keys)

### 使用方法

1. 打开助手窗口：菜单栏 → **Window** → **Smart UE Assistant**
2. 在对话框输入自然语言指令，例如：
   - "把场景中的灯光改成红色"
   - "选中所有立方体并向上移动10米"
   - "创建一个点光源，位置在(0,0,100)"
   - "将选中的物体批量重命名为 Prop_，从1开始编号"

---

## 🎯 核心功能

### 1. 智能属性修改

AI自动理解语义，无需记忆复杂的属性名：

```
用户: "把灯光调亮一点"
AI: ✅ 自动识别 → Intensity 属性 → 调整数值 → 实时显示
```

**支持的属性类型**

- 灯光：颜色、亮度、半径、温度
- 变换：位置、旋转、缩放
- 可见性：显示/隐藏、移动性
- 自定义：任意UObject属性

### 2. 批量操作工具


| 工具                 | 功能       | 示例                     |
| ---------------------- | ------------ | -------------------------- |
| batch_rename_actors  | 批量重命名 | "重命名选中物体为 Wall_" |
| batch_set_visibility | 批量显隐   | "隐藏所有立方体"         |
| batch_set_mobility   | 批量移动性 | "设置为可移动"           |
| distribute_actors    | 分布排列   | "按网格排列选中物体"     |
| align_to_ground      | 对齐地面   | "让物体贴到地面"         |

### 3. 场景分析工具


| 工具                    | 功能           |
| ------------------------- | ---------------- |
| analyze_level_stats     | 关卡统计信息   |
| find_missing_references | 查找缺失引用   |
| find_duplicate_names    | 查找重复命名   |
| find_oversized_meshes   | 查找超大网格   |
| validate_level          | 验证关卡完整性 |

### 4. 相机书签


| 工具                    | 功能             |
| ------------------------- | ------------------ |
| save_camera_bookmark    | 保存当前相机视角 |
| jump_to_camera_bookmark | 跳转到已保存视角 |
| list_camera_bookmarks   | 列出所有书签     |

演示视频:https://pan.baidu.com/s/18iD-KYJevhXpjvWzB0NZJQ?pwd=w66x
---

## 🏗️ 架构设计

<p align="center">
  <img src="Plugins/SmartUEAssistant/doc/architecture.svg" alt="架构图" width="800" />
</p>

### 四层架构

```
┌─────────────────────────────────────────┐
│  UI层 - AIAssistantWindow (Slate)      │
│  对话界面、消息展示、用户交互           │
├─────────────────────────────────────────┤
│  服务层 - AIService                     │
│  API调用、请求封装、响应解析            │
├─────────────────────────────────────────┤
│  上下文层 - SceneContextProvider        │
│  场景信息、选择集、环境数据采集         │
├─────────────────────────────────────────┤
│  工具层 - Tools/*                       │
│  30+编辑器工具、属性修改、批量操作      │
└─────────────────────────────────────────┘
```

### 关键模块

- **AIService** - AI模型调用与工具编排
- **PropertyModificationHelper** - 反射式属性修改
- **EditorAIToolRegistry** - 工具注册与分发
- **SceneContextProvider** - 场景上下文采集

---

## 🛠️ 工具列表

### Actor操作 (5个)

- select_focus_actor - 选择并聚焦
- set_actor_transform - 设置变换
- create_actor_basic - 创建Actor
- delete_actor - 删除Actor
- transform_actors_delta - 相对变换

### 批量操作 (7个)

- batch_rename_actors - 批量重命名
- batch_set_visibility - 批量可见性
- batch_set_mobility - 批量移动性
- batch_move_to_level - 移动到关卡
- batch_set_tags - 批量标签
- align_to_ground - 对齐地面
- distribute_actors - 分布排列

### 场景分析 (5个)

- analyze_level_stats - 关卡统计
- find_missing_references - 查找缺失引用
- find_duplicate_names - 查找重复名称
- find_oversized_meshes - 查找超大网格
- validate_level - 验证关卡

### 相机书签 (4个)

- save_camera_bookmark - 保存书签
- jump_to_camera_bookmark - 跳转书签
- list_camera_bookmarks - 列出书签
- delete_camera_bookmark - 删除书签

### 其他工具 (9个)

- 选择工具：select_actors_by_rule
- 查询工具：list_actors, get_actor_properties, list_selection_presets
- 视口工具：focus_viewport
- 系统工具：run_console_command, save_level, pie_control
- 灯光工具：set_light_property

**完整工具文档**: 30个工具，涵盖编辑器主要操作场景

---

## ⚙️ 配置选项

### 基础配置

```ini
[/Script/SmartUEAssistant.SmartUEAssistantSettings]
# API配置
APIKey=your-api-key-here
BaseURL=https://api.openai.com
ModelName=gpt-4
SupportedModels=gpt-3.5-turbo,gpt-4

# 执行策略
bAutoExecuteSafeTools=true              # 安全工具自动执行
bSkipConfirmForConsoleCommands=false    # 控制台命令跳过确认
```

### DeepSeek配置

```ini
BaseURL=https://api.deepseek.com
ModelName=deepseek-chat
SupportedModels=deepseek-chat
```

---

## 📝 版本历史

### v1.0.9 (2026-03-12) - 最新版本

- ✅ 降低对话成本，提升响应时间
- ✅ 区分用户意图将对话划分成操作对话和询问对话
- ✅ 优化功能实现工具支持模糊命名
- ✅ 添加内部文档跳转和快捷工具

### v1.0.8 (2026-02-28) 
- ✅ AI输出文本可选择和复制
- ✅ 流式显示支持文本选择
- ✅ 优化UI控件和用户体验

### v1.0.6 (2025-10-09)

- ✅ AI语义理解系统
- ✅ 通用属性修改工具 (modify)
- ✅ 属性修改辅助类
- ✅ 完整事务支持 (Undo/Redo)

### v1.0.0 (2025-09-10)

- ✅ 初始版本发布
- ✅ OpenAI/DeepSeek支持
- ✅ 30+编辑器工具
- ✅ Slate对话界面

---

## 🚀 打包发布

使用PowerShell脚本一键打包：

```powershell
# 默认版本，输出到 dist/
./Scripts/PackSmartUEAssistantPlugin.ps1

# 指定版本和输出目录
./Scripts/PackSmartUEAssistantPlugin.ps1 -Version 1.0.8 -OutputRoot release
```

---

## 🔧 开发扩展

### 添加自定义工具

1. **定义工具接口** (`Public/Tools/MyTools.h`)

```cpp
class FMyTools {
public:
    static FAIToolResult MyCustomTool(const TSharedPtr<FJsonObject>& Args);
};
```

2. **实现工具逻辑** (`Private/Tools/MyTools.cpp`)

```cpp
FAIToolResult FMyTools::MyCustomTool(const TSharedPtr<FJsonObject>& Args) {
    // 工具实现
    return FAIToolResult{true, TEXT("执行成功")};
}
```

3. **注册到工具清单** (`EditorAIToolRegistry`)

```cpp
Registry.RegisterTool(TEXT("my_custom_tool"), &FMyTools::MyCustomTool);
```

---

## 📚 目录结构

```
xAssistant/
├── Config/
│   └── DefaultSmartUEAssistant.ini    # 插件配置
├── Content/                            # 内容资产
├── Plugins/
│   └── SmartUEAssistant/              # 插件主体
│       ├── Source/                     # 源代码
│       │   └── SmartUEAssistant/
│       │       ├── Private/           # 实现文件
│       │       │   ├── Tools/         # 工具实现
│       │       │   ├── AIService.cpp
│       │       │   └── ...
│       │       └── Public/            # 头文件
│       │           ├── Tools/         # 工具接口
│       │           ├── AIService.h
│       │           └── ...
│       ├── Resources                  # 插件资源
│       ├── Config/                    # 插件配置
│       └── doc/
│           └── architecture.svg       # 架构图
├── Scripts/
│   └── PackSmartUEAssistantPlugin.ps1 # 打包脚本
├── CONTRIBUTING.md                     # 贡献指南
└── README.md                          # 本文档
```

---

## 🐛 故障排查

### API调用失败

- 检查网络连接和代理设置
- 确认API密钥正确且有效
- 查看日志：`LogSmartUEAssistant`

### 工具执行失败

- 检查对话窗口的错误提示
- 查看编辑器输出日志
- 确认操作权限和确认设置

### 属性修改不生效

- 使用 `get_available_properties` 查看可用属性
- 确认属性名大小写正确
- 检查值类型是否匹配

---

## 🗺️ Roadmap

### 短期目标

- [ ] 更多编辑器工具（资产批处理、材质替换）
- [ ] 流式响应优化
- [ ] 对话历史持久化

### 中期目标

- [ ] 多模型路由与降级策略
- [ ] 工具能力权限沙箱
- [ ] 自定义工具模板生成

### 长期目标

- [ ] 上下文记忆与检索（离线方案）
- [ ] 工具市场与插件化加载
- [ ] 团队协作与多会话管理

---

## 📄 许可证

Apache License 2.0 - 详见 [LICENSE](LICENSE) 文件

---

## 🤝 贡献

欢迎提交Issue和Pull Request！

- 📖 [贡献指南](CONTRIBUTING.md)
- 🐛 [问题反馈](https://github.com/jd-cg/xAssistant/issues)
- 💬 讨论与建议

---

## 🔗 相关链接

- **项目仓库**: https://github.com/jd-cg/xAssistant
- **架构图**: [Plugins/SmartUEAssistant/doc/architecture.svg](Plugins/SmartUEAssistant/doc/architecture.svg)
- **打包脚本**: [Scripts/PackSmartUEAssistantPlugin.ps1](Scripts/PackSmartUEAssistantPlugin.ps1)
- **演示视频**: https://pan.baidu.com/s/18iD-KYJevhXpjvWzB0NZJQ?pwd=w66x
---

<p align="center">
  <strong>AI驱动，智能高效 | 让UE编辑器操作更简单</strong><br>
  使用自然语言，专注创作本身
</p>
