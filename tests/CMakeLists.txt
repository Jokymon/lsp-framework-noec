add_executable(lsp-tests
	json_tests.cpp
	jsonrpc_tests.cpp
	uri_tests.cpp
	../lsp/json/json.cpp
	../lsp/jsonrpc/jsonrpc.cpp
	../lsp/uri.cpp

	${Catch2_SOURCE_DIR}/src/catch2/internal/catch_main.cpp
)

target_include_directories(lsp-tests
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(lsp-tests
	PRIVATE
		Catch2::Catch2
)

include(Catch)
catch_discover_tests(lsp-tests DISCOVERY_MODE PRE_TEST)
