# set minimum cmake version
cmake_minimum_required(VERSION 3.5.0)

# project name, version and language
project(zxtape VERSION 0.1.0 LANGUAGES C)

# create library
add_library(zxtape lib/zxtape.c lib/macos/tzx_compat_macos.c lib/tzx/tzx.c)
target_compile_definitions(zxtape PRIVATE __ZX_TAPE__)
target_compile_definitions(zxtape PRIVATE __ZX_TAPE_MACOS__) # TODO: move to platform specific configuration

# testing binaries
add_executable(zxtape_test test/zxtape.test.c)

target_include_directories(zxtape_test PRIVATE include)
target_link_libraries(zxtape_test PRIVATE zxtape)

# enble testing
include(CTest)
enable_testing()

# define tests
add_test(NAME HelloWord COMMAND zxtape_test 1)