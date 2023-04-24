cmake_minimum_required(VERSION 3.21)

project(HelloOpenCGL)

find_package(OpenCL REQUIRED)

add_executable(${PROJECT_NAME} main.cpp 3rd_party/glad/src/glad.c)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)

target_include_directories(${PROJECT_NAME} PUBLIC 3rd_party/glad/include)
if (WIN32)
    target_include_directories(${PROJECT_NAME} PUBLIC 3rd_party/glfw-3.3.8.bin.WIN64/include)
    target_link_directories(${PROJECT_NAME} PUBLIC 3rd_party/glfw-3.3.8.bin.WIN64/lib-vc2022)
else()
    message(FATAL_ERROR "Include glfw in macOS")
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE OpenCL::OpenCL)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw3)

target_compile_definitions(${PROJECT_NAME} PRIVATE CL_TARGET_OPENCL_VERSION=300)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/kernel.cl ${CMAKE_CURRENT_BINARY_DIR}/kernel.cl COPYONLY)
