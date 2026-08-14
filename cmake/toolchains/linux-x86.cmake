include( "${CMAKE_CURRENT_LIST_DIR}/linux-common.cmake" )

set( CMAKE_SYSTEM_PROCESSOR i686 )

a51_select_linux_gnu_compilers( "i686-linux-gnu-" )
