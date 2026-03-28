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

set(BGFX_EXAMPLES_COMMON_DIR "${bgfx_cmake_SOURCE_DIR}/bgfx/examples/common")
set(VIBECRAFT_SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders")
set(VIBECRAFT_SHADER_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders")

function(vibecraft_compile_shader OUT_VAR INPUT_FILE SHADER_TYPE PLATFORM_NAME PROFILE_NAME OUTPUT_SUBDIR)
    get_filename_component(SHADER_BASENAME "${INPUT_FILE}" NAME_WE)
    set(OUTPUT_FILE "${VIBECRAFT_SHADER_OUTPUT_DIR}/${OUTPUT_SUBDIR}/${SHADER_BASENAME}.bin")

    add_custom_command(
        OUTPUT "${OUTPUT_FILE}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${VIBECRAFT_SHADER_OUTPUT_DIR}/${OUTPUT_SUBDIR}"
        COMMAND $<TARGET_FILE:bgfx::shaderc>
            --platform "${PLATFORM_NAME}"
            --type "${SHADER_TYPE}"
            -p "${PROFILE_NAME}"
            -O 0
            --varyingdef "${VIBECRAFT_SHADER_DIR}/varying.def.sc"
            -i "${bgfx_cmake_SOURCE_DIR}/bgfx/src"
            -i "${VIBECRAFT_SHADER_DIR}"
            -f "${INPUT_FILE}"
            -o "${OUTPUT_FILE}"
        MAIN_DEPENDENCY "${INPUT_FILE}"
        DEPENDS "${VIBECRAFT_SHADER_DIR}/varying.def.sc" bgfx::shaderc
        VERBATIM
    )

    set(${OUT_VAR} "${OUTPUT_FILE}" PARENT_SCOPE)
endfunction()

set(VIBECRAFT_SHADER_OUTPUTS "")

if(APPLE)
    vibecraft_compile_shader(VS_CHUNK_METAL "${VIBECRAFT_SHADER_DIR}/vs_chunk.sc" vertex osx metal metal)
    vibecraft_compile_shader(FS_CHUNK_METAL "${VIBECRAFT_SHADER_DIR}/fs_chunk.sc" fragment osx metal metal)
    list(APPEND VIBECRAFT_SHADER_OUTPUTS "${VS_CHUNK_METAL}" "${FS_CHUNK_METAL}")
endif()

if(WIN32)
    vibecraft_compile_shader(VS_CHUNK_DXBC "${VIBECRAFT_SHADER_DIR}/vs_chunk.sc" vertex windows s_5_0 dxbc)
    vibecraft_compile_shader(FS_CHUNK_DXBC "${VIBECRAFT_SHADER_DIR}/fs_chunk.sc" fragment windows s_5_0 dxbc)
    vibecraft_compile_shader(VS_CHUNK_DXIL "${VIBECRAFT_SHADER_DIR}/vs_chunk.sc" vertex windows s_6_0 dxil)
    vibecraft_compile_shader(FS_CHUNK_DXIL "${VIBECRAFT_SHADER_DIR}/fs_chunk.sc" fragment windows s_6_0 dxil)
    list(APPEND VIBECRAFT_SHADER_OUTPUTS "${VS_CHUNK_DXBC}" "${FS_CHUNK_DXBC}" "${VS_CHUNK_DXIL}" "${FS_CHUNK_DXIL}")
endif()

if(UNIX AND NOT APPLE)
    vibecraft_compile_shader(VS_CHUNK_GLSL "${VIBECRAFT_SHADER_DIR}/vs_chunk.sc" vertex linux 120 glsl)
    vibecraft_compile_shader(FS_CHUNK_GLSL "${VIBECRAFT_SHADER_DIR}/fs_chunk.sc" fragment linux 120 glsl)
    list(APPEND VIBECRAFT_SHADER_OUTPUTS "${VS_CHUNK_GLSL}" "${FS_CHUNK_GLSL}")
endif()

vibecraft_compile_shader(VS_CHUNK_SPIRV "${VIBECRAFT_SHADER_DIR}/vs_chunk.sc" vertex linux spirv spirv)
vibecraft_compile_shader(FS_CHUNK_SPIRV "${VIBECRAFT_SHADER_DIR}/fs_chunk.sc" fragment linux spirv spirv)
vibecraft_compile_shader(VS_CHUNK_WGSL "${VIBECRAFT_SHADER_DIR}/vs_chunk.sc" vertex linux wgsl wgsl)
vibecraft_compile_shader(FS_CHUNK_WGSL "${VIBECRAFT_SHADER_DIR}/fs_chunk.sc" fragment linux wgsl wgsl)
list(APPEND VIBECRAFT_SHADER_OUTPUTS "${VS_CHUNK_SPIRV}" "${FS_CHUNK_SPIRV}" "${VS_CHUNK_WGSL}" "${FS_CHUNK_WGSL}")

add_custom_target(
    vibecraft_shaders
    DEPENDS
        ${VIBECRAFT_SHADER_OUTPUTS}
)

set(VIBECRAFT_HEADERS
    include/vibecraft/app/Application.hpp
    include/vibecraft/app/Inventory.hpp
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
    src/app/Inventory.cpp
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
    ${BGFX_EXAMPLES_COMMON_DIR}/debugdraw/debugdraw.cpp
)

add_executable(vibecraft
    ${VIBECRAFT_HEADERS}
    ${VIBECRAFT_SOURCES}
)

add_dependencies(vibecraft vibecraft_shaders)

if(APPLE)
    set_target_properties(vibecraft PROPERTIES BUILD_RPATH "@loader_path")
elseif(UNIX)
    set_target_properties(vibecraft PROPERTIES BUILD_RPATH "$ORIGIN")
endif()

target_include_directories(vibecraft
    PRIVATE
        include
        ${BGFX_EXAMPLES_COMMON_DIR}
        ${BGFX_EXAMPLES_COMMON_DIR}/debugdraw
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

add_custom_command(
    TARGET vibecraft
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:vibecraft>
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/textures"
        "$<TARGET_FILE_DIR:vibecraft>/textures"
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
