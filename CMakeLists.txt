# CMakeLists.txt 文件
cmake_minimum_required(VERSION 3.10)

project(TicketSystem)

# Default to Release for single-config generators (Makefiles/Ninja)
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/include/container)
include_directories(${PROJECT_SOURCE_DIR}/include/System)
include_directories(${PROJECT_SOURCE_DIR}/include/memoryriver)

# 为 MyProject 目标添加包含目录
add_executable(code
 src/system/System.cpp
 src/system/TrainSystem.cpp
src/system/TicketSystem.cpp
src/system/UserSystem.cpp
src/parser.cpp
src/main.cpp)

if(MINGW OR GNUC)
    target_compile_options(code PRIVATE -Ofast -DNDEBUG -pipe -flto)
endif()

# 设置栈大小（Windows MSVC）
# 统一设置栈大小 (1GB)
set(EXE_STACK_SIZE 1073741824)

if(MSVC)
    set_target_properties(code PROPERTIES LINK_FLAGS "/STACK:${EXE_STACK_SIZE}")
elseif(MINGW OR GNUC)
    target_link_options(code PRIVATE "-Wl,--stack,1073741824" -flto)
endif()