cmake_minimum_required(VERSION 3.21)

project(vdefender VERSION 0.2.0 LANGUAGES C CXX)

# Vector Defender: Williams' Defender (1980, Red Label) for VectorPie. The
# game is the original's program, translated from its 6809 source into C++
# (tools/translate, src/gen); what it draws is drawn again as vectors
# (src/scene, src/vp). See BUILD.md.
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(VD_PI_BUILD)
    # cmake/toolchain-pi-clang.cmake: the cabinet's own SDL2, by its file.
    add_library(SDL2::SDL2 SHARED IMPORTED GLOBAL)
    set_target_properties(SDL2::SDL2 PROPERTIES
        IMPORTED_LOCATION "${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu/libSDL2-2.0.so.0"
        IMPORTED_SONAME "libSDL2-2.0.so.0"
        INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SYSROOT}/usr/include/SDL2"
        INTERFACE_COMPILE_DEFINITIONS "_REENTRANT")
    set(THREADS_PREFER_PTHREAD_FLAG ON)
endif()

# --- The game: the translated program, the board around it, the scene ------
add_library(dfgame STATIC
    src/game/machine.cpp
    src/game/board.cpp
    src/game/sound.cpp
    src/gen/sound_code.cpp
    src/gen/defender_code.cpp
    src/scene/scene.cpp
    src/scene/designs.cpp
    src/scene/logos.cpp)
target_include_directories(dfgame PUBLIC src)
# The translation is one statement block per instruction with gotos between
# them: fall-through and unused labels are its nature.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(src/gen/defender_code.cpp src/gen/sound_code.cpp PROPERTIES
        COMPILE_OPTIONS "-Wno-unused-label;-Wno-implicit-fallthrough;-Wno-unused-variable")
endif()

# --- The VectorPie program -------------------------------------------------
set(VP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(VP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(vendor/VectorPie/sdk vectorpie)

file(GLOB VD_SOURCES CONFIGURE_DEPENDS src/vp/*.cpp src/vp/*.c)
add_executable(vdefender ${VD_SOURCES})
target_link_libraries(vdefender PRIVATE dfgame VectorPie::vectorpie)
vp_program(vdefender)
add_custom_command(TARGET vdefender POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/package/game.ini
            $<TARGET_FILE_DIR:vdefender>/game.ini)

# Headless test runner (PC only): tools/defrun, see BUILD.md.
if(NOT VD_PI_BUILD)
    # with the 3D view, drawn through the SDK's vp3d into a stand-in board
    set(VP_SDK ${CMAKE_CURRENT_SOURCE_DIR}/vendor/VectorPie/sdk)
    add_executable(defrun tools/defrun/defrun.cpp tools/defrun/dvg_stub.cpp src/vp/view3d.cpp
        ${VP_SDK}/src/3d/vp_3d.cpp ${VP_SDK}/src/3d/vp_texture.cpp ${VP_SDK}/src/core/vp_error.cpp)
    target_include_directories(defrun PRIVATE src ${VP_SDK}/include ${VP_SDK}/src)
    target_link_libraries(defrun PRIVATE dfgame)
endif()
