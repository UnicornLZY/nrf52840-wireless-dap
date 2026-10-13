# 上传GitHub与发布源码

本目录是准备上传的仓库根目录。不要上传外层的整个MCU开发目录或ZIP作为唯一源码；GitHub页面应直接显示README、LICENSE、libraries等文件。

## 方法一：Git命令（推荐，后续更新方便）

1. 注册/登录GitHub，在 https://github.com/new 新建仓库，名称建议 `nrf52840-wireless-dap`，选择 **Public**。
2. 不勾选自动添加README、.gitignore或License，因为本地已有这些文件。
3. 安装Git for Windows，在整理后的仓库根目录打开PowerShell，执行以下命令。

```powershell
git init -b main
# 仅在尚未配置Git作者时执行；用你的真实值替换占位符。
git config user.name '你的GitHub用户名'
git config user.email '你的GitHub隐私邮箱'
git add .
git status --short
git commit -m 'Initial open-source UART latency firmware'
git remote add origin https://github.com/你的GitHub用户名/nrf52840-wireless-dap.git
git push -u origin main
```

首次push按Git Credential Manager弹出的浏览器完成GitHub登录。GitHub HTTPS不能用账号密码充当Git密码；按提示用浏览器授权，或使用个人访问令牌。隐私邮箱从GitHub Settings → Emails复制，避免把私人邮箱写进公开commit。

后续修改：

```powershell
git add .
git commit -m 'Describe this change'
git push
```

## 方法二：GitHub Desktop

在GitHub Desktop登录后使用 File → New repository；将该源码放在新仓库根目录（README与.git同级）。检查Changes，填写摘要并Commit，然后Publish repository；要公开时取消 **Keep this code private**。以后修改后Commit并Push origin。

## 固件与Release

首次上传以源码为主，构建输出UF2已被.gitignore忽略。`firmware/README.md`和 `source-manifest.json`记录了原参考固件哈希；用户可从源码构建。若要发布预编译固件，可在GitHub Releases创建预发布 `v0.1.0-uart-latency`，写明两板同刷、接线和验证状态。

预编译固件链接了多种第三方组件，不能只附MIT后把整个固件宣称为MIT。发布二进制前请按THIRD_PARTY_NOTICES保留声明并满足相应源码/重新链接要求；源码包当前不包含外部SDK二进制。不要发布实板CURRENT.UF2全闪存快照。

## 发布前检查

- README描述和许可证是你认可的内容；当前自编代码默认MIT，可在首次发布前改成你的选择。
- `git status --short` 中没有build、test-build、工具.exe、账号密钥或个人诊断日志。
- 已跑 `run_tests.ps1` 和 `build.ps1`；将实板结果与模型结果分别说明。
- 仓库About可填 `nRF52840 wireless CMSIS-DAP v2 and UART bridge`，topics可选 `nrf52840`、`cmsis-dap`、`swd`、`uart`。

参考：[GitHub官方上传说明](https://docs.github.com/en/migrations/importing-source-code/using-the-command-line-to-import-source-code/adding-locally-hosted-code-to-github)、[GitHub许可证说明](https://docs.github.com/en/communities/setting-up-your-project-for-healthy-contributions/adding-a-license-to-a-repository)。
