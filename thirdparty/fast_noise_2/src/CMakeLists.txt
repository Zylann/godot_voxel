set(CMAKE_CXX_STANDARD 17)

set(install_targets ${install_targets} FastNoise PARENT_SCOPE)

file(GLOB_RECURSE FastSIMD_headers "../include/FastSIMD/*.h")
file(GLOB_RECURSE FastSIMD_include_inl "../include/FastSIMD/*.inl")
file(GLOB FastSIMD_inline "FastSIMD/*.inl")
file(GLOB_RECURSE FastSIMD_internal_headers "FastSIMD/Internal/*.h")
file(GLOB_RECURSE FastSIMD_internal_inl "FastSIMD/Internal/*.inl")

set(install_fastsimd_headers ${FastSIMD_headers} PARENT_SCOPE)

list(APPEND FastSIMD_headers ${FastSIMD_inline})
list(APPEND FastSIMD_headers ${FastSIMD_include_inl})
list(APPEND FastSIMD_internal_headers ${FastSIMD_internal_inl})



if(FASTSIMD_COMPILE_HAVE_NEON)

    set(FastSIMD_sources
        FastSIMD/FastSIMD.cpp
        FastSIMD/FastSIMD_Level_NEON.cpp
        FastSIMD/FastSIMD_Level_Scalar.cpp
    )

elseif(FASTSIMD_COMPILE_ARM)

    set(FastSIMD_sources
        FastSIMD/FastSIMD.cpp
        FastSIMD/FastSIMD_Level_Scalar.cpp
    )
    
else()

    set(FastSIMD_sources
        FastSIMD/FastSIMD.cpp
        FastSIMD/FastSIMD_Level_AVX2.cpp
        FastSIMD/FastSIMD_Level_AVX512.cpp
        FastSIMD/FastSIMD_Level_Scalar.cpp
        FastSIMD/FastSIMD_Level_SSE2.cpp
        FastSIMD/FastSIMD_Level_SSE3.cpp
        FastSIMD/FastSIMD_Level_SSE41.cpp
        FastSIMD/FastSIMD_Level_SSE42.cpp
        FastSIMD/FastSIMD_Level_SSSE3.cpp
    )

endif()



file(GLOB FastNoise_headers "../include/FastNoise/*.h")
file(GLOB FastNoise_inl "../include/FastNoise/*.inl")
file(GLOB_RECURSE FastNoise_generators_headers "../include/FastNoise/Generators/*.h")
file(GLOB_RECURSE FastNoise_generators_inl "../include/FastNoise/Generators/*.inl")

set(install_fastnoise_headers ${FastNoise_headers} PARENT_SCOPE)
set(install_fastnoise_generators_headers ${FastNoise_generators_headers} PARENT_SCOPE)

list(APPEND FastNoise_headers ${FastNoise_inl})
list(APPEND FastNoise_generators_headers ${FastNoise_generators_inl})

set(FastNoise_source
    FastNoise/Metadata.cpp
    FastNoise/SmartNode.cpp
    FastNoise/FastNoise_C.cpp)

source_group("FastSIMD" FILES ${FastSIMD_headers})
source_group("FastSIMD" FILES ${FastSIMD_sources})
source_group("FastSIMD\\internals" FILES ${FastSIMD_internal_headers})

source_group("FastNoise" FILES ${FastNoise_headers})
source_group("FastNoise" FILES ${FastNoise_source})
source_group("FastNoise\\Generators" FILES ${FastNoise_generators_headers})

add_library(FastNoise
    ${FastNoise_headers}
    ${FastNoise_source}
    ${FastNoise_generators_headers}
    ${FastSIMD_headers}
    ${FastSIMD_internal_headers}
    ${FastSIMD_sources}
)

add_library(FastNoise2 ALIAS FastNoise)

target_include_directories(FastNoise PUBLIC 
    $<BUILD_INTERFACE:${FastNoise2_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(NOT BUILD_SHARED_LIBS)
    target_compile_definitions(FastNoise PUBLIC FASTNOISE_STATIC_LIB)
endif()

set_target_properties(FastNoise PROPERTIES
    DEFINE_SYMBOL FASTNOISE_EXPORT
    DEBUG_POSTFIX D
    COMPILE_PDB_NAME_DEBUG FastNoiseD)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    target_compile_options(FastNoise PRIVATE /GL- /GS- /fp:fast /wd4251)
    
    if(NOT FASTSIMD_COMPILE_ARM)
    
        if(CMAKE_SIZEOF_VOID_P EQUAL 4)
            set_source_files_properties(FastSIMD/FastSIMD_Level_Scalar.cpp PROPERTIES COMPILE_FLAGS "/arch:SSE")
            set_source_files_properties(FastSIMD/FastSIMD_Level_SSE2.cpp PROPERTIES COMPILE_FLAGS "/arch:SSE2")
            set_source_files_properties(FastSIMD/FastSIMD_Level_SSE3.cpp PROPERTIES COMPILE_FLAGS "/arch:SSE2")
            set_source_files_properties(FastSIMD/FastSIMD_Level_SSSE3.cpp PROPERTIES COMPILE_FLAGS "/arch:SSE2")
            set_source_files_properties(FastSIMD/FastSIMD_Level_SSE41.cpp PROPERTIES COMPILE_FLAGS "/arch:SSE2")
            set_source_files_properties(FastSIMD/FastSIMD_Level_SSE42.cpp PROPERTIES COMPILE_FLAGS "/arch:SSE2")
        endif()
        set_source_files_properties(FastSIMD/FastSIMD_Level_AVX2.cpp PROPERTIES COMPILE_FLAGS "/arch:AVX2")
        set_source_files_properties(FastSIMD/FastSIMD_Level_AVX512.cpp PROPERTIES COMPILE_FLAGS "/arch:AVX512")
    
    elseif(FASTSIMD_COMPILE_ARMV7)
        set_source_files_properties(FastSIMD/FastSIMD_Level_NEON.cpp PROPERTIES COMPILE_FLAGS "/arch:NEON")
    endif()

elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
    if(MSVC)
        target_compile_options(FastNoise PRIVATE /GL- /GS- /fp:fast)
    else()
        target_compile_options(FastNoise PRIVATE -ffast-math -fno-stack-protector)        
    endif()
    
    if(NOT FASTSIMD_COMPILE_ARM)

        if(CMAKE_SIZEOF_VOID_P EQUAL 4 OR "${CMAKE_CXX_FLAGS}" MATCHES "-m32")
            set_source_files_properties(FastSIMD/FastSIMD_Level_Scalar.cpp PROPERTIES COMPILE_FLAGS "-msse")
            set_source_files_properties(FastSIMD/FastSIMD_Level_SSE2.cpp PROPERTIES COMPILE_FLAGS "-msse2")
        endif()
        set_source_files_properties(FastSIMD/FastSIMD_Level_SSE3.cpp PROPERTIES COMPILE_FLAGS "-msse3")
        set_source_files_properties(FastSIMD/FastSIMD_Level_SSSE3.cpp PROPERTIES COMPILE_FLAGS "-mssse3")
        set_source_files_properties(FastSIMD/FastSIMD_Level_SSE41.cpp PROPERTIES COMPILE_FLAGS "-msse4.1")
        set_source_files_properties(FastSIMD/FastSIMD_Level_SSE42.cpp PROPERTIES COMPILE_FLAGS "-msse4.2")
        set_source_files_properties(FastSIMD/FastSIMD_Level_AVX2.cpp PROPERTIES COMPILE_FLAGS "-mavx2 -mfma")
        set_source_files_properties(FastSIMD/FastSIMD_Level_AVX512.cpp PROPERTIES COMPILE_FLAGS "-mavx512f -mavx512dq -mfma")
    
    elseif(FASTSIMD_COMPILE_ARMV7)
        set_source_files_properties(FastSIMD/FastSIMD_Level_NEON.cpp PROPERTIES COMPILE_FLAGS "-march=armv7-a -mfpu=neon")
    endif()
    
    
endif()

