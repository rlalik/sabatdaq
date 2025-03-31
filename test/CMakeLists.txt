# Parent project does not export its library target, so this CML implicitly
# depends on being added from it, i.e. the testing is done only from the build
# tree and is not feasible from an install location

project(sabatdaqTests LANGUAGES CXX)

# ---- Tests ----

add_executable(sabatdaq_test source/sabatdaq_test.cpp)
target_link_libraries(sabatdaq_test PRIVATE sabatdaq_lib)
target_compile_features(sabatdaq_test PRIVATE cxx_std_20)

add_test(NAME sabatdaq_test COMMAND sabatdaq_test)

# ---- End-of-file commands ----

add_folders(Test)
