add_executable(lsp-tests
	smoke.cpp
)

target_link_libraries(lsp-tests
	PRIVATE
		Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(lsp-tests DISCOVERY_MODE PRE_TEST)
