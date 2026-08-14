set( CMAKE_SYSTEM_PROCESSOR arm )

include( "${CMAKE_CURRENT_LIST_DIR}/linux-common.cmake" )

a51_select_linux_gnu_compilers( "arm-linux-gnueabihf-" )
