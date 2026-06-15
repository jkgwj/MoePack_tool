/*
 * Copyright 2026 jkgwj
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once
#include <memory>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <vulkan/vulkan.h>
#include "ktx.h"
#include "zstd.h"
#include "stb_image.h"
#include "MoeTypes.h"
#include "MoeUtils.h"
#include "MoeHeader.h"

namespace fs = std::filesystem;

/**
 * @brief MOE_ImageFormat 转 stb_image 格式参数
 * @param format 源图片格式枚举
 * @return int stb_image 格式参数（STBI_rgb / STBI_rgb_alpha 等），不支持返回 -1
 */
static inline int _moe_format_to_stb_format(MOE_ImageFormat format) {
    switch (format) {
    case MOE_ImageFormat::RGBA8888:  return STBI_rgb_alpha;
    case MOE_ImageFormat::RGB888:    return STBI_rgb;
    case MOE_ImageFormat::L8:        return STBI_grey;
    case MOE_ImageFormat::LA88:      return STBI_grey_alpha;
    default:                         return -1;
    }
}

/**
 * @brief 根据目标平台获取默认 GPU 压缩纹理格式
 * @param platform 目标平台枚举
 * @return VkFormat 对应平台的 Vulkan 压缩格式（BC7/ETC2/ASTC）
 */
static inline VkFormat GetGpuCompressionFormat(MOE_Platform platform) {
    switch (platform) {
    case WINDOWS:   return VK_FORMAT_BC7_UNORM_BLOCK;
    case LINUX:     return VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
    case MACOS:     return VK_FORMAT_ASTC_8x8_UNORM_BLOCK;
    case IOS:       return VK_FORMAT_ASTC_6x6_UNORM_BLOCK;
    case ANDROID:   return VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
    default:        return VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
    }
}

/**
 * @brief Vulkan 格式 → KTX 转码格式
 * @param vkFmt Vulkan 格式枚举
 * @return ktx_transcode_fmt_e KTX 转码格式，无法匹配返回 KTX_TTF_NOSELECTION
 */
static inline ktx_transcode_fmt_e VkFormatToKtxTranscodeFmt(VkFormat vkFmt) {
    switch (vkFmt) {
    case VK_FORMAT_BC7_UNORM_BLOCK:             return KTX_TTF_BC7_RGBA;
    case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:   return KTX_TTF_ETC2_RGBA;
    case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:         return KTX_TTF_ASTC_4x4_RGBA;
    case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:         return KTX_TTF_ASTC_4x4_RGBA;
    default:                                     return KTX_TTF_NOSELECTION;
    }
}

/**
 * @brief 打包参数结构体
 * @note 控制打包过程中的所有选项
 */
struct PackParams {
    VkFormat texture_format;            ///< GPU 压缩目标格式（如 BC7_RGBA_UNORM），由目标平台自动设置
    uint32_t mip_levels = 1;            ///< 生成的 MIP 级别数（暂不支持 >1）
    bool generate_mipmaps = false;      ///< 是否自动生成 MIP 贴图

    bool is_2D = true;                  ///< 是否为 2D 纹理（false 为 3D，暂不支持）

    bool ztsd_on = false;               ///< 是否启用 ZSTD 压缩
    int  ztsd_level = 10;               ///< ZSTD 压缩等级（1-22，默认 10）

    bool is_encryption = false;         ///< 是否启用加密
    std::string encryption_key;         ///< 加密密钥（为空则不加密）

    bool use_fixed_salt = false;        ///< 是否使用固定 salt（流式加密用，跨文件密钥可共享）
    std::string fixed_salt_value;       ///< 自定义固定 salt（空 = 使用默认常量 "MoeAudioSaltV1.0"）

    MOE_ImageFormat src_image_format = MOE_ImageFormat::RGBA8888;  ///< 源图像像素格式
};

/**
 * @brief MoePack 打包器类
 *
 * 核心功能：
 * - pack()：图片文件 → stb_image 解码 → KTX2 GPU 压缩 → (ZSTD) → (加密) → .moe
 * - pack_ex()：任意文件（含图片，不做纹理压缩）→ (ZSTD) → (加密) → .moe
 * - pack_ex_stream()：任意文件 → 分块加密(Chacha20-Poly1305) → .moe
 */
class MoePack {
public:
    MoePack();
    ~MoePack();

    /// 设置目标平台（pack 命令必须调用）
    void set_dst_platform(MOE_Platform platform) { dst_platform = platform; }

    /// 设置日志等级（0=简洁 / 1=正常 / 2=详细 / 3=调试）
    void set_log_level(int level) { log_level = level; }

    /// 设置打包参数
    void set_pack_params(const PackParams& params) { pack_params = params; }

    /// 设置源图像格式
    void set_src_image_format(MOE_ImageFormat format) { pack_params.src_image_format = format; }

    /**
     * @brief 配置 ZSTD 压缩
     * @param enable 是否启用
     * @param level 压缩等级 (1-22)
     */
    void ztsd_compression(bool enable, int level);

    /**
     * @brief 配置加密
     * @param enable 是否启用
     * @param key 加密密钥
     */
    void encryption(bool enable, const std::string key);

    /**
     * @brief 启用/禁用固定 salt（流式加密）
     * @param enable 是否启用（salt 使用默认常量）
     */
    void use_fixed_salt(bool enable);

    /**
     * @brief 启用/禁用固定 salt（流式加密），指定自定义 salt
     * @param enable 是否启用
     * @param salt 自定义 salt 字符串（长度 ≤ 16）
     */
    void use_fixed_salt(bool enable, const std::string& salt);

    /**
     * @brief 图片资源封包：完整流程处理图片并输出加密/压缩后的 KTX2 文件
     *
     * 核心流程：图片文件(png/bmp/jpg等) → stb_image 解码(RGBA8888) →
     * 转为 KTX2 GPU 压缩纹理 → (ZSTD压缩) → (XSalsa20-Poly1305加密) → 写入输出文件
     *
     * @param in_path 输入图片文件路径（支持 stb_image 兼容格式：png/bmp/jpg/tga 等）
     * @param out_path 输出打包文件路径（最终为加密/压缩后的 KTX2 二进制数据）
     * @return std::string 成功执行返回 "SUCCESS"
     * @throw std::runtime_error 图片解码失败、KTX2 转换失败、ZSTD 压缩失败、加密失败、文件写入失败时抛出异常
     * @note 1. ZSTD 压缩开关/等级由 pack_params.ztsd_on / pack_params.ztsd_level 控制；
     *       2. 加密开关/密钥由 pack_params.is_encryption / pack_params.encryption_key 控制；
     *       3. 源图片格式由 pack_params.src_image_format 指定，默认 RGBA8888；
     *       4. KTX2 纹理格式由 pack_params.texture_format 指定，需匹配目标平台(dst_platform)
     */
    std::string pack(std::string in_path, const std::string out_path);

    /**
     * @brief 通用资源封包：直接对原始文件进行加密/压缩并添加 MOE 文件头（支持任意格式，包括图片）
     *
     * 核心流程：读取原始文件 → (ZSTD压缩) → (加密) → 添加文件头 → 写入 .moe
     * 与 pack() 的区别：不做图像解码和 KTX2 纹理转换，保持原始文件数据不变
     *
     * @param in_path 输入文件路径（支持任意二进制文件，也支持文件夹批量处理）
     * @param out_path 输出打包文件路径
     * @return std::string 成功执行返回 "SUCCESS"
     * @throw std::runtime_error 文件读取/ZSTD 压缩/加密/写入失败时抛出异常
     * @note 1. ZSTD 压缩开关/等级由 pack_params.ztsd_on / pack_params.ztsd_level 控制；
     *       2. 加密开关/密钥由 pack_params.is_encryption / pack_params.encryption_key 控制；
     *       3. 输出文件扩展名为 .moe
     */
    std::string pack_ex(std::string in_path, std::string out_path);

    /**
     * @brief 分块加密封包：对文件进行分块加密并输出 .moe 文件
     *
     * 适用场景：需要流式播放的大音频文件，自动检测音频格式(WAV/FLAC/MP3/VORBIS)写入文件头
     *
     * @param in_path 输入文件路径
     * @param out_path 输出打包文件路径
     * @param chunk_size 加密块大小(字节)，默认 65536 (64KB)
     * @return std::string 成功执行返回 "SUCCESS"
     * @throw std::runtime_error 文件读取/加密/写入失败时抛出异常
     * @note 使用 Chacha20-Poly1305 流式加密，不支持 ZSTD 压缩
     */
    std::string pack_ex_stream(std::string in_path, std::string out_path,
                                uint32_t chunk_size = 65536);

    // -- 底层接口（开放但不建议直接使用，如需精细控制打包流程可使用） --

    /**
     * @brief 加载图片并转换为 pack_params.src_image_format 指定的格式
     * @param image_path 输入图片路径（支持 PNG/JPG/BMP 等 stb_image 兼容格式）
     * @param h [输出] 图片高度
     * @param w [输出] 图片宽度
     * @return UniquePtr_uChar 图片数据智能指针
     * @throw std::runtime_error 解码失败或格式不支持时抛出异常
     */
    UniquePtr_uChar _load_image_to_src_format(const std::string& image_path, int& h, int& w);

    /**
     * @brief 将源格式数据转换为 KTX2 GPU 压缩纹理
     * @param src_data 源图片数据裸指针（RGBA8888 等格式）
     * @param h [输入/输出] 图片高度
     * @param w [输入/输出] 图片宽度
     * @param size [输出] KTX2 数据大小 (字节)
     * @return UniquePtr_uChar KTX2 数据智能指针
     * @throw std::runtime_error 创建/写入/压缩 KTX 失败时抛出异常
     */
    UniquePtr_uChar _src_format_to_ktx(const unsigned char* src_data, int& h, int& w, int& size);

    /**
     * @brief ZSTD 压缩
     * @param data 待压缩数据裸指针
     * @param size [输入/输出] 输入为原始大小，输出为压缩后大小
     * @return UniquePtr_uChar 压缩后数据智能指针
     * @throw std::runtime_error 压缩失败或参数无效时抛出异常
     */
    UniquePtr_uChar _ztsd_compression(const unsigned char* data, int& size);

    /**
     * @brief XSalsa20-Poly1305 加密
     * @param data 待加密数据裸指针
     * @param size [输入/输出] 输入为原始大小，输出为加密后大小
     * @param key 加密密钥（空则使用 pack_params.encryption_key）
     * @return UniquePtr_uChar 加密后数据智能指针
     * @throw std::runtime_error 加密初始化/密钥派生/内存分配失败时抛出异常
     * @note 输出格式：IV(24) + Salt(16) + 加密内容(原始大小+16 MAC)
     */
    UniquePtr_uChar _encrypt(unsigned char* data, int& size, std::string key);

    /**
     * @brief Chacha20-Poly1305 分块加密
     * @param data 待加密数据
     * @param size 数据大小 (字节)
     * @param key 加密密码
     * @param chunk_size 每块最大明文大小 (字节)
     * @param out_chunk_count [输出] 加密后的总块数
     * @return UniquePtr_uChar 加密后数据 (stream_header + salt + 所有块)
     * @throw std::runtime_error 加密失败时抛出异常
     */
    UniquePtr_uChar _encrypt_stream(unsigned char* data, int size, std::string key,
                                     uint32_t chunk_size, uint32_t& out_chunk_count);

private:
    MOE_Platform dst_platform;      ///< 目标平台
    PackParams pack_params;         ///< 打包参数
    int log_level = 0;              ///< 日志等级

    /**
     * @brief Basis Universal 压缩参数
     */
    struct CompressionParams {
        ktx_uint32_t structSize;                ///< 结构体大小（必须设为 sizeof(ktxBasisParams)）
        ktx_bool_t uastc;                       ///< 使用 UASTC 模式（固定 KTX_TRUE）
        ktx_uint32_t threadCount;               ///< 压缩线程数（默认 8）
        ktx_pack_uastc_flags uastcFlags;        ///< UASTC 压缩标志（级别 + 优化标志）
        ktx_bool_t uastcRDO;                    ///< 是否启用 UASTC RDO（默认 KTX_TRUE）
        float uastcRDOQualityScalar;            ///< UASTC RDO 质量标量（默认 1.5）
        ktx_bool_t useGpuCompression;           ///< 是否使用 GPU 压缩（当前无效）
        ktx_bool_t forceCpuCompression;         ///< 是否强制 CPU 压缩（当前无效）

        CompressionParams() {
            structSize = sizeof(ktxBasisParams);
            uastc = KTX_TRUE;
            threadCount = 8;
            uastcFlags = KTX_PACK_UASTC_LEVEL_DEFAULT;
            uastcRDO = KTX_TRUE;
            uastcRDOQualityScalar = 1.5f;
            useGpuCompression = KTX_TRUE;
            forceCpuCompression = KTX_FALSE;
        }
    };

    /**
     * @brief 根据目标格式和压缩等级设置 Basis 压缩参数
     * @param params [输出] 压缩参数
     * @param target_format 目标 VkFormat（BC7/ETC2/ASTC）
     */
    void _set_basis_compression_params(CompressionParams& params, VkFormat target_format);

    /**
     * @brief 使用 Basis Universal 压缩 KTX2 纹理
     * @param texture KTX2 纹理对象
     * @param target_format 目标 GPU 格式
     * @return ktxResult KTX_SUCCESS 表示成功
     */
    ktxResult _compress_ktx_with_basis(ktxTexture2* texture, VkFormat target_format);

    /**
     * @brief 根据源图像格式计算数据总字节数
     * @param width 图像宽度
     * @param height 图像高度
     * @return size_t 源数据总字节数
     * @throw std::runtime_error 宽高无效或格式不支持时抛出异常
     */
    size_t _calc_src_data_size(int width, int height);
};
