add_executable(${PROJECT_NAME}
    "src/MoePack.cpp"
    "src/cmd.cpp"
    "src/MoeUtils.cpp")

target_link_libraries(${PROJECT_NAME}
    PUBLIC
    moe_pack_includes
    moe_pack_compile_options
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
