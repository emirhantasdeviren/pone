@echo off

mkdir build
mkdir build\shaders
pushd build\shaders
C:\VulkanSDK\1.4.328.1\Bin\glslc.exe ..\..\shaders\gradient.comp -o gradient.spv
C:\VulkanSDK\1.4.328.1\Bin\glslc.exe ..\..\shaders\sky.comp -o sky.spv
C:\VulkanSDK\1.4.328.1\Bin\glslc.exe ..\..\shaders\colored_triangle.vert -o colored_triangle.vert.spv
C:\VulkanSDK\1.4.328.1\Bin\glslc.exe ..\..\shaders\colored_triangle.frag -o colored_triangle.frag.spv
C:\VulkanSDK\1.4.328.1\Bin\glslc.exe ..\..\shaders\colored_triangle_mesh.vert -o colored_triangle_mesh.vert.spv
popd
