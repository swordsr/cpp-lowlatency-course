cmake_minimum_required(VERSION 3.25)
project(cpp-lowlatency-course LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/Sanitizers.cmake)
include(cmake/Dependencies.cmake)
include(cmake/CourseAssignment.cmake)

# Course-wide portability shims (course::Jthread etc.) — available to
# every assignment's targets via course_assignment().
add_library(course_common INTERFACE)
target_include_directories(course_common INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/common/include")

enable_testing()

# Modules are scaffolded one at a time; pick up whichever exist.
file(GLOB _course_modules LIST_DIRECTORIES true CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/modules/*")
foreach(_dir IN LISTS _course_modules)
    if(EXISTS "${_dir}/CMakeLists.txt")
        add_subdirectory("${_dir}")
    endif()
endforeach()
