# CMakeList.txt : CMake project for vulkan, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project (vulkan)

# Set our C++ standard
set(CMAKE_CXX_STANDARD 23)

# Include fetch content
include(FetchContent)

# Fetch the latest version of wincpp
FetchContent_Declare (
	wincpp 
	URL https://github.com/atrexus/wincpp/releases/download/v1.5.3.1/wincpp-src.zip
	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable (wincpp)

# Fetch the latest version of spdlog
FetchContent_Declare (
	spdlog 
	URL https://github.com/gabime/spdlog/archive/refs/tags/v1.15.0.zip
	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable (spdlog)

# Fetch the latest version of argparse
FetchContent_Declare (
	argparse 
	URL https://github.com/p-ranav/argparse/archive/refs/tags/v3.1.zip
	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable (argparse)

# Set the header files.
set (HDR
	"include/dumper.hpp"

	"include/pe/image.hpp"
	"include/pe/section_headers.hpp"
	"include/pe/import_directory.hpp"
	"include/pe/util.hpp"
)

# Set the source files.
set (SRC
	"src/main.cpp"
	"src/dumper.cpp"

	"src/pe/image.cpp"
	"src/pe/section_headers.cpp"
	"src/pe/import_directory.cpp"
)

# Add source to this project's executable.
add_executable (vulkan ${SRC} ${HDR})

# Set the include directories.
target_include_directories(vulkan PRIVATE "include")

# Link project dependencies.
target_link_libraries(vulkan PRIVATE wincpp)
target_link_libraries(vulkan PRIVATE spdlog::spdlog)
target_link_libraries(vulkan PRIVATE argparse)