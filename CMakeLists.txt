cmake_minimum_required(VERSION 3.12)

project(obs-async-audio-filter VERSION 0.3.0)

set(PLUGIN_AUTHOR "Norihiro Kamae")

set(MACOS_BUNDLEID "net.nagater.obs-async-audio-filter")
set(ID_PREFIX "net.nagater.obs.")
set(MACOS_PACKAGE_UUID "51FA3068-9263-4F49-8426-FD789736BFCC")
set(MACOS_INSTALLER_UUID "D9714285-DD24-48E0-B90C-8BE2AE026754")
set(PLUGIN_URL "https://obsproject.com/forum/resources/asynchronous-audio-filter.1482/")

set(LINUX_MAINTAINER_EMAIL "norihiro@nagater.net")

# In case you need C++
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
	find_package(libobs REQUIRED)
	include(cmake/ObsPluginHelpers.cmake)
endif()

find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
	pkg_check_modules(PC_LIBSAMPLERATE samplerate)
endif()

if(PC_LIBSAMPLERATE_FOUND)
	option(USE_ERIKD_LIBSAMPLERATE_DEPS "Use libsamplerate by Erik de Castro Lopo in deps" OFF)
	option(USE_ERIKD_LIBSAMPLERATE_SYSTEM "Use libsamplerate by Erik de Castro Lopo in the system" ON)
else()
	option(USE_ERIKD_LIBSAMPLERATE_DEPS "Use libsamplerate by Erik de Castro Lopo in deps" ON)
	option(USE_ERIKD_LIBSAMPLERATE_SYSTEM "Use libsamplerate by Erik de Castro Lopo in the system" OFF)
endif()

option(USE_FFMPEG_SWRESAMPLE "Use swresample from FFmpeg" OFF)

if(USE_ERIKD_LIBSAMPLERATE_DEPS OR USE_ERIKD_LIBSAMPLERATE_SYSTEM)
	set(USE_ERIKD_LIBSAMPLERATE ON)
else()
	set(USE_ERIKD_LIBSAMPLERATE OFF)
endif()

if(USE_FFMPEG_SWRESAMPLE)
	list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/finders")
	find_package(FFmpeg REQUIRED COMPONENTS avutil swresample)
endif()

configure_file(
	src/plugin-macros.h.in
	plugin-macros.generated.h
)

add_library(${PROJECT_NAME} MODULE
	src/plugin-main.c
	src/async-audio-filter.c
	src/resampler.c
)

target_link_libraries(${PROJECT_NAME}
	OBS::libobs
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

if(USE_FFMPEG_SWRESAMPLE)
	target_sources(${PROJECT_NAME} PRIVATE
		src/resampler-ffmpeg.c
	)
	target_link_libraries(${PROJECT_NAME}
		FFmpeg::swresample
		FFmpeg::avutil
	)
endif()

if (USE_ERIKD_LIBSAMPLERATE)
	target_sources(${PROJECT_NAME} PRIVATE
		src/resampler-erikd.c
	)
endif()

if(USE_ERIKD_LIBSAMPLERATE_SYSTEM)
	target_include_directories(${PROJECT_NAME}
		PRIVATE
		${PC_LIBSAMPLERATE_INCLUDE_DIRS}
	)
	find_library(SAMPLERATE
		NAMES ${PC_LIBSAMPLERATE_LIBRARIES}
		HINTS ${PC_LIBSAMPLERATE_LIBRARY_DIRS}
	)
	target_link_libraries(${PROJECT_NAME}
		${SAMPLERATE}
	)
endif()

if(USE_ERIKD_LIBSAMPLERATE_DEPS)
	list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/deps/libsamplerate/cmake/")
	set(BUILD_SHARED_LIBS OFF)
	set(LIBSAMPLERATE_ENABLE_SINC_FAST_CONVERTER OFF)
	set(LIBSAMPLERATE_ENABLE_SINC_MEDIUM_CONVERTER OFF)
	set(LIBSAMPLERATE_INSTALL_PKGCONFIG_MODULE OFF)
	option(BUILD_TESTING "" OFF) # ON by default in CTest
	add_subdirectory(deps/libsamplerate)
	target_link_libraries(${PROJECT_NAME}
		samplerate
	)
endif()

if(OS_WINDOWS)
	# Enable Multicore Builds and disable FH4 (to not depend on VCRUNTIME140_1.DLL when building with VS2019)
	if (MSVC)
		add_definitions(/MP /d2FH4-)
		add_definitions("-D_USE_MATH_DEFINES")
	endif()

	target_link_libraries(${PROJECT_NAME} OBS::w32-pthreads)
endif()

if(OS_LINUX)
	target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
	target_link_options(${PROJECT_NAME} PRIVATE -Wl,-z,defs)
endif()

if(APPLE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -fvisibility=default")

	set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
	set(MACOSX_PLUGIN_GUI_IDENTIFIER "${MACOS_BUNDLEID}")
	set(MACOSX_PLUGIN_BUNDLE_VERSION "${PROJECT_VERSION}")
	set(MACOSX_PLUGIN_SHORT_VERSION_STRING "1")
endif()

setup_plugin_target(${PROJECT_NAME})

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
	configure_file(
		ci/ci_includes.sh.in
		ci/ci_includes.generated.sh
	)

	configure_file(
		installer/installer-macOS.pkgproj.in
		installer-macOS.generated.pkgproj
	)

	configure_file(
		installer/installer-Windows.iss.in
		installer-Windows.generated.iss
	)
endif()
