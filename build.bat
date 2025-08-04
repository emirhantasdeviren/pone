@echo off

mkdir build
pushd build
clang -Wall -DPONE_INTERNAL=1 -DPONE_DEBUG=1 -g -O0 -c -I..\include -o win32_platform.obj ..\src\platform\win32_platform.cpp
clang -Wall -DPONE_INTERNAL=1 -DPONE_DEBUG=1 -g -O0 -c -I..\include -o pone.obj ..\src\pone.cpp
clang -Wall -DPONE_INTERNAL=1 -DPONE_DEBUG=1 -g -O0 -c -I..\include  -o main.obj ..\src\main.cpp
clang -Wall -DPONE_INTERNAL=1 -DPONE_DEBUG=1 -g -O0 -I..\include -luser32 -lGdi32 -lWinmm -o pone.exe win32_platform.obj pone.obj main.obj
popd
