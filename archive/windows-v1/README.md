# SendToCmd

## English

SendToCmd is a standalone Windows command notepad for 64-bit Windows 10/11. Run `SendToCmd.exe` directly; it uses the built-in .NET Framework 4.x and needs no Python. The interface defaults to English. Switch at **Settings → Language → 中文 / English**. Your choice is restored on the next launch from `%APPDATA%\SendToCmd\language.txt`.

### Use

1. Connect to your board through SSH or a serial port in a terminal, and activate the intended input area.
2. Type one command per line in SendToCmd, or open a text file such as `examples.txt`.
3. Hold the left mouse button on the top-right crosshair, drag it over the terminal window, then release to bind. Long target titles stay on one line; hover to see the full title.
4. Click a command line or its line number. Leave **Auto send** unchecked, then click the paper-plane button or press F8 to send the highlighted line followed by Enter. Hover over the button to see the action and F8 shortcut. After a successful send, the highlight advances. A blank line sends Enter.

To send a range, right-click the first line number and choose **Set Start (green)**; right-click the last and choose **Set End (red)**. Both endpoints are included, and they may be the same line. Set **Interval** to 0.1–60 seconds (default 1 second), check **Auto send**, then click the paper-plane button or press F8. During auto send, the button shows a stop icon; click it or press F8 to interrupt. The line-number menu can clear the markers. Editing or opening another file clears them; the editor is temporarily read-only during auto send.

F8 works while the SendToCmd window has focus.

The File and Edit menus provide New, Open, Save, Save As, Find, Undo, Cut, Copy, Paste, and Select All. Files are UTF-8; unsaved changes prompt before closing or replacing the document. Switching language preserves the text and bound target.

### Sending and limits

The app uses the Windows keyboard input API without changing the clipboard. It checks that the bound window still exists and has focus before sending; auto send stops if the target closes or loses focus. A successful status means Windows accepted the input, not that the board executed the command. Select the correct tab or pane in a multi-session terminal. If the terminal runs as administrator, SendToCmd needs matching privileges.

Try a simple command such as `pwd` first. Lines over 4096 characters or containing control characters are not sent. Terminals may differ in their support for simulated Unicode input.

### Files and build

- `SendToCmd.exe`: standalone 64-bit Windows application; the only runtime file required.
- `SendToCmd.cs`: complete C# source.
- `SendToCmd.ico`: embedded multi-size icon.
- `SendToCmd.manifest`: DPI awareness declaration.
- `tools/make_icon.py`: optional icon generator; run `python3 tools/make_icon.py` after editing the design.
- `examples.txt`: editable command examples.

Build from the source directory in Windows PowerShell:

```powershell
& "$env:WINDIR\Microsoft.NET\Framework64\v4.0.30319\csc.exe" /nologo /target:winexe /platform:x64 /r:System.Windows.Forms.dll /r:System.Drawing.dll /win32manifest:SendToCmd.manifest /win32icon:SendToCmd.ico /out:SendToCmd.exe SendToCmd.cs
```

`legacy-python/` and `legacy-windows/` contain older prototypes and are not used by the current EXE.

## 中文

SendToCmd 是适用于 64 位 Windows 10/11 的独立命令记事本。直接运行 `SendToCmd.exe`，使用系统自带的 .NET Framework 4.x，无需安装 Python。界面默认英文，可通过**设置 → 语言 → 中文 / English**切换。选择保存在 `%APPDATA%\SendToCmd\language.txt`，下次启动会自动恢复。

### 使用方法

1. 在终端中通过 SSH 或串口连接单板，并选中正确的输入区域。
2. 在 SendToCmd 中每行输入一条命令，也可以打开 `examples.txt` 等文本文件。
3. 用鼠标左键按住右上角十字准星，拖到终端窗口后松开，即可绑定。过长的目标名称保持单行显示；悬停可查看完整名称。
4. 点击命令行或左侧行号。保持**自动发送**未勾选，点击纸飞机按钮或按 F8，发送阴影标出的行并自动回车。鼠标悬停按钮可查看当前操作和 F8 快捷键。成功后阴影前进一行；空行会发送一次回车。

自动发送范围：在起始行号上右键选择**设为起点（绿点）**，在结束行号上右键选择**设为终点（红点）**。范围包含两端，也可只选一行。底部**间隔**可设为 0.1～60 秒，默认 1 秒。勾选**自动发送**后，点击纸飞机按钮或按 F8 开始逐行发送。发送期间按钮显示停止图形，点击它或按 F8 可中止。行号右键菜单可清除标记；修改文本或打开其他文件也会清除标记。自动发送期间文本暂时只读。

F8 快捷键在 SendToCmd 窗口获得焦点时生效。

文件和编辑菜单提供新建、打开、保存、另存为、查找、撤销、剪切、复制、粘贴及全选。文件采用 UTF-8 编码；关闭或更换文件前会提示未保存的修改。切换语言不会改变文本或已绑定的目标。

### 发送与限制

程序使用 Windows 键盘输入接口，不改动剪贴板。发送前会检查目标窗口是否存在且获得焦点；目标关闭或失焦后，自动发送会停止。成功状态仅表示 Windows 接受了输入，无法判断单板是否执行。多会话终端请先选中正确标签页或分屏；若终端以管理员身份运行，本程序也需要相应权限。

首次可用 `pwd` 等简单命令验证。超过 4096 字符或含控制字符的单行不会发送；不同终端对模拟 Unicode 输入的支持可能不同。

### 文件与编译

- `SendToCmd.exe`：可独立运行的 64 位 Windows 程序，运行时只需要此文件。
- `SendToCmd.cs`：完整 C# 源代码。
- `SendToCmd.ico`：嵌入 EXE 的多尺寸图标。
- `SendToCmd.manifest`：DPI 感知声明，使界面清晰。
- `tools/make_icon.py`：可选图标生成脚本；修改图标后运行 `python3 tools/make_icon.py`。
- `examples.txt`：可修改的命令示例。

在 Windows PowerShell 中进入源文件目录，执行上面的编译命令即可生成 EXE。`legacy-python/` 和 `legacy-windows/` 保存旧版原型，当前 EXE 不使用它们。
