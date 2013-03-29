cmake_minimum_required(VERSION 2.6)
PROJECT( CoolDown )
#ADD_DEFINITIONS( -DWall )
set(CMAKE_CXX_FLAGS "-Wall")
SET( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/public/lib )

#recompile the proto files
SET( protoc.cmd "protoc")
SET( protoc.arg "-I=${CMAKE_SOURCE_DIR}/proto")
SET( protoc.arg2 "--cpp_out=${CMAKE_SOURCE_DIR}/proto" )
SET( protoc.file1 "${CMAKE_SOURCE_DIR}/proto/tracker.proto")
SET( protoc.file2 "${CMAKE_SOURCE_DIR}/proto/netpack_header.proto")
EXECUTE_PROCESS( COMMAND ${protoc.cmd} ${protoc.arg} ${protoc.arg2} ${protoc.file1} ${protoc.file2} )

ADD_SUBDIRECTORY( public )
ADD_SUBDIRECTORY( Tracker )
ADD_SUBDIRECTORY( Client )
