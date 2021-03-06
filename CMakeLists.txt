cmake_minimum_required(VERSION 3.1)
project(renderer VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(
  renderer

  renderer/array.h
  renderer/math.h
  renderer/allocator.h
  renderer/allocator.c
  renderer/pool.h
  renderer/pool.c
  renderer/format.h
  renderer/format.c
  renderer/lexer.h
  renderer/lexer.cpp
  renderer/string_map.hpp
  renderer/config.h
  renderer/config.cpp
  renderer/string_builder.h
  renderer/string_builder.cpp

  renderer/engine.h
  renderer/engine.c
  renderer/buffer_pool.h
  renderer/buffer_pool.c
  renderer/camera.h
  renderer/camera.c
  renderer/mesh.h
  renderer/mesh.c
  renderer/model_asset.h
  renderer/model_asset.c
  renderer/pipeline_util.h
  renderer/pipeline_util.c
  renderer/pbr.h
  renderer/pbr.c

  thirdparty/rg/rg.h
  thirdparty/rg/rg.c
  thirdparty/rg/volk.h
  thirdparty/rg/volk.c

  thirdparty/tinyshader/tinyshader/tinyshader.h
  thirdparty/tinyshader/tinyshader/tinyshader_unity.c
)

add_library(
  renderer_libs

  thirdparty/cgltf/cgltf.h
  thirdparty/cgltf/cgltf.c

  thirdparty/stb_image/stb_image.h
  thirdparty/stb_image/stb_image.c

  thirdparty/imgui/cimgui.cpp
  thirdparty/imgui/cimgui.h
  thirdparty/imgui/imconfig.h
  thirdparty/imgui/imgui.cpp
  thirdparty/imgui/imgui_demo.cpp
  thirdparty/imgui/imgui_draw.cpp
  thirdparty/imgui/imgui.h
  thirdparty/imgui/imgui_widgets.cpp

  thirdparty/glfw/include/GLFW/glfw3.h
  thirdparty/glfw/include/GLFW/glfw3native.h
  thirdparty/glfw/glfw_unity.c
)

target_include_directories(
  renderer
  PUBLIC
  .
  thirdparty/rg
  thirdparty/tinyshader
  thirdparty/stb_image
  thirdparty/imgui
  thirdparty/glfw/include
  thirdparty/cgltf
)

target_link_libraries(renderer PUBLIC renderer_libs)

if (UNIX)
  target_link_libraries(renderer PUBLIC dl m pthread X11 Xau)
endif(UNIX)

if (WIN32)
  target_link_libraries(renderer PUBLIC gdi32 user32 ole32 oleaut32 advapi32 shlwapi)
endif(WIN32)

add_executable(app app/main.c)
target_link_libraries(app PUBLIC renderer)

if(MSVC)
  target_compile_options(renderer PUBLIC /W3 /std:c++latest)
else()
  target_compile_options(renderer PUBLIC -Wall -Wextra)
endif()
