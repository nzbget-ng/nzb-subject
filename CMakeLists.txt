cmake_minimum_required( VERSION 3.25 )
project( nzb-subject C )

set( CMAKE_C_STANDARD 17 )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS}" )
set( CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG=1" )

add_executable( nzb-subject nzb-subject.c yxml.c yxml.h )
