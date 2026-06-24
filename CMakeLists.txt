cmake_minimum_required(VERSION 3.16)
project(FireIceOnline VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

# 共享逻辑：地图、物理、房间仿真（客户端与服务端共用）
add_library(fireice_core STATIC
    shared/src/Map.cpp
    shared/src/Physics.cpp
    shared/src/Paths.cpp
    shared/src/LevelCatalog.cpp
    shared/src/Pickup.cpp
    shared/src/LevelMechanics.cpp
    shared/src/Room.cpp
)
target_include_directories(fireice_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/src
)
target_link_libraries(fireice_core PUBLIC sfml-system)
if(MSVC)
    target_compile_options(fireice_core PRIVATE /utf-8)
endif()

# 服务端：UDP 房间管理 + 网络同步
add_executable(fireice_server
    server/src/GameServer.cpp
    server/src/RoomNetwork.cpp
    server/src/main.cpp
)
target_include_directories(fireice_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/server/src
)
target_link_libraries(fireice_server PRIVATE fireice_core sfml-network sfml-system)
if(NOT MSVC)
    find_package(Threads REQUIRED)
    target_link_libraries(fireice_server PRIVATE Threads::Threads)
endif()
if(MSVC)
    target_compile_options(fireice_server PRIVATE /utf-8)
endif()

# 客户端：渲染 + UI + 网络客户端
add_executable(fireice_client
    client/src/GameClient.cpp
    client/src/LocalGameSession.cpp
    client/src/net/ClientNetwork.cpp
    client/src/UiHelper.cpp
    client/src/AssetManager.cpp
    client/src/CharacterAnimator.cpp
    client/src/tiled/TmxUtil.cpp
    client/src/tiled/TiledMapRenderer.cpp
    client/src/main.cpp
)
target_include_directories(fireice_client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/client/src
    ${CMAKE_CURRENT_SOURCE_DIR}/client/src/tiled
    ${CMAKE_CURRENT_SOURCE_DIR}/client/src/net
)
target_link_libraries(fireice_client PRIVATE fireice_core sfml-graphics sfml-window sfml-network sfml-system sfml-audio)
if(MSVC)
    target_compile_options(fireice_client PRIVATE /utf-8)
    target_link_libraries(fireice_client PRIVATE ws2_32)
endif()

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

foreach(target_name fireice_server fireice_client)
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target_name}>/levels"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets/levels"
            "$<TARGET_FILE_DIR:${target_name}>/levels"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target_name}>/maps"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets/maps"
            "$<TARGET_FILE_DIR:${target_name}>/maps"
    )
endforeach()
add_custom_command(TARGET fireice_client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:fireice_client>/textures"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets/textures"
        "$<TARGET_FILE_DIR:fireice_client>/textures"
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

    add_executable(fireice_tests tests/unit_tests.cpp client/src/tiled/TmxUtil.cpp)
    target_include_directories(fireice_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/client/src/tiled
    )
    target_link_libraries(fireice_tests PRIVATE fireice_core GTest::gtest_main)
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
