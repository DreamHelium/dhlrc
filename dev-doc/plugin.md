# Plugin Base Information

## Plugin Base Data

We need to get the base name of the region type. It can be done using:

```c++
const char* region_type();
```

The return value must get from `CString` from Rust, and should use `string_free` to free.

We can add the optional region **file** suffix using

```c++
const char* region_file_suffix();
```

We can also use the optional region **base type** to get the base type of the file:

```c++
const char* region_base_type();
```

We can also use the optional region **file type** to get the filter of the file:

```c++
const char* region_file_type();
```

It can be `JavaNBT`, `BedrockNBT` or `JSON`. Here is the reference of the libraries and the base type used in the project:

- `crab-nbt` : `JavaNBT`
- `zuri-nbt` : `BedrockNBT`

## Multiple region support

If a region type support multiple regions, a value of `1` could be got as the following function:

```c++
int32_t region_is_multi();
```

Based on the result, we have different type of functions to support the region creation.