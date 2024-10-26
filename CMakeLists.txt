# set minimum cmake version
cmake_minimum_required(VERSION 3.5.0)

# project name, version and language
project(zxtape VERSION 0.1.0 LANGUAGES C)


if(ZXTAPE_TARGET STREQUAL "macos")
  set(MACOS 1)
else(ZXTAPE_TARGET STREQUAL "circle")
  set(CIRCLE 1)
endif()

message("ZXTAPE_TARGET = ${ZXTAPE_TARGET}")
message("MACOS = ${MACOS}")
message("CIRCLE = ${CIRCLE}")
message("TOOLCHAIN_PREFIX = ${TOOLCHAIN_PREFIX}")


# global compile definitions
if(MACOS)
  add_compile_definitions(__ZX_TAPE_MACOS__)
else(CIRCLE)
  add_compile_definitions(__ZX_TAPE_CIRCLE__)
endif()

# create library
add_library(
  zxtape
  lib/zxtape.c
  lib/file/zxtape_file_api_dummy.c
  lib/file/zxtape_file_api_buffer.c
  lib/file/zxtape_file_api_file.c
  lib/tzx/tzx.c
)
target_compile_definitions(zxtape PRIVATE __ZX_TAPE__)
if(MACOS)
  add_library(
    tzx_compat
    lib/compat/macos/tzx_compat_macos.c
  )
else(CIRCLE)
  add_library(
    tzx_compat
    lib/compat/circle/tzx_compat_circle.c
  )
endif()

# testing binaries
add_executable(zxtape_test test/zxtape.test.c)

target_include_directories(zxtape_test PRIVATE include)
target_link_libraries(zxtape_test PRIVATE zxtape)
target_link_libraries(zxtape_test PRIVATE tzx_compat)

if(ZXTAPE_TARGET STREQUAL "macos")
  target_compile_definitions(zxtape_test PRIVATE __ZX_TAPE_MACOS__)
else(ZXTAPE_TARGET STREQUAL "circle")
  target_compile_definitions(zxtape_test PRIVATE __ZX_TAPE_CIRCLE__)
endif()


# enable testing
include(CTest)
enable_testing()

# define tests
add_test(NAME HelloWord COMMAND zxtape_test 1)