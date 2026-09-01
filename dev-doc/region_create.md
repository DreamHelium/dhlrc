# Create Region

To create a region, we need to get the base information of the region.

## Region type

We need to get the region type first. The original function type is like:

```c++
const char* region_type();
```

The return value must get from `CString` from Rust, and should use `string_free` to free.

## Multiple region support

If a region type support multiple regions, a value of `1` could be got as the following function:

```c++
int32_t region_is_multi();
```

Based on the result, we have different type of functions to support the region creation.

## Loading file to object

Since the original NBT file (or might not) uses the `GZip` or `ZLib` compression method, we need to load the file into a
vector. But the type is not used by C/C++, so we use a `void*` to store.

```c++
/* This is from the `region.h` as the output symbol of `region-rs` */
VecU8 *file_try_uncompress (const char *filename, HelperStruct* helper_struct,
                           int *failed);
```

After using, we need to free it using:

```c++
/* This is from the `region.h` as the output symbol of `region-rs` */
void vec_free (VecU8 *vec);
```

So now we get a `void *vec` to store a vector of the original file. We can also use a `unique_ptr` to temporarily store
it. Then we need to convert the vector to the object (Basically a `NBT Object` or a `JSON Object`).

```c++
const char* region_get_object(VecU8 *bytes, void **object,
                              HelperStruct *helper_struct);
```

Also we need to free it by:

```c++
void object_free(void *object);
```

## Real loading

### For single region type

Just use this function, and you will get a new region struct:

```c++
const char* region_create_from_file(void *object,
    ProgressFunc progress_fn, void **region, void *main_klass, const void *cancel_flag,
    uint64_t elapsed_millisecs, uint64_t free_memory);
```

### For multiple region type

You need to get region number by this:

```c++
int32_t region_num(void *object);
```

Then get region name by this:

```c++
const char* region_name_index(void *object, int32_t index);
```

Finally, we can get region by:

```c++
const char* region_create_from_file_as_index(void *object, void **region,
                                             int32_t index, HelperStruct* helper_struct);
```
