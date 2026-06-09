add_executable(lsp-tests
	json_tests.cpp
	../lsp/json/json.cpp
)

target_include_directories(lsp-tests
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(lsp-tests
	PRIVATE
		Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(lsp-tests DISCOVERY_MODE PRE_TEST)
