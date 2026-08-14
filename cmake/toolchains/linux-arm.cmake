include( "${CMAKE_CURRENT_LIST_DIR}/linux-common.cmake" )

set( CMAKE_SYSTEM_PROCESSOR arm )

a51_select_linux_gnu_compilers( "arm-linux-gnueabihf-" )
