# TestEtna
## Overview
A small vulkan renderer written using the C programming language. This is written for the `c17` standard and `Vulkan 1.3`. The code has only been tested on windows and compiled using the MSVC compiler. The project uses CMake as the build system which compiles the project as well as runs the commands to compile the shaders using glslang at the moment.

## Features
* Window handling done using the [glfw](https://github.com/glfw/glfw) library.
* Shader Reflection data gotten using the [SPIRV-Reflect](https://github.com/KhronosGroup/SPIRV-Reflect) library.
* GLTF & GLB loading with the [cgltf](https://github.com/jkuhlmann/cgltf) GLTF loading library.
* Image loading using [stb_image](https://github.com/nothings/stb/blob/master/stb_image.h).
* Vector & Matrix math done using the [cglm](https://github.com/recp/cglm) math libary.
* Bare bones file reading using stdio.h
* An event system that is currently blocking
* Logger outputting log information to log file
* Basic Scene rendering using forward rendering and blinn-phong lighting with one scene light.
* Depth buffering done using reverse Z.
* Mesh vertices passed using buffer device address.
* Shader compilation to SPV bytecode at project **compile time** using glslang.
* Vulkan 1.3's dynamic rendering feature.
* Multiple frames in flight
## Planned
* Custom material shader support
* Custom file formats for scenes, materials, and meshes
* Serialization support to read in these formats as well as write them out
* Non Blocking event system
* Editor using Immediate Mode UI [Nuklear](https://github.com/vurtun/nuklear)

# Images


## About
I am making this renderer in order to learn about render engines, shaders, and graphics programming in general.
Thank you to [Travis Vroman](https://github.com/travisvroman) for his [Kohi game engine](https://kohiengine.com/) [tutorial series](https://www.youtube.com/playlist?list=PLv8Ddw9K0JPg1BEO-RS-0MYs423cvLVtj) on youtube.
Thank you to [vblanco20-1](https://github.com/vblanco20-1/) for their [vulkan guide](https://vkguide.dev/).
