set(HEADERS
    Zmeya.h
    ZmeyaConfig.h
    ZmeyaHash.h
    ZmeyaTypes.h
    ZmeyaPointer.h
    ZmeyaString.h
    ZmeyaArray.h
    ZmeyaHashAdapters.h
    ZmeyaHashSet.h
    ZmeyaHashMap.h
    ZmeyaSerializeFoundation.h
    ZmeyaBuilderBase.h
    ZmeyaBuilderBase.inc
    ZmeyaBuilderAssignImpl.h
    ZmeyaBuilderAssignImpl.inc
    ZmeyaBuilder.h
    ZmeyaBuilder.inc
    ZmeyaBlobWriter.h
    ZmeyaBlobWriter.inc
    ZmeyaSerializeApi.h
    ZmeyaSerializeApi.inc
    ZmeyaStdHash.h
    )

add_library(Zmeya INTERFACE)
target_include_directories(Zmeya INTERFACE ./)
target_compile_features(Zmeya INTERFACE cxx_std_17)

