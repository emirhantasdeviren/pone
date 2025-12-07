#!/bin/bash

PONE_ROOT_DIR="${1:-$(pwd)}"
PONE_SRC_DIR="$PONE_ROOT_DIR/src"
PONE_INCLUDE_DIR="$PONE_ROOT_DIR/include"
PONE_BUILD_DIR="$PONE_ROOT_DIR/build"

CFLAGS="-Wall -Wno-writable-strings -g -O0 -c -I$PONE_INCLUDE_DIR"
LDFLAGS="-lm -lwayland-client -lrt"

add_object_file() {
    local file_name=$1
    local ext=${2:-"cpp"}
    
    if [ -n $file_name ]; then
        echo "Building object file $file_name.$ext"
        clang $CFLAGS -o $PONE_BUILD_DIR/$file_name.o $PONE_SRC_DIR/$file_name.$ext
    fi
}

mkdir -p $PONE_BUILD_DIR

echo "Building object file pone_platform.o"
clang $CFLAGS -o $PONE_BUILD_DIR/pone_platform.o $PONE_SRC_DIR/pone_platform_linux.cpp
add_object_file "pone_memory"
add_object_file "pone_arena"
add_object_file "pone_string"
add_object_file "main"
add_object_file "pone_json"
add_object_file "pone_gltf"
add_object_file "pone_vulkan"
add_object_file "pone_truetype"
add_object_file "pone_math"
add_object_file "pone_vec2"
add_object_file "pone_rect"
# add_object_file "pone_atomic"
add_object_file "pone_work_queue"
add_object_file "pone_rect_pack"
add_object_file "xdg-shell-protocol" "c"

clang $LDFLAGS -o $PONE_BUILD_DIR/pone \
    $PONE_BUILD_DIR/pone_platform.o \
    $PONE_BUILD_DIR/pone_memory.o \
    $PONE_BUILD_DIR/pone_arena.o \
    $PONE_BUILD_DIR/pone_string.o \
    $PONE_BUILD_DIR/pone_json.o \
    $PONE_BUILD_DIR/pone_gltf.o \
    $PONE_BUILD_DIR/pone_vulkan.o \
    $PONE_BUILD_DIR/pone_truetype.o \
    $PONE_BUILD_DIR/pone_math.o \
    $PONE_BUILD_DIR/pone_vec2.o \
    $PONE_BUILD_DIR/pone_rect.o \
    $PONE_BUILD_DIR/pone_work_queue.o \
    $PONE_BUILD_DIR/pone_rect_pack.o \
    $PONE_BUILD_DIR/xdg-shell-protocol.o \
    $PONE_BUILD_DIR/main.o
