project( fc )
cmake_minimum_required(VERSION 2.8)

if( NOT WIN32 )
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -Wall -Wno-unused-local-typedefs" )
else()

endif( NOT WIN32 )

set( BUILD_SHARED_LIBS NO )
set(Boost_USE_STATIC_LIBS ON)
find_package(Boost 1.53 COMPONENTS thread date_time system filesystem program_options signals serialization chrono unit_test_framework context coroutine ) 
include_directories( ${Boost_INCLUDE_DIR} )

include_directories( include )
add_library( fc STATIC 
    src/future.cpp
    src/fiber.cpp 
#    src/thread.cpp 
    )

add_executable( fiber_test examples/fiber_test.cpp )
target_link_libraries( fiber_test fc ${Boost_THREAD_LIBRARY} ${Boost_SYSTEM_LIBRARY} ${Boost_CONTEXT_LIBRARY} )
#add_executable( pingpong examples/pingpong.cpp )
#target_link_libraries( pingpong fc ${Boost_THREAD_LIBRARY} ${Boost_SYSTEM_LIBRARY} )
