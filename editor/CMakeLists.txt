add_executable(AREngineEditor
    src/main.cpp
)

target_link_libraries(AREngineEditor
    PRIVATE
        AREngine::Runtime
)

target_compile_features(AREngineEditor PRIVATE cxx_std_20)
