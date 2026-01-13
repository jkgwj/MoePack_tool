
# 将源代码添加到此项目的可执行文件。
add_executable (${PROJECT_NAME} "MoePack.cpp"  "cmd.cpp" )

#添加依赖库
target_link_libraries(${PROJECT_NAME} 
    PUBLIC
    global_includes
    global_compile_options
    Vulkan::Vulkan
    zstd::libzstd
    ktx
    unofficial-sodium::sodium
)

target_link_options(${PROJECT_NAME} PRIVATE
    /FORCE:MULTIPLE  #ktx的zstd冲突问题，暂时不知如何解决，先强制链接
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET MoePack PROPERTY CXX_STANDARD 17)
endif()

