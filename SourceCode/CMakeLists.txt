cmake_minimum_required(VERSION 3.16)
project(MyPngApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find libpng via vcpkg
find_package(PNG REQUIRED)

# Your executable
add_executable(my_app src/main.cpp)

# Include the patched png++ headers
target_include_directories(my_app PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Link libpng
target_link_libraries(my_app PRIVATE PNG::PNG)