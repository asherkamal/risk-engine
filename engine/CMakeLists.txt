cmake_minimum_required(VERSION 3.20)
project(risk_engine_core CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

option(ENABLE_TSAN "Build with ThreadSanitizer" OFF)

add_library(engine_core STATIC
    src/spatial/geo_projection.cpp
    src/spatial/kdtree.cpp
)
target_include_directories(engine_core PUBLIC include)

if(MSVC)
    target_compile_options(engine_core PRIVATE /W4)
else()
    target_compile_options(engine_core PRIVATE -Wall -Wextra)
endif()

if(ENABLE_TSAN)
    if(MSVC)
        message(FATAL_ERROR "ENABLE_TSAN requires GCC/Clang (ThreadSanitizer is not available with MSVC)")
    endif()
    target_compile_options(engine_core PUBLIC -fsanitize=thread -g)
    target_link_options(engine_core PUBLIC -fsanitize=thread)
endif()

add_executable(risk_engine_cli apps/cli/main.cpp)
target_link_libraries(risk_engine_cli PRIVATE engine_core)

enable_testing()
add_subdirectory(tests)
