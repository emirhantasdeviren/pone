@echo off

mkdir build
mkdir build\shaders
pushd build\shaders
glslc.exe ..\..\shaders\gradient.comp -o gradient.spv
glslc.exe ..\..\shaders\sky.comp -o sky.spv
glslc.exe ..\..\shaders\colored_triangle.vert -o colored_triangle.vert.spv
glslc.exe ..\..\shaders\colored_triangle.frag -o colored_triangle.frag.spv
glslc.exe ..\..\shaders\colored_triangle_mesh.vert -o colored_triangle_mesh.vert.spv
popd
