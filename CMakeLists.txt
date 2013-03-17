cmake_minimum_required(VERSION 2.6)
PROJECT( POCOTEST )
SET(CMAKE_CXX_COMPILER "clang++") 
SET( DIR_OUTPUT output )
#SET( CMAKE_BUILD_TYPE "release" )
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/${DIR_OUTPUT})
SET( BIN_NAME ${DIR_OUTPUT}/test )
INCLUDE_DIRECTORIES( include )

#recompile the proto files and link it to include & src 
SET( protoc.cmd "protoc")
SET( protoc.arg "-I=${CMAKE_SOURCE_DIR}/proto")
SET( protoc.arg2 "--cpp_out=${CMAKE_SOURCE_DIR}/proto" )
SET( protoc.file1 "${CMAKE_SOURCE_DIR}/proto/tracker.proto")
SET( protoc.file2 "${CMAKE_SOURCE_DIR}/proto/netpack_header.proto")

execute_process(COMMAND ${protoc.cmd} ${protoc.arg} ${protoc.arg2} ${protoc.file1} ${protoc.file2} )
execute_process(COMMAND ln -Ff ${CMAKE_SOURCE_DIR}/proto/tracker.pb.h ${CMAKE_SOURCE_DIR}/include/tracker.pb.h)
execute_process(COMMAND ln -Ff ${CMAKE_SOURCE_DIR}/proto/tracker.pb.cc ${CMAKE_SOURCE_DIR}/src/tracker.pb.cc)
execute_process(COMMAND ln -Ff ${CMAKE_SOURCE_DIR}/proto/netpack_header.pb.h ${CMAKE_SOURCE_DIR}/include/netpack_header.pb.h)
execute_process(COMMAND ln -Ff ${CMAKE_SOURCE_DIR}/proto/netpack_header.pb.cc ${CMAKE_SOURCE_DIR}/src/netpack_header.pb.cc)

FIND_LIBRARY(POCO_FOUNDATION PocoFoundation)
FIND_LIBRARY(POCO_NET PocoNet)
FIND_LIBRARY(POCO_DATA PocoData)
FIND_LIBRARY(POCO_DATA_SQLITE PocoDataSQLite)
FIND_LIBRARY(POCO_UTIL PocoUtil)
FIND_LIBRARY(PTHREAD pthread )
FIND_LIBRARY(PROTOBUF protobuf)

AUX_SOURCE_DIRECTORY( src DIR_SRCS )
ADD_EXECUTABLE( ${BIN_NAME} ${DIR_SRCS} )
TARGET_LINK_LIBRARIES( ${BIN_NAME} ${POCO_FOUNDATION} ${POCO_NET} ${POCO_DATA}
    ${POCO_DATA_SQLITE} ${POCO_UTIL} ${PROTOBUF})
