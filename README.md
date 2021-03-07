# Vk renderer

A new vulkan renderer, hopefully less buggy than my previous ones... 
written in (mostly) C-style C++ and using descriptor indexing this time.

The vulkan abstraction layer is in
[rg.h](https://github.com/felipeagc/vk_renderer/blob/master/thirdparty/rg/rg.h)
and [rg.c](https://github.com/felipeagc/vk_renderer/blob/master/thirdparty/rg/rg.c).

Using [tinyshader](https://github.com/felipeagc/tinyshader) for shader compilation at runtime.

## Building and running

Only tested on Linux for now.

```
cmake -Bbuild -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/app # run
```

## Current screenshots

### Rendering a GLTF model

![image](https://user-images.githubusercontent.com/17355488/110253221-e24d2e00-7f67-11eb-917a-785fde95acfc.png)
