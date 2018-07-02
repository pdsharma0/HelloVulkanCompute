cmake_minimum_required(VERSION 3.11)

# Project
project(HelloVulkanCompute)

# Global variables
set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/bin)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

# Include 
include_directories($ENV{VULKAN_SDK}/Include)
include_directories("${PROJECT_SOURCE_DIR}/src")

add_executable(HelloVulkanCompute ${PROJECT_SOURCE_DIR}/src/main.cpp)

find_library(VULKAN_LIBRARY vulkan-1 HINTS $ENV{VULKAN_SDK}/Lib32)
target_link_libraries(HelloVulkanCompute ${VULKAN_LIBRARY})