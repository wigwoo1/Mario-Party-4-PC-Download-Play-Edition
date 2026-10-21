cmake_minimum_required(VERSION 3.25)
project(partyboard_browser_renderer LANGUAGES C CXX)
if(NOT EMSCRIPTEN)
  message(FATAL_ERROR "Use the Emscripten toolchain; this is an experimental browser build")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
get_filename_component(PARTYBOARD_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(AURORA_ROOT "${PARTYBOARD_ROOT}/extern/aurora")
set(PARTYBOARD_DEPENDENCIES "" CACHE PATH "Existing dependency source/header directory")
set(PARTYBOARD_EMDAWN_WEBGPU_PORT "emdawnwebgpu" CACHE STRING "Emscripten EmdawnWebGPU port name or local port file path")
option(PARTYBOARD_BROWSER_OPTIMIZE_RENDERER "Optimize renderer CPU work while retaining assertions" OFF)
if(NOT PARTYBOARD_EMDAWN_WEBGPU_PORT STREQUAL "emdawnwebgpu" AND NOT EXISTS "${PARTYBOARD_EMDAWN_WEBGPU_PORT}")
  message(FATAL_ERROR "PARTYBOARD_EMDAWN_WEBGPU_PORT must be emdawnwebgpu or an existing local port file")
endif()
set(EMDAWN_WEBGPU_PORT_FLAG "--use-port=${PARTYBOARD_EMDAWN_WEBGPU_PORT}")
if(NOT EXISTS "${PARTYBOARD_DEPENDENCIES}/fmt-src/include/fmt/format.h")
  message(FATAL_ERROR "Set PARTYBOARD_DEPENDENCIES to the populated native build _deps directory")
endif()
# Compile the existing GX source list; do not copy or substitute renderer implementations.
file(READ "${AURORA_ROOT}/cmake/aurora_gx.cmake" GX_CMAKE)
string(REGEX MATCH "add_library\\(aurora_gx STATIC[^)]*" GX_BLOCK "${GX_CMAKE}")
string(REGEX MATCHALL "lib/[A-Za-z0-9_/]+\\.cpp" GX_SOURCES "${GX_BLOCK}")
list(TRANSFORM GX_SOURCES PREPEND "${AURORA_ROOT}/")
add_library(partyboard_browser_renderer STATIC ${GX_SOURCES} "${AURORA_ROOT}/lib/webgpu/gpu.cpp")
target_compile_definitions(partyboard_browser_renderer PRIVATE AURORA TARGET_PC AURORA_ENABLE_GX)
target_compile_options(partyboard_browser_renderer PUBLIC "${EMDAWN_WEBGPU_PORT_FLAG}" -sUSE_SDL=3 -sUSE_LIBPNG=1)
if(PARTYBOARD_BROWSER_OPTIMIZE_RENDERER)
  # Keep game/scheduler compilation unchanged and retain renderer invariants.
  target_compile_options(partyboard_browser_renderer PRIVATE -O2 -UNDEBUG)
endif()
target_link_options(partyboard_browser_renderer PUBLIC "${EMDAWN_WEBGPU_PORT_FLAG}" -sUSE_SDL=3 -sUSE_LIBPNG=1)
target_include_directories(partyboard_browser_renderer PRIVATE
  "${AURORA_ROOT}/include" "${PARTYBOARD_DEPENDENCIES}/fmt-src/include"
  "${PARTYBOARD_DEPENDENCIES}/abseil-cpp-src" "${PARTYBOARD_DEPENDENCIES}/xxhash-src"
  "${PARTYBOARD_DEPENDENCIES}/tracy-src/public" "${PARTYBOARD_DEPENDENCIES}/sqlite3-src"

)
# This executable tests the browser dependency path; it does not link the renderer archive yet.
add_executable(browser_gpu_contract "${PARTYBOARD_ROOT}/tools/test_wasm_webgpu.cpp")
target_compile_options(browser_gpu_contract PRIVATE "${EMDAWN_WEBGPU_PORT_FLAG}")
target_link_options(browser_gpu_contract PRIVATE "${EMDAWN_WEBGPU_PORT_FLAG}" -sASYNCIFY -sASSERTIONS=2 -sENVIRONMENT=web -sMODULARIZE -sEXPORT_ES6)
set_target_properties(browser_gpu_contract PROPERTIES OUTPUT_NAME contract SUFFIX .mjs)

# Core platform compile target, still separate from a linked game runtime.
add_library(partyboard_browser_core STATIC
  "${AURORA_ROOT}/lib/aurora.cpp"
  "${AURORA_ROOT}/lib/device.cpp"
  "${AURORA_ROOT}/lib/input.cpp"
  "${AURORA_ROOT}/lib/window.cpp"
  "${AURORA_ROOT}/lib/logging.cpp"
  "${AURORA_ROOT}/lib/system_info.cpp"
  "${AURORA_ROOT}/lib/imgui.cpp"
  "${AURORA_ROOT}/lib/webgpu/gpu_cache.cpp")
get_target_property(RENDERER_INCLUDES partyboard_browser_renderer INCLUDE_DIRECTORIES)
target_include_directories(partyboard_browser_core PRIVATE ${RENDERER_INCLUDES}
  "${PARTYBOARD_DEPENDENCIES}/imgui-src" "${PARTYBOARD_DEPENDENCIES}/imgui-src/backends")
target_compile_definitions(partyboard_browser_core PRIVATE AURORA TARGET_PC AURORA_ENABLE_GX ENABLE_BACKEND_WEBGPU)
target_compile_options(partyboard_browser_core PRIVATE "${EMDAWN_WEBGPU_PORT_FLAG}" -sUSE_SDL=3)

set(FMT_TEST OFF CACHE BOOL "" FORCE)
set(FMT_DOC OFF CACHE BOOL "" FORCE)
set(ABSL_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)
add_subdirectory("${PARTYBOARD_DEPENDENCIES}/fmt-src" dependencies/fmt EXCLUDE_FROM_ALL)
add_subdirectory("${PARTYBOARD_DEPENDENCIES}/abseil-cpp-src" dependencies/abseil EXCLUDE_FROM_ALL)
add_library(browser_xxhash STATIC "${PARTYBOARD_DEPENDENCIES}/xxhash-src/xxhash.c")
target_include_directories(browser_xxhash PUBLIC "${PARTYBOARD_DEPENDENCIES}/xxhash-src")
add_library(browser_sqlite STATIC "${PARTYBOARD_DEPENDENCIES}/sqlite3-src/sqlite3.c")
target_include_directories(browser_sqlite PUBLIC "${PARTYBOARD_DEPENDENCIES}/sqlite3-src")
target_compile_definitions(browser_sqlite PRIVATE SQLITE_THREADSAFE=0 SQLITE_OMIT_LOAD_EXTENSION)
target_link_libraries(partyboard_browser_renderer PUBLIC fmt::fmt absl::btree absl::flat_hash_map browser_xxhash browser_sqlite)
target_link_libraries(partyboard_browser_core PUBLIC fmt::fmt absl::btree absl::flat_hash_map browser_xxhash browser_sqlite)
add_executable(browser_dependency_contract "${PARTYBOARD_ROOT}/tools/test_wasm_dependencies.cpp")
target_link_libraries(browser_dependency_contract PRIVATE fmt::fmt absl::flat_hash_map browser_xxhash browser_sqlite)
target_link_options(browser_dependency_contract PRIVATE -sASSERTIONS=2 -sENVIRONMENT=web -sMODULARIZE -sEXPORT_ES6)
set_target_properties(browser_dependency_contract PROPERTIES OUTPUT_NAME contract SUFFIX .mjs RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dependency-contract")
target_compile_options(fmt PRIVATE -include cstdlib) # Pinned fmt assumes allocation declarations are already visible.

# ImGui 1.91.9b assumes Emscripten always uses legacy WebGPU headers.
# Our pinned emdawnwebgpu uses the modern Dawn API; adjust only the build copy.
file(READ "${PARTYBOARD_DEPENDENCIES}/imgui-src/backends/imgui_impl_wgpu.cpp" IMGUI_WEBGPU_SOURCE)
string(REPLACE "#ifndef __EMSCRIPTEN__" "#if !defined(__EMSCRIPTEN__) || defined(IMGUI_IMPL_WEBGPU_BACKEND_DAWN)" IMGUI_WEBGPU_SOURCE "${IMGUI_WEBGPU_SOURCE}")
file(WRITE "${CMAKE_BINARY_DIR}/imgui_impl_wgpu_emdawn.cpp" "${IMGUI_WEBGPU_SOURCE}")
add_library(browser_imgui STATIC
  "${PARTYBOARD_DEPENDENCIES}/imgui-src/imgui.cpp"
  "${PARTYBOARD_DEPENDENCIES}/imgui-src/imgui_draw.cpp"
  "${PARTYBOARD_DEPENDENCIES}/imgui-src/imgui_tables.cpp"
  "${PARTYBOARD_DEPENDENCIES}/imgui-src/imgui_widgets.cpp"
  "${PARTYBOARD_DEPENDENCIES}/imgui-src/imgui_demo.cpp"
  "${PARTYBOARD_DEPENDENCIES}/imgui-src/misc/cpp/imgui_stdlib.cpp"
  "${PARTYBOARD_DEPENDENCIES}/imgui-src/backends/imgui_impl_sdl3.cpp"
  "${CMAKE_BINARY_DIR}/imgui_impl_wgpu_emdawn.cpp")
target_include_directories(browser_imgui PUBLIC "${PARTYBOARD_DEPENDENCIES}/imgui-src")
target_compile_options(browser_imgui PUBLIC "${EMDAWN_WEBGPU_PORT_FLAG}" -sUSE_SDL=3)
target_link_libraries(partyboard_browser_core PUBLIC browser_imgui)

target_compile_definitions(browser_imgui PRIVATE IMGUI_IMPL_WEBGPU_BACKEND_DAWN)
target_include_directories(browser_imgui PRIVATE "${PARTYBOARD_DEPENDENCIES}/imgui-src/backends")

target_compile_options(browser_dependency_contract PRIVATE -sUSE_LIBPNG=1)
target_link_options(browser_dependency_contract PRIVATE -sUSE_LIBPNG=1)
add_executable(browser_aurora_contract "${PARTYBOARD_ROOT}/tools/test_wasm_aurora.cpp")
target_include_directories(browser_aurora_contract PRIVATE "${AURORA_ROOT}/include")
target_link_libraries(browser_aurora_contract PRIVATE "-Wl,--start-group" partyboard_browser_core partyboard_browser_renderer "-Wl,--end-group")
target_link_options(browser_aurora_contract PRIVATE -sASYNCIFY -sASYNCIFY_STACK_SIZE=65536 -sASSERTIONS=2 -sALLOW_MEMORY_GROWTH=1 -sENVIRONMENT=web -sMODULARIZE -sEXPORT_ES6 -sEXPORTED_RUNTIME_METHODS=requestFullscreen)
set_target_properties(browser_aurora_contract PROPERTIES OUTPUT_NAME contract SUFFIX .mjs RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/aurora-contract")
target_sources(partyboard_browser_core PRIVATE "${AURORA_ROOT}/lib/dolphin/vi/vi.cpp")
target_sources(browser_imgui PRIVATE "${PARTYBOARD_DEPENDENCIES}/imgui-src/backends/imgui_impl_sdlrenderer3.cpp")
target_compile_definitions(browser_aurora_contract PRIVATE AURORA TARGET_PC)

add_executable(browser_bind_group_lifetime_contract "${PARTYBOARD_ROOT}/tools/test_wasm_bind_group_lifetime.cpp")
target_include_directories(browser_bind_group_lifetime_contract PRIVATE ${RENDERER_INCLUDES})
target_compile_definitions(browser_bind_group_lifetime_contract PRIVATE AURORA TARGET_PC)
target_link_libraries(browser_bind_group_lifetime_contract PRIVATE "-Wl,--start-group" partyboard_browser_core partyboard_browser_renderer "-Wl,--end-group")
target_link_options(browser_bind_group_lifetime_contract PRIVATE -sASYNCIFY -sASYNCIFY_STACK_SIZE=65536 -sASSERTIONS=2 -sALLOW_MEMORY_GROWTH=1 -sENVIRONMENT=web -sMODULARIZE -sEXPORT_ES6)
set_target_properties(browser_bind_group_lifetime_contract PROPERTIES OUTPUT_NAME contract SUFFIX .mjs RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bind-group-lifetime-contract")

add_executable(browser_pipeline_readiness_contract "${PARTYBOARD_ROOT}/tools/test_wasm_pipeline_readiness.cpp")
target_include_directories(browser_pipeline_readiness_contract PRIVATE ${RENDERER_INCLUDES})
target_compile_definitions(browser_pipeline_readiness_contract PRIVATE AURORA TARGET_PC)
target_link_libraries(browser_pipeline_readiness_contract PRIVATE "-Wl,--start-group" partyboard_browser_core partyboard_browser_renderer "-Wl,--end-group")
target_link_options(browser_pipeline_readiness_contract PRIVATE -sASYNCIFY -sASYNCIFY_STACK_SIZE=65536 -sASSERTIONS=2 -sALLOW_MEMORY_GROWTH=1 -sENVIRONMENT=web -sMODULARIZE -sEXPORT_ES6)
set_target_properties(browser_pipeline_readiness_contract PROPERTIES OUTPUT_NAME contract SUFFIX .mjs RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/pipeline-readiness-contract")

# Build the shared game as Wasm objects, retaining unresolved platform references
# until the browser bootstrap supplies real implementations.
include("${PARTYBOARD_ROOT}/files.cmake")
list(TRANSFORM GAME_FILES PREPEND "${PARTYBOARD_ROOT}/")
add_library(partyboard_browser_game STATIC ${GAME_FILES}
  "${PARTYBOARD_ROOT}/src/port/board_random.c"
  "${PARTYBOARD_ROOT}/src/port/simulation_clock.c"
  "${PARTYBOARD_ROOT}/src/port/wasm_coroutine.c")
target_sources(partyboard_browser_game PRIVATE "${PARTYBOARD_ROOT}/src/port/wasm_frame_limiter.c")
target_sources(partyboard_browser_game PRIVATE "${PARTYBOARD_ROOT}/src/port/wasm_retrace.c")
target_sources(partyboard_browser_game PRIVATE "${PARTYBOARD_ROOT}/src/port/wasm_input.c")
# Existing native compatibility behavior, including its documented unfinished
# audio/movie/hardware paths. Linking this does not certify those features.
target_sources(partyboard_browser_game PRIVATE "${PARTYBOARD_ROOT}/src/port/stubs.c")
target_compile_definitions(partyboard_browser_game PRIVATE TARGET_PC TARGET_DOL NON_MATCHING VERSION=1 OPTIMIZED_TEXTURE_LOADING BYTESWAPPING MUSY_TARGET=MUSY_TARGET_PC)
target_compile_options(partyboard_browser_game PRIVATE -sUSE_SDL=3)
target_include_directories(partyboard_browser_game PRIVATE
  "${PARTYBOARD_ROOT}/include" "${PARTYBOARD_ROOT}/extern"
  "${AURORA_ROOT}/include" "${AURORA_ROOT}/include/dolphin"
  "${PARTYBOARD_ROOT}/extern/musyx/include")
target_include_directories(partyboard_browser_game PRIVATE "${PARTYBOARD_ROOT}/extern/libco")

add_subdirectory("${PARTYBOARD_ROOT}/extern/musyx" "musyx" EXCLUDE_FROM_ALL)
target_sources(partyboard_browser_game PRIVATE
  "${PARTYBOARD_ROOT}/src/port/audio.c"
  "${PARTYBOARD_ROOT}/src/port/OS.c")
target_link_libraries(partyboard_browser_game PUBLIC musyx)

set(BROWSER_DOLPHIN_SOURCES)
foreach(component mtx os pad si card)
  file(READ "${AURORA_ROOT}/cmake/aurora_${component}.cmake" COMPONENT_CMAKE)
  string(REGEX MATCHALL "lib/[A-Za-z0-9_/]+\\.(cpp|c)" COMPONENT_SOURCES "${COMPONENT_CMAKE}")
  list(TRANSFORM COMPONENT_SOURCES PREPEND "${AURORA_ROOT}/")
  list(APPEND BROWSER_DOLPHIN_SOURCES ${COMPONENT_SOURCES})
endforeach()
add_library(partyboard_browser_dolphin STATIC ${BROWSER_DOLPHIN_SOURCES})
target_compile_definitions(partyboard_browser_dolphin PRIVATE AURORA TARGET_PC)
target_include_directories(partyboard_browser_dolphin PRIVATE
  "${AURORA_ROOT}/include" "${PARTYBOARD_DEPENDENCIES}/tracy-src/public")
target_link_libraries(partyboard_browser_dolphin PUBLIC partyboard_browser_core)
target_link_libraries(partyboard_browser_game PUBLIC partyboard_browser_dolphin)
target_link_options(partyboard_browser_game INTERFACE "-Wl,-u,OSReport")

# Opt-in strict link audit. Missing real platform implementations must remain
# link errors; do not execute this unconfigured entry point as a game launcher.
add_executable(browser_game_link_audit EXCLUDE_FROM_ALL "${PARTYBOARD_ROOT}/tools/test_wasm_game_link.c")
target_link_libraries(browser_game_link_audit PRIVATE "-Wl,--start-group" partyboard_browser_game partyboard_browser_renderer "-Wl,--end-group")
target_link_options(browser_game_link_audit PRIVATE -sASYNCIFY -sASYNCIFY_STACK_SIZE=65536 -sALLOW_MEMORY_GROWTH=1 -sENVIRONMENT=web -Wl,--error-limit=0 -Wl,--fatal-warnings)

target_sources(partyboard_browser_game PRIVATE
  "${PARTYBOARD_ROOT}/src/port/byteswap.cpp"
  "${PARTYBOARD_ROOT}/src/port/dolassets.cpp"
  "${PARTYBOARD_ROOT}/src/port/version.cpp"
  "${PARTYBOARD_ROOT}/src/port/wasm_language.cpp")
target_link_libraries(browser_aurora_contract PRIVATE partyboard_browser_game)

# Use the pinned C API header only; native nod binaries cannot be linked to Wasm.
add_library(partyboard_browser_dvd STATIC
  "${AURORA_ROOT}/lib/dolphin/dvd/dvd.cpp"
  "${AURORA_ROOT}/lib/dolphin/dvd/fst.cpp")
target_compile_definitions(partyboard_browser_dvd PRIVATE AURORA TARGET_PC)
target_include_directories(partyboard_browser_dvd PRIVATE "${AURORA_ROOT}/include"
  "${PARTYBOARD_DEPENDENCIES}/nod_prebuilt-src/include"
  "${PARTYBOARD_DEPENDENCIES}/tracy-src/public")
target_link_libraries(partyboard_browser_dvd PUBLIC partyboard_browser_core)
target_link_libraries(partyboard_browser_game PUBLIC partyboard_browser_dvd)

# Actual UI/font dependencies built for Wasm from pinned source trees.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Static browser dependencies" FORCE)
# FreeType's CMake uses the host OS variable to select file access sources.
# The Emscripten toolchain is a UNIX host, but the browser build has no POSIX
# open/read/close API; use FreeType's portable implementation instead.
set(_PARTYBOARD_BROWSER_SAVED_WIN32 "${WIN32}")
set(_PARTYBOARD_BROWSER_SAVED_UNIX "${UNIX}")
set(WIN32 FALSE)
set(UNIX FALSE)
foreach(feature ZLIB BZIP2 PNG HARFBUZZ BROTLI)
  set(FT_DISABLE_${feature} ON CACHE BOOL "Optional font compression/shaping" FORCE)
endforeach()
add_subdirectory("${PARTYBOARD_DEPENDENCIES}/freetype-src" "dependencies/freetype" EXCLUDE_FROM_ALL)
set(WIN32 "${_PARTYBOARD_BROWSER_SAVED_WIN32}")
set(UNIX "${_PARTYBOARD_BROWSER_SAVED_UNIX}")
unset(_PARTYBOARD_BROWSER_SAVED_WIN32)
unset(_PARTYBOARD_BROWSER_SAVED_UNIX)
add_library(Freetype::Freetype ALIAS freetype)
set(CMAKE_DISABLE_FIND_PACKAGE_Freetype TRUE)
set(RMLUI_FONT_ENGINE freetype CACHE STRING "Browser font engine" FORCE)
set(RMLUI_PRECOMPILED_HEADERS OFF CACHE BOOL "No cross-build PCH" FORCE)
set(RMLUI_SAMPLES OFF CACHE BOOL "No samples" FORCE)
add_subdirectory("${PARTYBOARD_DEPENDENCIES}/rmlui-src" "dependencies/rmlui" EXCLUDE_FROM_ALL)
add_custom_target(browser_ui_dependencies ALL DEPENDS rmlui_core rmlui_debugger)

add_library(browser_rmlui_backend STATIC
  "${AURORA_ROOT}/lib/rmlui.cpp"
  "${AURORA_ROOT}/lib/rmlui/RuntimeTextureProvider.cpp"
  "${AURORA_ROOT}/lib/rmlui/RmlUi_Backend_Aurora.cpp"
  "${AURORA_ROOT}/lib/rmlui/WebGPURenderInterface.cpp"
  "${AURORA_ROOT}/lib/rmlui/SystemInterface_Aurora.cpp"
  "${AURORA_ROOT}/lib/rmlui/FileInterface_SDL.cpp"
  "${AURORA_ROOT}/lib/rmlui/pipeline.cpp"
  "${PARTYBOARD_DEPENDENCIES}/rmlui-src/Backends/RmlUi_Platform_SDL.cpp")
target_compile_definitions(browser_rmlui_backend PRIVATE AURORA TARGET_PC AURORA_ENABLE_RMLUI AURORA_ENABLE_GX RMLUI_SDL_VERSION_MAJOR=3)
target_include_directories(browser_rmlui_backend PRIVATE "${AURORA_ROOT}/include"
  "${PARTYBOARD_DEPENDENCIES}/tracy-src/public"
  "${PARTYBOARD_DEPENDENCIES}/rmlui-src/Backends")
target_link_libraries(browser_rmlui_backend PUBLIC partyboard_browser_core RmlUi::Core)

foreach(target partyboard_browser_core partyboard_browser_renderer browser_aurora_contract)
  target_compile_definitions(${target} PRIVATE AURORA_ENABLE_RMLUI)
  target_link_libraries(${target} PUBLIC RmlUi::Core)
endforeach()
target_link_libraries(partyboard_browser_core PUBLIC browser_rmlui_backend)
target_link_options(browser_aurora_contract PRIVATE "--embed-file=${PARTYBOARD_ROOT}/res/Inter-Regular.ttf@/Inter-Regular.ttf")

# Preserve the real configuration system, including validation exceptions.
add_library(partyboard_browser_settings STATIC
  "${PARTYBOARD_ROOT}/src/port/settings.cpp"
  "${PARTYBOARD_ROOT}/src/port/config.cpp"
  "${PARTYBOARD_ROOT}/src/port/io.cpp")
target_compile_definitions(partyboard_browser_settings PRIVATE TARGET_PC AURORA)
target_include_directories(partyboard_browser_settings PRIVATE
  "${PARTYBOARD_ROOT}/include" "${PARTYBOARD_ROOT}/extern"
  "${AURORA_ROOT}/include" "${AURORA_ROOT}/include/dolphin"
  "${PARTYBOARD_DEPENDENCIES}/json-src/include")
target_compile_options(partyboard_browser_settings PRIVATE -fexceptions)
# These archives are part of the browser MAIN_MODULE alongside dynamically
# loaded game overlays. Emit relocatable table references for wasm-ld.
target_compile_options(partyboard_browser_settings PRIVATE -fPIC)
target_link_options(partyboard_browser_settings INTERFACE -fexceptions)
target_link_libraries(partyboard_browser_settings PUBLIC partyboard_browser_core)
target_link_libraries(partyboard_browser_game PUBLIC partyboard_browser_settings)

add_executable(browser_settings_contract "${PARTYBOARD_ROOT}/tools/test_wasm_settings.cpp")
target_compile_definitions(browser_settings_contract PRIVATE TARGET_PC AURORA)
target_include_directories(browser_settings_contract PRIVATE
  "${PARTYBOARD_ROOT}/include" "${AURORA_ROOT}/include" "${AURORA_ROOT}/include/dolphin"
  "${PARTYBOARD_DEPENDENCIES}/json-src/include")
target_compile_options(browser_settings_contract PRIVATE -fexceptions)
target_link_libraries(browser_settings_contract PRIVATE partyboard_browser_settings)
target_link_options(browser_settings_contract PRIVATE -sASSERTIONS=2 -sALLOW_MEMORY_GROWTH=1 -sENVIRONMENT=web -sMODULARIZE -sEXPORT_ES6)
set_target_properties(browser_settings_contract PROPERTIES OUTPUT_NAME contract SUFFIX .mjs RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/settings-contract")

# Compile real menu components. Native prelaunch still needs browser file/thread
# integration; keep it explicitly excluded rather than replace it with a stub.
set(BROWSER_MENU_SOURCES ${PORT_FILES})
list(FILTER BROWSER_MENU_SOURCES INCLUDE REGEX "^src/port/ui/.*\\.cpp$")
list(FILTER BROWSER_MENU_SOURCES EXCLUDE REGEX "/prelaunch\\.cpp$")
list(TRANSFORM BROWSER_MENU_SOURCES PREPEND "${PARTYBOARD_ROOT}/")
add_library(partyboard_browser_menu STATIC ${BROWSER_MENU_SOURCES})
target_compile_definitions(partyboard_browser_menu PRIVATE TARGET_PC AURORA AURORA_ENABLE_RMLUI)
target_include_directories(partyboard_browser_menu PRIVATE
  "${PARTYBOARD_ROOT}/include" "${PARTYBOARD_ROOT}/extern"
  "${AURORA_ROOT}/include" "${AURORA_ROOT}/include/dolphin"
  "${PARTYBOARD_DEPENDENCIES}/json-src/include")
target_compile_options(partyboard_browser_menu PRIVATE -fexceptions)
target_link_libraries(partyboard_browser_menu PUBLIC partyboard_browser_settings RmlUi::Core)
target_link_libraries(browser_aurora_contract PRIVATE partyboard_browser_menu)
target_link_libraries(partyboard_browser_game PUBLIC partyboard_browser_menu)

add_library(partyboard_browser_trace STATIC "${PARTYBOARD_ROOT}/src/port/input_trace.cpp")
target_compile_definitions(partyboard_browser_trace PUBLIC TARGET_PC AURORA)
target_include_directories(partyboard_browser_trace PUBLIC "${PARTYBOARD_ROOT}/include" "${AURORA_ROOT}/include")
target_compile_options(partyboard_browser_trace PRIVATE -fexceptions)
target_compile_options(partyboard_browser_trace PRIVATE -fPIC)
target_link_options(partyboard_browser_trace INTERFACE -fexceptions)
target_link_libraries(partyboard_browser_game PUBLIC partyboard_browser_trace)
add_executable(browser_trace_contract "${PARTYBOARD_ROOT}/tools/test_input_trace.cpp")
target_link_libraries(browser_trace_contract PRIVATE partyboard_browser_trace)
target_link_options(browser_trace_contract PRIVATE -sASSERTIONS=2 -sALLOW_MEMORY_GROWTH=1 -sENVIRONMENT=web -sMODULARIZE -sEXPORT_ES6)
set_target_properties(browser_trace_contract PROPERTIES OUTPUT_NAME contract SUFFIX .mjs RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/trace-contract")
add_executable(browser_input_contract "${PARTYBOARD_ROOT}/tools/test_wasm_input.cpp" "${PARTYBOARD_ROOT}/src/port/wasm_input.c")
target_compile_definitions(browser_input_contract PRIVATE TARGET_PC AURORA)
target_include_directories(browser_input_contract PRIVATE "${PARTYBOARD_ROOT}/include" "${AURORA_ROOT}/include")
target_link_options(browser_input_contract PRIVATE -sASYNCIFY -sASSERTIONS=2 -sENVIRONMENT=web -sMODULARIZE -sEXPORT_ES6)
set_target_properties(browser_input_contract PROPERTIES OUTPUT_NAME contract SUFFIX .mjs RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/input-contract")

add_executable(browser_rel_asset_contract "${PARTYBOARD_ROOT}/tools/test_rel_assets.cpp" "${PARTYBOARD_ROOT}/src/port/dolassets.cpp")
target_compile_definitions(browser_rel_asset_contract PRIVATE TARGET_PC AURORA)
target_include_directories(browser_rel_asset_contract PRIVATE "${PARTYBOARD_ROOT}/include" "${AURORA_ROOT}/include" "${AURORA_ROOT}/include/dolphin")
target_link_options(browser_rel_asset_contract PRIVATE -sASSERTIONS=2 -sENVIRONMENT=web -sMODULARIZE -sEXPORT_ES6)
set_target_properties(browser_rel_asset_contract PROPERTIES OUTPUT_NAME contract SUFFIX .mjs RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/rel-asset-contract")
set(PARTYBOARD_NOD_WASM_LIBRARY "" CACHE FILEPATH "Pinned nod-ffi Emscripten static archive")
add_executable(browser_fiber_log_contract "${PARTYBOARD_ROOT}/tools/test_wasm_fiber_log.c" "${PARTYBOARD_ROOT}/src/port/stubs.c" "${PARTYBOARD_ROOT}/src/game/memory.c" "${PARTYBOARD_ROOT}/src/port/wasm_coroutine.c")
target_compile_definitions(browser_fiber_log_contract PRIVATE TARGET_PC AURORA MUSY_TARGET=MUSY_TARGET_PC)
target_include_directories(browser_fiber_log_contract PRIVATE "${PARTYBOARD_ROOT}/extern/musyx/include" "${PARTYBOARD_ROOT}/include" "${PARTYBOARD_ROOT}/extern" "${PARTYBOARD_ROOT}/extern/libco" "${AURORA_ROOT}/include" "${AURORA_ROOT}/include/dolphin")
target_link_options(browser_fiber_log_contract PRIVATE -sASYNCIFY -sASYNCIFY_STACK_SIZE=65536 -sSTACK_OVERFLOW_CHECK=2 -sASSERTIONS=2 -sENVIRONMENT=web -sMODULARIZE -sEXPORT_ES6 "-Wl,-u,OSReport")
set_target_properties(browser_fiber_log_contract PROPERTIES OUTPUT_NAME contract SUFFIX .mjs RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/fiber-log-contract")
set(PARTYBOARD_BOOT_WASM "" CACHE FILEPATH "Compiled bootDll side module for optional real-engine load audit")
set(PARTYBOARD_OVERLAY_PACKAGE_DIR "" CACHE PATH "Directory containing the complete current-browser overlay Wasm package")
if(PARTYBOARD_OVERLAY_PACKAGE_DIR)
  if(NOT EXISTS "${PARTYBOARD_OVERLAY_PACKAGE_DIR}/bootDll.wasm")
    message(FATAL_ERROR "PARTYBOARD_OVERLAY_PACKAGE_DIR must contain bootDll.wasm")
  endif()
  set(PARTYBOARD_BOOT_WASM "${PARTYBOARD_OVERLAY_PACKAGE_DIR}/bootDll.wasm" CACHE FILEPATH "Compiled bootDll side module for optional real-engine load audit" FORCE)
endif()
set(PARTYBOARD_MENT_WASM "" CACHE FILEPATH "Compiled game-setup module for optional browser load audit")
set(PARTYBOARD_W04_WASM "" CACHE FILEPATH "Compiled w04 board module for optional browser load audit")
set(PARTYBOARD_INST_WASM "" CACHE FILEPATH "Compiled minigame instruction module for optional browser load audit")
set(PARTYBOARD_RESULT_WASM "" CACHE FILEPATH "Compiled results module for optional browser load audit")
set(PARTYBOARD_MINIGAME_AUDIT_DIR "" CACHE PATH "Directory containing m416 through m424 modules for load-only audits")
set(PARTYBOARD_MODESEL_WASM "" CACHE FILEPATH "Compiled mode-select side module for optional real-engine load audit")
if(PARTYBOARD_MODESEL_WASM)
  add_executable(browser_modesel_link_contract EXCLUDE_FROM_ALL "${PARTYBOARD_ROOT}/tools/test_wasm_modesel_link.c")
  target_link_libraries(browser_modesel_link_contract PRIVATE "-Wl,--start-group" partyboard_browser_game partyboard_browser_renderer "-Wl,--end-group" "${PARTYBOARD_MODESEL_WASM}")
  target_link_options(browser_modesel_link_contract PRIVATE -sMAIN_MODULE=2 -sASYNCIFY -sASYNCIFY_STACK_SIZE=65536 -sASSERTIONS=2 -sALLOW_MEMORY_GROWTH=1 -sENVIRONMENT=web -sMODULARIZE -sEXPORT_ES6 "--embed-file=${PARTYBOARD_MODESEL_WASM}@/modeseldll.wasm")
  set_target_properties(browser_modesel_link_contract PROPERTIES OUTPUT_NAME contract SUFFIX .mjs RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/modesel-link-contract")
  add_custom_command(TARGET browser_modesel_link_contract POST_BUILD COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${PARTYBOARD_MODESEL_WASM}" "${CMAKE_BINARY_DIR}/modesel-link-contract/modeseldll.wasm")
endif()
if(PARTYBOARD_BOOT_WASM)
  add_executable(browser_boot_reset_contract EXCLUDE_FROM_ALL "${PARTYBOARD_ROOT}/tools/test_wasm_boot_link.c")
  target_compile_definitions(browser_boot_reset_contract PRIVATE TEST_OVERLAY_RESET)
  find_program(PARTYBOARD_NODE_EXECUTABLE node REQUIRED)
  execute_process(COMMAND "${PARTYBOARD_NODE_EXECUTABLE}" "${PARTYBOARD_ROOT}/tools/wasm_overlay_exports.cjs" "${PARTYBOARD_BOOT_WASM}" OUTPUT_VARIABLE PARTYBOARD_BOOT_EXPORT_FLAGS COMMAND_ERROR_IS_FATAL ANY)
  target_link_libraries(browser_boot_reset_contract PRIVATE "-Wl,--start-group" partyboard_browser_game partyboard_browser_renderer "-Wl,--end-group")
  target_link_options(browser_boot_reset_contract PRIVATE ${PARTYBOARD_BOOT_EXPORT_FLAGS} -sMAIN_MODULE=2 -sASYNCIFY -sASYNCIFY_STACK_SIZE=65536 -sASSERTIONS=2 -sALLOW_MEMORY_GROWTH=1 -sENVIRONMENT=web -sMODULARIZE -sEXPORT_ES6 "--embed-file=${PARTYBOARD_BOOT_WASM}@/bootDll.wasm" "--js-library=${PARTYBOARD_ROOT}/tools/wasm_overlay_state.js")
  set_target_properties(browser_boot_reset_contract PROPERTIES OUTPUT_NAME contract SUFFIX .mjs RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/boot-reset-contract")
  add_custom_command(TARGET browser_boot_reset_contract POST_BUILD COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${PARTYBOARD_BOOT_WASM}" "${CMAKE_BINARY_DIR}/boot-reset-contract/bootDll.wasm")
  add_executable(browser_boot_link_contract EXCLUDE_FROM_ALL "${PARTYBOARD_ROOT}/tools/test_wasm_boot_link.c")
  target_link_libraries(browser_boot_link_contract PRIVATE "-Wl,--start-group" partyboard_browser_game partyboard_browser_renderer "-Wl,--end-group" "${PARTYBOARD_BOOT_WASM}")
  target_link_options(browser_boot_link_contract PRIVATE -sMAIN_MODULE=2 -sASYNCIFY -sASYNCIFY_STACK_SIZE=65536 -sASSERTIONS=2 -sALLOW_MEMORY_GROWTH=1 -sENVIRONMENT=web -sMODULARIZE -sEXPORT_ES6 "--embed-file=${PARTYBOARD_BOOT_WASM}@/bootDll.wasm")
  set_target_properties(browser_boot_link_contract PROPERTIES OUTPUT_NAME contract SUFFIX .mjs RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/boot-link-contract")
  add_custom_command(TARGET browser_boot_link_contract POST_BUILD COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${PARTYBOARD_BOOT_WASM}" "${CMAKE_BINARY_DIR}/boot-link-contract/bootDll.wasm")
endif()
add_executable(browser_overlay_gate_contract "${PARTYBOARD_ROOT}/tools/test_wasm_overlay_gate.c" "${PARTYBOARD_ROOT}/src/game/objdll.c")
target_compile_definitions(browser_overlay_gate_contract PRIVATE TARGET_PC AURORA)
target_include_directories(browser_overlay_gate_contract PRIVATE "${PARTYBOARD_ROOT}/extern" "${PARTYBOARD_ROOT}/include" "${AURORA_ROOT}/include" "${AURORA_ROOT}/include/dolphin")
target_link_options(browser_overlay_gate_contract PRIVATE -sASSERTIONS=2 -sENVIRONMENT=web -sMODULARIZE -sEXPORT_ES6)
set_target_properties(browser_overlay_gate_contract PROPERTIES OUTPUT_NAME contract SUFFIX .mjs RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/overlay-gate-contract")
if(PARTYBOARD_NOD_WASM_LIBRARY)
  if(NOT EXISTS "${PARTYBOARD_NOD_WASM_LIBRARY}")
    message(FATAL_ERROR "PARTYBOARD_NOD_WASM_LIBRARY does not exist")
  endif()
  add_library(browser_nod STATIC IMPORTED)
  set_target_properties(browser_nod PROPERTIES IMPORTED_LOCATION "${PARTYBOARD_NOD_WASM_LIBRARY}")
  target_link_libraries(partyboard_browser_dvd PUBLIC browser_nod)
  add_executable(browser_disc_contract "${PARTYBOARD_ROOT}/tools/test_wasm_disc.cpp")
  target_compile_definitions(browser_disc_contract PRIVATE TARGET_PC AURORA)
  target_include_directories(browser_disc_contract PRIVATE "${AURORA_ROOT}/include")
  target_include_directories(browser_disc_contract PRIVATE "${PARTYBOARD_DEPENDENCIES}/nod_prebuilt-src/include")
  target_link_libraries(browser_disc_contract PRIVATE partyboard_browser_dvd partyboard_browser_dolphin browser_nod)
  target_link_options(browser_disc_contract PRIVATE -fexceptions -sUSE_SDL=3 -sASSERTIONS=2 -sALLOW_MEMORY_GROWTH=1 -sENVIRONMENT=web -sMODULARIZE -sEXPORT_ES6)
  set_target_properties(browser_disc_contract PROPERTIES OUTPUT_NAME contract SUFFIX .mjs RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/disc-contract")
  add_executable(browser_asset_mount_contract "${PARTYBOARD_ROOT}/tools/test_wasm_asset_mount.cpp"
    "${PARTYBOARD_ROOT}/src/port/version.cpp" "${PARTYBOARD_ROOT}/src/port/dolassets.cpp")
  target_compile_definitions(browser_asset_mount_contract PRIVATE TARGET_PC AURORA)
  target_include_directories(browser_asset_mount_contract PRIVATE "${AURORA_ROOT}/include" "${AURORA_ROOT}/include/dolphin" "${PARTYBOARD_ROOT}/include" "${PARTYBOARD_ROOT}/extern")
  target_link_libraries(browser_asset_mount_contract PRIVATE partyboard_browser_dvd partyboard_browser_dolphin browser_nod)
  target_link_options(browser_asset_mount_contract PRIVATE -fexceptions -sUSE_SDL=3 -sASSERTIONS=2 -sALLOW_MEMORY_GROWTH=1 -sENVIRONMENT=web,worker -sMODULARIZE -sEXPORT_ES6 -lworkerfs.js -sEXPORTED_RUNTIME_METHODS=FS,WORKERFS)
  set_target_properties(browser_asset_mount_contract PROPERTIES OUTPUT_NAME contract SUFFIX .mjs RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/asset-mount-contract")
  # Experimental main-thread Blob reads; explicitly opt in for runtime integration.
  add_executable(browser_async_asset_contract EXCLUDE_FROM_ALL "${PARTYBOARD_ROOT}/tools/test_wasm_asset_mount.cpp"
    "${PARTYBOARD_ROOT}/src/port/version.cpp" "${PARTYBOARD_ROOT}/src/port/dolassets.cpp")
  target_compile_definitions(browser_async_asset_contract PRIVATE TARGET_PC AURORA)
  target_include_directories(browser_async_asset_contract PRIVATE "${AURORA_ROOT}/include" "${AURORA_ROOT}/include/dolphin" "${PARTYBOARD_ROOT}/include" "${PARTYBOARD_ROOT}/extern")
  target_link_libraries(browser_async_asset_contract PRIVATE partyboard_browser_dvd partyboard_browser_dolphin browser_nod)
  target_link_options(browser_async_asset_contract PRIVATE -fexceptions -sUSE_SDL=3 -sASSERTIONS=2 -sALLOW_MEMORY_GROWTH=1 -sENVIRONMENT=web,worker -sMODULARIZE -sEXPORT_ES6 -lworkerfs.js -sEXPORTED_RUNTIME_METHODS=FS,WORKERFS,Asyncify,HEAPU8 -sASYNCIFY -sASYNCIFY_STACK_SIZE=65536 "-sASYNCIFY_IMPORTS=*.fd_read,env.__syscall_pread64")
  set_target_properties(browser_async_asset_contract PROPERTIES OUTPUT_NAME contract SUFFIX .mjs RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/async-asset-contract")
  target_link_options(browser_async_asset_contract PRIVATE "--js-library=${PARTYBOARD_ROOT}/tools/async_blob_read.js")
  add_executable(browser_disc_renderer_contract EXCLUDE_FROM_ALL "${PARTYBOARD_ROOT}/tools/test_wasm_disc_renderer.cpp")
  target_compile_definitions(browser_disc_renderer_contract PRIVATE TARGET_PC AURORA)
  target_include_directories(browser_disc_renderer_contract PRIVATE "${PARTYBOARD_DEPENDENCIES}/json-src/include" "${AURORA_ROOT}/include" "${AURORA_ROOT}/include/dolphin" "${PARTYBOARD_ROOT}/include")
  target_link_libraries(browser_disc_renderer_contract PRIVATE "-Wl,--start-group" partyboard_browser_core partyboard_browser_renderer partyboard_browser_game "-Wl,--end-group")
  target_link_options(browser_disc_renderer_contract PRIVATE -sASYNCIFY -sASYNCIFY_STACK_SIZE=65536 -sASSERTIONS=2 -sALLOW_MEMORY_GROWTH=1 -sENVIRONMENT=web,worker -sMODULARIZE -sEXPORT_ES6 -lworkerfs.js -sEXPORTED_RUNTIME_METHODS=FS,WORKERFS,Asyncify,HEAPU8,requestFullscreen,ccall "--js-library=${PARTYBOARD_ROOT}/tools/async_blob_read.js" "-sASYNCIFY_IMPORTS=*.fd_read")
  set_target_properties(browser_disc_renderer_contract PROPERTIES OUTPUT_NAME contract SUFFIX .mjs RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/disc-renderer-contract")
  if(PARTYBOARD_BOOT_WASM)
    set(PARTYBOARD_SETUP_OUTPUT_DIR "${CMAKE_BINARY_DIR}/boot-setup-contract" CACHE PATH "Standalone boot test output directory")
    add_executable(browser_boot_setup_contract EXCLUDE_FROM_ALL "${PARTYBOARD_ROOT}/tools/test_wasm_disc_renderer.cpp" "${PARTYBOARD_ROOT}/src/port/wasm_platform.cpp")
    set_property(TARGET browser_boot_setup_contract APPEND PROPERTY LINK_DEPENDS "${PARTYBOARD_BOOT_WASM}")
    target_compile_definitions(browser_boot_setup_contract PRIVATE TARGET_PC AURORA PARTYBOARD_BOOT_SETUP_PROBE OPTIMIZED_TEXTURE_LOADING BYTESWAPPING)
    target_include_directories(browser_boot_setup_contract PRIVATE "${PARTYBOARD_DEPENDENCIES}/json-src/include" "${AURORA_ROOT}/include" "${AURORA_ROOT}/include/dolphin" "${PARTYBOARD_ROOT}/include" "${PARTYBOARD_ROOT}/extern")
    target_link_libraries(browser_boot_setup_contract PRIVATE "-Wl,--start-group" partyboard_browser_core partyboard_browser_renderer partyboard_browser_game "-Wl,--end-group")
    set(PARTYBOARD_SETUP_EXPORT_FLAGS ${PARTYBOARD_BOOT_EXPORT_FLAGS})
    target_link_options(browser_boot_setup_contract PRIVATE "--pre-js=${PARTYBOARD_ROOT}/tools/asyncify_rewind_diagnostics.js")
    set_property(TARGET browser_boot_setup_contract APPEND PROPERTY LINK_DEPENDS "${PARTYBOARD_ROOT}/tools/asyncify_rewind_diagnostics.js")
    target_link_options(browser_boot_setup_contract PRIVATE "@${CMAKE_BINARY_DIR}/setup-overlay-exports.rsp" "--js-library=${PARTYBOARD_ROOT}/tools/wasm_overlay_state.js")
    target_link_options(browser_boot_setup_contract PRIVATE -sMAIN_MODULE=2 -sASYNCIFY -sASYNCIFY_STACK_SIZE=65536 -sASSERTIONS=2 -sALLOW_MEMORY_GROWTH=1 -sENVIRONMENT=web,worker -sMODULARIZE -sEXPORT_ES6 -lworkerfs.js -sEXPORTED_RUNTIME_METHODS=FS,WORKERFS,Asyncify,HEAPU8,requestFullscreen,ccall "--js-library=${PARTYBOARD_ROOT}/tools/async_blob_read.js" "-sASYNCIFY_IMPORTS=*.fd_read" "--embed-file=${PARTYBOARD_BOOT_WASM}@/bootDll.wasm")
    set_target_properties(browser_boot_setup_contract PROPERTIES OUTPUT_NAME contract SUFFIX .mjs RUNTIME_OUTPUT_DIRECTORY "${PARTYBOARD_SETUP_OUTPUT_DIR}")
    add_custom_command(TARGET browser_boot_setup_contract POST_BUILD COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${PARTYBOARD_BOOT_WASM}" "${PARTYBOARD_SETUP_OUTPUT_DIR}/bootDll.wasm")
    if(PARTYBOARD_OVERLAY_PACKAGE_DIR)
      file(GLOB PARTYBOARD_OVERLAY_PACKAGE_FILES CONFIGURE_DEPENDS "${PARTYBOARD_OVERLAY_PACKAGE_DIR}/*.wasm")
      if(NOT PARTYBOARD_OVERLAY_PACKAGE_FILES)
        message(FATAL_ERROR "No Wasm side modules found in PARTYBOARD_OVERLAY_PACKAGE_DIR")
      endif()
      foreach(PARTYBOARD_OVERLAY_PATH IN LISTS PARTYBOARD_OVERLAY_PACKAGE_FILES)
        get_filename_component(PARTYBOARD_OVERLAY_NAME "${PARTYBOARD_OVERLAY_PATH}" NAME)
        if(NOT PARTYBOARD_OVERLAY_NAME STREQUAL "bootDll.wasm")
          execute_process(COMMAND "${PARTYBOARD_NODE_EXECUTABLE}" "${PARTYBOARD_ROOT}/tools/wasm_overlay_exports.cjs" "${PARTYBOARD_OVERLAY_PATH}" OUTPUT_VARIABLE PARTYBOARD_OVERLAY_EXPORT_FLAGS COMMAND_ERROR_IS_FATAL ANY)
          list(APPEND PARTYBOARD_SETUP_EXPORT_FLAGS ${PARTYBOARD_OVERLAY_EXPORT_FLAGS})
          set_property(TARGET browser_boot_setup_contract APPEND PROPERTY LINK_DEPENDS "${PARTYBOARD_OVERLAY_PATH}")
          target_link_options(browser_boot_setup_contract PRIVATE "--embed-file=${PARTYBOARD_OVERLAY_PATH}@/${PARTYBOARD_OVERLAY_NAME}")
        endif()
      endforeach()
    else()
    if(PARTYBOARD_MODESEL_WASM)
      execute_process(COMMAND "${PARTYBOARD_NODE_EXECUTABLE}" "${PARTYBOARD_ROOT}/tools/wasm_overlay_exports.cjs" "${PARTYBOARD_MODESEL_WASM}" OUTPUT_VARIABLE PARTYBOARD_MENU_EXPORT_FLAGS COMMAND_ERROR_IS_FATAL ANY)
      list(APPEND PARTYBOARD_SETUP_EXPORT_FLAGS ${PARTYBOARD_MENU_EXPORT_FLAGS})
      set_property(TARGET browser_boot_setup_contract APPEND PROPERTY LINK_DEPENDS "${PARTYBOARD_MODESEL_WASM}")
      target_link_options(browser_boot_setup_contract PRIVATE "--embed-file=${PARTYBOARD_MODESEL_WASM}@/modeseldll.wasm")
      add_custom_command(TARGET browser_boot_setup_contract POST_BUILD COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${PARTYBOARD_MODESEL_WASM}" "${PARTYBOARD_SETUP_OUTPUT_DIR}/modeseldll.wasm")
    endif()
    if(PARTYBOARD_MENT_WASM)
      execute_process(COMMAND "${PARTYBOARD_NODE_EXECUTABLE}" "${PARTYBOARD_ROOT}/tools/wasm_overlay_exports.cjs" "${PARTYBOARD_MENT_WASM}" OUTPUT_VARIABLE PARTYBOARD_MENT_EXPORT_FLAGS COMMAND_ERROR_IS_FATAL ANY)
      list(APPEND PARTYBOARD_SETUP_EXPORT_FLAGS ${PARTYBOARD_MENT_EXPORT_FLAGS})
      set_property(TARGET browser_boot_setup_contract APPEND PROPERTY LINK_DEPENDS "${PARTYBOARD_MENT_WASM}")
      target_compile_definitions(browser_boot_setup_contract PRIVATE PARTYBOARD_MENT_LOAD_PROBE)
      target_link_options(browser_boot_setup_contract PRIVATE "--embed-file=${PARTYBOARD_MENT_WASM}@/mentDll.wasm")
    endif()
    if(PARTYBOARD_W04_WASM)
      execute_process(COMMAND "${PARTYBOARD_NODE_EXECUTABLE}" "${PARTYBOARD_ROOT}/tools/wasm_overlay_exports.cjs" "${PARTYBOARD_W04_WASM}" OUTPUT_VARIABLE PARTYBOARD_W04_EXPORT_FLAGS COMMAND_ERROR_IS_FATAL ANY)
      list(APPEND PARTYBOARD_SETUP_EXPORT_FLAGS ${PARTYBOARD_W04_EXPORT_FLAGS})
      set_property(TARGET browser_boot_setup_contract APPEND PROPERTY LINK_DEPENDS "${PARTYBOARD_W04_WASM}")
      target_compile_definitions(browser_boot_setup_contract PRIVATE PARTYBOARD_W04_LOAD_PROBE)
      target_link_options(browser_boot_setup_contract PRIVATE "--embed-file=${PARTYBOARD_W04_WASM}@/w04Dll.wasm")
    endif()
    if(PARTYBOARD_INST_WASM)
      execute_process(COMMAND "${PARTYBOARD_NODE_EXECUTABLE}" "${PARTYBOARD_ROOT}/tools/wasm_overlay_exports.cjs" "${PARTYBOARD_INST_WASM}" OUTPUT_VARIABLE PARTYBOARD_INST_EXPORT_FLAGS COMMAND_ERROR_IS_FATAL ANY)
      list(APPEND PARTYBOARD_SETUP_EXPORT_FLAGS ${PARTYBOARD_INST_EXPORT_FLAGS})
      set_property(TARGET browser_boot_setup_contract APPEND PROPERTY LINK_DEPENDS "${PARTYBOARD_INST_WASM}")
      target_compile_definitions(browser_boot_setup_contract PRIVATE PARTYBOARD_INST_LOAD_PROBE)
      target_link_options(browser_boot_setup_contract PRIVATE "--embed-file=${PARTYBOARD_INST_WASM}@/instDll.wasm")
    endif()
    if(PARTYBOARD_RESULT_WASM)
      execute_process(COMMAND "${PARTYBOARD_NODE_EXECUTABLE}" "${PARTYBOARD_ROOT}/tools/wasm_overlay_exports.cjs" "${PARTYBOARD_RESULT_WASM}" OUTPUT_VARIABLE PARTYBOARD_RESULT_EXPORT_FLAGS COMMAND_ERROR_IS_FATAL ANY)
      list(APPEND PARTYBOARD_SETUP_EXPORT_FLAGS ${PARTYBOARD_RESULT_EXPORT_FLAGS})
      set_property(TARGET browser_boot_setup_contract APPEND PROPERTY LINK_DEPENDS "${PARTYBOARD_RESULT_WASM}")
      target_compile_definitions(browser_boot_setup_contract PRIVATE PARTYBOARD_RESULT_LOAD_PROBE)
      target_link_options(browser_boot_setup_contract PRIVATE "--embed-file=${PARTYBOARD_RESULT_WASM}@/resultDll.wasm")
    endif()
    if(PARTYBOARD_MINIGAME_AUDIT_DIR)
      foreach(PARTYBOARD_MG_ID RANGE 416 424)
        set(PARTYBOARD_MG_PATH "${PARTYBOARD_MINIGAME_AUDIT_DIR}/m${PARTYBOARD_MG_ID}Dll.wasm")
        execute_process(COMMAND "${PARTYBOARD_NODE_EXECUTABLE}" "${PARTYBOARD_ROOT}/tools/wasm_overlay_exports.cjs" "${PARTYBOARD_MG_PATH}" OUTPUT_VARIABLE PARTYBOARD_MG_EXPORT_FLAGS COMMAND_ERROR_IS_FATAL ANY)
        list(APPEND PARTYBOARD_SETUP_EXPORT_FLAGS ${PARTYBOARD_MG_EXPORT_FLAGS})
        set_property(TARGET browser_boot_setup_contract APPEND PROPERTY LINK_DEPENDS "${PARTYBOARD_MG_PATH}")
        target_link_options(browser_boot_setup_contract PRIVATE "--embed-file=${PARTYBOARD_MG_PATH}@/m${PARTYBOARD_MG_ID}Dll.wasm")
      endforeach()
      target_compile_definitions(browser_boot_setup_contract PRIVATE PARTYBOARD_MINIGAME_LOAD_AUDIT)
    endif()
    set(PARTYBOARD_BOARD_AUDIT_DIR "" CACHE PATH "Optional remaining board modules for load validation")
    set(PARTYBOARD_MODE_MENU_DIR "" CACHE PATH "Optional alternate mode and menu overlays")
    if(PARTYBOARD_MODE_MENU_DIR)
      foreach(PARTYBOARD_MENU_NAME IN ITEMS mgmodedll mpexDll option present mstoryDll mstory2Dll mstory3Dll mstory4Dll selmenuDll staffDll subchrselDll ztardll)
        set(PARTYBOARD_MENU_PATH "${PARTYBOARD_MODE_MENU_DIR}/${PARTYBOARD_MENU_NAME}.wasm")
        execute_process(COMMAND "${PARTYBOARD_NODE_EXECUTABLE}" "${PARTYBOARD_ROOT}/tools/wasm_overlay_exports.cjs" "${PARTYBOARD_MENU_PATH}" OUTPUT_VARIABLE PARTYBOARD_MENU_EXPORT_FLAGS COMMAND_ERROR_IS_FATAL ANY)
        list(APPEND PARTYBOARD_SETUP_EXPORT_FLAGS ${PARTYBOARD_MENU_EXPORT_FLAGS})
        set_property(TARGET browser_boot_setup_contract APPEND PROPERTY LINK_DEPENDS "${PARTYBOARD_MENU_PATH}")
        target_link_options(browser_boot_setup_contract PRIVATE "--embed-file=${PARTYBOARD_MENU_PATH}@/${PARTYBOARD_MENU_NAME}.wasm")
      endforeach()
      target_compile_definitions(browser_boot_setup_contract PRIVATE PARTYBOARD_MODE_MENU_AUDIT)
    endif()
    set(PARTYBOARD_REMAINING_MINIGAME_DIR "" CACHE PATH "Optional remaining minigames for load validation")
    if(PARTYBOARD_REMAINING_MINIGAME_DIR)
      foreach(PARTYBOARD_MG_ID IN ITEMS 450 451 453 455 456 457 458 459 460 461 462 463)
        set(PARTYBOARD_MG_PATH "${PARTYBOARD_REMAINING_MINIGAME_DIR}/m${PARTYBOARD_MG_ID}Dll.wasm")
        if(NOT EXISTS "${PARTYBOARD_MG_PATH}")
          set(PARTYBOARD_MG_PATH "${PARTYBOARD_REMAINING_MINIGAME_DIR}/m${PARTYBOARD_MG_ID}dll.wasm")
        endif()
        execute_process(COMMAND "${PARTYBOARD_NODE_EXECUTABLE}" "${PARTYBOARD_ROOT}/tools/wasm_overlay_exports.cjs" "${PARTYBOARD_MG_PATH}" OUTPUT_VARIABLE PARTYBOARD_MG_EXPORT_FLAGS COMMAND_ERROR_IS_FATAL ANY)
        list(APPEND PARTYBOARD_SETUP_EXPORT_FLAGS ${PARTYBOARD_MG_EXPORT_FLAGS})
        set_property(TARGET browser_boot_setup_contract APPEND PROPERTY LINK_DEPENDS "${PARTYBOARD_MG_PATH}")
        target_link_options(browser_boot_setup_contract PRIVATE "--embed-file=${PARTYBOARD_MG_PATH}@/m${PARTYBOARD_MG_ID}Dll.wasm")
      endforeach()
      target_compile_definitions(browser_boot_setup_contract PRIVATE PARTYBOARD_REMAINING_MINIGAME_AUDIT)
    endif()
    set(PARTYBOARD_COMPETITIVE_AUDIT_DIR "" CACHE PATH "Optional m425 through m449 modules for load validation")
    if(PARTYBOARD_COMPETITIVE_AUDIT_DIR)
      foreach(PARTYBOARD_MG_ID RANGE 425 449)
        set(PARTYBOARD_MG_PATH "${PARTYBOARD_COMPETITIVE_AUDIT_DIR}/m${PARTYBOARD_MG_ID}Dll.wasm")
        if(NOT EXISTS "${PARTYBOARD_MG_PATH}")
          set(PARTYBOARD_MG_PATH "${PARTYBOARD_COMPETITIVE_AUDIT_DIR}/m${PARTYBOARD_MG_ID}dll.wasm")
        endif()
        execute_process(COMMAND "${PARTYBOARD_NODE_EXECUTABLE}" "${PARTYBOARD_ROOT}/tools/wasm_overlay_exports.cjs" "${PARTYBOARD_MG_PATH}" OUTPUT_VARIABLE PARTYBOARD_MG_EXPORT_FLAGS COMMAND_ERROR_IS_FATAL ANY)
        list(APPEND PARTYBOARD_SETUP_EXPORT_FLAGS ${PARTYBOARD_MG_EXPORT_FLAGS})
        set_property(TARGET browser_boot_setup_contract APPEND PROPERTY LINK_DEPENDS "${PARTYBOARD_MG_PATH}")
        target_link_options(browser_boot_setup_contract PRIVATE "--embed-file=${PARTYBOARD_MG_PATH}@/m${PARTYBOARD_MG_ID}Dll.wasm")
      endforeach()
      target_compile_definitions(browser_boot_setup_contract PRIVATE PARTYBOARD_COMPETITIVE_LOAD_AUDIT)
    endif()
    set(PARTYBOARD_FOUR_PLAYER_AUDIT_DIR "" CACHE PATH "Optional four-player minigame modules for load validation")
    if(PARTYBOARD_FOUR_PLAYER_AUDIT_DIR)
      foreach(PARTYBOARD_MG_ID RANGE 401 415)
        if(PARTYBOARD_MG_ID EQUAL 410 AND PARTYBOARD_THREE_THROW_WASM)
          continue()
        endif()
        set(PARTYBOARD_MG_PATH "${PARTYBOARD_FOUR_PLAYER_AUDIT_DIR}/m${PARTYBOARD_MG_ID}Dll.wasm")
        # The upstream table spells m407dll with a lower-case suffix.
        if(PARTYBOARD_MG_ID EQUAL 407)
          set(PARTYBOARD_MG_PATH "${PARTYBOARD_FOUR_PLAYER_AUDIT_DIR}/m407dll.wasm")
        endif()
        execute_process(COMMAND "${PARTYBOARD_NODE_EXECUTABLE}" "${PARTYBOARD_ROOT}/tools/wasm_overlay_exports.cjs" "${PARTYBOARD_MG_PATH}" OUTPUT_VARIABLE PARTYBOARD_MG_EXPORT_FLAGS COMMAND_ERROR_IS_FATAL ANY)
        list(APPEND PARTYBOARD_SETUP_EXPORT_FLAGS ${PARTYBOARD_MG_EXPORT_FLAGS})
        set_property(TARGET browser_boot_setup_contract APPEND PROPERTY LINK_DEPENDS "${PARTYBOARD_MG_PATH}")
        target_link_options(browser_boot_setup_contract PRIVATE "--embed-file=${PARTYBOARD_MG_PATH}@/m${PARTYBOARD_MG_ID}Dll.wasm")
      endforeach()
      target_compile_definitions(browser_boot_setup_contract PRIVATE PARTYBOARD_FOUR_PLAYER_LOAD_AUDIT)
    endif()
    set(PARTYBOARD_THREE_THROW_WASM "" CACHE FILEPATH "Optional Three Throw module")
    if(PARTYBOARD_THREE_THROW_WASM)
      execute_process(COMMAND "${PARTYBOARD_NODE_EXECUTABLE}" "${PARTYBOARD_ROOT}/tools/wasm_overlay_exports.cjs" "${PARTYBOARD_THREE_THROW_WASM}" OUTPUT_VARIABLE PARTYBOARD_THREE_THROW_EXPORT_FLAGS COMMAND_ERROR_IS_FATAL ANY)
      list(APPEND PARTYBOARD_SETUP_EXPORT_FLAGS ${PARTYBOARD_THREE_THROW_EXPORT_FLAGS})
      set_property(TARGET browser_boot_setup_contract APPEND PROPERTY LINK_DEPENDS "${PARTYBOARD_THREE_THROW_WASM}")
      target_link_options(browser_boot_setup_contract PRIVATE "--embed-file=${PARTYBOARD_THREE_THROW_WASM}@/m410Dll.wasm")
      target_compile_definitions(browser_boot_setup_contract PRIVATE PARTYBOARD_THREE_THROW_LOAD_PROBE)
    endif()
    if(PARTYBOARD_BOARD_AUDIT_DIR)
      foreach(PARTYBOARD_BOARD_ID IN ITEMS 1 2 3 5 6 10)
        if(PARTYBOARD_BOARD_ID LESS 10)
          set(PARTYBOARD_BOARD_NAME "w0${PARTYBOARD_BOARD_ID}Dll")
        else()
          set(PARTYBOARD_BOARD_NAME "w${PARTYBOARD_BOARD_ID}Dll")
        endif()
        set(PARTYBOARD_BOARD_PATH "${PARTYBOARD_BOARD_AUDIT_DIR}/${PARTYBOARD_BOARD_NAME}.wasm")
        execute_process(COMMAND "${PARTYBOARD_NODE_EXECUTABLE}" "${PARTYBOARD_ROOT}/tools/wasm_overlay_exports.cjs" "${PARTYBOARD_BOARD_PATH}" OUTPUT_VARIABLE PARTYBOARD_BOARD_EXPORT_FLAGS COMMAND_ERROR_IS_FATAL ANY)
        list(APPEND PARTYBOARD_SETUP_EXPORT_FLAGS ${PARTYBOARD_BOARD_EXPORT_FLAGS})
        set_property(TARGET browser_boot_setup_contract APPEND PROPERTY LINK_DEPENDS "${PARTYBOARD_BOARD_PATH}")
        target_link_options(browser_boot_setup_contract PRIVATE "--embed-file=${PARTYBOARD_BOARD_PATH}@/${PARTYBOARD_BOARD_NAME}.wasm")
      endforeach()
      target_compile_definitions(browser_boot_setup_contract PRIVATE PARTYBOARD_BOARD_LOAD_AUDIT)
    endif()
    endif()
    # Import enumeration runs at configure time. Relinking an updated side
    # module alone would embed new code while retaining an obsolete main-module
    # export list. Reconfigure when any embedded module's bytes change.
    get_target_property(PARTYBOARD_SETUP_MODULE_DEPENDS browser_boot_setup_contract LINK_DEPENDS)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${PARTYBOARD_SETUP_MODULE_DEPENDS})
    list(REMOVE_DUPLICATES PARTYBOARD_SETUP_EXPORT_FLAGS)
    string(REPLACE ";" "\n" PARTYBOARD_SETUP_EXPORT_TEXT "${PARTYBOARD_SETUP_EXPORT_FLAGS}")
    file(WRITE "${CMAKE_BINARY_DIR}/setup-overlay-exports.rsp" "${PARTYBOARD_SETUP_EXPORT_TEXT}\n")
    # Embedding many modules exceeds cmd.exe's command-line limit on Windows.
    target_sources(browser_boot_setup_contract PRIVATE "${PARTYBOARD_ROOT}/src/port/wasm_fullscreen.cpp")
    target_link_options(browser_boot_setup_contract PRIVATE "-Wl,--wrap=emscripten_exit_fullscreen")
    # Keep the complete direct option list in a quoted response file.
    # Preserve Emscripten's incoming API defaults and allow fingerprinted Wasm
    # bytes to be supplied directly instead of fetching a second binary copy.
    target_link_options(browser_boot_setup_contract PRIVATE
      "-sINCOMING_MODULE_JS_API=['ENVIRONMENT','arguments','canvas','dynamicLibraries','elementPointerLock','instantiateWasm','locateFile','monitorRunDependencies','noExitRuntime','noInitialRun','onAbort','onExit','onRuntimeInitialized','postRun','preInit','preRun','print','printErr','setStatus','statusMessage','stderr','stdin','stdout','thisProgram','wasm','websocket','wasmBinary']")
    get_target_property(PARTYBOARD_SETUP_LINK_OPTIONS browser_boot_setup_contract LINK_OPTIONS)
    set(PARTYBOARD_SETUP_LINK_TEXT "")
    foreach(PARTYBOARD_LINK_OPTION IN LISTS PARTYBOARD_SETUP_LINK_OPTIONS)
      string(REPLACE "\"" "\\\"" PARTYBOARD_ESCAPED_OPTION "${PARTYBOARD_LINK_OPTION}")
      string(APPEND PARTYBOARD_SETUP_LINK_TEXT "\"${PARTYBOARD_ESCAPED_OPTION}\"\n")
    endforeach()
    file(WRITE "${CMAKE_BINARY_DIR}/setup-link-options.rsp" "${PARTYBOARD_SETUP_LINK_TEXT}")
    set_property(TARGET browser_boot_setup_contract PROPERTY LINK_OPTIONS "@${CMAKE_BINARY_DIR}/setup-link-options.rsp")
    set_property(TARGET browser_boot_setup_contract APPEND PROPERTY LINK_DEPENDS "${CMAKE_BINARY_DIR}/setup-link-options.rsp")
    # Reuse the same embedded executable package and link dependencies, with a
    # production entry point instead of any diagnostic main/observer functions.
    add_executable(partyboard_browser_runtime EXCLUDE_FROM_ALL
      "${PARTYBOARD_ROOT}/src/port/wasm_main.cpp"
      "${PARTYBOARD_ROOT}/src/port/wasm_platform.cpp"
      "${PARTYBOARD_ROOT}/src/port/wasm_fullscreen.cpp")
    target_compile_definitions(partyboard_browser_runtime PRIVATE TARGET_PC AURORA OPTIMIZED_TEXTURE_LOADING BYTESWAPPING)
    foreach(PARTYBOARD_RUNTIME_PROPERTY IN ITEMS INCLUDE_DIRECTORIES LINK_LIBRARIES LINK_OPTIONS LINK_DEPENDS)
      get_target_property(PARTYBOARD_RUNTIME_VALUE browser_boot_setup_contract ${PARTYBOARD_RUNTIME_PROPERTY})
      set_property(TARGET partyboard_browser_runtime PROPERTY ${PARTYBOARD_RUNTIME_PROPERTY} "${PARTYBOARD_RUNTIME_VALUE}")
    endforeach()
    # Check stack-pointer writes as well as cookies in the production runtime.
    # This catches a fiber stack crossing its bounds before adjacent state is lost.
    target_link_options(partyboard_browser_runtime PRIVATE -sSTACK_OVERFLOW_CHECK=2)
    # Browser control preferences marshal validated records into Wasm memory.
    target_link_options(partyboard_browser_runtime PRIVATE "-sEXPORTED_FUNCTIONS=_malloc,_free")
    set(PARTYBOARD_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/browser-runtime" CACHE PATH
      "Production runtime destination; select a separate directory while validating an existing package")
    set_target_properties(partyboard_browser_runtime PROPERTIES OUTPUT_NAME contract SUFFIX .mjs
      RUNTIME_OUTPUT_DIRECTORY "${PARTYBOARD_RUNTIME_OUTPUT_DIRECTORY}")
    # SDL's browser metadata helpers can receive a connection event after the
    # corresponding getGamepads slot has become empty. Keep its VID/PID parsing
    # unchanged for present devices and return unknown metadata for absent ones.
    # Apply at link output so a cached SDK port cannot silently omit the fix.
    find_program(PARTYBOARD_NODE_EXECUTABLE node REQUIRED)
    set(PARTYBOARD_GAMEPAD_METADATA_GUARD "${PARTYBOARD_ROOT}/tools/guard-browser-gamepad-metadata.cjs")
    set_property(TARGET partyboard_browser_runtime APPEND PROPERTY LINK_DEPENDS "${PARTYBOARD_GAMEPAD_METADATA_GUARD}")
    add_custom_command(TARGET partyboard_browser_runtime POST_BUILD
      COMMAND "${PARTYBOARD_NODE_EXECUTABLE}" "${PARTYBOARD_GAMEPAD_METADATA_GUARD}" "$<TARGET_FILE:partyboard_browser_runtime>"
      VERBATIM)

  endif()
endif()











