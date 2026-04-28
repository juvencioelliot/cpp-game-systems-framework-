cmake_minimum_required(VERSION 3.16)

project(GameCoreCPP
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(GameCoreCPP
    src/Core/Application.cpp
    src/Core/AssetLoaders.cpp
    src/Core/EntityManager.cpp
    src/Core/EntityPrefab.cpp
    src/Core/FileSystem.cpp
    src/Core/InputManager.cpp
    src/Core/KeyValuePrefabLoader.cpp
    src/Core/Scene.cpp
    src/Core/SceneManager.cpp
    src/Core/SystemScheduler.cpp
    src/Systems/CombatSystem.cpp
    src/Systems/RenderSystem.cpp
)

target_include_directories(GameCoreCPP
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(GameCoreDemo
    demo/main.cpp
)

target_compile_definitions(GameCoreDemo
    PRIVATE
        GAMECORE_DEMO_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/demo/assets"
        GAMECORE_DEMO_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(GameCoreDemo
    PRIVATE
        GameCoreCPP
)

enable_testing()

add_executable(GameCoreTests
    tests/basic_tests.cpp
)

target_link_libraries(GameCoreTests
    PRIVATE
        GameCoreCPP
)

add_test(NAME GameCoreBasicTests COMMAND GameCoreTests)
