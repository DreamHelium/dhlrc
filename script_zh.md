# 脚本系统

在版本20250930后，我们引入了一个试验的脚本系统，使用lua语言。

现在你可以使用脚本来增强使用的体验。

# 例子

```lua
uuid = add_file("file1", "file2")
descriptions = get_all_description("DhNbtInstanceCpp", uuid)
for i, v in ipairs(descriptions) do
    print(i, v)
end
```

# 函数

## `add_file`

往信息管理系统中添加NBT文件。

如果失败，文件被忽略并且继续。

- 参数：文件名
- 返回值：对应文件的唯一码的表

## `get_all_uuid`

得到对应类型的唯一码。

- 参数：类型名 (参考`common_info.h`，现在有`DhNbtInstanceCpp`, `DhRegion`, `DhItemList`, `DhModule` 以及 `DhFunc`)
- 返回值：对应类型的唯一码表

## `get_all_description`

得到对应唯一码和类型的描述。

- 参数：类型名，唯一码表
- 返回值：描述表