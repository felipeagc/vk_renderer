cmake_minimum_required(VERSION 3.1)
project(renderer VERSION 1.0 LANGUAGES C CXX)

add_library(
	renderer

	renderer/platform.hpp
	renderer/platform.cpp

	thirdparty/cgltf/cgltf.h
	thirdparty/cgltf/cgltf.c

	thirdparty/stb_image/stb_image.h
	thirdparty/stb_image/stb_image.c

	thirdparty/rg/rg.h
	thirdparty/rg/rg.c
	thirdparty/rg/rg_ext.h
	thirdparty/rg/rg_ext.c
	thirdparty/rg/volk.h
	thirdparty/rg/volk.c

	thirdparty/tinyshader/tinyshader/tinyshader.h
	thirdparty/tinyshader/tinyshader/tinyshader_unity.c

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
	renderer
	thirdparty/rg
	thirdparty/tinyshader
	thirdparty/stb_image
	thirdparty/imgui
	thirdparty/glfw/include
	thirdparty/cgltf
)

set_property(TARGET renderer PROPERTY CXX_STANDARD 17)

if (UNIX)
	target_link_libraries(renderer PUBLIC dl m pthread X11 Xau)
endif(UNIX)

if (WIN32)
	target_link_libraries(renderer PUBLIC gdi32 user32 ole32 oleaut32 advapi32 shlwapi)
endif(WIN32)

add_executable(app app/main.cpp)
target_link_libraries(app PUBLIC renderer)
set_property(TARGET app PROPERTY CXX_STANDARD 17)
