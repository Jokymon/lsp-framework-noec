cmake_minimum_required(VERSION 3.12)

project(lsp)

include(CTest)

# compiler flags

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

option(LSP_BUILD_EXAMPLES "Build the examples" OFF)
option(LSP_INSTALL "Configure lsp install configuration" ON)
option(LSP_BUILD_TESTS "Build the unit tests" ON)
option(LSP_BUILD_HOST_TOOLS "Build host-side tools required during cross compilation" ON)
set(LSP_HOST_TOOLS_CMAKE_GENERATOR "" CACHE STRING "Generator for the host-tools sub-build used during cross compilation")
set(LSP_HOST_TOOLS_CMAKE_GENERATOR_PLATFORM "" CACHE STRING "Generator platform for the host-tools sub-build used during cross compilation")
set(LSP_HOST_TOOLS_CMAKE_CXX_COMPILER "" CACHE STRING "Native C++ compiler for the host-tools sub-build used during cross compilation")

set(BUILD_TESTING ${LSP_BUILD_TESTS} CACHE BOOL "Enable CTest-based testing" FORCE)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	add_compile_options(/W4 /bigobj)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	option(LSP_USE_SANITIZERS "Use address and undefined behavior sanitizers for Debug builds" OFF)

	add_compile_options(
		-Wall
		-Wextra
		-pedantic
		-Wsign-conversion
		-Wfloat-conversion
	)

	if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
		add_compile_options(
			-Wimplicit-int-conversion
			-Wimplicit-float-conversion
		)
	endif()

	if(LSP_USE_SANITIZERS)
		add_compile_options(
			$<$<CONFIG:DEBUG>:-fsanitize=address,undefined>
			$<$<CONFIG:DEBUG>:-fno-omit-frame-pointer>
		)
		add_link_options($<$<CONFIG:DEBUG>:-fsanitize=address,undefined>)
	endif()

	if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		add_compile_options(-Wa,-mbig-obj)
	endif()
endif()

if(WIN32)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

# lspgen

if(CMAKE_CROSSCOMPILING)
	if(LSP_BUILD_HOST_TOOLS)
		include(ExternalProject)

		set(LSP_HOST_TOOLS_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/host-tools)
		set(LSP_HOST_TOOLS_CMAKE_ARGS
			-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
			-DLSP_ROOT_DIR:PATH=${CMAKE_CURRENT_SOURCE_DIR}
		)
		set(LSP_HOST_TOOLS_EP_ARGS)

		if(LSP_HOST_TOOLS_CMAKE_CXX_COMPILER)
			list(APPEND LSP_HOST_TOOLS_CMAKE_ARGS -DCMAKE_CXX_COMPILER:STRING=${LSP_HOST_TOOLS_CMAKE_CXX_COMPILER})
		endif()

		if(LSP_HOST_TOOLS_CMAKE_GENERATOR)
			list(APPEND LSP_HOST_TOOLS_EP_ARGS CMAKE_GENERATOR ${LSP_HOST_TOOLS_CMAKE_GENERATOR})
		elseif(CMAKE_HOST_WIN32)
			set(LSP_HOST_EXECUTABLE_SUFFIX .exe)

			execute_process(
				COMMAND ${CMAKE_COMMAND} -E capabilities
				OUTPUT_VARIABLE LSP_CMAKE_CAPABILITIES_JSON
			)
			foreach(LSP_HOST_TOOLS_GENERATOR_CANDIDATE
				"Visual Studio 18 2026"
				"Visual Studio 17 2022"
				"Visual Studio 16 2019"
				"Visual Studio 15 2017"
			)
				if(LSP_CMAKE_CAPABILITIES_JSON MATCHES "\"name\"[^\r\n]*\"${LSP_HOST_TOOLS_GENERATOR_CANDIDATE}\"")
					set(LSP_HOST_TOOLS_GENERATOR ${LSP_HOST_TOOLS_GENERATOR_CANDIDATE})
					break()
				endif()
			endforeach()

			if(NOT LSP_HOST_TOOLS_GENERATOR)
				message(FATAL_ERROR "Could not detect a supported Visual Studio generator for building host-side tools.")
			endif()

			list(APPEND LSP_HOST_TOOLS_EP_ARGS CMAKE_GENERATOR ${LSP_HOST_TOOLS_GENERATOR})
		endif()

		if(NOT CMAKE_HOST_WIN32)
			set(LSP_HOST_EXECUTABLE_SUFFIX)
		elseif(NOT DEFINED LSP_HOST_EXECUTABLE_SUFFIX)
			set(LSP_HOST_EXECUTABLE_SUFFIX .exe)
		endif()

		if(LSP_HOST_TOOLS_CMAKE_GENERATOR_PLATFORM)
			list(APPEND LSP_HOST_TOOLS_EP_ARGS CMAKE_GENERATOR_PLATFORM ${LSP_HOST_TOOLS_CMAKE_GENERATOR_PLATFORM})
		elseif(CMAKE_HOST_WIN32 AND NOT LSP_HOST_TOOLS_CMAKE_GENERATOR)
			list(APPEND LSP_HOST_TOOLS_EP_ARGS CMAKE_GENERATOR_PLATFORM x64)
		elseif(CMAKE_HOST_WIN32 AND LSP_HOST_TOOLS_CMAKE_GENERATOR MATCHES "^Visual Studio ")
			list(APPEND LSP_HOST_TOOLS_EP_ARGS CMAKE_GENERATOR_PLATFORM x64)
		endif()

		set(LSPGEN_EXECUTABLE ${LSP_HOST_TOOLS_BINARY_DIR}/lspgen-host${LSP_HOST_EXECUTABLE_SUFFIX})
		set(LSPGEN_DEPENDENCY lsp_host_tools)

		ExternalProject_Add(lsp_host_tools
			SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cmake/host-tools
			BINARY_DIR ${LSP_HOST_TOOLS_BINARY_DIR}
			CMAKE_ARGS ${LSP_HOST_TOOLS_CMAKE_ARGS}
			${LSP_HOST_TOOLS_EP_ARGS}
			INSTALL_COMMAND ""
			BUILD_BYPRODUCTS ${LSPGEN_EXECUTABLE}
		)
	else()
		if(NOT CMAKE_CROSSCOMPILING_EMULATOR)
			message(FATAL_ERROR "Cross compilation with LSP_BUILD_HOST_TOOLS=OFF requires CMAKE_CROSSCOMPILING_EMULATOR so lspgen can run during the build.")
		endif()

		add_executable(lspgen lspgen/lspgen.cpp lsp/json/json.cpp)
		target_include_directories(lspgen PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

		set(LSPGEN_DEPENDENCY lspgen)
	endif()
else()
	add_executable(lspgen lspgen/lspgen.cpp lsp/json/json.cpp)
	target_include_directories(lspgen PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

	set(LSPGEN_EXECUTABLE $<TARGET_FILE:lspgen>)
	set(LSPGEN_DEPENDENCY lspgen)
endif()

# liblsp

set(LSP_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(LSP_SRC_DIR ${LSP_DIR}/lsp)

set(LSP_HEADERS
	# lsp
	concepts.h
	connection.h
	enumeration.h
	error.h
	exception.h
	fileuri.h
	messagebase.h
	messagehandler.h
	nullable.h
	process.h
	requestresult.h
	serialization.h
	strmap.h
	threadpool.h
	uri.h
	# io
	io/socket.h
	io/standardio.h
	io/stream.h
	# json
	json/json.h
	# jsonrpc
	jsonrpc/jsonrpc.h
)
list(TRANSFORM LSP_HEADERS PREPEND ${LSP_SRC_DIR}/)

set(LSP_SOURCES
	# lsp
	connection.cpp
	fileuri.cpp
	messagehandler.cpp
	process.cpp
	threadpool.cpp
	uri.cpp
	# io
	io/socket.cpp
	io/standardio.cpp
	# json
	json/json.cpp
	# jsonrpc
	jsonrpc/jsonrpc.cpp
)
list(TRANSFORM LSP_SOURCES PREPEND ${LSP_SRC_DIR}/)

set(LSP_GENERATED_FILES_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
set(LSP_GENERATED_LSP_DIR ${LSP_GENERATED_FILES_DIR}/lsp)
file(MAKE_DIRECTORY ${LSP_GENERATED_LSP_DIR})

set(LSP_GENERATED_HEADERS
	${LSP_GENERATED_FILES_DIR}/lsp/types.h
	${LSP_GENERATED_FILES_DIR}/lsp/messages.h
)

set(LSP_GENERATED_SOURCES
	${LSP_GENERATED_FILES_DIR}/lsp/types.cpp
)

set(LSP_METAMODEL_JSON ${CMAKE_CURRENT_SOURCE_DIR}/lspgen/metaModel.json)
set(LSP_METAMODEL_JSON_GUEST /workspace/lspgen/metaModel.json)

if(CMAKE_CROSSCOMPILING AND NOT LSP_BUILD_HOST_TOOLS)
	set(LSPGEN_COMMAND
		${CMAKE_CROSSCOMPILING_EMULATOR}
		--dir ${LSP_GENERATED_LSP_DIR}::.
		--dir ${CMAKE_CURRENT_SOURCE_DIR}::/workspace
		$<TARGET_FILE:lspgen>
		${LSP_METAMODEL_JSON_GUEST}
	)
else()
	set(LSPGEN_COMMAND ${LSPGEN_EXECUTABLE} ${LSP_METAMODEL_JSON})
endif()

add_custom_command(
	COMMAND ${LSPGEN_COMMAND}
	WORKING_DIRECTORY ${LSP_GENERATED_LSP_DIR}
	OUTPUT ${LSP_GENERATED_HEADERS} ${LSP_GENERATED_SOURCES}
	DEPENDS ${LSPGEN_DEPENDENCY} ${LSP_METAMODEL_JSON}
	COMMENT "Generating lsp types from meta model..."
	VERBATIM
)

add_library(lsp
	${LSP_HEADERS}
	${LSP_SOURCES}
	${LSP_GENERATED_HEADERS}
	${LSP_GENERATED_SOURCES}
)
include(GNUInstallDirs)
target_include_directories(lsp PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
	$<BUILD_INTERFACE:${LSP_GENERATED_FILES_DIR}>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
	set_target_properties(
		lsp PROPERTIES
		DEBUG_POSTFIX d
	)	
endif()

if (LSP_INSTALL)
	include(cmake/utils.cmake)
	lsp_read_version()

	include(CMakePackageConfigHelpers)
	set(EXPORT_CMAKE_CONFIG_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/lsp/")

	# write and install config files
	write_basic_config_version_file(
		"${CMAKE_CURRENT_BINARY_DIR}/lspConfigVersion.cmake"
		VERSION ${LSP_VERSION}
		COMPATIBILITY SameMajorVersion
	)
	configure_package_config_file(
		"${CMAKE_CURRENT_SOURCE_DIR}/cmake/lspConfig.cmake.in"
		"${CMAKE_CURRENT_BINARY_DIR}/lspConfig.cmake"
		INSTALL_DESTINATION 
		${EXPORT_CMAKE_CONFIG_DIR}
	)
	install(
		EXPORT lsp
		DESTINATION ${EXPORT_CMAKE_CONFIG_DIR}
		NAMESPACE lsp::
		FILE "lspConfigTargets.cmake"
	)
	install(
		FILES
			"${CMAKE_CURRENT_BINARY_DIR}/lspConfigVersion.cmake"
			"${CMAKE_CURRENT_BINARY_DIR}/lspConfig.cmake"
		DESTINATION ${EXPORT_CMAKE_CONFIG_DIR}
	)

	install(TARGETS lsp EXPORT lsp ARCHIVE LIBRARY)
	install(
		FILES ${LSP_GENERATED_HEADERS}
		DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/lsp
	)
	install(
		DIRECTORY ${LSP_SRC_DIR}
		DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
		FILES_MATCHING REGEX ".*\\.(h|inl)"
	)
endif()

if(WIN32)
	target_link_libraries(lsp PUBLIC Ws2_32)
endif()

if(LSP_BUILD_EXAMPLES)
	# Server
	add_executable(LspServerExample ${LSP_DIR}/examples/server.cpp)
	target_link_libraries(LspServerExample lsp)
	# Client
	add_executable(LspClientExample ${LSP_DIR}/examples/client.cpp)
	target_link_libraries(LspClientExample lsp)
endif()

if(LSP_BUILD_TESTS)
	include(FetchContent)

	set(CATCH_CONFIG_NO_POSIX_SIGNALS ON CACHE BOOL "Disable POSIX signal handling in Catch2" FORCE)
	set(CATCH_CONFIG_NO_WINDOWS_SEH ON CACHE BOOL "Disable Windows SEH handling in Catch2" FORCE)

	FetchContent_Declare(
		Catch2
		GIT_REPOSITORY https://github.com/catchorg/Catch2.git
		GIT_TAG v3.7.1
	)
	FetchContent_MakeAvailable(Catch2)

	add_subdirectory(tests)
endif()
