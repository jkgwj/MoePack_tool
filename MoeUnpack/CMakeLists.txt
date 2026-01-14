# 将源代码添加到此项目的可执行文件。
add_library (MoeUnpack "MoeUnpack.cpp" )

target_link_libraries(MoeUnpack
    PRIVATE
     zstd::libzstd
     unofficial-sodium::sodium
    PUBLIC      
        global_compile_options
)

set(MOE_UNPACK_DEBUG 1)
if(MOE_UNPACK_DEBUG)
    target_compile_definitions(MoeUnpack PRIVATE ktx)
endif()
