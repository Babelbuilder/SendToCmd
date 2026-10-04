# Windows 签名环境

配置 Windows SDK 签名工具（不生成自签名证书、不修改系统信任根）：

```powershell
./packaging/setup-signing.ps1
```

优先使用已有 Windows SDK；否则从微软官方 NuGet 包提取 x64 SignTool 至 `%LOCALAPPDATA%\SendToCmd\SigningTools`，并验证工具的微软数字签名。无需管理员安装，签名脚本自动发现此路径。

如果 PowerShell 的执行策略禁止运行本项目脚本，可通过 `powershell.exe -NoProfile -ExecutionPolicy Bypass -File ./packaging/setup-signing.ps1` 仅为这次进程运行脚本，不修改系统策略；签名脚本也可用同样方式调用。这不改变智能应用控制设置。

获取受信任 CA 的代码签名证书，并按 CA 指引安装 USB Token/HSM 驱动，使证书及私钥可以通过 Windows 证书库访问。不要把私钥或密码放入仓库。签名脚本自动查找 Windows SDK x64 SignTool，不需要手动修改 PATH。

```powershell
Get-ChildItem Cert:\CurrentUser\My -CodeSigningCert | Select-Object Subject,Thumbprint,NotAfter
./packaging/sign-windows.ps1 -PackageDirectory ./dist/SendToCmd-2.0-Windows-x64 -CertificateThumbprint '替换为证书的40位指纹'
```

证书位于计算机证书库时增加 `-CertificateStoreLocation LocalMachine`。工具位于自定义路径时增加 `-SignToolPath 'C:\path\signtool.exe'`。

脚本检查私钥、有效期、代码签名用途和证书信任链，保留已有有效签名，为其他 EXE/DLL 添加 SHA256 签名与时间戳，逐个验证后才输出 `dist/SendToCmd-2.0-Windows-x64-signed.zip` 和 SHA256 文件。

也支持 Azure Artifact Signing Public Trust：完成账户及身份验证，并安装官方 Dlib 后，使用 README 中的 Artifact Signing 命令。该服务有申请地区限制；不符合条件时需选择 CA 代码签名证书。自签名证书不能解决面向公众分发时的智能应用控制信任要求。

当前尚无签名身份，因此现有 Windows 包仍未签名。取得证书后执行上面的签名命令，并在启用智能应用控制的 Windows 11 上验证运行。

官方说明：[签名选项](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/code-signing-options)、[智能应用控制签名要求](https://learn.microsoft.com/en-us/windows/apps/develop/smart-app-control/code-signing-for-smart-app-control)。
