add_executable(nlane_test "${NLANE_TEST_DIR}/main.cpp;${NLANE_TEST_DIR}/transactional/transactional_test.cpp")
target_link_libraries(nlane_test PRIVATE nlane_lib gtest_main)