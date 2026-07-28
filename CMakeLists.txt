cmake_minimum_required(VERSION 3.10)
project(alkyl C)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g -Wno-stringop-overflow")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g -Wno-stringop-overflow")

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wno-stringop-overflow")

add_compile_options(-Wall -Wextra -g -O3 -flto=auto
-Wno-missing-field-initializers
-Wno-stringop-overflow
)

set(BACKEND "llvm" CACHE STRING "The backend to use for compilation (llvm or qbe)")
set(BUILDALL "true" CACHE BOOL "Build all backends when true, only the selected backend when false")

string(TOLOWER "${BACKEND}" BACKEND_LOWER)

set(CODEGEN_LLVM_SOURCES
    src/codegen_llvm/codegen.c
    src/codegen_llvm/translate/core.c
    src/codegen_llvm/translate/stmt.c
    src/codegen_llvm/translate/expr.c
    src/codegen_llvm/translate/flow.c
    src/codegen_llvm/translate/misc.c
    src/codegen_llvm/fragment/flux.c
    src/codegen_llvm/driver.c
)

set(CODEGEN_QBE_SOURCES
    src/codegen_qbe/codegen.c
    src/codegen_qbe/value.c
    src/codegen_qbe/driver.c
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
    src/alir/fragment/helper.c
    src/alir/fragment/access.c
    src/alir/fragment/operator.c
    src/alir/fragment/function.c
    src/alir/fragment/addr.c
)

set(OPTIMIZE_ALIR_SOURCES
    src/optlir/unused.c
    src/optlir/local.c
    src/optlir/pure.c
)

set (ALICK_SOURCES
    src/alick/core.c
    src/alick/memory.c
    src/alick/type.c
    src/alick/cfg.c
)

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
  ${OPTIMIZE_ALIR_SOURCES}
  ${ALICK_SOURCES}
  ${HELPER_SOURCES}
)

set(VM_META_SOURCES
    src/metalir/vm.c
    src/metalir/eval_mem.c
    src/metalir/eval_math.c
    src/metalir/eval_flow.c
    src/metalir/eval_call.c
    src/metalir/eval_misc.c
    src/metalir/metalir.c
    src/metarse/metarse.c
)

set(ALKYL_SOURCES
    ${VM_META_SOURCES}
    src/driver/main.c
    ${COMMON_SOURCES}
)

set(ETHYL_SOURCES
  ${FRONTEND_SOURCES}
  ${SEMANTIC_SOURCES}
  ${ALIR_SOURCES}
  ${ALICK_SOURCES}
  ${HELPER_SOURCES}
)

include_directories(
  # src/old/codegen/ # fallback
    include/

    include/metalir

    include/lexer

    include/parser

    include/alir

    include/alick

    include/optlir

    include/codegen

    include/semantic
    include/semantic/modifier
    include/semantic/fragment
    include/metalir

    include/driver

    include/common
)

if (BACKEND_LOWER STREQUAL "llvm" OR BUILDALL)
    include_directories(include/codegen_llvm)
endif()
if (BACKEND_LOWER STREQUAL "qbe" OR BUILDALL)
    include_directories(include/codegen_qbe)
endif()

if (BACKEND_LOWER STREQUAL "llvm" OR BUILDALL)
    find_package(LLVM REQUIRED CONFIG)
    message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
    message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

    include_directories(${LLVM_INCLUDE_DIRS})
    add_definitions(${LLVM_DEFINITIONS})

    llvm_map_components_to_libnames(llvm_libs
        core
        executionengine
        mcjit
        interpreter
        native
        support
        analysis
    )
endif()

if (BACKEND_LOWER STREQUAL "llvm" OR BUILDALL)
    add_executable(alkyl_llvm ${ALKYL_SOURCES} ${CODEGEN_LLVM_SOURCES})

    target_include_directories(alkyl_llvm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${LLVM_INCLUDE_DIRS})
    target_link_libraries(alkyl_llvm PRIVATE LLVM)
    target_compile_definitions(alkyl_llvm PRIVATE BACKEND=LLVM)

    find_package(PkgConfig)
    pkg_check_modules(READLINE readline)
    if(READLINE_FOUND)
        target_include_directories(alkyl_llvm PRIVATE ${READLINE_INCLUDE_DIRS})
        target_link_libraries(alkyl_llvm PRIVATE ${READLINE_LIBRARIES})
        target_compile_definitions(alkyl_llvm PRIVATE HAVE_READLINE)
    endif()

    pkg_check_modules(LIBFFI libffi)
    if(LIBFFI_FOUND)
        target_include_directories(alkyl_llvm PRIVATE ${LIBFFI_INCLUDE_DIRS})
        target_link_libraries(alkyl_llvm PRIVATE ${LIBFFI_LIBRARIES})
        target_compile_definitions(alkyl_llvm PRIVATE HAVE_LIBFFI)
    endif()

    if(UNIX)
        target_link_libraries(alkyl_llvm PRIVATE m pthread dl z ncurses readline)
    endif()

    if(WIN32)
        target_sources(alkyl_llvm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/misc/installer/windows/alkyl.rc)
    endif()
endif()

if (BACKEND_LOWER STREQUAL "qbe" OR BUILDALL)
    add_executable(alkyl_qbe ${ALKYL_SOURCES} ${CODEGEN_QBE_SOURCES})

    target_include_directories(alkyl_qbe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_definitions(alkyl_qbe PRIVATE BACKEND=qbe)

    find_package(PkgConfig)
    pkg_check_modules(READLINE readline)
    if(READLINE_FOUND)
        target_include_directories(alkyl_qbe PRIVATE ${READLINE_INCLUDE_DIRS})
        target_link_libraries(alkyl_qbe PRIVATE ${READLINE_LIBRARIES})
        target_compile_definitions(alkyl_qbe PRIVATE HAVE_READLINE)
    endif()

    pkg_check_modules(LIBFFI libffi)
    if(LIBFFI_FOUND)
        target_include_directories(alkyl_qbe PRIVATE ${LIBFFI_INCLUDE_DIRS})
        target_link_libraries(alkyl_qbe PRIVATE ${LIBFFI_LIBRARIES})
        target_compile_definitions(alkyl_qbe PRIVATE HAVE_LIBFFI)
    endif()

    if(UNIX)
        target_link_libraries(alkyl_qbe PRIVATE m pthread dl z ncurses readline)
    endif()

    if(WIN32)
        target_sources(alkyl_qbe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/misc/installer/windows/alkyl.rc)
    endif()
endif()

if (BACKEND_LOWER STREQUAL "llvm")
    set(ALKYL_BACKEND_TARGET alkyl_llvm)
elseif (BACKEND_LOWER STREQUAL "qbe")
    set(ALKYL_BACKEND_TARGET alkyl_qbe)
endif()

# Create 'alkyl' symlink to the selected backend executable
add_custom_target(alkyl ALL
    COMMAND ${CMAKE_COMMAND} -E remove -f alkyl
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${ALKYL_BACKEND_TARGET} alkyl
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    DEPENDS ${ALKYL_BACKEND_TARGET}
)

add_executable(ethyl
    ${VM_META_SOURCES}
    src/driver/cli.c src/driver/keyboard.c ${ETHYL_SOURCES})
target_include_directories(ethyl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(LIBFFI_FOUND)
    target_include_directories(ethyl PRIVATE ${LIBFFI_INCLUDE_DIRS})
    target_link_libraries(ethyl PRIVATE ${LIBFFI_LIBRARIES})
    target_compile_definitions(ethyl PRIVATE HAVE_LIBFFI)
endif()

if(UNIX)
    target_link_libraries(ethyl PRIVATE m pthread dl z ncurses)
endif()

add_custom_target(run-tests
    COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/run_tests.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running Alkyl tests..."
)

message(STATUS "CODEGEN_SOURCES: ${CODEGEN_SOURCES}")
message(STATUS "BACKEND_LOWER: ${BACKEND_LOWER}")
message(STATUS "Backend target: ${ALKYL_BACKEND_TARGET}")
