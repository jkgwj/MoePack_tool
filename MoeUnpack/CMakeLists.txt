# 解包库（默认静态库，通过 MOE_BUILD_STATIC 控制）
if(MOE_BUILD_STATIC)
    add_library (MoeUnpack STATIC "MoeUnpack.cpp" )
else()
    add_library (MoeUnpack SHARED "MoeUnpack.cpp" )
endif()

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
