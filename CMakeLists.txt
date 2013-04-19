project( disruptor )
cmake_minimum_required(VERSION 2.8)

if( NOT WIN32 )
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -Wall -Wno-unused-local-typedefs" )
else()

endif( NOT WIN32 )

SET( BUILD_SHARED_LIBS NO )
SET(Boost_USE_STATIC_LIBS ON)
FIND_PACKAGE(Boost 1.51 COMPONENTS thread date_time system filesystem program_options signals serialization chrono unit_test_framework context ) 
include_directories( ${Boost_INCLUDE_DIR} )

include_directories( include )
#include_directories( fc/include )
#include_directories( fc/vendor/boost_1.51/include )


add_library( disruptor STATIC thread.cpp )
#add_subdirectory( fc )

add_executable( test test.cpp )
add_executable( pingpong pingpong.cpp)
target_link_libraries( pingpong disruptor ${Boost_THREAD_LIBRARY} ${Boost_SYSTEM_LIBRARY} )
#add_executable( fcpong fcpong.cpp )
#target_link_libraries( fcpong fc )
