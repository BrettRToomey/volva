<p align="center">
  <img src="logo.png" align="middle">
  <br>
  <br>
  <h1>Vǫlva</h1>
</p>

## Getting started

### Builtin commands

#### `plugins`:
Commands for creating and managing plugins
#### `env`:
Commands for creating and managing VCloud env. variables
#### `resource`:
Copy over Vapor Resources and Views

## Plugins

### Installing plugins
#### Using volva:
```
volv plugins install <plugin.so>
```
or the shorthand version:
```
volv plugins <plugin.so>
```

#### Manually:
Installing plugins manually is simple, just drop the `.so`/`.o`/`.dylib` file into your `~/.volva/plugins` folder.

### Creating plugins
Volva uses the C ABI which makes it very easy to create a plugin in your prferred language. Look at the [API documentation](#todo) for more information.

#### Example in C
`my_plugin.c`:
```c
#include "plugins.h"

int PluginInit() {
    VolLog("Hello, world!\n");
    return 0;
}
```
And then build with:

```
volv plugins build my_plugin
```
