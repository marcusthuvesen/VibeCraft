cmake_minimum_required(VERSION 3.28)

project(VibeCraft VERSION 0.1.0 LANGUAGES C CXX)

if(APPLE)
    enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

include(CTest)
include(cmake/Dependencies.cmake)

set(VIBECRAFT_HEADERS
    include/vibecraft/app/Application.hpp
    include/vibecraft/core/Logger.hpp
    include/vibecraft/game/Camera.hpp
    include/vibecraft/meshing/ChunkMesher.hpp
    include/vibecraft/platform/InputState.hpp
    include/vibecraft/platform/Window.hpp
    include/vibecraft/render/Renderer.hpp
    include/vibecraft/world/Block.hpp
    include/vibecraft/world/Chunk.hpp
    include/vibecraft/world/TerrainGenerator.hpp
    include/vibecraft/world/World.hpp
    include/vibecraft/world/WorldEditCommand.hpp
    include/vibecraft/world/WorldSerializer.hpp
)

set(VIBECRAFT_SOURCES
    src/app/Application.cpp
    src/app/main.cpp
    src/core/Logger.cpp
    src/game/Camera.cpp
    src/meshing/ChunkMesher.cpp
    src/platform/Window.cpp
    src/render/Renderer.cpp
    src/world/Chunk.cpp
    src/world/TerrainGenerator.cpp
    src/world/World.cpp
    src/world/WorldSerializer.cpp
)

add_executable(vibecraft
    ${VIBECRAFT_HEADERS}
    ${VIBECRAFT_SOURCES}
)

target_include_directories(vibecraft
    PRIVATE
        include
)

target_link_libraries(vibecraft
    PRIVATE
        SDL3::SDL3
        bgfx
        bx
        bimg
        glm
        fmt::fmt
)

if(MSVC)
    target_compile_options(vibecraft PRIVATE /W4 /permissive-)
else()
    target_compile_options(vibecraft PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(vibecraft_tests
    tests/world_smoke_tests.cpp
    src/meshing/ChunkMesher.cpp
    src/world/Chunk.cpp
    src/world/TerrainGenerator.cpp
    src/world/World.cpp
    src/world/WorldSerializer.cpp
    src/core/Logger.cpp
)

target_include_directories(vibecraft_tests
    PRIVATE
        include
)

target_link_libraries(vibecraft_tests
    PRIVATE
        doctest::doctest
        glm
        fmt::fmt
)

add_test(NAME vibecraft_tests COMMAND vibecraft_tests)
