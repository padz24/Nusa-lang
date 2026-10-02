cmake_minimum_required(VERSION 3.20)
project(nusa VERSION 3.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- LLVM 21.1.8 (wajib, compatibility check) ---
# CATATAN Termux: paket llvm memakai shared lib tunggal libLLVM-21.so dan
# file LLVMExports.cmake-nya merujuk ke .a yang tidak ada, sehingga
# find_package(LLVM) gagal. Karena itu gunakan `llvm-config` langsung
# (cara yang didukung resmi LLVM) alih-alih LLVM CMake exports.
find_program(LLVM_CONFIG llvm-config REQUIRED)
execute_process(COMMAND ${LLVM_CONFIG} --version
  OUTPUT_VARIABLE LLVM_PACKAGE_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION} via ${LLVM_CONFIG}")
if(NOT LLVM_PACKAGE_VERSION VERSION_EQUAL "21.1.8")
  message(WARNING "Nusa v3.0.0 menargetkan LLVM 21.1.8, ditemukan ${LLVM_PACKAGE_VERSION}. Build dilanjutkan, tapi API harus diverifikasi.")
endif()
execute_process(COMMAND ${LLVM_CONFIG} --cxxflags
  OUTPUT_VARIABLE LLVM_CXXFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${LLVM_CONFIG} --ldflags
  OUTPUT_VARIABLE LLVM_LDFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${LLVM_CONFIG} --libs core support
  OUTPUT_VARIABLE LLVM_LIBS OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${LLVM_CONFIG} --system-libs
  OUTPUT_VARIABLE LLVM_SYSTEM_LIBS OUTPUT_STRIP_TRAILING_WHITESPACE)
separate_arguments(LLVM_CXXFLAGS_LIST UNIX_COMMAND "${LLVM_CXXFLAGS}")
separate_arguments(LLVM_LDFLAGS_LIST UNIX_COMMAND "${LLVM_LDFLAGS}")
separate_arguments(LLVM_LIBS_LIST UNIX_COMMAND "${LLVM_LIBS} ${LLVM_SYSTEM_LIBS}")
add_compile_options(${LLVM_CXXFLAGS_LIST})
add_definitions(-D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS)
# Versi Nusa sumber-tunggal untuk kode (Driver/main/pkg).
# Phase 89: FINAL 3.0.0 — PROJECT_VERSION numerik sama dengan label
# manusia (suffix pra-rilis dihapus; deb tetap `3.0.0-1`).
set(NUSA_VERSION_STR "3.0.0")
add_compile_definitions(NUSA_VERSION="${NUSA_VERSION_STR}")

# --- Nusa compiler library (modular) ---
set(NUSA_LIB_SOURCES
  compiler/diagnostics/Diagnostic.cpp
  compiler/ast/AST.cpp
  compiler/frontend/Builtin.cpp
  compiler/bindgen/Bindgen.cpp
  compiler/bindgen/Headers.cpp
  compiler/mir/Mir.cpp
  compiler/mir/Opt.cpp
  compiler/mir/Cfg.cpp
  compiler/lexer/Lexer.cpp
  compiler/parser/Parser.cpp
  compiler/semantic/Semantic.cpp
  compiler/semantic/TypeChecker.cpp
  compiler/types/Type.cpp
  compiler/codegen/Codegen.cpp
  compiler/driver/Driver.cpp
  compiler/driver/Modules.cpp
  compiler/driver/Testing.cpp
  compiler/doc/DocGen.cpp
  compiler/fmt/Formatter.cpp
  compiler/incr/Incr.cpp
  compiler/pkg/Pkg.cpp
  compiler/lsp/Lsp.cpp
  compiler/driver/Sandbox.cpp
  compiler/driver/Playground.cpp
  compiler/driver/Repl.cpp
  compiler/plugin/Plugin.cpp
  compiler/pkg/Templates.cpp
)

add_library(nusa_compiler STATIC ${NUSA_LIB_SOURCES})
target_include_directories(nusa_compiler PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/compiler
)
target_link_options(nusa_compiler PUBLIC ${LLVM_LDFLAGS_LIST})
target_link_libraries(nusa_compiler PUBLIC ${LLVM_LIBS_LIST})

# --- nusa executable ---
add_executable(nusa compiler/driver/main.cpp)
target_link_libraries(nusa PRIVATE nusa_compiler ${LLVM_LIBS_LIST})

# --- Runtime C precompiled (Phase 34): menghemat ~35% waktu build
# (tanpa ini setiap `nusa build` mengkompilasi ulang nusa_rt.c).
# Objek diletakkan di sisi binary agar driver menemukannya.
# Runtime 2.0 (Phase 55): nusa_rt.c = agregator satu TU atas rt_*.c;
# semua bagian menjadi DEPENDS agar perubahan memicu precompile ulang.
find_program(CLANG_BIN clang REQUIRED)
file(GLOB NUSA_RT_PARTS
  ${CMAKE_CURRENT_SOURCE_DIR}/runtime/rt_*.c
  ${CMAKE_CURRENT_SOURCE_DIR}/runtime/rt_*.h)
add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/nusa_rt.o
  COMMAND ${CLANG_BIN} -ffunction-sections -fdata-sections -c ${CMAKE_CURRENT_SOURCE_DIR}/runtime/nusa_rt.c
          -o ${CMAKE_BINARY_DIR}/nusa_rt.o
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/runtime/nusa_rt.c ${NUSA_RT_PARTS}
  COMMENT "Precompiling Nusa runtime (nusa_rt.o)"
)
add_custom_target(nusa_runtime ALL DEPENDS ${CMAKE_BINARY_DIR}/nusa_rt.o)
add_dependencies(nusa nusa_runtime)

# --- Unit tests (tanpa framework eksternal, assert-based) ---
enable_testing()

add_executable(test_lexer compiler/tests/test_lexer.cpp)
target_link_libraries(test_lexer PRIVATE nusa_compiler ${LLVM_LIBS_LIST})
add_test(NAME test_lexer COMMAND test_lexer)

add_executable(test_parser compiler/tests/test_parser.cpp)
target_link_libraries(test_parser PRIVATE nusa_compiler ${LLVM_LIBS_LIST})
add_test(NAME test_parser COMMAND test_parser)

add_executable(test_codegen compiler/tests/test_codegen.cpp)
target_link_libraries(test_codegen PRIVATE nusa_compiler ${LLVM_LIBS_LIST})
add_test(NAME test_codegen COMMAND test_codegen)

add_executable(test_semantic compiler/tests/test_semantic.cpp)
target_link_libraries(test_semantic PRIVATE nusa_compiler ${LLVM_LIBS_LIST})
add_test(NAME test_semantic COMMAND test_semantic)

add_executable(test_typecheck compiler/tests/test_typecheck.cpp)
target_link_libraries(test_typecheck PRIVATE nusa_compiler ${LLVM_LIBS_LIST})
add_test(NAME test_typecheck COMMAND test_typecheck)

add_executable(test_builtins compiler/tests/test_builtins.cpp)
target_link_libraries(test_builtins PRIVATE nusa_compiler ${LLVM_LIBS_LIST})
add_test(NAME test_builtins COMMAND test_builtins)

add_executable(test_fmt compiler/tests/test_fmt.cpp)
target_link_libraries(test_fmt PRIVATE nusa_compiler ${LLVM_LIBS_LIST})
add_test(NAME test_fmt COMMAND test_fmt)

add_executable(test_doc compiler/tests/test_doc.cpp)
target_link_libraries(test_doc PRIVATE nusa_compiler ${LLVM_LIBS_LIST})
add_test(NAME test_doc COMMAND test_doc)

add_executable(test_pkg compiler/tests/test_pkg.cpp)
target_link_libraries(test_pkg PRIVATE nusa_compiler ${LLVM_LIBS_LIST})
add_test(NAME test_pkg COMMAND test_pkg)

add_executable(test_incr compiler/tests/test_incr.cpp)
target_link_libraries(test_incr PRIVATE nusa_compiler ${LLVM_LIBS_LIST})
add_test(NAME test_incr COMMAND test_incr)

add_executable(test_bindgen compiler/tests/test_bindgen.cpp)
target_link_libraries(test_bindgen PRIVATE nusa_compiler ${LLVM_LIBS_LIST})
add_test(NAME test_bindgen COMMAND test_bindgen)

add_executable(test_headers compiler/tests/test_headers.cpp)
target_link_libraries(test_headers PRIVATE nusa_compiler ${LLVM_LIBS_LIST})
add_test(NAME test_headers COMMAND test_headers)

add_executable(test_plugin compiler/tests/test_plugin.cpp)
target_link_libraries(test_plugin PRIVATE nusa_compiler ${LLVM_LIBS_LIST})
add_test(NAME test_plugin COMMAND test_plugin)

add_executable(test_mir compiler/tests/test_mir.cpp)
target_link_libraries(test_mir PRIVATE nusa_compiler ${LLVM_LIBS_LIST})
add_test(NAME test_mir COMMAND test_mir)

add_executable(test_verify compiler/tests/test_verify.cpp)
target_link_libraries(test_verify PRIVATE nusa_compiler ${LLVM_LIBS_LIST})
add_test(NAME test_verify COMMAND test_verify)

# Kapabilitas ORC LLJIT (Phase 105): pagar toolchain JIT in-process.
add_executable(test_orc compiler/tests/test_orc.cpp)
target_link_libraries(test_orc PRIVATE nusa_compiler ${LLVM_LIBS_LIST})
add_test(NAME test_orc COMMAND test_orc)

# --- Integration test: hello.ns end-to-end ---
add_test(
  NAME e2e_hello
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_hello.sh $<TARGET_FILE:nusa>
)

# --- Integration test: front-end v0.2.0 (let + precedence ter-parse,
# codegen menunda dengan pesan jelas) ---
add_test(
  NAME e2e_frontend
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_frontend.sh $<TARGET_FILE:nusa>
)

# --- Integration test: variables & expressions v0.3.0 (nilai eksekusi) ---
add_test(
  NAME e2e_variables
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_variables.sh $<TARGET_FILE:nusa>
)

# --- Integration test: static type system v0.4.0 (anotasi + mismatch) ---
add_test(
  NAME e2e_types
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_types.sh $<TARGET_FILE:nusa>
)

# --- Integration test: functions v0.5.0 (nilai eksekusi + validasi) ---
add_test(
  NAME e2e_functions
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_functions.sh $<TARGET_FILE:nusa>
)

# --- Integration test: control flow v0.6.0 (if/while + nested) ---
add_test(
  NAME e2e_control
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_control.sh $<TARGET_FILE:nusa>
)

# --- Integration test: string & array v0.7.0 (index/len/bounds) ---
add_test(
  NAME e2e_arrays
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_arrays.sh $<TARGET_FILE:nusa>
)

# --- Integration test: struct & module v0.8.0 ---
add_test(
  NAME e2e_struct_module
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_struct_module.sh $<TARGET_FILE:nusa>
)

# --- Integration test: hardening v0.9.5 (negative matrix + robustness) ---
add_test(
  NAME e2e_hardening
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_hardening.sh $<TARGET_FILE:nusa>
)

# --- Integration test: FINAL v1.0.0 (DoD spot checks) ---
add_test(
  NAME e2e_final
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_final.sh $<TARGET_FILE:nusa>
)

# --- Integration test: stdlib & builtins v0.9.0 (nilai runtime) ---
add_test(
  NAME e2e_stdlib
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_stdlib.sh $<TARGET_FILE:nusa>
)

# --- Integration test: tooling v0.9.0 (check/test/fmt/doc) ---
add_test(
  NAME e2e_tooling
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_tooling.sh $<TARGET_FILE:nusa>
)

# --- Integration test: diagnostics v1.1.0 (recovery/cap/english-only) ---
add_test(
  NAME e2e_diagnostics
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_diagnostics.sh $<TARGET_FILE:nusa>
)

# --- Integration test: type aliases v1.2.0 (Phase 21a) ---
add_test(
  NAME e2e_alias
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_alias.sh $<TARGET_FILE:nusa>
)

# --- Integration test: function types v1.2.0 (Phase 21b) ---
add_test(
  NAME e2e_fn
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_fn.sh $<TARGET_FILE:nusa>
)

# --- Integration test: generics v1.2.0 (Phase 21c) ---
add_test(
  NAME e2e_generics
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_generics.sh $<TARGET_FILE:nusa>
)

# --- Integration test: generic structs v1.2.0 (Phase 21d) ---
add_test(
  NAME e2e_structs_gen
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_structs_gen.sh $<TARGET_FILE:nusa>
)

# --- Integration test: traits v1.3.0 (Phase 22) ---
add_test(
  NAME e2e_traits
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_traits.sh $<TARGET_FILE:nusa>
)

# --- Integration test: memory model v1.4.0 (Phase 23) ---
add_test(
  NAME e2e_memory
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_memory.sh $<TARGET_FILE:nusa>
)

# --- Integration test: error handling v1.5.0 (Phase 24) ---
add_test(
  NAME e2e_error
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_error.sh $<TARGET_FILE:nusa>
)

# --- Integration test: pattern matching v1.6.0 (Phase 25) ---
add_test(
  NAME e2e_match
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_match.sh $<TARGET_FILE:nusa>
)

# --- Integration test: enums v1.7.0 (Phase 26) ---
add_test(
  NAME e2e_enums
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_enums.sh $<TARGET_FILE:nusa>
)

# --- Integration test: closures v1.8.0 (Phase 27) ---
add_test(
  NAME e2e_closures
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_closures.sh $<TARGET_FILE:nusa>
)

# --- Integration test: concurrency v1.9.0 (Phase 28) ---
add_test(
  NAME e2e_concurrency
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_concurrency.sh $<TARGET_FILE:nusa>
)

# --- Integration test: stdlib 2.0 v1.10.0 (Phase 29) ---
add_test(
  NAME e2e_stdlib2
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_stdlib2.sh $<TARGET_FILE:nusa>
)

# --- Integration test: stdlib 3.0 v2.26.0 (Phase 75) ---
add_test(
  NAME e2e_stdlib3
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_stdlib3.sh $<TARGET_FILE:nusa>
)

# --- Integration test: cryptography v2.27.0 (Phase 76) ---
add_test(
  NAME e2e_crypto
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_crypto.sh $<TARGET_FILE:nusa>
)

# --- Integration test: data tooling v2.28.0 (Phase 77) ---
add_test(
  NAME e2e_data
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_data.sh $<TARGET_FILE:nusa>
)

# --- Integration test: server foundation v2.29.0 (Phase 78) ---
add_test(
  NAME e2e_server
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_server.sh $<TARGET_FILE:nusa>
)

# --- Integration test: distributed foundation v2.30.0 (Phase 79) ---
add_test(
  NAME e2e_dist
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_dist.sh $<TARGET_FILE:nusa>
)

# --- Integration test: compatibility sweep v2.30.1 (Phase 80) ---
add_test(
  NAME e2e_compat
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_compat.sh $<TARGET_FILE:nusa>
)

# --- Integration test: language freeze v3.0-candidate (Phase 81) ---
add_test(
  NAME e2e_freeze
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_freeze.sh $<TARGET_FILE:nusa>
)

# --- Integration test: compatibility audit + migrate CLI (Phase 83) ---
add_test(
  NAME e2e_migrate
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_migrate.sh $<TARGET_FILE:nusa>
)

# --- Beta gate v3.0.0-beta (Phase 84; Phase 88: banner beta|rc.*) ---
add_test(
  NAME e2e_beta
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_beta.sh $<TARGET_FILE:nusa>
)

# --- RC gate v3.0.0-rc.1 (Phase 88; Phase 89: banner rc.1|final) ---
add_test(
  NAME e2e_rc
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_rc.sh $<TARGET_FILE:nusa>
)

# --- Release gate v3.0.0 final (Phase 89): banner final + DoD + build-all ---
add_test(
  NAME e2e_release
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_release.sh $<TARGET_FILE:nusa>
)

# --- Real-world validation (Phase 85): 6 proyek nyata ---
add_test(
  NAME e2e_realworld
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_realworld.sh $<TARGET_FILE:nusa>
)

# --- Performance gate (Phase 86): suite resmi 8 dimensi ---
add_test(
  NAME e2e_perf
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_perf.sh $<TARGET_FILE:nusa>
)

# --- Integration test: package manager v1.11.0 (Phase 30) ---
add_test(
  NAME e2e_pkg
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_pkg.sh $<TARGET_FILE:nusa>
)

# --- Integration test: registry v1.12.0 (Phase 31) ---
add_test(
  NAME e2e_registry
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_registry.sh $<TARGET_FILE:nusa>
)

# --- Integration test: C FFI v1.13.0 (Phase 32) ---
add_test(
  NAME e2e_ffi
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_ffi.sh $<TARGET_FILE:nusa>
)

# --- Integration test: incremental v1.16.0 (Phase 35) ---
add_test(
  NAME e2e_incr
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_incr.sh $<TARGET_FILE:nusa>
)

# --- Integration test: debug info v1.18.0 (Phase 37) ---
add_test(
  NAME e2e_debug
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_debug.sh $<TARGET_FILE:nusa>
)

# --- Integration test: self-host lexer v1.19.0 (Phase 38) ---
add_test(
  NAME e2e_selfhost
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_selfhost.sh $<TARGET_FILE:nusa>
)

# --- Integration test: self-host parser v2.23.0 (Phase 72) ---
add_test(
  NAME e2e_parse
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_parse.sh $<TARGET_FILE:nusa>
)

# --- Integration test: bootstrap chain v1.20.0 (Phase 39) ---
add_test(
  NAME e2e_bootstrap
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_bootstrap.sh $<TARGET_FILE:nusa>
)

# --- Conformance test: language specification 1.0 (Phase 40) ---
add_test(
  NAME e2e_spec
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_spec.sh $<TARGET_FILE:nusa>
)

# --- Consistency test: RFC system (Phase 41) ---
add_test(
  NAME e2e_rfc
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_rfc.sh $<TARGET_FILE:nusa>
)

# --- Conformance test: language specification 2.0 (Phase 45) ---
add_test(
  NAME e2e_spec2
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_spec2.sh $<TARGET_FILE:nusa>
)

# --- Conformance test: language specification 3.0 final (Phase 82) ---
add_test(
  NAME e2e_spec3
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_spec3.sh $<TARGET_FILE:nusa>
)

# --- Integration test: Termux .deb package ---
add_test(
  NAME e2e_deb
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_deb.sh $<TARGET_FILE:nusa>
)

# --- Integration test: sandbox + playground (Phase 42) ---
add_test(
  NAME e2e_playground
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_playground.sh $<TARGET_FILE:nusa>
)

# --- Integration test: project templates (Phase 43) ---
add_test(
  NAME e2e_new
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_new.sh $<TARGET_FILE:nusa>
)

# --- Integration test: ecosystem website/tutorials/examples (Phase 43 lanjutan) ---
add_test(
  NAME e2e_ecosystem
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_ecosystem.sh $<TARGET_FILE:nusa>
)
# --- Integration test: language server v1.17.0 (Phase 36) ---
add_test(
  NAME e2e_lsp
  COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_lsp.py $<TARGET_FILE:nusa>
)
# --- Integration test: cross-compilation v1.14.0 (Phase 33) ---
add_test(
  NAME e2e_cross
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_cross.sh $<TARGET_FILE:nusa>
)

# --- Regression test: stability v2.1.0 (Phase 50) ---
add_test(
  NAME e2e_stability
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_stability.sh $<TARGET_FILE:nusa>
)

# --- Architecture conformance v2.2.0 (Phase 51) ---
add_test(
  NAME e2e_arch
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_arch.sh $<TARGET_FILE:nusa>
)

# --- IR conformance v2.3.0 (Phase 52) ---
add_test(
  NAME e2e_mir
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_mir.sh $<TARGET_FILE:nusa>
)

# --- Optimization conformance + benchmark v2.4.0 (Phase 53) ---
add_test(
  NAME e2e_opt
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_opt.sh $<TARGET_FILE:nusa>
)

# --- Memory-safety conformance v2.5.0 (Phase 54) ---
add_test(
  NAME e2e_memsafe
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_memsafe.sh $<TARGET_FILE:nusa>
)

# --- Runtime conformance v2.6.0 (Phase 55) ---
add_test(
  NAME e2e_runtime
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_runtime.sh $<TARGET_FILE:nusa>
)
# e2e_runtime memindahkan build/nusa_rt.o sementara (uji fallback) —
# wajib serial agar tak balapan dengan test paralel lain.
set_tests_properties(e2e_runtime PROPERTIES RUN_SERIAL TRUE)

# --- Concurrency stress v2.7.0 (Phase 56) ---
add_test(
  NAME e2e_stress
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_stress.sh $<TARGET_FILE:nusa>
)

# --- Async runtime conformance v2.8.0 (Phase 57) ---
add_test(
  NAME e2e_async
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_async.sh $<TARGET_FILE:nusa>
)

# --- Networking conformance v2.9.0 (Phase 58) ---
add_test(
  NAME e2e_http
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_http.sh $<TARGET_FILE:nusa>
)

# --- Security conformance v2.10.0 (Phase 59) ---
add_test(
  NAME e2e_sec
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_sec.sh $<TARGET_FILE:nusa>
)

# --- Ecosystem conformance v2.11.0 (Phase 60) ---
add_test(
  NAME e2e_eco2
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_eco2.sh $<TARGET_FILE:nusa>
)

# --- Reproducible-build conformance v2.12.0 (Phase 61) ---
add_test(
  NAME e2e_repro
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_repro.sh $<TARGET_FILE:nusa>
)

# --- Build-system conformance v2.13.0 (Phase 62) ---
add_test(
  NAME e2e_buildsys
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_buildsys.sh $<TARGET_FILE:nusa>
)

# --- Profiler conformance v2.16.0 (Phase 65) ---
add_test(
  NAME e2e_prof
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_prof.sh $<TARGET_FILE:nusa>
)

# --- ABI conformance v3.10 (Phase 109) ---
add_test(
  NAME e2e_abi
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_abi.sh $<TARGET_FILE:nusa>
)

# --- Headers conformance v3.11.0 (Phase 110) ---
add_test(
  NAME e2e_headers
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_headers.sh $<TARGET_FILE:nusa>
)

# --- WASI conformance v3.12.0 (Phase 111) ---
add_test(
  NAME e2e_wasi
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_wasi.sh $<TARGET_FILE:nusa>
)

# --- Plugin conformance v3.13.0 (Phase 112) ---
add_test(
  NAME e2e_plugin
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_plugin.sh $<TARGET_FILE:nusa>
)

# --- SBOM conformance v3.14.0 (Phase 113) ---
add_test(
  NAME e2e_sbom
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_sbom.sh $<TARGET_FILE:nusa>
)

# --- Supply-chain conformance v3.15.0 (Phase 114) ---
add_test(
  NAME e2e_supply
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_supply.sh $<TARGET_FILE:nusa>
)

# --- Trace conformance v3.20.0 (Phase 119) ---
add_test(
  NAME e2e_trace
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_trace.sh $<TARGET_FILE:nusa>
)

# --- GPU conformance v3.21.0 (Phase 120) ---
add_test(
  NAME e2e_gpu
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_gpu.sh $<TARGET_FILE:nusa>
)

# --- Parallel conformance v3.22.0 (Phase 121) ---
add_test(
  NAME e2e_parallel
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_parallel.sh $<TARGET_FILE:nusa>
)

# --- Latency conformance v3.25.0 (Phase 124) ---
add_test(
  NAME e2e_latency
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_latency.sh $<TARGET_FILE:nusa>
)

# --- Metamorphic conformance v3.26.0 (Phase 125) ---
add_test(
  NAME e2e_fmtprop
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_fmtprop.sh $<TARGET_FILE:nusa>
)

# --- Differential conformance v3.27.0 (Phase 126) ---
add_test(
  NAME e2e_diff
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_diff.sh $<TARGET_FILE:nusa>
)

# --- Actor conformance v3.18.0 (Phase 117) ---
add_test(
  NAME e2e_actor
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_actor.sh $<TARGET_FILE:nusa>
)

# --- PGO conformance v3.5.0 (Phase 104) ---
add_test(
  NAME e2e_pgo
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_pgo.sh $<TARGET_FILE:nusa>
)

# --- REPL conformance v3.7.0 (Phase 106) ---
add_test(
  NAME e2e_repl
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_repl.sh $<TARGET_FILE:nusa>
)

# --- Const conformance v2.21.0 (Phase 70) ---
add_test(
  NAME e2e_const
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_const.sh $<TARGET_FILE:nusa>
)

# --- Verification conformance v2.22.0 (Phase 71) ---
add_test(
  NAME e2e_verify
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_verify.sh $<TARGET_FILE:nusa>
)

# --- Bindgen conformance v2.17.0 (Phase 66) ---
add_test(
  NAME e2e_bindgen
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_bindgen.sh $<TARGET_FILE:nusa>
)

# --- Minimal-runtime conformance v2.19.0 (Phase 68) ---
add_test(
  NAME e2e_min
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/compiler/tests/e2e_min.sh $<TARGET_FILE:nusa>
)
