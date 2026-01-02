# Node-API Documentation - Node.js v25.2.1

## Overview

Node-API (formerly N-API) is an API for building native addons that is independent from the underlying JavaScript runtime. It provides Application Binary Interface (ABI) stability across Node.js versions.

## Key Characteristics

- **ABI Stable**: Addons compiled for one major version run on later versions without recompilation
- **VM Independent**: Not tied to V8 or other specific JavaScript engines
- **Status-Based Error Handling**: All API calls return `napi_status`
- **Opaque Types**: JavaScript values are abstracted as `napi_value`

## Basic API Properties

1. All Node-API calls return a `napi_status` indicating success or failure
2. API return values are passed via out parameters
3. JavaScript values use the opaque `napi_value` type
4. Extended error information available via `napi_get_last_error_info()`

## Core Data Types

### `napi_status`
Integral status code with values including:
- `napi_ok`
- `napi_invalid_arg`
- `napi_object_expected`
- `napi_string_expected`
- `napi_function_expected`
- `napi_pending_exception`
- And others for specific error conditions

### `napi_env`
Represents a context for VM-specific state. Must be passed to all Node-API calls.

### `napi_value`
Opaque pointer representing a JavaScript value.

### `node_api_basic_env`
A restricted variant of `napi_env` for use in synchronous finalizers with limited API access.

## Building Native Addons

### Build Tools

**node-gyp**: Python-based build system (most common)
```bash
# Installation comes with npm
```

**CMake.js**: Alternative build system based on CMake

### Precompiled Binary Tools

- **node-pre-gyp**: Uploads to custom servers (supports Amazon S3)
- **prebuild**: GitHub releases integration
- **prebuildify**: Bundles binaries with npm package

## Node-API Version Matrix

| Version | Supported In |
|---------|---|
| 10 | v22.14.0+, 23.6.0+ |
| 9 | v18.17.0+, 20.3.0+, 21.0.0+ |
| 8 | v12.22.0+, v14.17.0+, v15.12.0+, 16.0.0+ |
| 7 | v10.23.0+, v12.19.0+, v14.12.0+, 15.0.0+ |
| 6 | v10.20.0+, v12.17.0+, 14.0.0+ |

## Usage

### Including Node-API Header

```c
#include <node_api.h>
```

### Specifying API Version

```c
#define NAPI_VERSION 3
#include <node_api.h>
```

### Experimental Features

```c
#define NAPI_EXPERIMENTAL
#include <node_api.h>
```

## Error Handling

### Exception Types

- `napi_throw()`: Throw any JavaScript value
- `napi_throw_error()`: Standard Error
- `napi_throw_type_error()`: TypeError
- `napi_throw_range_error()`: RangeError
- `node_api_throw_syntax_error()`: SyntaxError

### Error Creation

```c
napi_create_error(env, code, msg, &result);
napi_create_type_error(env, code, msg, &result);
napi_create_range_error(env, code, msg, &result);
node_api_create_syntax_error(env, code, msg, &result);
```

### Exception Checking

```c
napi_is_exception_pending(env, &is_pending);
napi_get_and_clear_last_exception(env, &exception);
```

## Environment Lifecycle

### Instance Data Management

```c
// Set instance data
napi_set_instance_data(env, data, finalize_cb, finalize_hint);

// Get instance data
napi_get_instance_data(env, &data);
```

## Object Lifetime Management

### Handle Scopes

**Regular Scope:**
```c
napi_handle_scope scope;
napi_open_handle_scope(env, &scope);
// ... use handles ...
napi_close_handle_scope(env, scope);
```

**Escapable Scope (for returning values):**
```c
napi_escapable_handle_scope scope;
napi_open_escapable_handle_scope(env, &scope);
// ... create handles ...
napi_escape_handle(env, scope, value);
napi_close_escapable_handle_scope(env, scope);
```

### References

```c
napi_ref ref;
napi_create_reference(env, value, 1, &ref);
napi_get_reference_value(env, ref, &value);
napi_delete_reference(env, ref);
napi_reference_ref(env, ref, &new_refcount);
napi_reference_unref(env, ref, &new_refcount);
```

## Cleanup Hooks

### Environment Cleanup
```c
napi_add_env_cleanup_hook(env, cleanup_fn, data);
napi_remove_env_cleanup_hook(env, cleanup_fn, data);
```

### Asynchronous Cleanup
```c
napi_async_cleanup_hook_handle handle;
napi_add_async_cleanup_hook(env, async_cleanup_fn, data, &handle);
napi_remove_async_cleanup_hook(handle);
```

## Working with JavaScript Values

### Object Creation

```c
napi_create_object(env, &object);
napi_create_array(env, &array);
napi_create_array_with_length(env, 10, &array);
napi_create_external(env, data, finalize_cb, finalize_hint, &external);
```

### Type Conversion

```c
// C to JavaScript
napi_create_int32(env, 42, &value);
napi_create_double(env, 3.14, &value);
napi_create_string_utf8(env, "hello", NAPI_AUTO_LENGTH, &value);
napi_create_bigint_int64(env, 123, &value);

// JavaScript to C
napi_get_value_int32(env, value, &int_val);
napi_get_value_double(env, value, &double_val);
napi_get_value_string_utf8(env, value, buf, sizeof(buf), &len);
napi_get_value_bigint_int64(env, value, &int64_val);
```

### Type Checking

```c
napi_typeof(env, value, &type);
napi_is_array(env, value, &is_array);
napi_is_arraybuffer(env, value, &is_buffer);
napi_is_buffer(env, value, &is_buffer);
napi_is_date(env, value, &is_date);
napi_is_error(env, value, &is_error);
napi_is_typedarray(env, value, &is_typedarray);
```

## Working with Properties

### Property Access

```c
// Named properties
napi_set_named_property(env, obj, "foo", value);
napi_get_named_property(env, obj, "foo", &value);
napi_has_named_property(env, obj, "foo", &has);

// Indexed properties
napi_set_element(env, obj, 0, value);
napi_get_element(env, obj, 0, &value);
napi_has_element(env, obj, 0, &has);

// Generic properties
napi_set_property(env, obj, key, value);
napi_get_property(env, obj, key, &value);
napi_has_property(env, obj, key, &has);
```

### Property Enumeration

```c
napi_get_property_names(env, obj, &names);
napi_get_all_property_names(env, obj, mode, filter, conversion, &names);
```

### Property Definition

```c
napi_property_descriptor descriptors[] = {
  { "foo", NULL, callback, NULL, NULL, value, napi_default, NULL }
};
napi_define_properties(env, obj, 1, descriptors);
```

## Working with Functions

### Function Creation

```c
napi_create_function(env, "funcName", NAPI_AUTO_LENGTH,
                     callback_fn, data, &function);
```

### Function Calling

```c
napi_call_function(env, recv, func, argc, argv, &result);
napi_new_instance(env, constructor, argc, argv, &result);
```

### Callback Information

```c
napi_status callback(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value argv[2];
  napi_value thisArg;
  void* data;
  napi_get_cb_info(env, info, &argc, argv, &thisArg, &data);
  return NULL;
}
```

## Object Wrapping (Classes)

### Class Definition

```c
napi_value constructor;
napi_define_class(env, "MyClass", NAPI_AUTO_LENGTH,
                  constructor_cb, NULL, prop_count,
                  properties, &constructor);
```

### Instance Wrapping

```c
napi_wrap(env, obj, native_object, finalize_cb, finalize_hint, &ref);
napi_unwrap(env, obj, &native_object);
napi_remove_wrap(env, obj, &native_object);
```

### Type Tagging

```c
napi_type_tag tag = { 0x12345678, 0x87654321 };
napi_type_tag_object(env, obj, &tag);
napi_check_object_type_tag(env, obj, &tag, &matches);
```

## Asynchronous Operations

### Simple Async Work

```c
napi_async_work work;
napi_create_async_work(env, NULL, async_resource,
                       execute_cb, complete_cb, data, &work);
napi_queue_async_work(env, work);
napi_cancel_async_work(env, work);
napi_delete_async_work(env, work);
```

### Thread-Safe Functions

```c
napi_threadsafe_function tsfn;
napi_create_threadsafe_function(env, func, NULL, 1, 1, NULL,
                                finalize_cb, data, call_js_cb, &tsfn);

// From another thread:
napi_call_threadsafe_function(tsfn, data, napi_tsfn_nonblocking);
```

## Promises

```c
napi_deferred deferred;
napi_promise_type promise;
napi_create_promise(env, &deferred, &promise);

// Resolve
napi_resolve_deferred(env, deferred, result);

// Reject
napi_reject_deferred(env, deferred, reason);

// Check
napi_is_promise(env, value, &is_promise);
```

## ABI Stability Considerations

### Non-ABI-Stable APIs to Avoid

The following APIs do NOT provide ABI stability and should be avoided:

```c
#include <node.h>           // Node.js C++ APIs
#include <node_buffer.h>
#include <node_version.h>
#include <node_object_wrap.h>
#include <uv.h>             // libuv APIs
#include <v8.h>             // V8 APIs
```

### Recommended Approach

Use only Node-API headers:
```c
#include <node_api.h>       // Provides ABI stability
```

## Module Registration

```c
NAPI_MODULE_INIT(/* napi_env env, napi_value exports */) {
  // Module initialization code
  // Must return napi_value
  return exports;
}
```

## Example: Complete Addon

```c
#include <node_api.h>

static napi_value Add(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value argv[2];
  napi_get_cb_info(env, info, &argc, argv, NULL, NULL);

  int32_t a, b;
  napi_get_value_int32(env, argv[0], &a);
  napi_get_value_int32(env, argv[1], &b);

  napi_value result;
  napi_create_int32(env, a + b, &result);
  return result;
}

NAPI_MODULE_INIT(/* napi_env env, napi_value exports */) {
  napi_value add_fn;
  napi_create_function(env, "add", NAPI_AUTO_LENGTH, Add, NULL, &add_fn);
  napi_set_named_property(env, exports, "add", add_fn);
  return exports;
}
```

This documentation provides the foundation for building performant, cross-version compatible native Node.js addons with guaranteed ABI stability.
