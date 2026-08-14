include( "${CMAKE_CURRENT_LIST_DIR}/linux-common.cmake" )

set( CMAKE_SYSTEM_PROCESSOR aarch64 )

a51_select_linux_gnu_compilers( "aarch64-linux-gnu-" )
