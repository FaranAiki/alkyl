cmake_minimum_required(VERSION 3.10)
project(alkyl C CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)

set(CMAKE_VERBOSE_MAKEFILE OFF CACHE BOOL "Enable verbose makefile output")

set(OPT_LEVEL "")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

set(CMAKE_C_STANDARD 11)

if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-stringop-overflow")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-stringop-overflow")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wno-stringop-overflow")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

string(TOLOWER "${CMAKE_BUILD_TYPE}" BUILD_TYPE_LOWER)

if(BUILD_TYPE_LOWER STREQUAL "debug")
    set(DEFAULT_OPT_LEVEL "0")
    set(ENABLE_DEBUG_STEP_DEFAULT ON)
    set(ENABLE_DEBUG_ANY_DEFAULT ON)
    set(ENABLE_DEBUG_PARSER_OUT_DEFAULT ON)
    set(ENABLE_DEBUG_SEMANTIC_OUT_DEFAULT ON)
elseif(BUILD_TYPE_LOWER STREQUAL "release")
    set(DEFAULT_OPT_LEVEL "3")
    set(ENABLE_DEBUG_STEP_DEFAULT OFF)
    set(ENABLE_DEBUG_ANY_DEFAULT OFF)
    set(ENABLE_DEBUG_PARSER_OUT_DEFAULT OFF)
    set(ENABLE_DEBUG_SEMANTIC_OUT_DEFAULT OFF)
else()
    set(DEFAULT_OPT_LEVEL "0")
    set(ENABLE_DEBUG_STEP_DEFAULT ON)
    set(ENABLE_DEBUG_ANY_DEFAULT ON)
    set(ENABLE_DEBUG_PARSER_OUT_DEFAULT ON)
    set(ENABLE_DEBUG_SEMANTIC_OUT_DEFAULT ON)
endif()

if(NOT DEFINED OPT_LEVEL OR OPT_LEVEL STREQUAL "")
    set(OPT_LEVEL "${DEFAULT_OPT_LEVEL}")
endif()

option(ENABLE_DEBUG_STEP "Enable debug_step() output" ${ENABLE_DEBUG_STEP_DEFAULT})
option(ENABLE_DEBUG_ANY "Enable debug_any() output" ${ENABLE_DEBUG_ANY_DEFAULT})
option(ENABLE_DEBUG_PARSER_OUT "Enable parser AST output" ${ENABLE_DEBUG_PARSER_OUT_DEFAULT})
option(ENABLE_DEBUG_SEMANTIC_OUT "Enable semantic analysis output" ${ENABLE_DEBUG_SEMANTIC_OUT_DEFAULT})

if(BUILD_TYPE_LOWER STREQUAL "debug")
    set(ENABLE_DEBUG_STEP ${ENABLE_DEBUG_STEP_DEFAULT} CACHE BOOL "Enable debug_step() output" FORCE)
    set(ENABLE_DEBUG_ANY ${ENABLE_DEBUG_ANY_DEFAULT} CACHE BOOL "Enable debug_any() output" FORCE)
    set(ENABLE_DEBUG_PARSER_OUT ${ENABLE_DEBUG_PARSER_OUT_DEFAULT} CACHE BOOL "Enable parser AST output" FORCE)
    set(ENABLE_DEBUG_SEMANTIC_OUT ${ENABLE_DEBUG_SEMANTIC_OUT_DEFAULT} CACHE BOOL "Enable semantic analysis output" FORCE)
elseif(BUILD_TYPE_LOWER STREQUAL "release")
    set(ENABLE_DEBUG_STEP ${ENABLE_DEBUG_STEP_DEFAULT} CACHE BOOL "Enable debug_step() output" FORCE)
    set(ENABLE_DEBUG_ANY ${ENABLE_DEBUG_ANY_DEFAULT} CACHE BOOL "Enable debug_any() output" FORCE)
    set(ENABLE_DEBUG_PARSER_OUT ${ENABLE_DEBUG_PARSER_OUT_DEFAULT} CACHE BOOL "Enable parser AST output" FORCE)
    set(ENABLE_DEBUG_SEMANTIC_OUT ${ENABLE_DEBUG_SEMANTIC_OUT_DEFAULT} CACHE BOOL "Enable semantic analysis output" FORCE)
endif()

if(ENABLE_DEBUG_STEP)
    add_compile_definitions(DEBUG_STEP)
endif()
if(ENABLE_DEBUG_ANY)
    add_compile_definitions(DEBUG_ANY)
endif()
if(ENABLE_DEBUG_PARSER_OUT)
    add_compile_definitions(DEBUG_PARSER_OUT)
endif()
if(ENABLE_DEBUG_SEMANTIC_OUT)
    add_compile_definitions(DEBUG_SEMANTIC_OUT)
endif()

add_compile_options(-Wall -Wextra -O${OPT_LEVEL} -Wno-missing-field-initializers)

# Enable LTO in a cross-platform way
include(CheckIPOSupported)
check_ipo_supported(RESULT lto_supported)
if(lto_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

find_package(PkgConfig)
pkg_check_modules(LIBZIP libzip)
if(LIBZIP_FOUND)
    message(STATUS "Found libzip ${LIBZIP_VERSION}")
    add_compile_definitions(HAVE_LIBZIP)
else()
    message(STATUS "libzip not found, .zyl support disabled")
endif()

set(BACKEND "llvm" CACHE STRING "The backend to use for compilation (llvm or qbe)")
set(BUILDALL "true" CACHE BOOL "Build all backends when true, only the selected backend when false")

string(TOLOWER "${BACKEND}" BACKEND_LOWER)
if (BACKEND_LOWER STREQUAL "buildall")
    set(BUILDALL ON)
endif()

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
    src/lexer/c_lexer.c

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
    src/parser/c_parser.c
  )

set(SEMANTIC_SOURCES
    src/semantic/check.c
    src/semantic/table.c
    src/semantic/emitter.c
    src/semantic/type.c
    src/semantic/core.c

    src/semantic/modifier/func.c
    src/semantic/modifier/taint.c
    src/semantic/modifier/class.c

    src/semantic/fragment/lookup.c
    src/semantic/fragment/switch.c
    src/semantic/fragment/block.c
    src/semantic/fragment/symbolic.c
    src/semantic/fragment/field.c
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
    src/optlir/mem2reg.c
    src/optlir/pure.c
)

set (ALICK_SOURCES
    src/alick/core.c
    src/alick/memory.c
    src/alick/type.c
    src/alick/cfg.c
)

set(MLIR_SOURCES
    src/mlir/generate.c
    src/mlir/expr.c
    src/mlir/stmt.c
    src/mlir/mlir_wrapper.cpp
)

set(HELPER_SOURCES
    src/common/diagnostic.c
    src/common/common.c
    src/common/zip.c
    src/common/arena.c
    src/common/context.c
    src/common/hashmap.c
    src/common/linker.c
    src/driver/lsp.c
)

set(COMMON_SOURCES
  ${FRONTEND_SOURCES}
  ${SEMANTIC_SOURCES}
  ${ALIR_SOURCES}
  ${OPTIMIZE_ALIR_SOURCES}
  ${MLIR_SOURCES}
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
  ${MLIR_SOURCES}
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
        target_link_libraries(alkyl_llvm PRIVATE m pthread dl z ncurses)
    endif()

    if(LIBZIP_FOUND)
        target_include_directories(alkyl_llvm PRIVATE ${LIBZIP_INCLUDE_DIRS})
        target_link_libraries(alkyl_llvm PRIVATE ${LIBZIP_LIBRARIES})
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
        target_link_libraries(alkyl_qbe PRIVATE m pthread dl z ncurses)
    endif()

    if(LIBZIP_FOUND)
        target_include_directories(alkyl_qbe PRIVATE ${LIBZIP_INCLUDE_DIRS})
        target_link_libraries(alkyl_qbe PRIVATE ${LIBZIP_LIBRARIES})
    endif()

    if(WIN32)
        target_sources(alkyl_qbe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/misc/installer/windows/alkyl.rc)
    endif()
endif()



if (BACKEND_LOWER STREQUAL "mlir" OR BUILDALL)
    set(MLIR_DIR "${CMAKE_SOURCE_DIR}/cmake_patches")
    if (LLVM_DIR OR MLIR_DIR)
        find_package(MLIR CONFIG QUIET)
    endif()

    if (MLIR_FOUND)
        find_program(ACTUAL_MLIR_TABLEGEN_EXE mlir-tblgen)
        if (ACTUAL_MLIR_TABLEGEN_EXE)
            message(STATUS "Found MLIR ${MLIR_PACKAGE_VERSION}")
            message(STATUS "Using MLIRConfig.cmake in: ${MLIR_DIR}")

            add_executable(alkyl_mlir ${ALKYL_SOURCES} src/mlir/driver.c)
            target_include_directories(alkyl_mlir PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
            target_compile_definitions(alkyl_mlir PRIVATE BACKEND=mlir)

            # Setup TableGen
            set(ALKYL_TABLEGEN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/mlir/tablegen)
            set(ALKYL_TABLEGEN_INC_DIR ${CMAKE_CURRENT_BINARY_DIR}/include/mlir/tablegen)
            file(MAKE_DIRECTORY ${ALKYL_TABLEGEN_INC_DIR})

            # Generate Dialect declarations and definitions
            add_custom_command(OUTPUT ${ALKYL_TABLEGEN_INC_DIR}/AlkylDialect.h.inc
                COMMAND ${ACTUAL_MLIR_TABLEGEN_EXE} -gen-dialect-decls ${ALKYL_TABLEGEN_DIR}/AlkylDialect.td -I ${MLIR_INCLUDE_DIRS} -I ${ALKYL_TABLEGEN_DIR} -o ${ALKYL_TABLEGEN_INC_DIR}/AlkylDialect.h.inc
                DEPENDS ${ALKYL_TABLEGEN_DIR}/AlkylDialect.td)

            add_custom_command(OUTPUT ${ALKYL_TABLEGEN_INC_DIR}/AlkylDialect.cpp.inc
                COMMAND ${ACTUAL_MLIR_TABLEGEN_EXE} -gen-dialect-defs ${ALKYL_TABLEGEN_DIR}/AlkylDialect.td -I ${MLIR_INCLUDE_DIRS} -I ${ALKYL_TABLEGEN_DIR} -o ${ALKYL_TABLEGEN_INC_DIR}/AlkylDialect.cpp.inc
                DEPENDS ${ALKYL_TABLEGEN_DIR}/AlkylDialect.td)

            # Generate Ops declarations and definitions
            add_custom_command(OUTPUT ${ALKYL_TABLEGEN_INC_DIR}/AlkylOps.h.inc
                COMMAND ${ACTUAL_MLIR_TABLEGEN_EXE} -gen-op-decls ${ALKYL_TABLEGEN_DIR}/AlkylOps.td -I ${MLIR_INCLUDE_DIRS} -I ${ALKYL_TABLEGEN_DIR} -o ${ALKYL_TABLEGEN_INC_DIR}/AlkylOps.h.inc
                DEPENDS ${ALKYL_TABLEGEN_DIR}/AlkylOps.td)

            add_custom_command(OUTPUT ${ALKYL_TABLEGEN_INC_DIR}/AlkylOps.cpp.inc
                COMMAND ${ACTUAL_MLIR_TABLEGEN_EXE} -gen-op-defs ${ALKYL_TABLEGEN_DIR}/AlkylOps.td -I ${MLIR_INCLUDE_DIRS} -I ${ALKYL_TABLEGEN_DIR} -o ${ALKYL_TABLEGEN_INC_DIR}/AlkylOps.cpp.inc
                DEPENDS ${ALKYL_TABLEGEN_DIR}/AlkylOps.td)

            # Create a target for the generated files so they build before alkyl_mlir
            add_custom_target(alkyl_mlir_tblgen
                DEPENDS
                ${ALKYL_TABLEGEN_INC_DIR}/AlkylDialect.h.inc
                ${ALKYL_TABLEGEN_INC_DIR}/AlkylDialect.cpp.inc
                ${ALKYL_TABLEGEN_INC_DIR}/AlkylOps.h.inc
                ${ALKYL_TABLEGEN_INC_DIR}/AlkylOps.cpp.inc
            )

            target_include_directories(alkyl_mlir PRIVATE ${LLVM_INCLUDE_DIRS} ${MLIR_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR}/include)
            target_link_libraries(alkyl_mlir PRIVATE MLIRIR MLIRFuncDialect MLIRArithDialect MLIRMemRefDialect MLIRSCFDialect MLIRControlFlowDialect MLIRSupport MLIRTransforms MLIRLLVMDialect)
            target_compile_definitions(alkyl_mlir PRIVATE ALKYL_ENABLE_MLIR)
            add_dependencies(alkyl_mlir alkyl_mlir_tblgen)

            if(UNIX)
                target_link_libraries(alkyl_mlir PRIVATE m pthread dl z ncurses)
            endif()

            if(LIBZIP_FOUND)
                target_include_directories(alkyl_mlir PRIVATE ${LIBZIP_INCLUDE_DIRS})
                target_link_libraries(alkyl_mlir PRIVATE ${LIBZIP_LIBRARIES})
            endif()
        else()
            message(STATUS "mlir-tblgen not found, skipping building alkyl_mlir.")
        endif()
    else()
        message(STATUS "MLIR not found, skipping building alkyl_mlir.")
    endif()
endif()

if (BACKEND_LOWER STREQUAL "cranelift" OR BUILDALL)
    find_program(CARGO_EXECUTABLE cargo)
    if (CARGO_EXECUTABLE)
        # 1. Ask Cargo to build the static library
        add_custom_command(
            OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/src/codegen_cranelift/rust_backend/target/release/libalkyl_cranelift_backend.a
            COMMAND cargo build --release
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/codegen_cranelift/rust_backend
            COMMENT "Building Rust Cranelift backend..."
        )
        add_custom_target(alkyl_cranelift_rust_lib DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/codegen_cranelift/rust_backend/target/release/libalkyl_cranelift_backend.a)

        # 2. Add executable
        add_executable(alkyl_cranelift ${ALKYL_SOURCES} src/codegen_cranelift/driver.c) # We can reuse main.c
        target_include_directories(alkyl_cranelift PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_compile_definitions(alkyl_cranelift PRIVATE BACKEND=cranelift)

        # 3. Link the Rust staticlib
        target_link_libraries(alkyl_cranelift PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/codegen_cranelift/rust_backend/target/release/libalkyl_cranelift_backend.a
        )
        add_dependencies(alkyl_cranelift alkyl_cranelift_rust_lib)

        if(UNIX)
            target_link_libraries(alkyl_cranelift PRIVATE m pthread dl z ncurses)
        endif()

        if(LIBZIP_FOUND)
            target_include_directories(alkyl_cranelift PRIVATE ${LIBZIP_INCLUDE_DIRS})
            target_link_libraries(alkyl_cranelift PRIVATE ${LIBZIP_LIBRARIES})
        endif()
    else()
        message(STATUS "Cargo not found, skipping building alkyl_cranelift.")
    endif()
endif()


if (BACKEND_LOWER STREQUAL "llvm" OR BUILDALL)
    set(ALKYL_BACKEND_TARGET alkyl_llvm)
elseif (BACKEND_LOWER STREQUAL "qbe")
    set(ALKYL_BACKEND_TARGET alkyl_qbe)
endif()

# Create 'alkyl' symlink to the selected backend executable
add_custom_target(alkyl ALL
    COMMAND ${CMAKE_COMMAND} -E rm -f alkyl
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

if(LIBZIP_FOUND)
    target_include_directories(ethyl PRIVATE ${LIBZIP_INCLUDE_DIRS})
    target_link_libraries(ethyl PRIVATE ${LIBZIP_LIBRARIES})
endif()

add_custom_target(run-tests
    COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/run_tests.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running Alkyl tests..."
)

message(STATUS "CODEGEN_SOURCES: ${CODEGEN_SOURCES}")
message(STATUS "BACKEND_LOWER: ${BACKEND_LOWER}")
message(STATUS "Backend target: ${ALKYL_BACKEND_TARGET}")
message(STATUS "Enable Debug Any: ${ENABLE_DEBUG_ANY}")
message(STATUS "Enable Debug Step: ${ENABLE_DEBUG_STEP}")

if (BACKEND_LOWER STREQUAL "llvm" OR BUILDALL)
    install(TARGETS alkyl_llvm ethyl
        RUNTIME DESTINATION bin
    )
else()
    install(TARGETS ethyl
        RUNTIME DESTINATION bin
    )
endif()

if (BACKEND_LOWER STREQUAL "qbe" OR BUILDALL)
    install(TARGETS alkyl_qbe
        RUNTIME DESTINATION bin
    )
endif()

if (TARGET alkyl_mlir)
    install(TARGETS alkyl_mlir
        RUNTIME DESTINATION bin
    )
endif()

if (TARGET alkyl_cranelift)
    install(TARGETS alkyl_cranelift
        RUNTIME DESTINATION bin
    )
endif()

install(FILES ${CMAKE_SOURCE_DIR}/misc/asset/logo.png
    DESTINATION share/icons/hicolor/512x512/apps
    RENAME alkyl.png
)

install(FILES ${CMAKE_SOURCE_DIR}/misc/asset/script_logo.png
    DESTINATION share/icons/hicolor/512x512/mimetypes
    RENAME application-x-alkyl.png
)

install(FILES ${CMAKE_SOURCE_DIR}/misc/installer/linux/arch/alkyl.desktop
    DESTINATION share/applications
)
install(FILES ${CMAKE_SOURCE_DIR}/misc/installer/linux/arch/alkyl-llvm.desktop
    DESTINATION share/applications
)
install(FILES ${CMAKE_SOURCE_DIR}/misc/installer/linux/arch/alkyl-qbe.desktop
    DESTINATION share/applications
)
install(FILES ${CMAKE_SOURCE_DIR}/misc/installer/linux/arch/alkyl-mlir.desktop
    DESTINATION share/applications
)
install(FILES ${CMAKE_SOURCE_DIR}/misc/installer/linux/arch/alkyl-cranelift.desktop
    DESTINATION share/applications
)
install(FILES ${CMAKE_SOURCE_DIR}/misc/installer/linux/arch/ethyl.desktop
    DESTINATION share/applications
)

install(FILES ${CMAKE_SOURCE_DIR}/misc/installer/linux/arch/application-x-alkyl.xml
    DESTINATION share/mime/packages
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/lib/
    DESTINATION share/alkyl
    FILES_MATCHING PATTERN "*"
)
