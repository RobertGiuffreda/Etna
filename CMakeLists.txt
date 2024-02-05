cmake_minimum_required(VERSION 3.21)

#Set project
project(TestEtna VERSION 1.0.1 DESCRIPTION "Test writing Etna")

#Specify c standard.
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED True)

#Compile shaders and add them to the build directory
set(SHADER_COMPILER $ENV{VULKAN_SDK}/Bin/glslangValidator.exe)

file(GLOB_RECURSE SHADER_SRC_FILES "assets/shaders/*.vert" "assets/shaders/*.frag" "assets/shaders/*.comp")
file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/assets/shaders/)

foreach(SHADER_FILE ${SHADER_SRC_FILES})
    get_filename_component(SHADER_FILE_NAME ${SHADER_FILE} NAME)
    set(SPV_FILE ${PROJECT_BINARY_DIR}/assets/shaders/${SHADER_FILE_NAME}.spv)
    execute_process(COMMAND ${SHADER_COMPILER} -g ${SHADER_FILE} -o ${SPV_FILE} --target-env vulkan1.3)
endforeach(SHADER_FILE)

# Create obj folder in build folder
set(GLTF_DIRECTORY ${PROJECT_BINARY_DIR}/assets/gltf)
file(MAKE_DIRECTORY ${GLTF_DIRECTORY})

# Copy over the gltf files
configure_file(${CMAKE_SOURCE_DIR}/assets/gltf/structure.glb ${GLTF_DIRECTORY}/structure.glb COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/assets/gltf/house2.glb ${GLTF_DIRECTORY}/house2.glb COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/assets/gltf/zda_test.glb ${GLTF_DIRECTORY}/zda_test.glb COPYONLY)

#NOTE: External dependencies
#Add glfw as a dependency to be built alongside the engine
set(GLFW_BUILD_DOCS False)
set(GLFW_BUILD_EXAMPLES False)
set(GLFW_BUILD_TESTS False)
add_subdirectory(${CMAKE_SOURCE_DIR}/extern/glfw)

#Add spirv-reflect as dependency to be built alongside the engine
set(SPIRV_REFLECT_EXECUTABLE OFF)
set(SPIRV_REFLECT_STATIC_LIB ON)
set(SPIRV_REFLECT_BUILD_TESTS OFF)
add_subdirectory(${CMAKE_SOURCE_DIR}/extern/SPIRV-Reflect)

#HACK: Glob_recurse for now as structure is uncertain.
#Get library source files:
file(GLOB_RECURSE ENGINE_SRC_FILES "engine/src/*.c")
file(GLOB_RECURSE ENGINE_HEADER_FILES "engine/src/*.h")

#Create engine
add_library(engine ${ENGINE_SRC_FILES} ${ENGINE_HEADER_FILES})

#Compiler defines for engine
target_compile_definitions(engine PRIVATE _DEBUG)
# target_compile_options(engine PRIVATE /fsanitize=address)
# target_link_options(engine PRIVATE /INFERASANLIBS)

#Include directories for engine library
target_include_directories(engine
    PRIVATE ${CMAKE_SOURCE_DIR}/engine/src/
    PRIVATE ${CMAKE_SOURCE_DIR}/extern/glfw/include/
    PRIVATE ${CMAKE_SOURCE_DIR}/extern/cglm/include/
    PRIVATE ${CMAKE_SOURCE_DIR}/extern/SPIRV-Reflect/
    PRIVATE ${CMAKE_SOURCE_DIR}/extern/stb_image/
    PRIVATE ${CMAKE_SOURCE_DIR}/extern/cgltf/
    PRIVATE ${CMAKE_SOURCE_DIR}/extern/Nuklear/
    PRIVATE $ENV{VULKAN_SDK}/include/)

# Link directories for engine library
target_link_directories(engine PRIVATE $ENV{VULKAN_SDK}/Lib)

# Link libraries to engine
target_link_libraries(engine
    PRIVATE vulkan-1
    PRIVATE glfw
    PRIVATE spirv-reflect-static
)

#HACK: Glob_recurse for now as structure is uncertain.
#Get executable source files:
file(GLOB_RECURSE SANDBOX_SRC_FILES "sandbox/src/*.c")
file(GLOB_RECURSE SANDBOX_HEADER_FILES "sandbox/src/*.h")

#Specify executable
add_executable(sandbox ${SANDBOX_SRC_FILES} ${SANDBOX_HEADER_FILES})

# Compiler defines for sandbox
target_compile_definitions(sandbox PRIVATE _DEBUG)
# target_compile_options(sandbox PRIVATE /fsanitize=address)
# target_link_options(sandbox PRIVATE /INFERASANLIBS)

#Include directories for sandbox
target_include_directories(sandbox
    PRIVATE ${CMAKE_SOURCE_DIR}/engine/src
    PRIVATE ${CMAKE_SOURCE_DIR}/extern/cglm/include)

#Link libraries to sandbox
target_link_directories(sandbox PRIVATE $ENV{VULKAN_SDK}/Lib)
target_link_libraries(sandbox
    PRIVATE engine
    PRIVATE vulkan-1
)