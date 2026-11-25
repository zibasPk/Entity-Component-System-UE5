cmake_minimum_required(VERSION 3.23)

project(ECSCpp VERSION 0.1.0 LANGUAGES CXX)

option(ECS_BUILD_TESTS    "Build the ECS unit tests" ${PROJECT_IS_TOP_LEVEL})
# OFF until examples/main.cpp defines main(); an empty source fails to link.
option(ECS_BUILD_EXAMPLES "Build the ECS examples"   OFF)

# ------------------------------------------------------------------ library --
add_library(ecs STATIC
    src/entity_manager.cpp
    src/sparse_set.cpp
    src/registry.cpp
)

target_include_directories(ecs PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_features(ecs PUBLIC cxx_std_20)

# PRIVATE: these warnings apply while building this library, and are not
# inflicted on code that merely consumes it.
target_compile_options(ecs PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall;-Wextra;-Wpedantic>
)


# ----------------------------------------------------------------- examples --
if(ECS_BUILD_EXAMPLES)
    add_executable(ecs_example examples/main.cpp)
    target_link_libraries(ecs_example PRIVATE ecs)
endif()


# -------------------------------------------------------------------- tests --
if(ECS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
