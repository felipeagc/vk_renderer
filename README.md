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

### Rendering a sphere mesh

![image](https://user-images.githubusercontent.com/17355488/107703911-2287fc00-6c9b-11eb-9cb9-c8a1914eecb6.png)
