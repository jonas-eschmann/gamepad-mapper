cmake_minimum_required(VERSION 3.24)
project(gamepad_mapper VERSION 0.0.5 LANGUAGES CXX)

include(FetchContent)

set(SDL_SHARED OFF CACHE BOOL "Build shared SDL")
set(SDL_STATIC ON CACHE BOOL "Build static SDL")
set(SDL_TEST_LIBRARY OFF CACHE BOOL "Build SDL test library")
set(SDL_TESTS OFF CACHE BOOL "Build SDL tests")
FetchContent_Declare(SDL3
    URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-3.4.18.tar.gz
    URL_HASH SHA256=9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3
)
FetchContent_Declare(json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
)
FetchContent_MakeAvailable(SDL3 json)

add_library(gamepad_mapper STATIC src/gamepad_mapper.cpp)
add_library(gamepad_mapper::gamepad_mapper ALIAS gamepad_mapper)
target_compile_features(gamepad_mapper PUBLIC cxx_std_17)
target_include_directories(gamepad_mapper PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(gamepad_mapper PUBLIC SDL3::SDL3 PRIVATE nlohmann_json::nlohmann_json)

option(GAMEPAD_MAPPER_BUILD_CLIENT "Build the command-line client" ${PROJECT_IS_TOP_LEVEL})
option(GAMEPAD_MAPPER_BUILD_TESTS "Build virtual joystick tests" OFF)

if(GAMEPAD_MAPPER_BUILD_CLIENT)
    add_executable(gamepad-mapper src/main.cpp)
    target_link_libraries(gamepad-mapper PRIVATE gamepad_mapper::gamepad_mapper)
endif()

if(GAMEPAD_MAPPER_BUILD_TESTS)
    enable_testing()
    add_executable(gamepad_mapper_test tests/gamepad_mapper_test.cpp)
    target_link_libraries(gamepad_mapper_test PRIVATE gamepad_mapper::gamepad_mapper)
    add_test(NAME gamepad_mapper COMMAND gamepad_mapper_test)
    set_tests_properties(gamepad_mapper PROPERTIES TIMEOUT 10)
endif()
