if(MOE_BUILD_STATIC)
    add_library(MoeUnpack STATIC "MoeUnpack.cpp" "MoeStreamReader.cpp" "MoeSharedStreamReader.cpp")
else()
    add_library(MoeUnpack SHARED "MoeUnpack.cpp" "MoeStreamReader.cpp" "MoeSharedStreamReader.cpp")
endif()

target_include_directories(MoeUnpack
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_SOURCE_DIR}/libs/stb
)

target_link_libraries(MoeUnpack
    PRIVATE
    moe_pack_includes
    PUBLIC
     moe_pack_compile_options
     zstd::libzstd
     unofficial-sodium::sodium
)
