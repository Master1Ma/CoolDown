cmake_minimum_required(VERSION 2.6)
PROJECT( CoolDown-Public-error_code)
SET(CMAKE_CXX_COMPILER "clang++") 
SET( CMAKE_BUILD_TYPE "release" )
SET( LIB_NAME "error_code" )
INCLUDE_DIRECTORIES( include )

AUX_SOURCE_DIRECTORY( src DIR_SRCS )
ADD_LIBRARY( ${LIB_NAME} SHARED ${DIR_SRCS} )
