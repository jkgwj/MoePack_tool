
# 将源代码添加到此项目的可执行文件。
add_executable (${PROJECT_NAME} "MoePack.cpp"  "cmd.cpp" )

#添加依赖库
target_link_libraries(${PROJECT_NAME} 
    PUBLIC
    global_includes
    global_compile_options
    PRIVATE 
    unofficial-sodium::sodium
    Vulkan::Headers
    zstd::libzstd
    ktx

    MoeUnpack
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET MoePack PROPERTY CXX_STANDARD 17)
endif()

