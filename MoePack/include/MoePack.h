#pragma once
#include <memory>    
#include "ktx.h"
#include "zstd.h"
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
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
#include"stb/stb_image.h"
#include"../../../MoeUnpack/MoeHeader.h"
#include"pch.h"

/**
 * @brief 辅助函数：将 MOE_ImageFormat 转换为 stb_image 识别的格式参数
 * @param format 源图片格式枚举
 * @return stb_image 格式参数（如 STBI_rgba），不支持则返回 -1
 */
static inline int _moe_format_to_stb_format(MOE_ImageFormat format)
{
    switch (format) {
    case MOE_ImageFormat::RGBA8888:  return STBI_rgb_alpha;
    case MOE_ImageFormat::RGB888:    return STBI_rgb;
    case MOE_ImageFormat::L8:        return STBI_grey;
    case MOE_ImageFormat::LA88:      return STBI_grey_alpha;
    default:                         return -1; // 不支持的格式
    }
}

// 打包参数结构体
struct PackParams
{
	VkFormat texture_format;                                                // 目标纹理格式
	uint32_t mip_levels=1;                                                    // 生成的 MIP 级别数
	bool generate_mipmaps=0;                                                // 是否生成 MIP 贴图
									                                       
	bool is_2D=1;                                                           // 是否为 2D 纹理
									                                       
	bool ztsd_on = 0;                                                       // 是否启用 ZTSD 压缩
	int  ztsd_level = 10;                                                    // ZTSD 压缩等级
	bool is_encryption = 0;                                                 // 是否加密
	std::string encryption_key;                                                 // 加密密钥

	MOE_ImageFormat src_image_format=MOE_ImageFormat::RGBA8888;             // 源图像格式
};

// MoePack 类定义
class MoePack
{
public:
	MoePack();
	~MoePack();
	//设置目标平台(必要)
	void set_dst_platform(MOE_Platform platform) { dst_platform = platform; }

	//设置日志等级
	void set_log_level(int level) { log_level = level; }
	
	//设置打包参数
	void set_pack_params(const PackParams& params) { pack_params = params; }
	//void set_3D_texture() { pack_params.is_2D = 0; }//暂时不支持3D纹理
	void ztsd_compression(bool enable, int level);
	void encryption(bool enable, const std::string key);
	
	
    /**
    * @brief 封包函数：完整流程处理图片并输出加密/压缩后的KTX2文件
    *        核心流程：图片文件(png/bmp/jpg等) → stb_image解码(RGBA8888) →
    *        转为KTX2 GPU压缩纹理 → (ZSTD压缩) → (XSalsa20-Poly1305加密) → 写入输出文件
    * @param in_path 输入图片文件路径（支持stb_image兼容格式：png/bmp/jpg/tga等）
    * @param out_path 输出打包文件路径（最终为加密/压缩后的KTX2二进制数据）
    * @return std::string 成功执行返回"SUCCESS"
    * @throw std::runtime_error 图片解码失败、KTX2转换失败、ZSTD压缩失败、加密失败、文件写入失败时抛出异常
    * @note 1. ZSTD压缩开关/等级由pack_params.ztsd_on/pack_params.ztsd_level控制；
    *       2. 加密开关/密钥由pack_params.is_encryption/pack_params.encryption_key控制；
    *       3. 源图片格式由pack_params.src_image_format指定，默认RGBA8888；
    *       4. KTX2纹理格式由pack_params.texture_format指定，需匹配目标平台(dst_platform)
    */
	std::string pack( std::string in_path,const std::string out_path);





	//开放但不建议直接使用的接口
	//如果想仔细控制打包过程，请使用或修该这些接口
	void set_src_image_format(MOE_ImageFormat format) { pack_params.src_image_format = format; }
	
	/**
    * @brief 加载图片并转换为 pack_params.src_image_format 指定的格式
    * @param image_path 输入图片路径（支持PNG/JPG/BMP等stb_image兼容格式）
    * @param h [输出] 图片高度（引用传递，函数内自动填充）
    * @param w [输出] 图片宽度（引用传递，函数内自动填充）
    * @return UniquePtr_uChar 封装的图片数据智能指针
    */
	UniquePtr_uChar _load_image_to_src_format(const std::string& image_path,int& h,int& w);
    /**
    * @brief 将源格式数据转换为 KTX2 内存数据
    * @param src_data 源图片数据裸指针
    * @param h [输入/输出] 图片高度（输入：源数据高度；输出：KTX 纹理高度）
    * @param w [输入/输出] 图片宽度（输入：源数据宽度；输出：KTX 纹理宽度）
    * @param size [输入输出] 输入：原始 KTX 数据大小（字节）；输出：压缩后数据大小（字节）
    * @return UniquePtr_uChar 封装的 KTX2 内存数据
    * @throw std::runtime_error 创建/写入 KTX 失败时抛出异常
    */
	UniquePtr_uChar _src_format_to_ktx(const unsigned char* src_data,int& h, int& w,int& size);
    /**
    * @brief 对 KTX 数据进行 ZSTD 压缩
    * @param ktx_data 待压缩的 KTX 原始数据裸指针
    * @param size [输入输出] 输入：原始 KTX 数据大小（字节）；输出：压缩后数据大小（字节）
    * @return UniquePtr_uChar 封装压缩后数据的智能指针
    * @throw std::runtime_error 压缩失败/参数无效时抛出异常
    */
    UniquePtr_uChar _ztsd_compression(const unsigned char* ktx_data, int& size);
	/**
    * @brief 对二进制数据（如压缩后的KTX数据）进行加密（基于libsodium的XSalsa20-Poly1305算法）
    * @param data 待加密的原始数据裸指针
    * @param size 待加密数据的长度（字节），必须大于0
    * @param key 加密密钥
    * @return UniquePtr_uChar 加密后的数据（数据结构：IV(24)+盐值(16)+加密内容(原始长度+16MAC)）
    * @throw std::runtime_error 加密初始化/参数无效/密钥派生/内存分配失败时抛出异常
    */
	UniquePtr_uChar _encrypt( unsigned char* data,int& size,std::string key);

	

private:
	MOE_Platform dst_platform;// 目标平台
	PackParams pack_params;// 打包参数

	int log_level = 0; // 日志等级（0：基础日志，1：基本日志，2：详细日志，3：调试日志）

    // 压缩器参数
    struct CompressionParams {
        ktx_uint32_t structSize;                 // 结构体大小（必须设置）
        ktx_bool_t uastc;                        // 使用 UASTC 模式（固定为 true）
        ktx_uint32_t threadCount;                // 压缩线程数
        ktx_pack_uastc_flags uastcFlags;         // UASTC 压缩标志
        ktx_bool_t uastcRDO;                     // 是否启用UASTC RDO
        float uastcRDOQualityScalar;             // UASTC RDO质量标量
        ktx_bool_t useGpuCompression;            // 是否使用GPU压缩(暂不支持)
        ktx_bool_t forceCpuCompression;          // 强制使用CPU压缩(暂不支持)

        CompressionParams() {
            structSize = sizeof(ktxBasisParams);  // 正确设置结构体大小
            uastc = KTX_TRUE;                     // 固定使用 UASTC 模式
            threadCount = 8;                      // 默认8线程
            uastcFlags = KTX_PACK_UASTC_LEVEL_DEFAULT;  // 默认压缩级别
            uastcRDO = KTX_TRUE;                 // 默认启用UASTC RDO
            uastcRDOQualityScalar = 1.5f;         // 默认质量标量
            useGpuCompression = KTX_TRUE;        // 默认启用GPU加速
            forceCpuCompression = KTX_FALSE;     // 默认不强制CPU
        }
    };

    //basis 压缩 高级函数
    void _set_basis_compression_params(CompressionParams& params, VkFormat target_format);
    ktxResult _compress_ktx_with_basis(ktxTexture2* texture, VkFormat target_format);

    /**
    * @brief 核心：根据源图像格式计算数据总字节数
    * @param width  图像宽度
    * @param height 图像高度
    * @return size_t 源数据总字节数
    * @throw std::runtime_error 宽高无效或格式不支持时抛出异常
    */
    size_t _calc_src_data_size(int width, int height);

};
