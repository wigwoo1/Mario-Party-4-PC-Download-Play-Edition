include(FetchContent)
# Published Xiph checksums; local-file decoding only, with no URL/OpenSSL backend.
set(OPUS_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_SHARED_LIBRARY OFF CACHE BOOL "" FORCE)
FetchContent_Declare(partyboard_opus
    URL https://downloads.xiph.org/releases/opus/opus-1.6.1.tar.gz
    URL_HASH SHA256=6ffcb593207be92584df15b32466ed64bbec99109f007c82205f0194572411a1
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_Declare(partyboard_ogg
    URL https://downloads.xiph.org/releases/ogg/libogg-1.3.6.tar.gz
    URL_HASH SHA256=83e6704730683d004d20e21b8f7f55dcb3383cdf84c0daedf30bde175f774638
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_Declare(partyboard_opusfile_source
    URL https://downloads.xiph.org/releases/opus/opusfile-0.12.tar.gz
    URL_HASH SHA256=118d8601c12dd6a44f52423e68ca9083cc9f2bfe72da7a8c1acb22a80ae3550b
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR partyboard-no-upstream-cmake)
FetchContent_MakeAvailable(partyboard_opus partyboard_ogg partyboard_opusfile_source)
add_library(partyboard_opusfile STATIC
    ${partyboard_opusfile_source_SOURCE_DIR}/src/info.c
    ${partyboard_opusfile_source_SOURCE_DIR}/src/internal.c
    ${partyboard_opusfile_source_SOURCE_DIR}/src/opusfile.c
    ${partyboard_opusfile_source_SOURCE_DIR}/src/stream.c)
target_include_directories(partyboard_opusfile PUBLIC ${partyboard_opusfile_source_SOURCE_DIR}/include)
target_link_libraries(partyboard_opusfile PUBLIC opus ogg)
set_target_properties(partyboard_opusfile PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_executable(partyboard_lobby_opus_decode_test EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tools/native-lobby-opus-decode-test.cpp)
target_link_libraries(partyboard_lobby_opus_decode_test PRIVATE partyboard_opusfile)

target_compile_features(partyboard_lobby_opus_decode_test PRIVATE cxx_std_17)
# Keep Windows decoder DLLs beside the native executable, not only under bin/.
if(WIN32)
    install(FILES $<TARGET_FILE:opus> $<TARGET_FILE:ogg>
        DESTINATION . COMPONENT native_lobby_audio)
endif()
install(FILES ${partyboard_opus_SOURCE_DIR}/COPYING
    DESTINATION licenses RENAME opus-COPYING COMPONENT native_lobby_audio)
install(FILES ${partyboard_ogg_SOURCE_DIR}/COPYING
    DESTINATION licenses RENAME ogg-COPYING COMPONENT native_lobby_audio)
install(FILES ${partyboard_opusfile_source_SOURCE_DIR}/COPYING
    DESTINATION licenses RENAME opusfile-COPYING COMPONENT native_lobby_audio)
