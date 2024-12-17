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

# Set the source files.
set (SRC
	"src/main.c"
	"src/syscalls.c"
	"src/nt.c"
	"src/nt.x64.asm"
	"src/dumper.c"
	"src/sections.c"
	"src/imports.c"
)

# Add source to this project's executable.
add_executable (vulkan ${SRC})

# Set the include directories.
target_include_directories(vulkan PRIVATE "include")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET vulkan PROPERTY CXX_STANDARD 20)
endif()

# Check if the project is on Windows and set appropriate flags
if(WIN32)
    enable_language(ASM_MASM)
    set_source_files_properties(nt.asm PROPERTIES LANGUAGE ASM_MASM)
endif()

# TODO: Add tests and install targets if needed.