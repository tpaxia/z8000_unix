include("${CMAKE_CURRENT_LIST_DIR}/emulated-tests.cmake")
target_compile_definitions(test_driver PRIVATE Z8002_MMU)
