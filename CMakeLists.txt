project(xld)
set(CMAKE_PROJECT_VERSION 0.1.0)

cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()

add_library(headers INTERFACE)
target_include_directories(headers INTERFACE include)
target_compile_features(headers INTERFACE cxx_std_20)

add_subdirectory(src)
add_subdirectory(third_party/tbb)
