cmake_minimum_required(VERSION 3.20)
project(VoxelFrontier VERSION 2.0 LANGUAGES C CXX)

# GLAD 0.1.36 predates CMake 4.x; retain its intended policy behavior.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG 3.4 GIT_SHALLOW TRUE)
FetchContent_Declare(glad GIT_REPOSITORY https://github.com/Dav1dde/glad.git GIT_TAG v0.1.36 GIT_SHALLOW TRUE)
FetchContent_Declare(glm GIT_REPOSITORY https://github.com/g-truc/glm.git GIT_TAG 1.0.1 GIT_SHALLOW TRUE)
FetchContent_Declare(miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG 0.11.25
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR cmake-no-targets)
FetchContent_MakeAvailable(glfw glad glm miniaudio)

find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

add_executable(VoxelFrontier
    src/main.cpp
    src/Game.cpp
    src/InputManager.cpp
    src/Screenshot.cpp
    src/UIManager.cpp
    src/Noise.cpp
    src/Definitions.cpp
    src/SaveFile.cpp
    src/Farming.cpp
    src/InventoryUI.cpp
    src/Player.cpp
    src/Renderer.cpp
    src/Settings.cpp
    src/Sound.cpp
    src/Survival.cpp
    src/Structure.cpp
    src/CanyonCarver.cpp
    src/World.cpp
)
target_include_directories(VoxelFrontier PRIVATE include ${miniaudio_SOURCE_DIR})
target_link_libraries(VoxelFrontier PRIVATE glfw glad glm::glm OpenGL::GL Threads::Threads)

if(MSVC)
    target_compile_options(VoxelFrontier PRIVATE /W4 /permissive-)
else()
    target_compile_options(VoxelFrontier PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT VoxelFrontier)


add_custom_command(TARGET VoxelFrontier POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:VoxelFrontier>/assets)
add_custom_command(TARGET VoxelFrontier POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/sounds
            $<TARGET_FILE_DIR:VoxelFrontier>/sounds)
