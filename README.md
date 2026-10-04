# SendToCmd 2.0

## 中文

SendToCmd 2.0 是基于 Qt 6 Widgets、C++20 和 CMake 的跨平台命令记事本。当前版本实现了 Windows、macOS 和 Ubuntu X11 的外部终端窗口发送代码。Ubuntu Wayland 下可以打开和编辑文件，但桌面安全限制使外部窗口绑定及输入注入不可用。原 Windows C# 版本完整归档在 [`archive/windows-v1/`](archive/windows-v1/)。2.0 技术方案保存在 [`docs/TECHNICAL_PLAN.md`](docs/TECHNICAL_PLAN.md)，各平台验证情况见 [`docs/IMPLEMENTATION_STATUS.md`](docs/IMPLEMENTATION_STATUS.md)。

### 构建

安装 Qt 6.4+ 的 Core、Gui、Widgets 开发包和 CMake 3.21+。Linux X11 构建还需要 X11 和 XTest 开发包。然后执行：

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Windows 请用安装了 Qt 6 的 MSVC/MinGW 工具链构建；macOS 请用 Xcode Command Line Tools 和 Qt 6 构建。已提供交叉编译的 [Windows x64 测试包](dist/SendToCmd-2.0-Windows-x64.zip)，需完整解压后运行其中的 `SendToCmd.exe`。如果出现“找不到 `libstdc++-6.dll`”，说明只复制了 EXE；请把 ZIP 中的 DLL 和 `plugins` 文件夹一起解压到 EXE 所在目录。`build-windows/` 当前也已补齐这些运行文件。最新版本的 Windows 终端发送仍待实机复测。macOS 暂无已编译安装包。

已生成 [Windows x64 单文件 EXE](dist/SendToCmd-2.0-Windows-x64-static.exe)，约 14.3 MB，可单独复制到 Windows 10/11 x64 电脑试运行，无需附带 Qt 或 MinGW DLL。新版静态 Qt 以 `FEATURE_optimize_size=ON`（`-Os`）和 `FEATURE_ltcg=OFF` 构建，所有原有静态插件均保留；相比原版 20.8 MB 缩小约 31%。要自行重建，请使用 **MinGW x64 构建的静态 Qt 6**（Core、Gui、Widgets 和 Windows 平台插件）以及相同的 MinGW 工具链。普通 Qt SDK 里的 `.a` 可能只是 DLL 的导入库，不能用于真正的静态构建。在 Windows PowerShell 中运行 `./packaging/windows-static.ps1 -QtStaticPrefix 'C:\path\to\static-qt'`；脚本会拒绝动态 Qt 并检查 EXE 的 DLL 导入。系统自带的 Windows DLL 仍正常使用。静态链接不等于代码签名，也不解除智能应用控制的拦截。按 Qt 开源许可证分发静态版时，还需满足 Qt 的许可证、源码和重新链接要求；详见 [Qt 官方说明](https://www.qt.io/development/open-source-lgpl-obligations)。

Windows 测试包尚未进行代码签名。如果 Windows 11“智能应用控制”提示无法验证发布者并阻止运行，完整性校验或取消文件“解除锁定”不能把它变成受信任的已签名程序。微软目前不提供单个应用的放行例外；在启用该保护的设备上正式分发，需要使用受信任的代码签名证书签署并验证发布包。不要为了运行这个测试包关闭主机的安全保护。参见 [微软智能应用控制常见问题](https://support.microsoft.com/en-us/windows/security/threat-malware-protection/smart-app-control-frequently-asked-questions)。窗口、任务栏、Windows EXE、Linux 和 macOS 图标现由 `packaging/make_icons.py` 从同一套绿色纸飞机图形生成。

独立分发可在目标系统运行 `packaging/windows.ps1`（需要 `windeployqt`）、`packaging/macos.sh`（需要 `macdeployqt`）或 `packaging/linux.sh`（需要 `linuxdeployqt`）。分别生成包含 Qt 运行库的 Windows ZIP、macOS DMG 或 Linux AppImage；发布前仍需在目标系统测试窗口绑定和实际终端输入。Windows ZIP 是**解压即运行的文件夹**：Qt、平台插件和编译器运行库都放在包内，不需要目标电脑安装 Qt，但不能只复制 EXE。

### Windows 便携包与签名

在 Windows 上安装 CMake、Qt 6、同一套编译器，并让 `cmake` 与 `windeployqt.exe` 位于 PATH。推荐选用 Qt 对应的 **MinGW x64** 工具链，以便把编译器 DLL 一并带上；MSVC 构建在干净机器上可能仍需安装 Visual C++ Redistributable。运行 `./packaging/windows.ps1`，输出 `dist/SendToCmd-2.0-Windows-x64-unsigned.zip`。已有 Linux 交叉编译的完整 ZIP 也可以直接解压用于下述签名步骤，无需在 Windows 重新编译。签名前先确定这是自己的构建或核对其 SHA256。

若要让 Windows 11 智能应用控制认可发布者，需要能链到 Windows 受信任根的**代码签名**身份；自签名证书不够。可申请 [Azure Artifact Signing 的 Public Trust](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/code-signing-options)，但它对申请者地区有限制；若不符合条件，可从受信任 CA 获取 OV 代码签名证书。使用 Windows SDK 的 `signtool.exe`，在完成依赖复制后签名并验证包内 EXE/DLL。不要把私钥或 PFX 密码放进仓库。

已将受信任 CA 证书配置到 Windows 证书存储区时：

```powershell
./packaging/sign-windows.ps1 -PackageDirectory ./dist/SendToCmd-2.0-Windows-x64 -CertificateThumbprint '证书的40位SHA1指纹'
```

使用 Azure Artifact Signing 时，先按[微软集成说明](https://learn.microsoft.com/en-us/azure/artifact-signing/how-to-signing-integrations)安装 SignTool 和 Dlib，并完成身份验证，然后运行：

```powershell
./packaging/sign-windows.ps1 -PackageDirectory ./dist/SendToCmd-2.0-Windows-x64 -ArtifactSigningDlib 'C:\path\Azure.CodeSigning.Dlib.dll' -ArtifactSigningMetadata 'C:\path\metadata.json'
```

请在装有签名工具和你的签名身份的 Windows **构建机**上运行脚本，再把签名后的 ZIP 复制到目标电脑。脚本跳过已有有效签名的文件，签署其余 EXE/DLL，逐一执行 `signtool verify /pa /all`，最后生成 `SendToCmd-2.0-Windows-x64-signed.zip` 和 SHA256 文件。正式发布前仍需在开启智能应用控制的 Windows 11 上完整解压并实际运行验证；签名不代替功能测试。

单文件版请使用 `packaging/sign-static-windows.ps1`。先取得受信任的代码签名身份，并在 Windows 上安装 SignTool（已有 Windows SDK Signing Tools 时可直接使用；否则运行 `./packaging/setup-signing.ps1`）。证书及私钥在当前用户证书存储区时：

```powershell
./packaging/sign-static-windows.ps1 -CertificateThumbprint '证书的40位SHA1指纹'
```

使用 Azure Artifact Signing 时：

```powershell
./packaging/sign-static-windows.ps1 -ArtifactSigningDlib 'C:\path\Azure.CodeSigning.Dlib.dll' -ArtifactSigningMetadata 'C:\path\metadata.json'
```

脚本保留未签名原件，输出 `dist/SendToCmd-2.0-Windows-x64-static-signed.exe`，验证 Authenticode 签名，并为签名后的文件生成新的 SHA256。个人开发者可查看 [微软签名选项](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/code-signing-options)；自签名证书仅适用于受控测试环境，不满足公开分发时的受信任发布者要求。

### 使用

1. 在外部终端里建立 SSH 或串口连接，选中正确的输入区域。
2. 编辑或打开 UTF-8 命令文件，每行一条命令。
3. 按住右上角十字准星，拖到终端窗口后松开绑定。
4. 点击一行，保持“自动发送”未勾选，点击纸飞机或按 F8 发送该行并回车。空行只发回车，成功后移到下一行。
5. 自动发送时，在左侧行号上右键设置绿色起点和红色终点，设定 0.1～60 秒间隔，勾选“自动发送”，再点击纸飞机或按 F8。运行中按钮变成停止图形，点击或按 F8 可中止。

设置菜单可切换英语／中文、剪贴板粘贴／键盘输入、粘贴快捷键和焦点等待时间。“设置 → 终端粘贴快捷键”明确标出了“Shift+Insert（MobaXterm 推荐）”，也提供“自定义...”输入其他组合。默认使用剪贴板粘贴：程序临时保存剪贴板的 MIME 数据，粘贴一行文本，单独发送回车，然后尽可能恢复原剪贴板。如果其他程序在这段时间修改了剪贴板，SendToCmd 不会覆盖那次修改。不会在粘贴失败后自动改用键盘方式，以免重复发送。空行完全不触碰剪贴板。

Windows 通常默认使用 Ctrl+V；绑定 MobaXterm 时，若未手动设定粘贴快捷键，会自动改用 Shift+Insert。macOS 默认 Cmd+V，Linux X11 默认 Ctrl+Shift+V。可在设置菜单选择 Ctrl+V、Ctrl+Shift+V、Shift+Insert、Cmd+V，或输入自定义组合。自定义组合支持字母、数字和常用功能键；目标系统不支持的组合会报错停止。不同终端或 MobaXterm 自定义键位可能不同，请选择与终端一致的一项。Windows 剪贴板模式直接写入系统 Unicode 文本。如果终端只出现 `^M`，说明回车已送达、粘贴快捷键却未触发；请先手动测试 Shift+Insert，再到“设置 → 粘贴快捷键”选用终端实际支持的组合。键盘输入模式在 Windows 使用 Unicode `SendInput`，macOS 使用 Unicode `CGEvent`；X11 仅支持当前键盘映射中可输入的字符，遇到不支持的字符会拒绝整行文本输入，建议使用剪贴板模式。

发送前检查目标窗口及进程身份，并确认其获得前台焦点；失焦或输入失败时自动发送立即停止。软件无法验证命令是否被设备执行，也不能撤销已送到终端的部分输入。F8 只在 SendToCmd 窗口有焦点时生效。macOS 使用外部窗口输入前，需在“系统设置 → 隐私与安全性 → 辅助功能”授予权限。若 Windows 终端以管理员身份运行，SendToCmd 也需要相应权限。

串口直连和 SSH 直连按技术方案属于后续版本，2.0 仍通过外部终端工作。

## English

SendToCmd 2.0 is a Qt 6 Widgets, C++20, and CMake command notepad. External terminal sending code is implemented for Windows, macOS, and Ubuntu X11. On Ubuntu Wayland, the editor works, but external window binding and input injection are unavailable under desktop security restrictions. The original Windows C# release is preserved in [`archive/windows-v1/`](archive/windows-v1/); the design is in [`docs/TECHNICAL_PLAN.md`](docs/TECHNICAL_PLAN.md), and platform verification is tracked in [`docs/IMPLEMENTATION_STATUS.md`](docs/IMPLEMENTATION_STATUS.md).

### Build

Install the Qt 6.4+ Core, Gui, and Widgets development packages and CMake 3.21+. Linux X11 also needs X11 and XTest development packages. Run:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Build with a Qt 6 MSVC/MinGW toolchain on Windows or Xcode Command Line Tools and Qt 6 on macOS. A cross-compiled [Windows x64 test ZIP](dist/SendToCmd-2.0-Windows-x64.zip) is available; extract the whole ZIP before running `SendToCmd.exe`. A “missing `libstdc++-6.dll`” error means only the EXE was copied; keep the DLLs and `plugins` folder beside it. The current `build-windows/` directory also contains these runtime files. Terminal sending in the latest Windows build still needs retesting on a Windows machine. No prebuilt macOS installer is available.

A [single-file Windows x64 EXE](dist/SendToCmd-2.0-Windows-x64-static.exe), about 14.3 MB, is available for testing on Windows 10/11 x64 without separate Qt or MinGW DLLs. Static Qt was built with `FEATURE_optimize_size=ON` (`-Os`) and `FEATURE_ltcg=OFF`, retaining all previously linked static plugins; this is about 31% smaller than the original 20.8 MB build. To rebuild it, use a **static MinGW x64 Qt 6** build (Core, Gui, Widgets, and the Windows platform plugin) with its matching MinGW toolchain. The `.a` files in a regular Qt SDK can be DLL import libraries rather than static Qt libraries. Run `./packaging/windows-static.ps1 -QtStaticPrefix 'C:\path\to\static-qt'` in Windows PowerShell. It rejects shared Qt and inspects DLL imports. Normal Windows system DLLs are still used. Static linking does not sign the EXE or bypass Smart App Control. Distribution under Qt's open-source licenses also requires compliance with the Qt license, source, and relinking obligations; see [Qt's guidance](https://www.qt.io/development/open-source-lgpl-obligations).

The Windows test package is unsigned. If Windows 11 Smart App Control blocks it because the publisher cannot be verified, a checksum or the file's “Unblock” property cannot make it trusted signed code. Microsoft currently provides no per-app exception. Distribution on a protected device requires a properly signed and tested release. Do not disable the host's security protection just to run this test package. See [Microsoft's Smart App Control FAQ](https://support.microsoft.com/en-us/windows/security/threat-malware-protection/smart-app-control-frequently-asked-questions). The window, taskbar, Windows EXE, Linux, and macOS icons now share the green paper-plane artwork generated by `packaging/make_icons.py`.

For self-contained distribution, run `packaging/windows.ps1` with `windeployqt`, `packaging/macos.sh` with `macdeployqt`, or `packaging/linux.sh` with `linuxdeployqt` on the corresponding platform. They produce a Windows ZIP, macOS DMG, or Linux AppImage with Qt runtime files. The Windows ZIP is a **portable folder** containing Qt, platform plugins, and compiler runtimes; Qt need not be installed on the target PC, but the EXE cannot be copied alone. Test actual window binding and terminal input on each target system before release.

### Windows portable package and signing

On Windows, install CMake, Qt 6, the matching compiler, and put `cmake` and `windeployqt.exe` on PATH. Use Qt's matching **MinGW x64** toolchain for a no-install portable folder that includes compiler DLLs; an MSVC build may still need the Visual C++ Redistributable on a clean PC. Run `./packaging/windows.ps1` to produce `dist/SendToCmd-2.0-Windows-x64-unsigned.zip`. The complete cross-compiled ZIP can also be extracted and signed without rebuilding on Windows. Confirm the build's origin or check its SHA256 first.

For Windows 11 Smart App Control, obtain a code-signing identity chaining to a trusted Windows root; a self-signed certificate is insufficient. Options include [Azure Artifact Signing Public Trust](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/code-signing-options), subject to regional eligibility, or an OV code-signing certificate from a trusted CA. Use Windows SDK `signtool.exe` to sign the package's EXE and DLL files **after** dependency deployment. Keep private keys and PFX passwords out of the repository.

With a trusted CA certificate in the Windows certificate store:

```powershell
./packaging/sign-windows.ps1 -PackageDirectory ./dist/SendToCmd-2.0-Windows-x64 -CertificateThumbprint '40-digit SHA1 certificate thumbprint'
```

For Azure Artifact Signing, install the Dlib and SignTool and authenticate as described in [Microsoft's integration guide](https://learn.microsoft.com/en-us/azure/artifact-signing/how-to-signing-integrations), then run:

```powershell
./packaging/sign-windows.ps1 -PackageDirectory ./dist/SendToCmd-2.0-Windows-x64 -ArtifactSigningDlib 'C:\path\Azure.CodeSigning.Dlib.dll' -ArtifactSigningMetadata 'C:\path\metadata.json'
```

Run the script on a Windows **build machine** with signing tools and your signing identity, then transfer the signed ZIP to the target PC. The script keeps existing valid signatures, signs the remaining EXE/DLL files, verifies each with `signtool verify /pa /all`, and emits `SendToCmd-2.0-Windows-x64-signed.zip` plus a SHA256 file. Test the extracted signed folder on a Windows 11 machine with Smart App Control enabled before publishing.

For the standalone EXE, use `packaging/sign-static-windows.ps1`. First obtain a trusted code-signing identity and install SignTool on Windows (the Windows SDK Signing Tools, or `./packaging/setup-signing.ps1`). With a certificate and private key in the current user's certificate store:

```powershell
./packaging/sign-static-windows.ps1 -CertificateThumbprint '40-digit SHA1 certificate thumbprint'
```

With Azure Artifact Signing:

```powershell
./packaging/sign-static-windows.ps1 -ArtifactSigningDlib 'C:\path\Azure.CodeSigning.Dlib.dll' -ArtifactSigningMetadata 'C:\path\metadata.json'
```

The script preserves the unsigned original, produces `dist/SendToCmd-2.0-Windows-x64-static-signed.exe`, verifies its Authenticode signature, and writes a new SHA256 file. See [Microsoft's signing options](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/code-signing-options) for individual developers. A self-signed certificate is only suitable for controlled testing, not public publisher trust.

### Use

1. Establish an SSH or serial connection in an external terminal and select its intended input area.
2. Enter one command per line or open a UTF-8 text file.
3. Hold the top-right crosshair, drag it to the terminal, and release to bind.
4. With **Auto send** unchecked, click a line and then the plane button or press F8. The full line and Enter are sent; a blank line sends Enter only. The selection moves after a successful send.
5. For a range, right-click line numbers to set the green start and red end, choose a 0.1–60 second interval, check **Auto send**, and click the plane or press F8. Click the stop icon or press F8 to interrupt.

Settings allow English/Chinese, clipboard paste/keyboard input, paste shortcut, and focus delay. **Settings → Terminal paste shortcut** labels **Shift+Insert (MobaXterm)** and provides **Custom...** for other key combinations. Clipboard paste is the default: SendToCmd temporarily backs up MIME data, pastes one line, sends Enter separately, then attempts to restore the clipboard. It will not overwrite clipboard content another app changed during the operation. It never silently retries a failed paste through keyboard injection; a blank line never touches the clipboard.

The general Windows default is Ctrl+V; when MobaXterm is bound, SendToCmd selects Shift+Insert unless you have manually chosen a paste shortcut. Defaults are Cmd+V on macOS and Ctrl+Shift+V on X11. Choose a preset or enter a custom combination using letters, digits, or common function keys. An unsupported combination fails and stops the send. Choose a shortcut that matches your terminal, including any custom MobaXterm key binding. On Windows, clipboard mode writes native Unicode text. If only `^M` appears, Enter arrived but the paste shortcut was not handled; test Shift+Insert manually and select the terminal's actual paste shortcut under Settings. Keyboard mode uses Unicode `SendInput` on Windows and Unicode `CGEvent` on macOS. X11 keyboard mode is limited to characters present in the active keymap; clipboard paste is recommended there.

Before sending, the app checks target identity and foreground focus. Auto send stops on focus loss or input failure. It cannot confirm execution on the device or undo partially delivered input. F8 works only while SendToCmd has focus. macOS external input requires Accessibility permission; an elevated Windows terminal may require matching privileges.

Direct serial and SSH targets are planned for later versions; 2.0 uses an external terminal.
