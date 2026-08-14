# Native bundled LAME static library target.
set( _a51_lame_sources
    bitstream.c
    encoder.c
    fft.c
    id3tag.c
    lame.c
    mpglib_interface.c
    newmdct.c
    presets.c
    psymodel.c
    quantize.c
    quantize_pvt.c
    reservoir.c
    set_get.c
    tables.c
    takehiro.c
    util.c
    vbrquantize.c
    VbrTag.c
    version.c
    bitstream.h
    encoder.h
    fft.h
    gain_analysis.h
    id3tag.h
    lame-analysis.h
    lame_global_flags.h
    l3side.h
    machine.h
    newmdct.h
    quantize.h
    quantize_pvt.h
    reservoir.h
    tables.h
    tools.h
    util.h
    VbrTag.h
    version.h
    ../include/lame.h
    ../configMS.h
)

set( _a51_lame_source_paths )
foreach( _source IN LISTS _a51_lame_sources )
    list( APPEND _a51_lame_source_paths "${CMAKE_CURRENT_LIST_DIR}/${_source}" )
endforeach()

a51_add_project( libmp3lame_vc6 STATIC
    SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}"
    SOURCES ${_a51_lame_source_paths}
)

configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/../configMS.h"
    "${CMAKE_CURRENT_BINARY_DIR}/config.h"
    COPYONLY
)

target_include_directories( libmp3lame_vc6 PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}"
    "${CMAKE_CURRENT_LIST_DIR}/../include"
    "${CMAKE_CURRENT_LIST_DIR}/../mpglib"
    "${CMAKE_CURRENT_LIST_DIR}/.."
    "${CMAKE_CURRENT_BINARY_DIR}"
)

target_compile_definitions( libmp3lame_vc6 PRIVATE
    HAVE_CONFIG_H
    HAVE_MPGLIB
)
