set(HEADERS
    Zmeya.h
    ReadOnly/ZmeyaConfig.h
    ReadOnly/ZmeyaHash.h
    ReadOnly/ZmeyaTypes.h
    ReadOnly/ZmeyaPointer.h
    ReadOnly/ZmeyaString.h
    ReadOnly/ZmeyaArray.h
    ReadOnly/ZmeyaHashAdapters.h
    ReadOnly/ZmeyaHashSet.h
    ReadOnly/ZmeyaHashMap.h
    ReadOnly/ZmeyaBlobValidate.h
    ReadOnly/ZmeyaStdHash.h
    Serialize/ZmeyaSerializeFoundation.h
    Serialize/ZmeyaBuilderBase.h
    Serialize/ZmeyaBuilderBase.inc
    Serialize/ZmeyaBuilderBaseArena.inc
    Serialize/ZmeyaBuilderBaseRegistry.inc
    Serialize/ZmeyaBuilderBaseAlloc.inc
    Serialize/ZmeyaBuilderBaseCtor.inc
    Serialize/ZmeyaBuilderBaseStrings.inc
    Serialize/ZmeyaBuilderBaseArrays.inc
    Serialize/ZmeyaBuilderBaseFinalize.inc
    Serialize/ZmeyaBuilderHashChain.inc
    Serialize/ZmeyaBuilderHashChainNodesGrow.inc
    Serialize/ZmeyaBuilderAssignImpl.h
    Serialize/ZmeyaBuilderAssignImpl.inc
    Serialize/ZmeyaBuilder.h
    Serialize/ZmeyaBuilder.inc
    Serialize/ZmeyaBlobWriter.h
    Serialize/ZmeyaBlobWriter.inc
    Serialize/ZmeyaSerializeApi.h
    Serialize/ZmeyaSerializeApi.inc
    Serialize/ZmeyaSerializeApiAssignDetail.inc
    Serialize/ZmeyaSerializeApiAssign.inc
    Serialize/ZmeyaSerializeApiIncremental.inc
    Serialize/ZmeyaSerializeApiInstMembers.inc
    Serialize/ZmeyaSerializeApiBlobWriter.inc
    )

add_library(Zmeya INTERFACE)
target_include_directories(Zmeya INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/ReadOnly"
    "${CMAKE_CURRENT_SOURCE_DIR}/Serialize"
    )
target_compile_features(Zmeya INTERFACE cxx_std_17)

find_package(Threads REQUIRED)
target_link_libraries(Zmeya INTERFACE Threads::Threads)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_compile_definitions(Zmeya INTERFACE _GNU_SOURCE)
endif()
