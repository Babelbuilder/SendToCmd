# SendToCmd 2.0 实现与验证状态

本文件记录截至 2026-10-04 的代码状态，区分已实现源码与已在目标系统验证的行为。

| 范围 | 状态 |
| --- | --- |
| Windows 旧版 | 原 `SendToCmd.exe`、C# 源码、图标、Manifest、README 和需求文档均保存在 `archive/windows-v1/`。 |
| Qt 2.0 主程序 | 已实现 C++20 / Qt 6 Widgets / CMake 工程，包括纯文本编辑、行号、当前行阴影、文件操作、连续查找、起止点、手动与定时发送、F8、中英文界面、设置保存。 |
| 外部窗口目标 | Windows 使用 Win32，macOS 使用 AppKit / Quartz，Linux X11 使用 Xlib / XTest。均有目标身份和前台焦点检查；Wayland 明确禁止绑定外部窗口。 |
| 输入方式 | 默认剪贴板粘贴，另有 Unicode 键盘输入；可选终端常见粘贴组合或设置自定义组合。剪贴板按 MIME 格式备份，发送后尝试恢复；检测到其他应用已更改剪贴板时不覆盖。失败不自动切换输入方式。 |
| 安全停止 | 自动发送在下一条命令前检查目标是否仍处于前台；失焦、关闭或输入失败时停止。空行仅发送回车；单行限制 4096 个 UTF-16 单元且拒绝控制字符。 |
| Ubuntu 构建验证 | 在本环境用 Qt 6.4.2 实际编译通过；`core_tests` 和 `editor_tests` 均通过；无显示环境启动无报错。尚未在真实 X11 终端上做窗口拖拽和命令输入验收。 |
| Windows 验证 | 已在 Linux 上用 MinGW 和 Qt 6.4.2 交叉编译 Windows x64 GUI EXE；PE 架构、DLL 依赖和 ZIP 完整性已检查。用户已测试 2.0 单文件 Windows 版本并确认基本功能正常，作为 v2.0.0 正式发布。程序仍未签名，受智能应用控制保护的设备可能拦截。 |
| macOS 验证 | 通过 GitHub Actions 在 macOS Intel 和 Apple Silicon 构建机上构建、运行自动化测试并部署 Qt；实际外部终端交互未测试。发布附件明确标注未测试。 |
| 独立分发 | `dist/SendToCmd-2.0-Windows-x64.zip` 为含 Qt 运行库的未签名 Windows 便携测试包。`packaging/windows.ps1` 在 Windows 上生成独立目录和 ZIP，`packaging/sign-windows.ps1` 提供证书存储区或 Azure Artifact Signing 签名、验证、重打包流程；签名脚本尚待 Windows 实机执行。`packaging/` 另有 macOS DMG、Linux AppImage 脚本，v2.0.0 发布 Windows 单文件 EXE、Ubuntu 24.04 DEB 和 macOS 双架构 DMG；仅 Windows 基本功能已由用户验收，Ubuntu/macOS 未做实机功能测试。 |

与技术方案相比，当前中英文文字由轻量的界面字符串切换实现，尚未迁移到 `.ts` / `QTranslator` 工作流。Linux X11 的键盘模式受当前键盘映射限制，无法保证所有 Unicode 字符；默认剪贴板模式用于完整文本。串口直连、SSH 直连、等待输出与日志分别属于方案中的后续版本，2.0 尚未实现。

## 后续工作区改进：多文件标签页

编辑区新增可拖动排序的文件标签页，可多选打开文件并识别已打开文件。各标签独立保留内容、修改状态、撤销记录、光标、滚动位置和发送范围。关闭/退出检查所有文件的未保存修改；切换文件取消待发送输入并停止自动发送。终端绑定及输入设置共用。新增 `window_tests` 验证这些交互；本次改进在 Ubuntu 环境编译和无显示界面的自动化测试通过，尚未在 Windows/macOS 实机验证，也未更新已发布的 2.0 二进制。
