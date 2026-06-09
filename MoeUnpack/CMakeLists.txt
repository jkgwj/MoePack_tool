if(MOE_BUILD_STATIC)
    add_library(MoeUnpack STATIC "MoeUnpack.cpp" "MoeStreamReader.cpp" "MoeSharedStreamReader.cpp")
else()
    add_library(MoeUnpack SHARED "MoeUnpack.cpp" "MoeStreamReader.cpp" "MoeSharedStreamReader.cpp")
endif()

target_include_directories(MoeUnpack
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(MoeUnpack
    PRIVATE
    PUBLIC
     moe_pack_compile_options
     zstd::libzstd
     unofficial-sodium::sodium
)

option(MOE_UNPACK_DEBUG ON)
if(MOE_UNPACK_DEBUG)
    target_compile_definitions(MoeUnpack
    PRIVATE
    ktx
    zstd
    unofficial-sodium
    )
endif()
