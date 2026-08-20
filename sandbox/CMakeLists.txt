add_executable(AREngineSandbox
    src/main.cpp
)

target_link_libraries(AREngineSandbox
    PRIVATE
        AREngine::Runtime
)

target_compile_features(AREngineSandbox PRIVATE cxx_std_20)
