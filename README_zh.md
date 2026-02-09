# Minecraft结构修改器

[开发文档（英文）](dev-doc/README.md)

## 许可证问题

因为`crabnbt`使用GPL协议，所以本程序也得使用GPL协议，对此的不便敬请谅解！

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

你可能需要从`region-rs/target/debug(或release)`复制`region_rs`到项目根目录及构建目录中。如果条件允许，使用一个软链接。

## 功能

待写。

## 使用

只用运行`dhlrc_qt`，Qt后端将会启动，除非你用的是Linux tty。