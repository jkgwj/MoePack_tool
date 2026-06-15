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
#include <string>
#include "MoeHeader.h"

namespace MoeUnpack {
    /// 全局日志等级（0=简洁 / 1=正常 / 2=详细 / 3=调试）
    extern int unpack_log_level;

    /// 设置日志等级
    inline void set_unpack_log_level(int level) { unpack_log_level = level; }

    /**
     * @brief 内部解包处理结果
     */
    struct _UnpackResult {
        UniquePtr_uChar data;           ///< 处理后的数据指针
        int size = 0;                   ///< 数据大小（字节）
        bool was_encrypted = false;     ///< 原始数据是否加密
        bool was_compressed = false;    ///< 原始数据是否压缩
    };

    /**
     * @brief XSalsa20-Poly1305 解密
     *
     * 数据格式：IV(24) + Salt(16) + 加密内容(原始大小 + 16 MAC)
     *
     * @param data 待解密数据指针
     * @param size [输入/输出] 输入为加密数据大小，输出为解密后数据大小
     * @param key 解密密钥
     * @return UniquePtr_uChar 解密后数据智能指针
     * @throw std::runtime_error 解密失败时抛出异常
     */
    UniquePtr_uChar _decrypt(unsigned char* data, int& size, std::string key);

    /**
     * @brief ZSTD 解压
     * @param data 待解压数据指针
     * @param size [输入/输出] 输入为压缩数据大小，输出为解压后数据大小
     * @return UniquePtr_uChar 解压后数据智能指针
     * @throw std::runtime_error 解压失败时抛出异常
     */
    UniquePtr_uChar _ztsd_decompression(const unsigned char* data, int& size);

    /**
     * @brief SHA-256 数据完整性校验
     * @param data 待校验数据指针
     * @param size 待校验数据大小
     * @param expected_hash 期望的 SHA-256 哈希值（32 字节）
     * @return bool 校验通过返回 true
     */
    bool _verify_data_integrity(const unsigned char* data, int size, const uint8_t* expected_hash);

    /**
     * @brief 解析 MOE 文件头部数据
     * @param data MOE 头部原始字节数据指针（98 字节）
     * @param head_data [输出] 解析后的 MoeHeader 结构体
     * @throw std::runtime_error 魔数无效、头部大小不匹配、版本号不匹配时抛出异常
     */
    void unpack_moe_header(unsigned char* data, MoeHeader& head_data);

    /**
     * @brief Chacha20-Poly1305 分块解密并重组为连续缓冲区
     *
     * 数据格式：stream_header(24) + salt(16) + 分块密文...
     *
     * @param data 加密数据指针
     * @param size [输入/输出] 输入为加密数据总大小，输出为解密后总大小
     * @param header MOE 文件头部（提供 chunk_count / original_size）
     * @param key 解密密码
     * @return UniquePtr_uChar 解密后连续数据
     * @throw std::runtime_error 解密失败/数据不完整时抛出异常
     */
    UniquePtr_uChar _decrypt_chunked(unsigned char* data, int& size,
                                      const MoeHeader& header,
                                      const std::string& key);

    /**
     * @brief 内部函数：打开 .moe 文件并完成 读取→校验→解密→解压 全流程
     * @param in_put 输入 .moe 文件路径
     * @param _key 解密密钥（未加密可为空）
     * @return _UnpackResult 处理后的数据及元信息
     * @throw std::runtime_error 文件不存在/格式无效/解密失败/解压失败时抛出异常
     */
    _UnpackResult _unpack_process_file(const std::string& in_put, const std::string& _key);

    /**
     * @brief 图片资源解包：解密→解压→KTX2 验证→输出 .ktx2 文件
     * @param in_put 输入 .moe 文件路径
     * @param out_put 输出路径（目录或文件路径）
     * @param _key 解密密钥（未加密可为空）
     * @return std::string 成功返回 "SUCCESS"
     * @throw std::runtime_error 解密/解压/KTX验证/写入失败时抛出异常
     */
    std::string unpack(std::string in_put, std::string out_put, std::string _key = "");

    /**
     * @brief 图片资源解包（内存版本）：返回解密解压后的 KTX2 数据指针
     * @param in_put 输入 .moe 文件路径
     * @param size [输出] 解包后数据大小
     * @param _key 解密密钥
     * @return unsigned char* 解包后数据指针（调用者需 free 释放）
     */
    unsigned char* unpack(std::string in_put, int& size, std::string _key = "");

    /**
     * @brief 非图片资源解包：解密→解压→输出原始文件（不做 KTX2 验证）
     *
     * 与 unpack() 的区别：不解码 KTX 纹理，适用于 MP3/FLAC/OGG 等任意二进制文件
     *
     * @param in_put 输入 .moe 文件路径
     * @param out_put 输出路径（输出文件名不添加扩展名）
     * @param _key 解密密钥（未加密可为空）
     * @return std::string 成功返回 "SUCCESS"
     * @throw std::runtime_error 文件读取/解密/解压/写入失败时抛出异常
     */
    std::string unpack_ex(std::string in_put, std::string out_put, std::string _key = "");

    /**
     * @brief 非图片资源解包（内存版本）：返回原始数据指针
     * @param in_put 输入 .moe 文件路径
     * @param size [输出] 解包后数据大小
     * @param _key 解密密钥
     * @return unsigned char* 解包后数据指针（调用者需 free 释放）
     */
    unsigned char* unpack_ex(std::string in_put, int& size, std::string _key = "");

    /**
     * @brief 图片解包解码结果
     */
    struct DecodedImage {
        UniquePtr_uChar pixel_data;      ///< 解码后的像素数据
        int             width    = 0;    ///< 图片宽度
        int             height   = 0;    ///< 图片高度
        int             channels = 0;    ///< 原始文件通道数
        MOE_ImageFormat format;          ///< 输出像素格式
    };

    /**
     * @brief 解包并解码图片为像素数据
     *
     * 核心流程：文件读取→校验→解密→解压→stb_image解码→返回像素
     *
     * @param in_put 输入 .moe 文件路径
     * @param _key 解密密钥（未加密可为空）
     * @param desired_format 期望的输出像素格式，默认 RGBA8888
     * @return DecodedImage 解码后的像素数据及尺寸信息
     * @throw std::runtime_error 解密/解压/解码失败时抛出异常
     */
    DecodedImage unpack_ex_decode(std::string in_put, std::string _key = "",
                                   MOE_ImageFormat desired_format = MOE_ImageFormat::RGBA8888);
}
