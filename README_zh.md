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

Qt 界面需要 Rust 库 `region-rs` 和 `dhlrc-core`，`dhlrc-core` 所支持的模块也应使用 Rust 编写。此外还需要 Qt 6 与 KF6。在 Windows 上推荐使用 [MSYS2](https://www.msys2.org/) 构建（见 `build-msys2.sh`）。

## 编译

Qt 界面（需要 Qt 6 与 KF6）：

```bash
cmake -B build && cmake --build build
```

无界面构建（仅共享库与终端前端；无需 Qt/KF6）：

```bash
cmake -B build -DDH_BUILD_GUI=OFF && cmake --build build
```

无界面构建（不使用 gettext，如 Termux/Android）：

```bash
cmake -B build -DDH_BUILD_GUI=OFF -DDH_TRANSLATIONS=OFF && cmake --build build
```

Windows（[MSYS2](https://www.msys2.org/) UCRT64）：

```bash
./build-msys2.sh            # 以 release 构建到 build-ucrt64/
./build-msys2.sh --package  # 同时生成便携 .zip（装有 NSIS 时生成安装程序 .exe）
./build-msys2.sh --help     # 全部选项
```

## 功能

- 支持NBT、投影文件和Sponge结构文件（`.schem`）。
- 修改区域的基础数据。
- 3D预览：按方块模型与材质绘制区域，可旋转、缩放并点选方块。
- 老掉牙的界面，带通知中心与日志查看。
- 启动时恢复上次会话：重新打开标签页、回到当时活动的标签，并恢复窗口大小和侧栏宽度；已经加载但没有打开标签页的区域也会重新加载（可在“设置 -> 通用”中关闭）。

## 使用

只用运行`dhlrc_qt`，Qt后端将会启动，除非你用的是Linux tty。

程序内置了一份简短的使用说明：打开`帮助 -> 帮助`即可看到快速入门（英文原文在
`resources/usage.md`，中文译文在它旁边的`resources/usage.zh_CN.md`）。左侧列表里的
`程序目录`会列出程序使用的各个目录，卸载前请在那里查看配置和缓存目录。

`dhlrc-tui` 是可交互的终端前端：不带参数运行时进入支持鼠标的设置页（带侧栏）；
`--help` 列出其它子命令。

## 日志

Qt 前端会把日志按天写入`<缓存目录>/logs/dhlrc-YYYY-MM-DD.log`；缓存目录可在左侧列表的
`程序目录`中查看，`帮助 -> 打开日志文件夹`可直接打开日志目录。

只有不低于所设级别的消息才会写入，默认级别为`info`。级别可通过环境变量
`DHLRC_LOG_LEVEL`（`debug`、`info`、`warning`、`critical`或`off`）或命令行选项
`--log-level <级别>`设置，命令行选项优先；`off`只停写文件，终端输出保持不变。

[commit]: https://github.com/DreamHelium/dhlrc/commit/38448728e647b9b6665f0c502d1f79c00d1d4afc
