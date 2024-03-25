# TestEtna
## Overview
A small `vulkan` renderer written using the `C` programming language. This is written for the `c17` standard and `Vulkan 1.3`. The code has only been tested on windows and compiled using the MSVC compiler. The project uses CMake as the build system which compiles the project as well as runs the commands to compile the shaders using glslang at the moment.

## Features
* Window handling done using the [glfw](https://github.com/glfw/glfw) library.
* Shader Reflection data retrieved using the [SPIRV-Reflect](https://github.com/KhronosGroup/SPIRV-Reflect) library.
* GLTF & GLB loading with the [cgltf](https://github.com/jkuhlmann/cgltf) GLTF loading library.
* Image loading using [stb_image](https://github.com/nothings/stb/blob/master/stb_image.h).
* Vector & Matrix math done using the [cglm](https://github.com/recp/cglm) math libary.
* Bare bones file reading using stdio.h
* Logging to text file
* Scene rendering using simple Physically Based Rendering, Cook-Torrence BRDF.
* Depth buffering done using reverse-Z.
* Shader compilation to SPV bytecode at project **compile time** using [glslang](https://github.com/KhronosGroup/glslang).
* `Vulkan 1.3`'s dynamic rendering feature.
* vkCmdDrawIndirectCount used alongside descriptor indexing to render the scene with only as many draw calls as pipeline shader objects.
* Multiple frames in flight.
## Planned
* Custom file formats for scenes, materials, and meshes
* Serialization support to read in these formats as well as write them out
* Non Blocking event system
* Editor using C bindings for ImGui [cimgui](https://github.com/cimgui/cimgui/tree/docking_inter)

## Screenshots
![Screenshot 1](https://github.com/RobertGiuffreda/TestEtna/blob/main/demonstration/EtnaScreenshot.png)

## About
I am making this renderer in order to learn about render engines, shaders, and graphics programming in general.
Thank you to [Travis Vroman](https://github.com/travisvroman) for his [Kohi game engine](https://kohiengine.com/) [tutorial series](https://www.youtube.com/playlist?list=PLv8Ddw9K0JPg1BEO-RS-0MYs423cvLVtj) on youtube.
Thank you to [vblanco20-1](https://github.com/vblanco20-1/) for their [vulkan guide](https://vkguide.dev/).
