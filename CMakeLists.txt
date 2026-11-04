cmake_minimum_required(VERSION 3.23)
PROJECT(ECSCpp VERSION 0.1.0 LANGUAGES CXX)
add_library(ecs STATIC 
"${CMAKE_CURRENT_SOURCE_DIR}/src/entity_manager.cpp" 
"${CMAKE_CURRENT_SOURCE_DIR}/src/sparse_set.cpp")
target_include_directories(ecs PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(ecs PUBLIC cxx_std_20)

add_executable(ecs_test examples/main.cpp)
target_link_libraries(ecs_test PRIVATE ecs)