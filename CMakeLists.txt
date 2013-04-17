CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT( CoolDown )
#use gcc/g++ to fix boost::atomic compile error of clang3.0
#IF( ${CMAKE_SYSTEM_NAME} MATCHES "Linux" )
#SET(CMAKE_C_COMPILER "gcc") 
#SET(CMAKE_CXX_COMPILER "g++") 
#ELSE()
#SET(CMAKE_C_COMPILER "clang") 
#SET(CMAKE_CXX_COMPILER "clang++") 
#endif()

SET(CMAKE_CXX_FLAGS "-Wall")
SET( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/public/lib )
ADD_DEFINITIONS( -DWall )

#recompile the proto files
SET( protoc.cmd "protoc")
SET( protoc.arg "-I=${CMAKE_SOURCE_DIR}/proto")
SET( protoc.arg2 "--cpp_out=${CMAKE_SOURCE_DIR}/proto" )
SET( TrackerProto "${CMAKE_SOURCE_DIR}/proto/tracker.proto")
SET( NetpackHeaderProto "${CMAKE_SOURCE_DIR}/proto/netpack_header.proto")
SET( ClientProto "${CMAKE_SOURCE_DIR}/proto/client.proto" )
SET( Torrent "${CMAKE_SOURCE_DIR}/proto/torrent.proto" )
EXECUTE_PROCESS( COMMAND ${protoc.cmd} ${protoc.arg} ${protoc.arg2} 
    ${TrackerProto} ${NetpackHeaderProto} ${ClientProto} ${Torrent} )

ADD_SUBDIRECTORY( public )
ADD_SUBDIRECTORY( tracker )
ADD_SUBDIRECTORY( client )
