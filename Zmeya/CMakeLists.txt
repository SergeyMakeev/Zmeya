set(HEADERS
    Zmeya.h
    )

add_library(Zmeya INTERFACE)
target_include_directories(Zmeya INTERFACE ./)
target_compile_features(Zmeya INTERFACE cxx_std_17)

