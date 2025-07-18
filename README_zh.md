# 投影文件阅读器

[pic](funcshow_zh.png)

**注：本项目将迁移至C++，所以以后应该叫dhlrcpp了。**

## 简介

虽说它名为“投影文件阅读器”，但此程序立项时的目的其实是**处理材料列表，拆分出可合成物品的配方表**
，奈何要给外置程序处理的话使用翻译后的名称会有很大麻烦（投影模组默认导出时是已经翻译后的物品名而不是ID，要处理还需得到原本ID，比较麻烦，而且官方还会微改翻译，~
比如平滑石**头**台阶~），还需要指定版本。~虽然说调试时拿1.18的翻译文件处理原生投影文件也出现了翻译未出现的情况~

鉴于其为一个通用兼容型项目，`assets`和`data`文件需要由用户提供。幸运的是，在Qt模组的`工具`中有一个简易向导来帮助你完成此项目。

## 依赖

- [libnbt](https://github.com/djytw/libnbt)（已在本项目中的submodule中）
- `zlib`
- [cJSON](https://github.com/DaveGamble/cJSON) `>= 1.7.13`（~正好用上了两个这版本增加的函数~）
- `gettext` （其是`glibc`的一部分所以你大概不需要在GNU/Linux中进行额外安装，但是在其他系统可能需要）（可选——如果你需要翻译）
- `glib2`
- `minizip-ng`
- [dhutil](https://github.com/DreamHelium/dhutil)

## 编译

建议在编译使用的计算机中安装`git`，获取此仓库后获取submodule。

```bash
git submodule update --init
```

如果有CMake，可以直接使用这下两行编译：

```bash
cmake -B build
cmake --build build
```

## 功能

- NBT管理（NBT`接口`支持和NBT阅读器）
  注：如果你想要修改内容，你可能需要使用`get_current_nbt`方法来自己修改。
- 使用通用`Region`部分支持`NBT结构`和`Litematica投影`
  注：支持简单阅读方块偏移板和状态，生成方块列表（~虽说是物品列表~），从`Region`生成`NBT结构`。
- 部分的配方表支持

## 使用

只用运行`dhlrc.bin`，Qt后端将会启动，除非你用的是Linux tty。

如果你只想转换文件，只用运行`dhlrc.bin conv --help`然后按照指引操作。