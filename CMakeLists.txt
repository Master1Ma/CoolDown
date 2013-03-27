cmake_minimum_required(VERSION 2.6)
PROJECT( CoolDown )

#recompile the proto files
SET( protoc.cmd "protoc")
SET( protoc.arg "-I=${CMAKE_SOURCE_DIR}/proto")
SET( protoc.arg2 "--cpp_out=${CMAKE_SOURCE_DIR}/proto" )
SET( protoc.file1 "${CMAKE_SOURCE_DIR}/proto/tracker.proto")
SET( protoc.file2 "${CMAKE_SOURCE_DIR}/proto/netpack_header.proto")
ADD_SUBDIRECTORY( Tracker )
ADD_SUBDIRECTORY( Client )
