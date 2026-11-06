#=====================================================================
#
# MushIMTip - Win32 IME status tip that follows the text caret
#
# Directory layout:
#   src/     Source code
#   script/  Build scripts
#   bin/     Build artifacts (executables)
#   build/   CMake build directory
#
#=====================================================================
cmake_minimum_required(VERSION 3.20)

project(MushIMTip VERSION 0.1.0 LANGUAGES CXX)

#---------------------------------------------------------------------
# Global settings
#---------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Source files are UTF-8; MSVC requires this to be specified explicitly
if(MSVC)
	add_compile_options(/utf-8 /W4)
endif()

set(MUSHIMTIP_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

# Executables go to bin/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${MUSHIMTIP_ROOT}/bin)

#---------------------------------------------------------------------
# Targets
#---------------------------------------------------------------------
add_executable(MushIMTip WIN32
	src/main.cpp
	src/ime.cpp
	src/caret_uia.cpp
	src/caret_msaa.cpp
	src/tip_window.cpp
)

target_compile_definitions(MushIMTip PRIVATE
	UNICODE
	_UNICODE
	WIN32_LEAN_AND_MEAN
	NOMINMAX
)

target_link_libraries(MushIMTip PRIVATE imm32 oleacc)
