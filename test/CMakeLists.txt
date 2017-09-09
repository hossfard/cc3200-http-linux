cmake_minimum_required(VERSION 3.0)

project(app C)

# Application name
set(app "tests")

# Generate compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Source files
list(
  APPEND app_sources
  HttpRequestTest.c
  ../HttpRequest.c
  Unity/src/unity.c
#  ../jsmn/jsmn.c
)

# Include directories
include_directories("..")
include_directories("Unity/src")
#include_directories("jsmn")

add_executable(${app} ${app_sources})
