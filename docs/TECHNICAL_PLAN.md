# SendToCmd 2.0 — Qt 跨平台技术方案与字符发送方案

> 基线日期：2026-10-03  
> 目标平台：Windows 10/11、macOS、Ubuntu Linux  
> 技术方向：Qt 6 + C++20 + CMake  
> 本文用于指导现有约 1000 行 C# Windows 版本 SendToCmd 的跨平台重构。

---

# 1. 技术选型结论

SendToCmd 2.0 推荐采用：

```text
Qt 6
+
C++20
+
CMake
+
Qt Widgets
```

原因如下：

1. SendToCmd 当前代码规模仅约 1000 行，整体重构成本可控，没有必要为了复用少量 C# 代码长期绑定 .NET UI 技术栈。
2. 产品目标明确要求 Windows、macOS、Ubuntu 三平台长期支持。
3. SendToCmd 属于典型 Native Desktop 工具，而不是 Web 风格应用。
4. 后续可能涉及：
   - 外部窗口识别；
   - 前台窗口激活；
   - 键盘输入；
   - 剪贴板；
   - macOS Accessibility；
   - Linux X11 / Wayland；
   - SSH；
   - UART / Serial；
   - PTY；
   - 系统权限；
   - 原生打包。
5. Qt 在这些传统桌面场景具有成熟的跨平台能力。

截至 Qt 6.12，Qt 官方支持 Windows、macOS 和 Linux 桌面平台，包括 Windows 10/11、macOS x86_64/ARM64，以及 Ubuntu 22.04/24.04 等 Linux 环境。

Qt 官方同时推荐使用 CMake 作为 Qt 6 项目的主流构建系统。

---

# 2. 为什么选择 Qt Widgets

SendToCmd 当前属于传统桌面工具：

```text
菜单
工具按钮
文本编辑器
行号
状态栏
右键菜单
输入框
CheckBox
Dialog
```

因此推荐：

```text
Qt Widgets
```

而不是：

```text
Qt Quick / QML
```

主要原因：

- 更适合传统工具软件；
- C++ 控件结构直观；
- 文本编辑、菜单、快捷键成熟；
- Native Desktop 风格明显；
- 更容易进行窗口系统相关开发；
- 工程结构简单；
- 长期维护成本较低。

核心文本编辑控件采用：

```cpp
QPlainTextEdit
```

而不是 `QTextEdit`。

原因是 SendToCmd 编辑的是纯文本命令，不需要富文本格式。

---

# 3. 总体架构

推荐将 SendToCmd 从“窗口程序直接控制一切”改为分层架构：

```text
┌────────────────────────────────────┐
│              Qt UI                 │
│         SendToCmd Desktop           │
├────────────────────────────────────┤
│            Application             │
│                                    │
│ DocumentController                 │
│ AutoSendController                 │
│ TargetController                   │
├────────────────────────────────────┤
│              Core                  │
│                                    │
│ Document                           │
│ SendValidator                      │
│ AutoSendEngine                     │
│ SendRange                          │
│ Settings                           │
│ Localization                       │
├────────────────────────────────────┤
│           ICommandTarget           │
├────────────┬────────────┬──────────┤
│ External   │   Serial   │   SSH    │
│ Window     │            │          │
├────────────┴────────────┴──────────┤
│          Platform Layer            │
│ Windows │ macOS │ Linux            │
└────────────────────────────────────┘
```

核心原则：

> UI、自动发送逻辑和目标设备之间解耦。

这样以后无论目标是：

```text
Windows Terminal
iTerm2
GNOME Terminal
SSH
UART
Local Shell
```

自动发送逻辑都不需要修改。

---

# 4. 推荐工程目录

```text
SendToCmd/
│
├── CMakeLists.txt
│
├── src/
│
│   ├── app/
│   │   ├── main.cpp
│   │   ├── MainWindow.cpp
│   │   └── MainWindow.h
│   │
│   ├── ui/
│   │   ├── CommandEditor.cpp
│   │   ├── CommandEditor.h
│   │   ├── LineNumberArea.cpp
│   │   ├── LineNumberArea.h
│   │   ├── FindDialog.cpp
│   │   └── FindDialog.h
│   │
│   ├── core/
│   │   ├── Document.cpp
│   │   ├── Document.h
│   │   ├── SendValidator.cpp
│   │   ├── SendValidator.h
│   │   ├── AutoSendEngine.cpp
│   │   ├── AutoSendEngine.h
│   │   ├── SendRange.h
│   │   └── SendResult.h
│   │
│   ├── target/
│   │   ├── ICommandTarget.h
│   │   │
│   │   ├── external/
│   │   │   ├── ExternalWindowTarget.cpp
│   │   │   ├── ExternalWindowTarget.h
│   │   │   ├── IInputStrategy.h
│   │   │   ├── ClipboardPasteStrategy.cpp
│   │   │   └── KeyboardInputStrategy.cpp
│   │   │
│   │   ├── serial/
│   │   │   ├── SerialTarget.cpp
│   │   │   └── SerialTarget.h
│   │   │
│   │   └── ssh/
│   │       ├── SshTarget.cpp
│   │       └── SshTarget.h
│   │
│   ├── platform/
│   │   ├── windows/
│   │   │   ├── WinWindowService.cpp
│   │   │   └── WinInputService.cpp
│   │   │
│   │   ├── macos/
│   │   │   ├── MacWindowService.mm
│   │   │   └── MacInputService.mm
│   │   │
│   │   └── linux/
│   │       ├── X11WindowService.cpp
│   │       └── WaylandSupport.cpp
│   │
│   ├── settings/
│   │   ├── Settings.cpp
│   │   └── Settings.h
│   │
│   └── i18n/
│       ├── SendToCmd_en.ts
│       └── SendToCmd_zh_CN.ts
│
├── resources/
│   ├── icons/
│   └── SendToCmd.qrc
│
└── tests/
```

---

# 5. Qt 模块选择

第一阶段建议使用：

```cmake
Qt6::Core
Qt6::Gui
Qt6::Widgets
```

以后加入串口：

```cmake
Qt6::SerialPort
```

Qt 提供官方 `QSerialPort` API，因此 COM、Linux tty 和 macOS serial device 可以统一封装。

配置：

```text
Windows:
COM3

Linux:
/dev/ttyUSB0
/dev/ttyACM0

macOS:
/dev/cu.usbserial-xxxx
```

SSH 不建议自行实现协议，可以在 Qt Core 外集成：

```text
libssh2
```

或者：

```text
libssh
```

---

# 6. UI 对应关系

现有 UI 可以对应到 Qt：

| SendToCmd 功能 | Qt |
|---|---|
| 主窗口 | `QMainWindow` |
| 文本编辑 | `QPlainTextEdit` |
| 文件菜单 | `QMenu / QAction` |
| 状态显示 | `QStatusBar / QLabel` |
| 自动发送 | `QCheckBox` |
| 发送间隔 | `QDoubleSpinBox` |
| 发送按钮 | `QToolButton` |
| 右键菜单 | `QMenu` |
| 文件选择 | `QFileDialog` |
| 查找窗口 | `QDialog / QLineEdit` |
| 定时器 | `QTimer` |
| 配置 | `QSettings` |
| 剪贴板 | `QClipboard` |
| 多语言 | `QTranslator` |
| 图标 | `QIcon / .qrc` |

---

# 7. 文本编辑区域

采用：

```cpp
class CommandEditor : public QPlainTextEdit
```

扩展：

```text
行号
当前行背景
起点绿点
终点红点
右键菜单
禁止换行
```

左侧增加：

```cpp
LineNumberArea
```

例如：

```text
   ● 10  ls -l
     11  cd /data
     12
   ● 13  reboot
```

其中：

```text
绿色 ● = Start
红色 ● = End
```

当前行使用 `ExtraSelection` 设置背景色。

---

# 8. Core 层必须与操作系统无关

例如：

```cpp
class SendValidator
{
public:
    static ValidationResult validateLine(
        const QString& line);
};
```

负责：

```text
最大 4096 UTF-16 code units
控制字符检测
允许空行
禁止 TAB 等指定控制字符
```

这里不应该出现：

```text
HWND
SendInput
CGEvent
X11
QClipboard
```

---

# 9. ICommandTarget

所有目标统一实现：

```cpp
class ICommandTarget
{
public:
    virtual ~ICommandTarget() = default;

    virtual bool isAvailable() const = 0;

    virtual SendResult sendLine(
        const QString& line) = 0;

    virtual void cancel() = 0;
};
```

以后可以实现：

```text
ExternalWindowTarget
SerialTarget
SshTarget
LocalShellTarget
```

---

# 10. 自动发送架构

AutoSendEngine 只负责：

```text
Start
 ↓
读取 startLine
 ↓
发送
 ↓
等待 interval
 ↓
下一行
 ↓
End
```

伪代码：

```cpp
void AutoSendEngine::sendCurrentLine()
{
    const QString line =
        document_->line(currentLine_);

    SendResult result =
        target_->sendLine(line);

    if (!result.success) {
        stop(result.error);
        return;
    }

    if (currentLine_ >= endLine_) {
        finish();
        return;
    }

    ++currentLine_;

    timer_->start(intervalMs_);
}
```

这样 AutoSendEngine 完全不知道下面是：

```text
Clipboard
Keyboard
SSH
UART
```

---

# 11. 外部终端总体发送模型

ExternalWindowTarget 再抽象成：

```text
ExternalWindowTarget
        │
        ├── WindowService
        │
        │    ├── 查找目标
        │    ├── 判断目标存在
        │    ├── 判断目标进程
        │    ├── 激活窗口
        │    └── 判断前台
        │
        └── IInputStrategy
             │
             ├── ClipboardPaste
             └── KeyboardInput
```

---

# 12. 字符发送方案总体设计

SendToCmd 不应只支持一种输入方法。

推荐至少设计：

```text
External Window
│
├── Clipboard Paste       默认
│
└── Keyboard Injection    fallback
```

长期增加：

```text
Terminal Native API
SSH Direct
Serial Direct
```

推荐优先级：

```text
Direct connection
      ↓
Terminal-specific API
      ↓
Clipboard Paste
      ↓
Keyboard Injection
```

但对于当前“外部终端”兼容模式：

```text
Clipboard Paste
      ↓
Keyboard Injection
```

即可。

---

# 13. 方案 A：Clipboard Paste

这是推荐默认字符发送方式。

例如：

```text
ls -al /data/log
```

不是模拟：

```text
l
s
space
-
a
l
...
```

而是：

```text
临时 Clipboard
        ↓
Paste
        ↓
Enter
```

完整过程：

```text
保存原剪贴板
      ↓
设置 command
      ↓
等待 clipboard ready
      ↓
激活目标窗口
      ↓
再次确认 foreground
      ↓
发送 Paste Shortcut
      ↓
目标读取 clipboard
      ↓
发送 Enter
      ↓
恢复原剪贴板
```

---

# 14. 为什么 Clipboard 应成为默认方案

主要优势：

### 14.1 与字符串长度基本无关

100 个字符不需要制造 100 个键盘事件。

### 14.2 Unicode 处理简单

例如：

```text
echo 中文测试
```

无需解决：

```text
中文 IME
日语 IME
键盘布局
物理扫描码
```

### 14.3 特殊符号可靠

例如：

```text
/
\
:
_
-
=
&
|
<
>
"
'
[
]
{
}
```

不用逐个映射 Keyboard Layout。

### 14.4 性能高

整个字符串一次 Paste。

---

# 15. Qt Clipboard 实现

Qt 提供：

```cpp
QGuiApplication::clipboard()
```

获得：

```cpp
QClipboard*
```

因此三平台 Clipboard API 可以基本统一。

Qt 的剪贴板接口属于 `QClipboard`。因此 Windows/macOS/Linux 普通剪贴板操作不需要自己直接调用各平台系统 API。

基本示意：

```cpp
QClipboard* clipboard =
    QGuiApplication::clipboard();

clipboard->setText(command);
```

---

# 16. 不能简单覆盖用户剪贴板

SendToCmd 当前要求：

> 不修改剪贴板。

新版本可以重新定义为：

> 发送结束后不永久改变用户剪贴板内容。

因此需要：

```text
备份 Clipboard
↓
临时写 SendToCmd command
↓
Paste
↓
恢复 Clipboard
```

不能只保存 `text()`。

因为剪贴板可能包含：

```text
plain text
HTML
image
file URL
application-specific MIME
```

因此应该以：

```cpp
QMimeData
```

为基础保存 MIME 内容。

概念：

```text
Clipboard
 ├── text/plain
 ├── text/html
 ├── image/png
 └── ...
```

发送完成后尽可能完整恢复。

---

# 17. Clipboard 时序

必须避免：

```text
set command
↓
Paste
↓
马上 restore clipboard
```

因为目标程序可能还没有读取 Clipboard。

推荐：

```text
T0    写入 command
T0+20ms
      激活窗口

T0+40ms
      发送 Paste

T0+80~150ms
      发送 Enter

T0+150~300ms
      Restore Clipboard
```

具体 delay 应通过测试确定。

不要把值写死在业务逻辑中，应做成：

```cpp
struct InputTiming
{
    int focusDelayMs;
    int pasteDelayMs;
    int enterDelayMs;
    int restoreClipboardDelayMs;
};
```

---

# 18. Paste 快捷键不能写死

不同 Terminal 默认快捷键不同。

典型：

```text
Windows Terminal
Ctrl + V

Terminal.app
Cmd + V

iTerm2
Cmd + V

GNOME Terminal
Ctrl + Shift + V

Konsole
Ctrl + Shift + V

很多传统终端
Shift + Insert
```

因此定义：

```cpp
struct TerminalProfile
{
    QString name;

    KeySequence pasteShortcut;

    int focusDelayMs;
    int pasteDelayMs;
};
```

用户也允许自定义。

UI：

```text
Input Method:
[ Clipboard Paste ▼ ]

Paste Shortcut:
[ Ctrl+Shift+V ]

Focus Delay:
[ 100 ms ]
```

---

# 19. 方案 B：Keyboard Injection

作为 Clipboard Paste 的兼容 fallback。

不要首先采用：

```text
physical keycode
```

而应尽可能采用：

```text
Unicode character injection
```

因为 Physical KeyCode 会受到：

```text
Keyboard Layout
Shift
AltGr
IME
CapsLock
语言
```

影响。

例如：

```text
:
```

在不同键盘布局中对应的实际键组合并不完全一致。

---

# 20. Windows Keyboard 模式

Windows 可使用：

```text
SendInput()
```

建议发送：

```text
KEYEVENTF_UNICODE
```

而不是将 Unicode 转成普通 virtual-key。

概念：

```text
QString
 ↓
UTF-16
 ↓
每个 UTF-16 code unit
 ↓
SendInput(KEYEVENTF_UNICODE)
```

最后：

```text
VK_RETURN
```

发送 Enter。

优点：

```text
不依赖 US 键盘
减少 Keyboard Layout 问题
```

这与现有 SendToCmd 行为最接近。

---

# 21. macOS Keyboard 模式

macOS 可以基于：

```text
CGEvent
```

产生键盘事件。

对于 Unicode 文本应优先使用系统提供的 Unicode text event 能力，而不是自己建立字符到物理 keyboard scan code 的映射。

发送前：

```text
获取 Target
↓
激活应用
↓
确认前台
↓
发送文本
↓
发送 Enter
```

macOS 会涉及 Accessibility 权限。

例如用户需要允许：

```text
System Settings
→ Privacy & Security
→ Accessibility
→ SendToCmd
```

SendToCmd 启动时应能够检测权限并给出明确提示。

---

# 22. Linux X11 Keyboard 模式

X11 环境可以使用：

```text
XTest
```

发送模拟键盘事件。

但是不推荐把 XTest 作为默认字符串输入方式。

推荐：

```text
Clipboard Paste
```

优先。

XTest 主要作为：

```text
Paste 快捷键
Enter
fallback keyboard
```

使用。

---

# 23. Linux Wayland

Wayland 必须单独处理。

不能把：

```text
X11
```

和：

```text
Wayland
```

视为同一种 Linux 桌面环境。

Wayland 的安全模型不允许普通应用像 X11/Windows 一样任意：

```text
枚举其他窗口
激活其他窗口
向其他应用注入输入
```

因此：

```text
Ubuntu X11
External Window：完整支持

Ubuntu Wayland
External Window：受限
```

这是产品能力限制，不是 Qt UI 的限制。

SendToCmd README 中应该明确区分。

---

# 24. 三平台 External Window 能力矩阵

| 功能 | Windows | macOS | Ubuntu X11 | Ubuntu Wayland |
|---|---:|---:|---:|---:|
| 查找外部窗口 | 强 | 强 | 强 | 受限 |
| 激活窗口 | 强 | 较强 | 强 | 受限 |
| 验证前台 | 强 | 强 | 强 | 受限 |
| Clipboard | 强 | 强 | 强 | 强 |
| Paste Shortcut | 强 | 强 | 强 | 受系统限制 |
| Keyboard Injection | 强 | 强 | 强 | 受限 |
| External Target | 完整 | 基本完整 | 完整 | Limited |

因此：

> Linux 支持不能简单写成“完全支持”。

应该写：

```text
Ubuntu X11：External Window Full Support

Ubuntu Wayland：
External Window Limited Support
SSH / Serial Full Support
```

---

# 25. 字符发送安全流程

所有 External Window 输入必须使用：

```text
检查 Target 存在
      ↓
检查 PID / identity
      ↓
尝试激活
      ↓
确认 foreground
      ↓
等待 focusDelay
      ↓
输入文本
      ↓
输入 Enter
      ↓
再次检查 target
```

绝对不能：

```text
尝试激活
↓
不管是否成功
↓
直接输入
```

否则 AUTO SEND 可能把：

```text
reboot
rm ...
flash ...
```

发送到其他窗口。

---

# 26. 推荐发送状态机

```text
IDLE
 │
 ▼
VALIDATE
 │
 ▼
CHECK_TARGET
 │
 ▼
ACTIVATE_TARGET
 │
 ▼
VERIFY_FOREGROUND
 │
 ▼
WAIT_FOCUS
 │
 ▼
SEND_TEXT
 │
 ▼
SEND_ENTER
 │
 ▼
VERIFY
 │
 ├───────────────┐
 │               │
 ▼               ▼
SUCCESS         ERROR
```

自动发送：

```text
SUCCESS
 │
 ▼
WAIT_INTERVAL
 │
 ▼
NEXT_LINE
```

---

# 27. 空行

空行必须继续保留现在的语义：

```text
line == ""
```

行为：

```text
不发送文本
↓
只发送 Enter
```

即：

```text
Clipboard 模式：

Skip Clipboard
↓
Enter
```

不要把空行当成无操作。

---

# 28. SEND-03 保持

单行继续限制：

```text
<= 4096 UTF-16 code units
```

禁止指定控制字符。

但需要重新讨论：

```text
TAB
```

是否应该长期禁止。

如果以后 Serial/SSH 模式用于 shell scripting，TAB 本身可能没有必要支持。

第一版可以继续保持当前规则，确保行为兼容。

---

# 29. Enter 应独立于 Text

不要：

```text
Clipboard = command + "\n"
```

然后 Paste。

原因是终端的 bracketed paste 模式可能对换行产生特殊行为。

建议明确拆开：

```text
SendText(command)
↓
SendEnter()
```

这也与现有需求一致。

---

# 30. Bracketed Paste

现代 shell/terminal 经常支持：

```text
Bracketed Paste Mode
```

粘贴多行内容时，Terminal/Shell 可能对粘贴内容进行特殊处理。

由于 SendToCmd 的产品定义是：

> 一行命令 + 一个 Enter

因此应该始终：

```text
Paste 一行
↓
单独模拟 Enter
```

不要：

```text
Paste "command\n"
```

这样行为更可控。

---

# 31. ExternalWindowTarget 接口

建议：

```cpp
class ExternalWindowTarget :
    public ICommandTarget
{
public:
    bool bindWindow();

    bool isAvailable() const override;

    SendResult sendLine(
        const QString& line) override;

private:
    std::unique_ptr<IWindowService>
        windowService_;

    std::unique_ptr<IInputStrategy>
        inputStrategy_;
};
```

---

# 32. IInputStrategy

```cpp
class IInputStrategy
{
public:
    virtual ~IInputStrategy() = default;

    virtual SendResult sendText(
        const QString& text) = 0;

    virtual SendResult sendEnter() = 0;
};
```

实现：

```text
ClipboardPasteStrategy
KeyboardInputStrategy
```

以后增加：

```text
TerminalApiStrategy
```

---

# 33. ClipboardPasteStrategy

概念：

```cpp
SendResult ClipboardPasteStrategy::sendText(
    const QString& text)
{
    backupClipboard();

    setClipboard(text);

    waitClipboardReady();

    sendPasteShortcut();

    waitPasteConsumed();

    restoreClipboard();

    return SendResult::success();
}
```

Enter 独立：

```cpp
SendResult
ClipboardPasteStrategy::sendEnter()
{
    return nativeInput_->sendEnter();
}
```

---

# 34. KeyboardInputStrategy

```cpp
SendResult KeyboardInputStrategy::sendText(
    const QString& text)
{
    for (const auto ch : text) {
        if (!nativeInput_->sendUnicode(ch))
            return error;
    }

    return success;
}
```

但实际实现需要考虑 UTF-16 surrogate pair。

因此建议底层 API：

```cpp
sendUnicodeString(QString)
```

而不是：

```cpp
sendCharacter(QChar)
```

让平台实现自己正确处理 Unicode。

---

# 35. 平台抽象

```cpp
class INativeInputService
{
public:
    virtual bool sendUnicodeText(
        const QString& text) = 0;

    virtual bool sendKeySequence(
        const KeySequence& key) = 0;

    virtual bool sendEnter() = 0;
};
```

平台：

```text
WinInputService
MacInputService
X11InputService
```

---

# 36. External Window 默认模式

推荐默认：

```text
Send Method:
Clipboard Paste
```

第二选项：

```text
Keyboard Input
```

高级选项：

```text
Paste Shortcut:
Auto

Auto Detection:
Windows Terminal → Ctrl+V
macOS Terminal → Cmd+V
GNOME Terminal → Ctrl+Shift+V
Unknown → platform default
```

用户可以覆盖：

```text
Ctrl+V
Ctrl+Shift+V
Shift+Insert
Cmd+V
Custom
```

---

# 37. 自动 fallback

第一阶段不要自动 fallback。

例如：

```text
Clipboard Paste 失败
```

不能马上偷偷换 Keyboard 输入，因为：

```text
无法100%确定 Paste 是否实际上已经成功
```

否则可能发生：

```text
command
command
```

重复输入。

因此应该：

```text
一种 Send Strategy
对应一次明确发送
```

失败：

```text
STOP
↓
提示用户
```

用户主动换模式。

---

# 38. Serial 模式

长期增加：

```text
Target Type:
Serial
```

结构：

```text
COM / tty
baud rate
data bits
stop bits
parity
flow control
```

发送：

```cpp
serial.write(
    command.toUtf8());

serial.write(
    "\r\n");
```

注意：

Enter 类型应该允许配置：

```text
CR
LF
CRLF
```

因为串口设备不同。

例如：

```text
Linux shell:
LF

某些 Bootloader:
CR

部分 UART console:
CRLF
```

---

# 39. SSH 模式

未来：

```text
Target Type:
SSH
```

配置：

```text
Host
Port
Username
Password / Key
Shell Channel
```

发送：

```text
command
+
newline
```

不再依赖：

```text
Window Focus
Clipboard
Keyboard
```

可靠性显著高于 External Window。

---

# 40. 长期目标架构

最终建议：

```text
                 SendToCmd

                     │
              AutoSendEngine
                     │
              ICommandTarget
                     │
       ┌─────────────┼─────────────┐
       │             │             │
 External Window   Serial         SSH
       │
       ▼
  IInputStrategy
       │
 ┌─────┴──────────┐
 │                │
Clipboard       Keyboard
 Paste          Unicode
```

未来：

```text
External Window
      │
      ├── Clipboard
      ├── Keyboard
      └── Terminal API
```

---

# 41. 推荐版本规划

## V2.0

完成 Qt 重构：

```text
Windows
macOS
Ubuntu X11
```

保持现有功能：

```text
文件
编辑
行号
起止点
自动发送
双语
快捷键
目标窗口
```

External Window：

```text
Clipboard Paste
+
Keyboard Injection
```

---

## V2.1

增加：

```text
Serial Target
```

这对嵌入式、SoC、Bootloader、UART Console 使用场景价值非常高。

---

## V2.2

增加：

```text
SSH Target
```

External Window 不再是唯一入口。

---

## V2.3

增加：

```text
等待字符串
Timeout
日志
Prompt 检测
```

例如：

```text
SEND reboot

WAIT "login:" 60

SEND root

WAIT "#"

SEND dmesg
```

这时 SendToCmd 会从：

```text
Command Notepad
```

逐渐发展为：

```text
Device Interaction Automation Tool
```

---

# 42. 第一阶段最终技术栈

推荐明确固定：

```text
Language:
C++20

Framework:
Qt 6

UI:
Qt Widgets

Build:
CMake

Editor:
QPlainTextEdit

Settings:
QSettings

Localization:
QTranslator

Clipboard:
QClipboard

Timer:
QTimer

Serial:
QSerialPort

SSH:
libssh2 / libssh
```

---

# 43. 第一阶段字符发送决策

默认：

```text
Clipboard Paste
```

流程：

```text
Backup Clipboard
↓
Set command
↓
Activate target
↓
Verify foreground
↓
Paste
↓
Enter
↓
Restore Clipboard
```

Fallback：

```text
Unicode Keyboard Injection
```

流程：

```text
Activate target
↓
Verify foreground
↓
Unicode text injection
↓
Enter
```

两者均必须：

```text
Fail Safe
```

只要出现：

```text
目标不存在
目标 PID 改变
无法前台
发送 API 失败
```

立即：

```text
停止发送
```

自动发送模式同样立即终止。

---

# 44. 最终设计原则

SendToCmd 2.0 不应该定义成：

> 一个向其他窗口模拟键盘的软件。

而应该定义成：

> 一个按照用户定义的行序列，向目标 Command Target 安全发送命令的桌面工具。

External Window 只是其中一种：

```text
Target
```

而不是整个软件的架构基础。

因此核心设计应该围绕：

```text
Document
+
Send Engine
+
Command Target
```

展开。

这样才能保证未来从：

```text
Windows 模拟输入
```

自然扩展到：

```text
Windows
macOS
Linux

External Terminal
Serial
SSH
Local Shell
```

而无需再次重构整个程序。