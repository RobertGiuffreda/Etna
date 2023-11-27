cmake_minimum_required(VERSION 3.28)

#TODO: Set visual studio startup project to sandbox.
#Not needed from vscode tasks as sandbox.exe is run directly.

#TODO: Use variables to choose whether to build tests or not
#^^These would be defined from command line

#TODO: Setup show warnings as errors & other compiler flags

#Set project
project(TestEtna VERSION 1.0.1 DESCRIPTION "Test writing Etna")

#Specify c standard.
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED True)

#HACK: Glob_recurse for now as structure is uncertain.
#Get executable source files:
file(GLOB_RECURSE SANDBOX_SRC_FILES "sandbox/src/*.c")
file(GLOB_RECURSE SANDBOX_HEADER_FILES "sandbox/src/*.h")

#Specify exectuable
add_executable(sandbox ${SANDBOX_SRC_FILES} ${SANDBOX_HEADER_FILES})
#Compiler defines for sandbox
target_compile_definitions(sandbox PRIVATE _DEBUG)

#Include directories for executable
target_include_directories(sandbox PRIVATE ${CMAKE_SOURCE_DIR}/engine/src)

#HACK: Glob_recurse for now as structure is uncertain.
#Get library source files:
file(GLOB_RECURSE ENGINE_SRC_FILES "engine/src/*.c")
file(GLOB_RECURSE ENGINE_HEADER_FILES "engine/src/*.h")

#Create engine
add_library(engine ${ENGINE_SRC_FILES} ${ENGINE_HEADER_FILES})
#Compiler defines for engine
target_compile_definitions(engine PRIVATE _DEBUG)

#Include directories for engine library
target_include_directories(engine
    PRIVATE ${CMAKE_SOURCE_DIR}/engine/src
    PRIVATE ${CMAKE_SOURCE_DIR}/extern/glfw/include)

#Link library to executable
target_link_libraries(sandbox PRIVATE engine)

#Add glfw as dependency to be built alongside project
set(GLFW_BUILD_DOCS False)
set(FLFW_BUILD_EXAMPLES False)
set(FLFW_BUILD_TESTS False)
set(FLFW_VULKAN_STATIC True)
add_subdirectory(${CMAKE_SOURCE_DIR}/extern/glfw)

target_link_libraries(engine PRIVATE glfw)

#Build unit tests
#HACK: Glob_recurse for now as structure is uncertain.
#Get unit test source files:
file(GLOB_RECURSE TEST_SRC_FILES "tests/src/*.c")
file(GLOB_RECURSE TEST_HEADER_FILES "tests/src/*.h")

add_executable(tests ${TEST_SRC_FILES} ${TEST_HEADER_FILES})

target_include_directories(tests PRIVATE ${CMAKE_SOURCE_DIR}/engine/src)
target_link_libraries(tests PRIVATE engine)