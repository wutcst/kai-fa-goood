cmake_minimum_required(VERSION 3.16)
project(FireIceOnline VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Windows: 使用 WinMain 子系统时仍保留控制台（便于调试）
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
endif()

include(FetchContent)
set(SFML_BUILD_AUDIO ON CACHE BOOL "" FORCE)
set(SFML_BUILD_NETWORK ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG        2.6.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(SFML)

option(FIREICE_BUILD_TESTS "Build unit tests" ON)

add_library(fireice_common STATIC
    src/common/Map.cpp
    src/common/Physics.cpp
    src/common/Paths.cpp
    src/common/LevelCatalog.cpp
    src/common/Pickup.cpp
    src/common/LevelMechanics.cpp
)
target_include_directories(fireice_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common
)
target_link_libraries(fireice_common PUBLIC sfml-system)
if(MSVC)
    target_compile_options(fireice_common PRIVATE /utf-8)
endif()

add_executable(fireice_server
    src/server/GameServer.cpp
    src/server/main.cpp
)
target_include_directories(fireice_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/server
)
target_link_libraries(fireice_server PRIVATE fireice_common sfml-network sfml-system)
if(MSVC)
    target_compile_options(fireice_server PRIVATE /utf-8)
endif()

add_executable(fireice_client
    src/client/GameClient.cpp
    src/client/LocalGameSession.cpp
    src/client/UiHelper.cpp
    src/client/AssetManager.cpp
    src/client/CharacterAnimator.cpp
    src/client/tiled/TmxUtil.cpp
    src/client/tiled/TiledMapRenderer.cpp
    src/client/main.cpp
    src/server/GameServer.cpp
)
target_include_directories(fireice_client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/client
    ${CMAKE_CURRENT_SOURCE_DIR}/src/client/tiled
    ${CMAKE_CURRENT_SOURCE_DIR}/src/server
)
target_link_libraries(fireice_client PRIVATE fireice_common sfml-graphics sfml-window sfml-network sfml-system sfml-audio)
if(MSVC)
    target_compile_options(fireice_client PRIVATE /utf-8)
    target_link_libraries(fireice_client PRIVATE ws2_32)
endif()

# Windows: 将 SFML 动态库复制到可执行文件目录，便于直接运行
if(WIN32)
    add_custom_command(TARGET fireice_server POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:sfml-network>
            $<TARGET_FILE:sfml-system>
            $<TARGET_FILE_DIR:fireice_server>
    )
    add_custom_command(TARGET fireice_client POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:sfml-graphics>
            $<TARGET_FILE:sfml-window>
            $<TARGET_FILE:sfml-network>
            $<TARGET_FILE:sfml-system>
            $<TARGET_FILE:sfml-audio>
            $<TARGET_FILE_DIR:fireice_client>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/_deps/sfml-src/extlibs/bin/x64/openal32.dll"
            $<TARGET_FILE_DIR:fireice_client>
    )
endif()

# Copy all level maps to build output
foreach(target_name fireice_server fireice_client)
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target_name}>/levels"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets/levels"
            "$<TARGET_FILE_DIR:${target_name}>/levels"
    )
endforeach()
add_custom_command(TARGET fireice_client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:fireice_client>/textures"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets/textures"
        "$<TARGET_FILE_DIR:fireice_client>/textures"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:fireice_client>/maps"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets/maps"
        "$<TARGET_FILE_DIR:fireice_client>/maps"
)

if(FIREICE_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
        GIT_SHALLOW TRUE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()

    add_executable(fireice_tests tests/unit_tests.cpp src/client/tiled/TmxUtil.cpp)
    target_include_directories(fireice_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/client/tiled
    )
    target_link_libraries(fireice_tests PRIVATE fireice_common GTest::gtest_main)
    if(MSVC)
        target_compile_options(fireice_tests PRIVATE /utf-8)
    endif()

    if(WIN32)
        add_custom_command(TARGET fireice_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:sfml-system>
                $<TARGET_FILE:GTest::gtest>
                $<TARGET_FILE:GTest::gtest_main>
                $<TARGET_FILE_DIR:fireice_tests>
        )
    endif()

    include(GoogleTest)
    gtest_discover_tests(fireice_tests DISCOVERY_MODE PRE_TEST)
endif()
