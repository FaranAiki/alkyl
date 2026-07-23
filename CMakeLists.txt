cmake_minimum_required(VERSION 3.10)
project(alkyl C)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g")

set(CMAKE_C_STANDARD 11)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

add_compile_options(-Wall -Wextra -g -O3 -flto=auto
-Wno-missing-field-initializers
)

llvm_map_components_to_libnames(llvm_libs
    core
    executionengine
    mcjit
    interpreter
    native
    support
    analysis
)

include_directories(
  # src/old/codegen/ # fallback
    include/

    include/lexer

    include/parser

    include/alir

    include/alick

    include/codegen

    include/semantic
    include/semantic/modifier
    include/semantic/fragment

    include/driver

    include/common

    include/codegen_llvm
    include/codegen_qbe
)

set(FRONTEND_SOURCES
    src/lexer/lexer.c
    src/lexer/emitter.c

    src/parser/core.c
    src/parser/expr.c
    src/parser/stmt.c
    src/parser/fragment/cond.c
    src/parser/fragment/loop.c
    src/parser/fragment/decl.c
    src/parser/fragment/class.c
    src/parser/top.c
    src/parser/ast_clone.c
    src/parser/emitter.c
    src/parser/link.c
    src/parser/modif.c
    src/parser/modifier.c

)

set(SEMANTIC_SOURCES
    src/semantic/check.c
    src/semantic/table.c
    src/semantic/emitter.c
    src/semantic/type.c
    src/semantic/core.c

    src/semantic/modifier/func.c
    src/semantic/modifier/class.c

    src/semantic/fragment/lookup.c
    src/semantic/fragment/switch.c
    src/semantic/fragment/symbolic.c
)

set(ALIR_SOURCES
    src/alir/core.c
    src/alir/generator.c
    src/alir/const.c
    src/alir/flux.c
    src/alir/utils.c
    src/alir/lvalue.c
    src/alir/emitter.c
    src/alir/binary_write.c
    src/alir/binary_read.c
    src/alir/stmt.c

    src/alir/fragment/generate.c
    src/alir/fragment/addr.c
)

set (ALICK_SOURCES
    src/alick/core.c
    src/alick/memory.c
    src/alick/type.c
    src/alick/cfg.c
)

set(BACKEND "llvm" CACHE STRING "The backend to use for compilation (llvm or qbe)")

string(TOLOWER "${BACKEND}" BACKEND_LOWER)

set(CODEGEN_LLVM_SOURCES
    src/codegen_llvm/codegen.c
    src/codegen_llvm/translate.c
    src/codegen_llvm/fragment/flux.c
    src/codegen_llvm/driver.c
)

set(CODEGEN_QBE_SOURCES
    src/codegen_qbe/codegen.c
)

if (BACKEND_LOWER STREQUAL "llvm")
    set(CODEGEN_SOURCES ${CODEGEN_LLVM_SOURCES})
    add_definitions(-DBACKEND_LLVM)
elseif(BACKEND_LOWER STREQUAL "qbe")
    set(CODEGEN_SOURCES ${CODEGEN_QBE_SOURCES})
    add_definitions(-DBACKEND_QBE)
else()
    message(FATAL_ERROR "Unknown BACKEND: ${BACKEND}")
endif()

set(HELPER_SOURCES
    src/common/diagnostic.c
    src/common/common.c
    src/common/arena.c
    src/common/context.c
    src/common/hashmap.c
    src/driver/lsp.c
)

set(COMMON_SOURCES
  ${FRONTEND_SOURCES}
  ${SEMANTIC_SOURCES}
  ${ALIR_SOURCES}
  ${ALICK_SOURCES}
  ${CODEGEN_SOURCES}
  ${HELPER_SOURCES}
)

add_executable(alkyl src/meta/vm.c
    src/driver/main.c ${COMMON_SOURCES})

if(WIN32)
    target_sources(alkyl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/misc/installer/windows/alkyl.rc)
endif()


# Headers
target_include_directories(alkyl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${LLVM_INCLUDE_DIRS})


# target_link_libraries(alkyl PRIVATE ${llvm_libs})
# target_link_libraries(alkyl-cli PRIVATE ${llvm_libs})
target_link_libraries(alkyl PRIVATE LLVM)


find_package(PkgConfig)
pkg_check_modules(READLINE readline)
if(READLINE_FOUND)
    target_include_directories(alkyl PRIVATE ${READLINE_INCLUDE_DIRS})
    target_link_libraries(alkyl PRIVATE ${READLINE_LIBRARIES})
    target_compile_definitions(alkyl PRIVATE HAVE_READLINE)
endif()

pkg_check_modules(LIBFFI libffi)
if(LIBFFI_FOUND)
    target_include_directories(alkyl PRIVATE ${LIBFFI_INCLUDE_DIRS})
    target_link_libraries(alkyl PRIVATE ${LIBFFI_LIBRARIES})
    target_compile_definitions(alkyl PRIVATE HAVE_LIBFFI)
endif()

if(UNIX)
    target_link_libraries(alkyl PRIVATE m pthread dl z ncurses readline)
endif()

set(ETHYL_SOURCES
  ${FRONTEND_SOURCES}
  ${SEMANTIC_SOURCES}
  ${ALIR_SOURCES}
  ${ALICK_SOURCES}
  ${HELPER_SOURCES}
)

add_executable(ethyl src/meta/vm.c src/driver/cli.c ${ETHYL_SOURCES})
target_include_directories(ethyl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(READLINE_FOUND)
    target_include_directories(ethyl PRIVATE ${READLINE_INCLUDE_DIRS})
    target_link_libraries(ethyl PRIVATE ${READLINE_LIBRARIES})
    target_compile_definitions(ethyl PRIVATE HAVE_READLINE)
endif()
if(LIBFFI_FOUND)
    target_include_directories(ethyl PRIVATE ${LIBFFI_INCLUDE_DIRS})
    target_link_libraries(ethyl PRIVATE ${LIBFFI_LIBRARIES})
    target_compile_definitions(ethyl PRIVATE HAVE_LIBFFI)
endif()
if(UNIX)
    target_link_libraries(ethyl PRIVATE m pthread dl z ncurses readline)
endif()

add_custom_target(run-tests
    COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/run_tests.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running Alkyl tests..."
)
