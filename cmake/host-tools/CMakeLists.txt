cmake_minimum_required(VERSION 3.12)

project(lsp-host-tools LANGUAGES CXX)

if(NOT DEFINED LSP_ROOT_DIR)
	message(FATAL_ERROR "LSP_ROOT_DIR must point at the repository root.")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

add_executable(lspgen-host
	${LSP_ROOT_DIR}/lspgen/lspgen.cpp
	${LSP_ROOT_DIR}/lsp/json/json.cpp
)

target_include_directories(lspgen-host PRIVATE ${LSP_ROOT_DIR})

set_target_properties(lspgen-host PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

if(CMAKE_CONFIGURATION_TYPES)
	foreach(cfg ${CMAKE_CONFIGURATION_TYPES})
		string(TOUPPER ${cfg} cfg_upper)
		set_target_properties(lspgen-host PROPERTIES
			RUNTIME_OUTPUT_DIRECTORY_${cfg_upper} ${CMAKE_BINARY_DIR}
		)
	endforeach()
endif()
