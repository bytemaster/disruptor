cmake_minimum_required(VERSION 2.8)
project( disruptor )

if( NOT WIN32 )
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -Wall -Wno-unused-local-typedefs" )
else()

endif( NOT WIN32 )

include_directories( . )
#add_library( disruptor STATIC disruptor.cpp )
add_executable( test test.cpp )
#target_link_libraries( test disruptor )
