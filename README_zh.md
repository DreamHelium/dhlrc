# Minecraft结构修改器

[English](README.md) | [开发文档（英文）](dev-doc/README.md)

---

## 目录

- [免责声明](#免责声明)
- [报告 Bug 与错误](#报告-bug-与错误)
- [许可证问题](#许可证问题)
- [简介](#简介)
- [依赖](#依赖)
- [编译](#编译)
- [功能](#功能)
- [使用](#使用)
- [日志](#日志)

---

## 免责声明

从版本20260930（虽然尚未发布）/ [提交3844872][commit]起，本项目开始使用LLM辅助开发。不必担心，界面部分仍
会经过完整的人工测试。你也可以使用更早的版本（虽然在发行版0.3.6到20260820之间改动很多，你可能需要逐个
测试提交才能选出可用的版本），它们都是人工编写的。

## 报告 Bug 与错误

在报告之前，你应当确认：

- 你使用的是最新的发行版（如果可能，或是用最新提交构建的二进制文件）；
- 该 Bug/错误在较新的提交中尚未修复；

要确认你的版本，可以运行`dhlrc_qt --version`，或在界面中查看`Help -> About`。之后你就可以创建一个新的
issue，并附上版本号以及 Bug/错误的描述。

## 许可证问题

我懒得改许可证，对此的不便敬请谅解！

## 简介

你可以把你的结构导入到一个`Region`结构中，然后做些修改（虽然仍在写中）。之后你可以导出结构到你想要的类型。

## 依赖

见`*-rs`目录中的`Cargo.toml`, 以及`src`中的`CMakeLists.txt`。

## 编译

如果有CMake，可以直接使用这下两行编译：

```bash
cmake -B build
cmake --build build
```

若不需要 Qt 界面（只编译共享库与终端前端），加上 `-DDH_BUILD_GUI=OFF`，此时不再需要 Qt/KF6；安装时也不会带上只有 Qt 前端才加载的 `libdhlrc_core`（终端前端静态链接核心）。再加上 `-DDH_TRANSLATIONS=OFF` 可跳过 gettext 目录（否则需要 `xgettext`/`msgfmt`），程序则一律显示英文。

Termux/Android 上没有 `libintl`，Rust 侧的翻译层会自动失效（见 `gettext-shim`）：构建不再需要任何 C 库，字符串保持英文。在那里请一并加上 `-DDH_TRANSLATIONS=OFF`。

在 Windows 上，配合 [MSYS2](https://www.msys2.org/) 的 `build-msys2.sh` 可从 UCRT64 环境一键构建：脚本会检查所需软件包（并询问是否安装缺少的包）、配置并编译；加上 `--package` 还会打包成可分发压缩包，若安装了 NSIS 则生成安装程序。

```bash
./build-msys2.sh            # 以 release 构建到 build-ucrt64/
./build-msys2.sh --package  # 同时生成便携 .zip；装了 NSIS 还会生成安装程序 .exe
```

安装程序可让用户选择要安装的前端（Qt 界面和/或终端前端）、对象编解码器与区域模块，以及是否创建开始菜单快捷方式；勾选任一区域模块会自动带上它所需的 NBT 编解码器（模块只能处理编解码器解析出的对象）。它还会注册核心的桌面通知所用的 AppUserModelID，使 Windows 把通知显示为本程序而不是 PowerShell。

其它选项（`--debug`、`--no-gui`、`--no-translations`、`--offline`、`--clean`、`--build-dir`、`--jobs`）见 `./build-msys2.sh --help`。

## 功能

- 支持NBT、投影文件和Sponge结构文件（`.schem`）。
- 修改区域的基础数据。
- 3D预览：按方块模型与材质绘制区域，可旋转、缩放并点选方块（见[开发文档](dev-doc/render.md)）。
- 老掉牙的界面。

## 使用

只用运行`dhlrc_qt`，Qt后端将会启动，除非你用的是Linux tty。

程序内置了一份简短的使用说明：打开`帮助 -> 帮助`即可看到快速入门（英文原文在
`resources/usage.md`，中文译文在它旁边的`resources/usage.zh_CN.md`）。左侧列表里的
`程序目录`会列出程序使用的各个目录，卸载前请在那里查看配置和缓存目录。

tty支持可能已在计划中。

## 日志

Qt 前端会把日志按天写入`<缓存目录>/logs/dhlrc-YYYY-MM-DD.log`；缓存目录可在左侧列表的
`程序目录`中查看，`帮助 -> 打开日志文件夹`可直接打开日志目录。

只有不低于所设级别的消息才会写入，默认级别为`info`。级别可通过环境变量
`DHLRC_LOG_LEVEL`（`debug`、`info`、`warning`、`critical`或`off`）或命令行选项
`--log-level <级别>`设置，命令行选项优先；`off`只停写文件，终端输出保持不变。

[commit]: https://github.com/DreamHelium/dhlrc/commit/38448728e647b9b6665f0c502d1f79c00d1d4afc
