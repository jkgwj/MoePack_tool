cmake_minimum_required (VERSION 3.8)

# 如果支持，请为 MSVC 编译器启用热重载。
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

set(PROJECT_NAME "MoePack")

project (${PROJECT_NAME}
         VERSION 1.0
         LANGUAGES C CXX
)


# 全局语言标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# ========== MoePack 编译选项 ==========
option(MOE_BUILD_STATIC "构建为静态库（默认ON），OFF则构建动态库" ON)
option(MOE_BUILD_PACK "是否编译封包库和封包工具（默认OFF，仅编译解包库）" OFF)


# 设置输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/$<CONFIG>)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/$<CONFIG>)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/$<CONFIG>)

#第三方库

# 输出Vulkan的搜索路径
message(STATUS "Vulkan search paths: ${CMAKE_PREFIX_PATH}")
# 查找Vulkan库
find_package(Vulkan REQUIRED)
# 输出关键信息
if(Vulkan_FOUND)
    message(STATUS "Vulkan 是否找到: ${Vulkan_FOUND}")          # 成功则为TRUE/1
    message(STATUS "Vulkan 版本: ${Vulkan_VERSION}")            # 输出Vulkan SDK版本
    message(STATUS "Vulkan 头文件目录: ${Vulkan_INCLUDE_DIRS}")  # 头文件路径（含vulkan.h）
    message(STATUS "Vulkan 库文件: ${Vulkan_LIBRARIES}")         # 链接的库文件（如vulkan-1.lib）
else()
    message(FATAL_ERROR "Vulkan库未找到，请检查是否已通过vcpkg安装（vcpkg install vulkan）或Vulkan SDK是否正确配置")
endif()

#压缩
find_package(zstd CONFIG REQUIRED)
message(STATUS "zstd 是否找到: ${zstd_FOUND}")          # 成功则为TRUE/1


#sodium(加密)
find_package(unofficial-sodium CONFIG REQUIRED)
message(STATUS "sodium 是否找到: ${unofficial-sodium_FOUND}")

add_definitions(-DKHRONOS_STATIC)
set(KTX_FEATURE_BASIS_COMPRESSION ON CACHE BOOL "启用 Basis 压缩模块" FORCE)
set(KTX_FEATURE_TRANSCODING ON CACHE BOOL "启用转码模块" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "编译 KTX 静态库" FORCE)
set(KTX_FEATURE_LOADTEST OFF CACHE BOOL "关闭测试程序" FORCE)
set(KTX_FEATURE_TOOLS OFF CACHE BOOL "关闭工具程序" FORCE)
add_subdirectory(libs/KTX-Software EXCLUDE_FROM_ALL)


# 设置头文件目录
add_library(moe_pack_includes INTERFACE)
target_include_directories(moe_pack_includes INTERFACE
 "MoePack/include"
 "libs/stb"
)

# 全局编译选项
add_library(moe_pack_compile_options INTERFACE)
if(MSVC)
     target_compile_options(moe_pack_compile_options INTERFACE
        /utf-8
        /D_UNICODE
        /DUNICODE
        /MP  # 多处理器编译

    )
else()
    target_compile_options(moe_pack_compile_options INTERFACE
        -finput-charset=UTF-8
        -fexec-charset=UTF-8
    )
    target_compile_definitions(moe_pack_compile_options INTERFACE
        _LARGEFILE64_SOURCE
        _FILE_OFFSET_BITS=64
    )

endif()

# 解包库（默认编译）
message(STATUS "MoeUnpack 模块已启用")
add_subdirectory ("MoeUnpack")

# 封包库和封包工具（仅在 MOE_BUILD_PACK=ON 时编译）
if(MOE_BUILD_PACK)
    message(STATUS "MoePack 模块已启用")
    add_subdirectory ("MoePack")
else()
    message(STATUS "MoePack 模块未启用（使用 -DMOE_BUILD_PACK=ON 开启）")
endif()
