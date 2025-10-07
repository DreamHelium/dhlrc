# Script System

[中文](script_zh.md)

After version 20250930, we shipped an experimental script support by lua language.
Now you can use script to enhance the experience of the application.

# Example

```lua
uuid = add_file("file1", "file2")
descriptions = get_all_description("DhNbtInstanceCpp", uuid)
for i, v in ipairs(descriptions) do
    print(i, v)
end
```

# Functions

## `add_file`

Add NBT file(s) to the Info Management System.

If fails, it just ignores the file and continue.

- Params: Filename(s).
- Return: table of UUIDs in System corresponding to the file(s).

## `get_all_uuid`

Get the UUIDs of the corresponding type.

- Param: type name. (Refer to `common_info.h` now, we have `DhNbtInstanceCpp`, `DhRegion`, `DhItemList`, `DhModule` and `DhFunc`)
- Return: table of UUIDs corresponding to the type.

## `get_all_description`

Get the descriptions corresponding to the UUIDs and type.

- Param: type name, table of the UUIDs.
- Return: table of descriptions.