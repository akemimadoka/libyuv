project(YUV CXX)
cmake_minimum_required(VERSION 3.14)

if(CONAN_EXPORTED)
    include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
    conan_basic_setup(TARGETS)
else()
	if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
		message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
		file(DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/v0.14/conan.cmake"
					  "${CMAKE_BINARY_DIR}/conan.cmake")
	endif()

	include(${CMAKE_BINARY_DIR}/conan.cmake)

	conan_cmake_run(CONANFILE conanfile.py
					BASIC_SETUP CMAKE_TARGETS
					BUILD missing)
endif()

option(YUV_TEST "Build unit tests" OFF)
option(YUV_TOOL "Build tools" OFF)

set(ly_base_dir ${CMAKE_CURRENT_SOURCE_DIR})
set(ly_src_dir ${ly_base_dir}/source)
set(ly_inc_dir ${ly_base_dir}/include)
set(ly_tst_dir ${ly_base_dir}/unit_test)
set(ly_lib_name yuv)
set(ly_lib ${ly_lib_name})

file(GLOB_RECURSE ly_inc_files ${ly_inc_dir}/*.h)
list(SORT ly_inc_files)

file(GLOB_RECURSE ly_source_files ${ly_src_dir}/*.cc)
list(SORT ly_source_files)

file(GLOB_RECURSE ly_unittest_sources ${ly_tst_dir}/*.cc)
list(SORT ly_unittest_sources)

add_library(${ly_lib} ${ly_inc_files} ${ly_source_files})
target_include_directories(${ly_lib} PUBLIC
	$<BUILD_INTERFACE:${ly_inc_dir}>
	$<INSTALL_INTERFACE:include>)
target_link_libraries(${ly_lib} PUBLIC
	CONAN_PKG::libjpeg-turbo)
target_compile_definitions(${ly_lib} PUBLIC
	HAVE_JPEG)

# this creates the conversion tool
if(YUV_TOOL)
	add_executable(yuvconvert ${ly_base_dir}/util/yuvconvert.cc)
	target_link_libraries(yuvconvert PRIVATE ${ly_lib})
endif()

if(YUV_TEST)
	add_executable(libyuv_unittest ${ly_unittest_sources})
	target_link_libraries(libyuv_unittest PRIVATE ${ly_lib_name} CONAN_PKG::gtest)
	find_library(PTHREAD_LIBRARY pthread)
	if(NOT PTHREAD_LIBRARY STREQUAL "PTHREAD_LIBRARY-NOTFOUND")
		target_link_libraries(libyuv_unittest PRIVATE pthread)
	endif()

	target_link_libraries(libyuv_unittest PRIVATE CONAN_PKG::gflags)
	target_compile_definitions(libyuv_unittest PRIVATE LIBYUV_USE_GFLAGS)
endif()

# install the conversion tool, .so, .a, and all the header files
if(YUV_TOOL)
	install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/yuvconvert
		TYPE BIN)
endif()
install(TARGETS ${ly_lib}
	EXPORT ${ly_lib})
install(DIRECTORY ${ly_inc_dir}/
	TYPE INCLUDE
	FILES_MATCHING PATTERN "*.h")
install(EXPORT ${ly_lib}
	DESTINATION cmake)
