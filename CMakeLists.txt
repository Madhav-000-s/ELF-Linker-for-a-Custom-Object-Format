cmake_minimum_required(VERSION 3.16)
project(elf_linker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(elf-linker
    src/main.cpp
    src/Driver.cpp
    src/elf/ElfReader.cpp
    src/model/InputObject.cpp
    src/model/OutputImage.cpp
    src/passes/ParseObjects.cpp
    src/passes/MergeSections.cpp
    src/passes/ResolveSymbols.cpp
    src/passes/AssignAddresses.cpp
    src/passes/ApplyRelocations.cpp
    src/passes/EmitExecutable.cpp
    src/util/Error.cpp
)

target_include_directories(elf-linker PRIVATE src)

if(MSVC)
    target_compile_options(elf-linker PRIVATE /W4)
else()
    target_compile_options(elf-linker PRIVATE -Wall -Wextra -Wpedantic)
endif()
