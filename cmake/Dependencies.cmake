include(FetchContent)

set(FETCHCONTENT_QUIET OFF)
set(
    FETCHCONTENT_UPDATES_DISCONNECTED
    ON
    CACHE BOOL
    "Disable FetchContent network updates during normal configure/build"
    FORCE)
set(FETCHCONTENT_UPDATES_DISCONNECTED_SDL3 ON CACHE BOOL "" FORCE)
set(FETCHCONTENT_UPDATES_DISCONNECTED_BGFX_CMAKE ON CACHE BOOL "" FORCE)
set(FETCHCONTENT_UPDATES_DISCONNECTED_GLM ON CACHE BOOL "" FORCE)
set(FETCHCONTENT_UPDATES_DISCONNECTED_FMT ON CACHE BOOL "" FORCE)
set(FETCHCONTENT_UPDATES_DISCONNECTED_DOCTEST ON CACHE BOOL "" FORCE)

set(BGFX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BGFX_BUILD_TOOLS ON CACHE BOOL "" FORCE)
set(BGFX_BUILD_TOOLS_SHADER ON CACHE BOOL "" FORCE)
set(BGFX_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG main
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    bgfx_cmake
    GIT_REPOSITORY https://github.com/bkaradzic/bgfx.cmake.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(SDL3 bgfx_cmake glm fmt doctest)
