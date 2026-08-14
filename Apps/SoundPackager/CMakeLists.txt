# Native SoundPackager target.
add_subdirectory( libmp3lame )
a51_add_project( SoundPackager EXECUTABLE
    SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/aiff_file.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/audio_file.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Compress_GCN.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Compress_PC.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Compress_PS2.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Compress_XBOX.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/DspTool.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/endian.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ExportPackage.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/imaadpcm.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ParseScript.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ProcessSample.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/SoundPackager.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/wav_file.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/aiff_file.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/audio_file.hpp"
        "${CMAKE_SOURCE_DIR}/xCore/Entropy/Audio/audio_package_format.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Compress.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/DspTool.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/ENCVAG.H"
        "${CMAKE_CURRENT_LIST_DIR}/endian.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/ExportPackage.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/imaadpcm.h"
        "${CMAKE_CURRENT_LIST_DIR}/PackageTypes.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/ParseScript.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/SoundPackager.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/wav_file.hpp"
    DEPENDENCIES
        CommandLine
        Parsing
        x_files
        libmp3lame_vc6
)

target_include_directories( SoundPackager PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/include"
    "${CMAKE_CURRENT_LIST_DIR}/libmp3lame"
    "${CMAKE_CURRENT_LIST_DIR}/mpglib"
    "${CMAKE_SOURCE_DIR}/xCore/Entropy/Audio"
)

string( TOLOWER "${CMAKE_GENERATOR_PLATFORM}" _a51_soundpackager_platform )
set( _a51_soundpackager_is_x86 FALSE )
if( _a51_soundpackager_platform STREQUAL "win32" OR ( NOT _a51_soundpackager_platform AND CMAKE_SIZEOF_VOID_P EQUAL 4 ) )
    set( _a51_soundpackager_is_x86 TRUE )
endif()

if( _a51_soundpackager_is_x86 AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/ENCVAG.LIB" )
    target_link_libraries( SoundPackager PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/ENCVAG.LIB"
    )
endif()

if( _a51_soundpackager_is_x86 AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/ENCVAG.DLL" )
    add_custom_command( TARGET SoundPackager POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_LIST_DIR}/ENCVAG.DLL"
            "$<TARGET_FILE_DIR:SoundPackager>/ENCVAG.DLL"
    )
endif()

if( NOT _a51_soundpackager_is_x86 )
    # ENCVAG is a 32-bit PS2 codec binary. Keep the SoundPackager target
    # usable on x64 while retaining the legacy PS2 source for 32-bit builds.
    get_target_property( _a51_soundpackager_sources SoundPackager SOURCES )
    list( FILTER _a51_soundpackager_sources EXCLUDE REGEX "Compress_PS2\\.cpp$" )
    set_property( TARGET SoundPackager PROPERTY SOURCES "${_a51_soundpackager_sources}" )
    target_compile_definitions( SoundPackager PRIVATE A51_SOUND_PACKAGER_NO_ENCVAG )
    message( STATUS "SoundPackager: excluding the 32-bit ENCVAG PS2 codec from this architecture." )
endif()
