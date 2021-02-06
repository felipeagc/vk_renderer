cmake_minimum_required(VERSION 3.1)
project(renderer VERSION 1.0 LANGUAGES C CXX)

add_library(
  renderer

  renderer/platform.h
  renderer/platform.cpp
  renderer/array.h
  renderer/math.h
  renderer/allocator.h
  renderer/allocator.c
  renderer/format.h
  renderer/format.cpp
  renderer/lexer.h
  renderer/lexer.cpp

  renderer/engine.h
  renderer/engine.cpp
  renderer/camera.h
  renderer/camera.cpp
  renderer/mesh.h
  renderer/mesh.cpp
  renderer/uniform_arena.h
  renderer/uniform_arena.cpp
  renderer/model_asset.h
  renderer/model_asset.cpp
  renderer/pipeline_asset.h
  renderer/pipeline_asset.cpp

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

add_executable(app app/main.cpp)
target_link_libraries(app PUBLIC renderer)

if(MSVC)
  target_compile_options(renderer PUBLIC /W3 /std:c++latest)
else()
  target_compile_options(renderer PUBLIC -Wall -Wextra -Werror)
endif()
