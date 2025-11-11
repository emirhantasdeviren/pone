@echo off

mkdir build
pushd build
clang -Wall -Wno-writable-strings -g -O0 -c -I..\include  -o main.obj ..\src\main.cpp
clang -Wall -Wno-writable-strings -g -O0 -c -I..\include  -o pone_arena.obj ..\src\pone_arena.cpp
clang -Wall -Wno-writable-strings -g -O0 -c -I..\include  -o pone_json.obj ..\src\pone_json.cpp
clang -Wall -Wno-writable-strings -g -O0 -c -I..\include  -o pone_memory.obj ..\src\pone_memory.cpp
clang -Wall -Wno-writable-strings -g -O0 -c -I..\include  -o pone_string.obj ..\src\pone_string.cpp
clang -Wall -Wno-writable-strings -g -O0 -c -I..\include  -o pone_gltf.obj ..\src\pone_gltf.cpp
clang -Wall -Wno-writable-strings -g -O0 -c -I..\include  -o pone_vulkan.obj ..\src\pone_vulkan.cpp
clang -Wall -Wno-writable-strings -g -O0 -c -I..\include  -o pone_truetype.obj ..\src\pone_truetype.cpp
clang -Wall -Wno-writable-strings -g -O0 -c -I..\include  -o pone_math.obj ..\src\pone_math.cpp
clang -Wall -Wno-writable-strings -g -O0 -c -I..\include  -o pone_vec2.obj ..\src\pone_vec2.cpp
clang -Wall -Wno-writable-strings -g -O0 -c -I..\include  -o pone_rect.obj ..\src\pone_rect.cpp
clang -Wall -Wno-writable-strings -g -O0 -c -I..\include  -o pone_atomic.obj ..\src\pone_atomic.cpp
clang -Wall -Wno-writable-strings -g -O0 -c -I..\include  -o pone_work_queue.obj ..\src\pone_work_queue.cpp
REM clang -Wall -g -O0 -c -I..\include -o imgui.obj ..\src\imgui.cpp
REM clang -Wall -g -O0 -c -I..\include -o imgui_demo.obj ..\src\imgui_demo.cpp
REM clang -Wall -g -O0 -c -I..\include -o imgui_draw.obj ..\src\imgui_draw.cpp
REM clang -Wall -g -O0 -c -I..\include -o imgui_tables.obj ..\src\imgui_tables.cpp
REM clang -Wall -g -O0 -c -I..\include -o imgui_widgets.obj ..\src\imgui_widgets.cpp
REM clang -Wall -g -O0 -c -I..\include -DIMGUI_IMPL_VULKAN_NO_PROTOTYPES -o imgui_impl_vulkan.obj ..\src\imgui_impl_vulkan.cpp
REM clang -Wall -g -O0 -c -I..\include -o imgui_impl_win32.obj ..\src\imgui_impl_win32.cpp
clang -Wall -Wno-writable-strings -g -O0 -luser32 -lGdi32 -lWinmm -lSynchronization -o vkguide.exe imgui.obj imgui_demo.obj imgui_draw.obj imgui_tables.obj imgui_widgets.obj imgui_impl_vulkan.obj imgui_impl_win32.obj pone_arena.obj pone_json.obj pone_memory.obj pone_string.obj pone_gltf.obj pone_vulkan.obj pone_truetype.obj pone_math.obj pone_vec2.obj pone_rect.obj pone_atomic.obj pone_work_queue.obj main.obj
popd
