cmake_minimum_required(VERSION 3.16)

project(GameCoreCPP
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(GAMECORE_BUILD_BENCHMARKS "Build GameCore benchmark executables" OFF)

if(EXISTS "/opt/homebrew")
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew")
endif()

add_library(GameCoreCPP
    src/Core/Application.cpp
    src/Core/AssetLoaders.cpp
    src/Core/Diagnostics.cpp
    src/Core/EntityManager.cpp
    src/Core/EntityPrefab.cpp
    src/Core/FileSystem.cpp
    src/Core/InputManager.cpp
    src/Core/KeyValuePrefabLoader.cpp
    src/Core/Scene.cpp
    src/Core/SceneManager.cpp
    src/Core/SystemScheduler.cpp
    src/Core/Time.cpp
    src/Systems/MovementSystem.cpp
    src/Systems/RenderSystem.cpp
    src/Systems/TransformSystem.cpp
)

find_package(SDL2 QUIET)
if(SDL2_FOUND)
    target_sources(GameCoreCPP
        PRIVATE
            src/Core/SdlInputBackend.cpp
            src/Systems/SdlRenderBackend.cpp
    )

    target_compile_definitions(GameCoreCPP
        PUBLIC
            GAMECORE_HAS_SDL2=1
    )

    target_link_libraries(GameCoreCPP
        PUBLIC
            SDL2::SDL2
    )
endif()

target_include_directories(GameCoreCPP
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(GameCoreDemo
    demo/CombatGameplay.cpp
    demo/main.cpp
)

target_compile_definitions(GameCoreDemo
    PRIVATE
        GAMECORE_DEMO_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/demo/assets"
)

target_link_libraries(GameCoreDemo
    PRIVATE
        GameCoreCPP
)

if(GAMECORE_BUILD_BENCHMARKS)
    add_executable(GameCoreRuntimeBenchmark
        benchmarks/runtime_benchmark.cpp
    )

    target_link_libraries(GameCoreRuntimeBenchmark
        PRIVATE
            GameCoreCPP
    )
endif()

enable_testing()

function(add_gamecore_test target)
    add_executable(${target}
        ${ARGN}
    )

    target_link_libraries(${target}
        PRIVATE
            GameCoreCPP
    )

    add_test(NAME ${target} COMMAND ${target})
endfunction()

add_gamecore_test(GameCoreBasicTests tests/basic_tests.cpp)
add_gamecore_test(GameCoreWorldTests tests/world_tests.cpp)
add_gamecore_test(GameCoreCombatGameplayTests
    tests/combat_gameplay_tests.cpp
    demo/CombatGameplay.cpp
)

target_include_directories(GameCoreCombatGameplayTests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/demo
)
