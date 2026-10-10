cmake_minimum_required(VERSION 3.20)
project(VoxelFrontier VERSION 2.11.0 LANGUAGES C CXX)

option(VOXEL_STANDALONE "Embed runtime assets and link the Windows runtime statically" OFF)
option(VOXELFRONTIER_STANDALONE_DEMO "Build the stateless standalone demo" OFF)
if(VOXELFRONTIER_STANDALONE_DEMO AND NOT VOXEL_STANDALONE)
    message(FATAL_ERROR "VOXELFRONTIER_STANDALONE_DEMO requires VOXEL_STANDALONE")
endif()
if(VOXEL_STANDALONE)
    if(NOT WIN32 OR NOT MSVC)
        message(FATAL_ERROR "VOXEL_STANDALONE requires an MSVC Windows build")
    endif()
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

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
    src/ChatUI.cpp
    src/CommandSystem.cpp
    src/ContentValidation.cpp
    src/AmbientOcclusion.cpp
    src/Screenshot.cpp
    src/UIManager.cpp
    src/WorldLibrary.cpp
    src/Biome.cpp
    src/BiomeVegetation.cpp
    src/Noise.cpp
    src/Definitions.cpp
    src/Explosion.cpp
    src/SaveFile.cpp
    src/Weather.cpp
    src/WeatherRenderer.cpp
    src/WeatherValidation.cpp
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
    src/WorldBiomeValidation.cpp
    src/WorldValidation.cpp
)
target_include_directories(VoxelFrontier PRIVATE include ${miniaudio_SOURCE_DIR})
target_compile_definitions(VoxelFrontier PRIVATE VOXELFRONTIER_VERSION="${PROJECT_VERSION}")
target_link_libraries(VoxelFrontier PRIVATE glfw glad glm::glm OpenGL::GL Threads::Threads)

if(WIN32)
    target_link_libraries(VoxelFrontier PRIVATE windowscodecs ole32 psapi)
    set(VOXEL_RESOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/embedded")
    file(MAKE_DIRECTORY "${VOXEL_RESOURCE_DIR}")
    file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/packaging/VoxelFrontier.ico" VOXEL_ICON_RC_PATH)
    configure_file(packaging/VoxelVersion.rc.in
                   "${VOXEL_RESOURCE_DIR}/VoxelVersion.rc" @ONLY)
    target_sources(VoxelFrontier PRIVATE "${VOXEL_RESOURCE_DIR}/VoxelVersion.rc")
endif()

file(GLOB VOXEL_SPRITE_FILES CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/sprites" "${CMAKE_CURRENT_SOURCE_DIR}/sprites/*.png")
list(SORT VOXEL_SPRITE_FILES)
list(LENGTH VOXEL_SPRITE_FILES VOXEL_SPRITE_COUNT)
if(NOT VOXEL_SPRITE_COUNT EQUAL 4)
    message(FATAL_ERROR "Exactly four hostile mob PNG sprites are required")
endif()
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/embedded")
set(VOXEL_SPRITE_MANIFEST "${CMAKE_CURRENT_BINARY_DIR}/embedded/SpriteManifest.h")
file(WRITE "${VOXEL_SPRITE_MANIFEST}" "#pragma once\nstruct SpriteAsset { const char* name; int id; };\ninline constexpr SpriteAsset SpriteAssets[] = {\n")
set(VOXEL_SPRITE_ID 2000)
foreach(VOXEL_SPRITE IN LISTS VOXEL_SPRITE_FILES)
    file(APPEND "${VOXEL_SPRITE_MANIFEST}" "    {\"${VOXEL_SPRITE}\", ${VOXEL_SPRITE_ID}},\n")
    math(EXPR VOXEL_SPRITE_ID "${VOXEL_SPRITE_ID} + 1")
endforeach()
file(APPEND "${VOXEL_SPRITE_MANIFEST}" "};\n")
target_include_directories(VoxelFrontier PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/embedded")

if(VOXEL_STANDALONE)
    target_sources(VoxelFrontier PRIVATE src/UserData.cpp)
    target_link_libraries(VoxelFrontier PRIVATE shell32 ole32)
    if(MSVC)
        # Keep main(argc, argv) while avoiding a console in distribution builds.
        target_link_options(VoxelFrontier PRIVATE
            "$<$<CONFIG:Release>:/SUBSYSTEM:WINDOWS>"
            "$<$<CONFIG:Release>:/ENTRY:mainCRTStartup>")
        target_compile_definitions(VoxelFrontier PRIVATE
            "$<$<CONFIG:Release>:VOXEL_GUI_RELEASE>")
    endif()
    file(GLOB_RECURSE VOXEL_SOUND_FILES CONFIGURE_DEPENDS
        RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/sounds" "${CMAKE_CURRENT_SOURCE_DIR}/sounds/*.ogg")
    list(SORT VOXEL_SOUND_FILES)
    set(VOXEL_RC "${VOXEL_RESOURCE_DIR}/VoxelResources.rc")
    set(VOXEL_HEADER "${VOXEL_RESOURCE_DIR}/EmbeddedResources.h")
    file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/assets/item_icons_expansion.rgba" VOXEL_ATLAS_PATH)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/item_icons_expansion.rgba")
    file(WRITE "${VOXEL_RC}" "100 RCDATA \"${VOXEL_ATLAS_PATH}\"\n")
    file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/assets/crafting_blocks.rgba" VOXEL_BLOCK_ATLAS_PATH)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/crafting_blocks.rgba")
    file(APPEND "${VOXEL_RC}" "101 RCDATA \"${VOXEL_BLOCK_ATLAS_PATH}\"\n")
    file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/sprites/panorama/panorama.jpg" VOXEL_PANORAMA_PATH)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/sprites/panorama/panorama.jpg")
    file(APPEND "${VOXEL_RC}" "102 RCDATA \"${VOXEL_PANORAMA_PATH}\"\n")
    file(WRITE "${VOXEL_HEADER}" "#pragma once\nstruct EmbeddedSound { const char* name; int id; };\ninline constexpr int EmbeddedItemAtlasId = 100;\ninline constexpr EmbeddedSound EmbeddedSounds[] = {\n")
    set(VOXEL_SOUND_ID 1000)
    foreach(VOXEL_SOUND IN LISTS VOXEL_SOUND_FILES)
        file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/sounds/${VOXEL_SOUND}" VOXEL_SOUND_PATH)
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/sounds/${VOXEL_SOUND}")
        file(APPEND "${VOXEL_RC}" "${VOXEL_SOUND_ID} RCDATA \"${VOXEL_SOUND_PATH}\"\n")
        file(APPEND "${VOXEL_HEADER}" "    {\"${VOXEL_SOUND}\", ${VOXEL_SOUND_ID}},\n")
        math(EXPR VOXEL_SOUND_ID "${VOXEL_SOUND_ID} + 1")
    endforeach()
    file(APPEND "${VOXEL_HEADER}" "};\n")
    set(VOXEL_SPRITE_ID 2000)
    foreach(VOXEL_SPRITE IN LISTS VOXEL_SPRITE_FILES)
        file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/sprites/${VOXEL_SPRITE}" VOXEL_SPRITE_PATH)
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/sprites/${VOXEL_SPRITE}")
        file(APPEND "${VOXEL_RC}" "${VOXEL_SPRITE_ID} RCDATA \"${VOXEL_SPRITE_PATH}\"\n")
        math(EXPR VOXEL_SPRITE_ID "${VOXEL_SPRITE_ID} + 1")
    endforeach()
    target_sources(VoxelFrontier PRIVATE "${VOXEL_RC}")
    target_include_directories(VoxelFrontier PRIVATE "${VOXEL_RESOURCE_DIR}")
    target_compile_definitions(VoxelFrontier PRIVATE VOXEL_STANDALONE)
    if(VOXELFRONTIER_STANDALONE_DEMO)
        target_compile_definitions(VoxelFrontier PRIVATE VOXELFRONTIER_STANDALONE_DEMO)
    endif()
endif()

if(MSVC)
    target_compile_options(VoxelFrontier PRIVATE /W4 /permissive-)
else()
    target_compile_options(VoxelFrontier PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT VoxelFrontier)


if(NOT VOXEL_STANDALONE)
    add_custom_command(TARGET VoxelFrontier POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/assets
                $<TARGET_FILE_DIR:VoxelFrontier>/assets)
    add_custom_command(TARGET VoxelFrontier POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/sounds
                $<TARGET_FILE_DIR:VoxelFrontier>/sounds)
    add_custom_command(TARGET VoxelFrontier POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/sprites
                $<TARGET_FILE_DIR:VoxelFrontier>/sprites)
endif()
