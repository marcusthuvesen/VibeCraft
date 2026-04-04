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
    vibecraft_compile_shader(FS_LOGO_METAL "${VIBECRAFT_SHADER_DIR}/fs_logo.sc" fragment osx metal metal)
    vibecraft_compile_shader(FS_UI_METAL "${VIBECRAFT_SHADER_DIR}/fs_ui.sc" fragment osx metal metal)
    vibecraft_compile_shader(FS_UI_SOLID_METAL "${VIBECRAFT_SHADER_DIR}/fs_ui_solid.sc" fragment osx metal metal)
    list(
        APPEND
        VIBECRAFT_SHADER_OUTPUTS
        "${VS_CHUNK_METAL}"
        "${FS_CHUNK_METAL}"
        "${FS_LOGO_METAL}"
        "${FS_UI_METAL}"
        "${FS_UI_SOLID_METAL}")
endif()

if(WIN32)
    vibecraft_compile_shader(VS_CHUNK_DXBC "${VIBECRAFT_SHADER_DIR}/vs_chunk.sc" vertex windows s_5_0 dxbc)
    vibecraft_compile_shader(FS_CHUNK_DXBC "${VIBECRAFT_SHADER_DIR}/fs_chunk.sc" fragment windows s_5_0 dxbc)
    vibecraft_compile_shader(FS_LOGO_DXBC "${VIBECRAFT_SHADER_DIR}/fs_logo.sc" fragment windows s_5_0 dxbc)
    vibecraft_compile_shader(FS_UI_DXBC "${VIBECRAFT_SHADER_DIR}/fs_ui.sc" fragment windows s_5_0 dxbc)
    vibecraft_compile_shader(FS_UI_SOLID_DXBC "${VIBECRAFT_SHADER_DIR}/fs_ui_solid.sc" fragment windows s_5_0 dxbc)
    vibecraft_compile_shader(VS_CHUNK_DXIL "${VIBECRAFT_SHADER_DIR}/vs_chunk.sc" vertex windows s_6_0 dxil)
    vibecraft_compile_shader(FS_CHUNK_DXIL "${VIBECRAFT_SHADER_DIR}/fs_chunk.sc" fragment windows s_6_0 dxil)
    vibecraft_compile_shader(FS_LOGO_DXIL "${VIBECRAFT_SHADER_DIR}/fs_logo.sc" fragment windows s_6_0 dxil)
    vibecraft_compile_shader(FS_UI_DXIL "${VIBECRAFT_SHADER_DIR}/fs_ui.sc" fragment windows s_6_0 dxil)
    vibecraft_compile_shader(FS_UI_SOLID_DXIL "${VIBECRAFT_SHADER_DIR}/fs_ui_solid.sc" fragment windows s_6_0 dxil)
    list(
        APPEND
        VIBECRAFT_SHADER_OUTPUTS
        "${VS_CHUNK_DXBC}"
        "${FS_CHUNK_DXBC}"
        "${FS_LOGO_DXBC}"
        "${FS_UI_DXBC}"
        "${FS_UI_SOLID_DXBC}"
        "${VS_CHUNK_DXIL}"
        "${FS_CHUNK_DXIL}"
        "${FS_LOGO_DXIL}"
        "${FS_UI_DXIL}"
        "${FS_UI_SOLID_DXIL}")
endif()

if(UNIX AND NOT APPLE)
    vibecraft_compile_shader(VS_CHUNK_GLSL "${VIBECRAFT_SHADER_DIR}/vs_chunk.sc" vertex linux 120 glsl)
    vibecraft_compile_shader(FS_CHUNK_GLSL "${VIBECRAFT_SHADER_DIR}/fs_chunk.sc" fragment linux 120 glsl)
    vibecraft_compile_shader(FS_LOGO_GLSL "${VIBECRAFT_SHADER_DIR}/fs_logo.sc" fragment linux 120 glsl)
    vibecraft_compile_shader(FS_UI_GLSL "${VIBECRAFT_SHADER_DIR}/fs_ui.sc" fragment linux 120 glsl)
    vibecraft_compile_shader(FS_UI_SOLID_GLSL "${VIBECRAFT_SHADER_DIR}/fs_ui_solid.sc" fragment linux 120 glsl)
    list(
        APPEND
        VIBECRAFT_SHADER_OUTPUTS
        "${VS_CHUNK_GLSL}"
        "${FS_CHUNK_GLSL}"
        "${FS_LOGO_GLSL}"
        "${FS_UI_GLSL}"
        "${FS_UI_SOLID_GLSL}")
endif()

vibecraft_compile_shader(VS_CHUNK_SPIRV "${VIBECRAFT_SHADER_DIR}/vs_chunk.sc" vertex linux spirv spirv)
vibecraft_compile_shader(FS_CHUNK_SPIRV "${VIBECRAFT_SHADER_DIR}/fs_chunk.sc" fragment linux spirv spirv)
vibecraft_compile_shader(FS_LOGO_SPIRV "${VIBECRAFT_SHADER_DIR}/fs_logo.sc" fragment linux spirv spirv)
vibecraft_compile_shader(FS_UI_SPIRV "${VIBECRAFT_SHADER_DIR}/fs_ui.sc" fragment linux spirv spirv)
vibecraft_compile_shader(FS_UI_SOLID_SPIRV "${VIBECRAFT_SHADER_DIR}/fs_ui_solid.sc" fragment linux spirv spirv)
vibecraft_compile_shader(VS_CHUNK_WGSL "${VIBECRAFT_SHADER_DIR}/vs_chunk.sc" vertex linux wgsl wgsl)
vibecraft_compile_shader(FS_CHUNK_WGSL "${VIBECRAFT_SHADER_DIR}/fs_chunk.sc" fragment linux wgsl wgsl)
vibecraft_compile_shader(FS_LOGO_WGSL "${VIBECRAFT_SHADER_DIR}/fs_logo.sc" fragment linux wgsl wgsl)
vibecraft_compile_shader(FS_UI_WGSL "${VIBECRAFT_SHADER_DIR}/fs_ui.sc" fragment linux wgsl wgsl)
vibecraft_compile_shader(FS_UI_SOLID_WGSL "${VIBECRAFT_SHADER_DIR}/fs_ui_solid.sc" fragment linux wgsl wgsl)
list(
    APPEND
    VIBECRAFT_SHADER_OUTPUTS
    "${VS_CHUNK_SPIRV}"
    "${FS_CHUNK_SPIRV}"
    "${FS_LOGO_SPIRV}"
    "${FS_UI_SPIRV}"
    "${FS_UI_SOLID_SPIRV}"
    "${VS_CHUNK_WGSL}"
    "${FS_CHUNK_WGSL}"
    "${FS_LOGO_WGSL}"
    "${FS_UI_WGSL}"
    "${FS_UI_SOLID_WGSL}")

add_custom_target(
    vibecraft_shaders
    DEPENDS
        ${VIBECRAFT_SHADER_OUTPUTS}
)

set(VIBECRAFT_HEADERS
    include/vibecraft/ChunkAtlasLayout.hpp
    include/vibecraft/app/Application.hpp
    include/vibecraft/app/ApplicationBotanyRuntime.hpp
    include/vibecraft/app/ApplicationChunkStreaming.hpp
    include/vibecraft/app/ApplicationCombat.hpp
    include/vibecraft/app/ApplicationConfig.hpp
    include/vibecraft/app/WaterMovement.hpp
    include/vibecraft/app/ApplicationEquipment.hpp
    include/vibecraft/app/ApplicationMultiplayerLog.hpp
    include/vibecraft/app/ApplicationMovementHelpers.hpp
    include/vibecraft/app/ChatCommands.hpp
    include/vibecraft/app/Furnace.hpp
    include/vibecraft/app/Mining.hpp
    include/vibecraft/app/ApplicationSpawnHelpers.hpp
    include/vibecraft/app/ApplicationTerraformingRuntime.hpp
    include/vibecraft/app/ApplicationSurvival.hpp
    include/vibecraft/app/crafting/Crafting.hpp
    include/vibecraft/app/input/ApplicationInputMenuHelpers.hpp
    include/vibecraft/app/Inventory.hpp
    include/vibecraft/app/SingleplayerSave.hpp
    include/vibecraft/audio/MusicCatalog.hpp
    include/vibecraft/audio/MusicDirector.hpp
    include/vibecraft/audio/RuntimeAudioRoot.hpp
    include/vibecraft/audio/SharedAudioOutput.hpp
    include/vibecraft/audio/SoundEffects.hpp
    include/vibecraft/core/Logger.hpp
    include/vibecraft/multiplayer/Protocol.hpp
    include/vibecraft/multiplayer/Session.hpp
    include/vibecraft/multiplayer/Transport.hpp
    include/vibecraft/multiplayer/UdpTransport.hpp
    include/vibecraft/game/Camera.hpp
    include/vibecraft/game/CollisionHelpers.hpp
    include/vibecraft/game/MobNavigation.hpp
    include/vibecraft/game/mobs/MobBreeding.hpp
    include/vibecraft/game/mobs/MobSpecies.hpp
    include/vibecraft/game/mobs/MobTargeting.hpp
    include/vibecraft/game/MobTypes.hpp
    include/vibecraft/game/MobSpawnSystem.hpp
    include/vibecraft/game/PlayerVitals.hpp
    include/vibecraft/meshing/ChunkMesher.hpp
    include/vibecraft/platform/InputState.hpp
    include/vibecraft/platform/LocalNetworkAddress.hpp
    include/vibecraft/platform/Window.hpp
    include/vibecraft/render/Renderer.hpp
    include/vibecraft/render/RendererDetail.hpp
    include/vibecraft/render/RendererInternal.hpp
    include/vibecraft/world/Block.hpp
    include/vibecraft/world/biomes/BiomeProfile.hpp
    include/vibecraft/world/biomes/BiomeSelection.hpp
    include/vibecraft/world/biomes/BiomeTerrainContribution.hpp
    include/vibecraft/world/biomes/FloraGenerationTuning.hpp
    include/vibecraft/world/biomes/SurfaceBiome.hpp
    include/vibecraft/world/biomes/TreeGenerationTuning.hpp
    include/vibecraft/world/Chunk.hpp
    include/vibecraft/world/TerrainNoise.hpp
    include/vibecraft/world/TerrainGenerator.hpp
    include/vibecraft/world/WorldVerticalScale.hpp
    include/vibecraft/world/underground/CaveBiomeRules.hpp
    include/vibecraft/world/underground/CaveRules.hpp
    include/vibecraft/world/underground/OreVeinRules.hpp
    include/vibecraft/world/World.hpp
    include/vibecraft/world/WorldEditCommand.hpp
    include/vibecraft/world/WorldSerializer.hpp
)

set(VIBECRAFT_SOURCES
    src/app/Application.cpp
    src/app/ApplicationAmbientLife.cpp
    src/app/ApplicationBotanyRuntime.cpp
    src/app/ApplicationChunkStreaming.cpp
    src/app/ApplicationCombat.cpp
    src/app/ApplicationCraftingMenu.cpp
    src/app/ApplicationChat.cpp
    src/app/ApplicationDroppedItems.cpp
    src/app/ApplicationEquipment.cpp
    src/app/runtime/ApplicationFrameDebug.cpp
    src/app/ApplicationMobAudio.cpp
    src/app/ApplicationMultiplayerSession.cpp
    src/app/ApplicationMultiplayerRuntime.cpp
    src/app/ApplicationMovementHelpers.cpp
    src/app/WaterMovement.cpp
    src/app/Furnace.cpp
    src/app/Mining.cpp
    src/app/ApplicationPlayerLifecycle.cpp
    src/app/ApplicationPersistence.cpp
    src/app/ApplicationSingleplayerLoad.cpp
    src/app/ApplicationSpawnHelpers.cpp
    src/app/ApplicationTerraformingRuntime.cpp
    src/app/ApplicationWorldSync.cpp
    src/app/ApplicationSurvival.cpp
    src/app/ChatCommands.cpp
    src/app/crafting/Crafting.cpp
    src/app/crafting/CraftingRecipesCatalog.cpp
    src/app/crafting/CraftingRecipesVanilla.cpp
    src/app/input/ApplicationInputPlayingMovement.cpp
    src/app/input/ApplicationInputRouter.cpp
    src/app/input/ApplicationInputMainMenu.cpp
    src/app/input/ApplicationInputPlayingActions.cpp
    src/app/input/ApplicationInputPaused.cpp
    src/app/input/ApplicationInputMenuHelpers.cpp
    src/app/Inventory.cpp
    src/app/SingleplayerSave.cpp
    src/app/main.cpp
    src/audio/MusicCatalog.cpp
    src/audio/MusicDirector.cpp
    src/audio/RuntimeAudioRoot.cpp
    src/audio/SharedAudioOutput.cpp
    src/audio/SoundEffects.cpp
    src/core/Logger.cpp
    src/game/Camera.cpp
    src/game/CollisionHelpers.cpp
    src/game/MobNavigation.cpp
    src/game/mobs/MobBreeding.cpp
    src/game/mobs/MobCombat.cpp
    src/game/mobs/MobSpawning.cpp
    src/game/mobs/MobTargeting.cpp
    src/game/MobSpawnSystem.cpp
    src/game/PlayerVitals.cpp
    src/multiplayer/Protocol.cpp
    src/multiplayer/Session.cpp
    src/multiplayer/UdpTransport.cpp
    src/meshing/ChunkMesher.cpp
    src/platform/LocalNetworkAddress.cpp
    src/platform/Window.cpp
    src/render/Renderer.cpp
    src/render/RendererChatDbgText.cpp
    src/render/RendererCulling.cpp
    src/render/RendererDbgText.cpp
    src/render/RendererDebugOverlay.cpp
    src/render/RendererFrame.cpp
    src/render/RendererHudGpu.cpp
    src/render/RendererHitTests.cpp
    src/render/RendererSkyEffects.cpp
    src/render/RendererShaderTexture.cpp
    src/render/RendererUiGpu.cpp
    src/render/RendererWeather.cpp
    src/world/Chunk.cpp
    src/world/TerrainGenerator.cpp
    src/world/WoodlandRavine.cpp
    src/world/biomes/BiomeProfile.cpp
    src/world/biomes/BiomeSelection.cpp
    src/world/biomes/BiomeTerrainContribution.cpp
    src/world/biomes/FloraGenerationTuning.cpp
    src/world/biomes/TreeGenerationTuning.cpp
    src/world/WorldGeneration.cpp
    src/world/WorldGenerationCaveDecor.cpp
    src/world/WorldGenerationTrees.cpp
    src/world/WorldGenerationFlora.cpp
    src/world/underground/CaveBiomeRules.cpp
    src/world/underground/CaveRules.cpp
    src/world/underground/OreVeinRules.cpp
    src/world/World.cpp
    src/world/WorldSerializer.cpp
    third_party/stb_vorbis.c
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
        third_party
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
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/audio"
        "$<TARGET_FILE_DIR:vibecraft>/audio"
)

if(MSVC)
    target_compile_options(vibecraft PRIVATE /W4 /permissive-)
else()
    target_compile_options(vibecraft PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(vibecraft_tests
    tests/armor_equipment_tests.cpp
    tests/botany_runtime_tests.cpp
    tests/chat_command_tests.cpp
    tests/crafting/crafting_tests.cpp
    tests/furnace_tests.cpp
    tests/game/mob_breeding_tests.cpp
    tests/game/player_vitals_smoke_tests.cpp
    tests/multiplayer/multiplayer_smoke_tests.cpp
    tests/multiplayer_dropped_item_tests.cpp
    tests/inventory_mining_tests.cpp
    tests/terraforming_runtime_tests.cpp
    tests/terrain_biome_pass_tests.cpp
    tests/world/world_ambient_smoke_tests.cpp
    tests/world/world_chunk_spawn_smoke_tests.cpp
    tests/world/world_persistence_smoke_tests.cpp
    tests/world/world_queries_smoke_tests.cpp
    tests/world/world_terrain_smoke_tests.cpp
    tests/world_content_pivot_tests.cpp
    tests/world_smoke_main.cpp
    src/app/crafting/Crafting.cpp
    src/app/crafting/CraftingRecipesCatalog.cpp
    src/app/crafting/CraftingRecipesVanilla.cpp
    src/app/Inventory.cpp
    src/app/ApplicationEquipment.cpp
    src/app/ApplicationAmbientLife.cpp
    src/app/ApplicationBotanyRuntime.cpp
    src/app/ChatCommands.cpp
    src/app/Furnace.cpp
    src/app/Mining.cpp
    src/app/ApplicationTerraformingRuntime.cpp
    src/app/SingleplayerSave.cpp
    src/app/ApplicationSurvival.cpp
    src/game/CollisionHelpers.cpp
    src/game/MobNavigation.cpp
    src/game/mobs/MobBreeding.cpp
    src/game/mobs/MobCombat.cpp
    src/game/mobs/MobSpawning.cpp
    src/game/mobs/MobTargeting.cpp
    src/game/MobSpawnSystem.cpp
    src/game/PlayerVitals.cpp
    src/multiplayer/Protocol.cpp
    src/multiplayer/Session.cpp
    src/multiplayer/UdpTransport.cpp
    src/meshing/ChunkMesher.cpp
    src/world/Chunk.cpp
    src/world/TerrainGenerator.cpp
    src/world/WoodlandRavine.cpp
    src/world/biomes/BiomeProfile.cpp
    src/world/biomes/BiomeSelection.cpp
    src/world/biomes/BiomeTerrainContribution.cpp
    src/world/biomes/FloraGenerationTuning.cpp
    src/world/biomes/TreeGenerationTuning.cpp
    src/world/WorldGeneration.cpp
    src/world/WorldGenerationCaveDecor.cpp
    src/world/WorldGenerationTrees.cpp
    src/world/WorldGenerationFlora.cpp
    src/world/underground/CaveBiomeRules.cpp
    src/world/underground/CaveRules.cpp
    src/world/underground/OreVeinRules.cpp
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
