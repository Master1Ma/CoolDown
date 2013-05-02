CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
SET(CMAKE_C_COMPILER "clang") 
SET(CMAKE_CXX_COMPILER "clang++")

#use Debug mode by default
#IF(NOT DEFINED CMAKE_BUILD_TYPE)
#ENDIF()
ADD_DEFINITIONS(-D_DEBUG=1)
PROJECT( CoolDown )

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
SET( JobHistory "${CMAKE_SOURCE_DIR}/proto/job_history.proto" )
EXECUTE_PROCESS( COMMAND ${protoc.cmd} ${protoc.arg} ${protoc.arg2} 
    ${TrackerProto} ${NetpackHeaderProto} ${ClientProto} ${Torrent} ${JobHistory} )

ADD_SUBDIRECTORY( public )
ADD_SUBDIRECTORY( tracker )
ADD_SUBDIRECTORY( client )
