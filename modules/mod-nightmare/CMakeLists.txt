# AzerothCore discovers src/ and the Addmod_nightmareScripts loader automatically.
# Native tests are included in the core unit_tests target when BUILD_TESTING=ON.
set_property(GLOBAL APPEND PROPERTY ACORE_MODULE_TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/nightmare_northshire_test.cpp")
set_property(GLOBAL APPEND PROPERTY ACORE_MODULE_TEST_INCLUDES
    "${CMAKE_CURRENT_SOURCE_DIR}/src")
