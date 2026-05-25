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
#include<string>
#include<fstream>
#include<filesystem>
#include<vector>
#include"MoeHeader.h"
#ifdef MOE_UNPACK_CHECK_DATA
#include"ktx.h"
#endif
#include"zstd.h"
#include"sodium.h"

namespace MoeUnpack{
	int unpack_log_level = 1; //默认日志等级
	//设置日志等级
	void set_unpack_log_level(int level) { unpack_log_level = level; }

	/**
    * @brief 解密函数
    *
    * @param data 待解密数据指针
    * @param size 待解密数据大小（输入时是加密数据大小，输出时是解密后数据大小）
    * @param key 解密密钥
    * @return UniquePtr_uChar 解密后的数据智能指针（不对外返回）
    * @throws std::runtime_error 解密失败时抛出异常
    */
    UniquePtr_uChar _decrypt(unsigned char* data, int& size, std::string key);
    /**
    * @brief ZSTD解压函数
    *
    * @param data 待解压数据指针
    * @param size 待解压数据大小（输入时是压缩数据大小，输出时是解压后数据大小）
    * @return UniquePtr_uChar 解压后的数据智能指针（不对外返回）
    * @throws std::runtime_error 解压失败时抛出异常
    */
    UniquePtr_uChar _ztsd_decompression(const unsigned char* data, int& size);
    /**
    * @brief 校验数据完整性（验证SHA256哈希）
    *
    * @param data 待校验数据指针
    * @param size 待校验数据大小
    * @param expected_hash 期望的SHA256哈希值
    * @return bool 校验通过返回true，否则返回false
    */
    bool _verify_data_integrity(const unsigned char* data, int size, const uint8_t* expected_hash);

    /**
    * @brief 解析MOE格式文件头部数据
    *
    * @details 从原始字节数据中解析MOE格式文件头部
    *
    * @param data 原始MOE头部字节数据指针
    * @param head_data 输出参数，解析后的MOE头部结构体（会覆盖原有值）
    * @throw std::runtime_error 当输入指针为空、魔数非法、头部大小不匹配时抛出异常
    */
    void unpack_moe_header(unsigned char* data, MoeHeader& head_data);

    /**
    * @brief 解包函数
    *
    */
    std::string unpack(std::string in_put/*某个文件的绝对路径*/, std::string out_put,std::string _key);

    /**
    * @brief 解包函数
    *
    */
    unsigned char* unpack(std::string in_put/*某个文件的绝对路径*/, int& size, std::string _key);

    /**
    * @brief 非图片资源解包函数：解密/解压后直接返回原始数据，不做KTX验证
    *        与 unpack() 的区别：不解码KTX纹理，适用于MP3/FLAC/OGG等任意二进制文件
    *        核心流程：读取.moe → 解析头部 → 校验 → (解密) → (解压) → 输出原始数据
    * @param in_put 输入文件路径（.moe文件）
    * @param out_put 输出路径（输出文件名不添加扩展名，由解码器根据文件头判断格式）
    * @param _key 解密密钥（文件未加密时可为空）
    * @return std::string 成功执行返回"SUCCESS"
    * @throw std::runtime_error 文件读取/解密/解压/写入失败时抛出异常
    */
    std::string unpack_ex(std::string in_put, std::string out_put, std::string _key = "");

    /**
    * @brief 非图片资源解包函数（内存版本）：返回原始数据指针
    * @param in_put 输入文件路径（.moe文件）
    * @param size [输出] 解包后数据大小
    * @param _key 解密密钥
    * @return unsigned char* 解包后的原始数据指针（调用者需 free 释放）
    */
    unsigned char* unpack_ex(std::string in_put, int& size, std::string _key = "");
}

#ifdef MOE_UNPACK_IMPLEMENTATION

UniquePtr_uChar MoeUnpack::_decrypt(unsigned char* data, int& size, std::string key) {
    // 初始化 libsodium
    if (sodium_init() == -1) {
        throw std::runtime_error("解密失败：libsodium 初始化失败");
    }

    // 校验输入数据
    if (!data) {
        throw std::runtime_error("解密失败：待解密数据指针为空");
    }
    if (size <= 0) {
        throw std::runtime_error("解密失败：待解密数据长度无效");
    }

    // 保存加密数据大小用于日志
    int encrypted_size = size;

    // 检查数据长度是否足够包含 IV 和盐值
    size_t min_required_size = crypto_secretbox_NONCEBYTES + crypto_pwhash_SALTBYTES + crypto_secretbox_MACBYTES;
    if (static_cast<size_t>(size) < min_required_size) {
        throw std::runtime_error("解密失败：数据长度不足，无法解析加密格式");
    }

    // 解析加密数据格式：IV(24字节) + 盐值(16字节) + 加密数据
    unsigned char* nonce = data;  // 前24字节为IV
    unsigned char* salt = data + crypto_secretbox_NONCEBYTES;  // 接着16字节为盐值
    unsigned char* encrypted_content = data + crypto_secretbox_NONCEBYTES + crypto_pwhash_SALTBYTES;  // 剩余为加密内容
    size_t encrypted_content_len = static_cast<size_t>(size) - crypto_secretbox_NONCEBYTES - crypto_pwhash_SALTBYTES;

    // 密钥派生（使用相同的盐值）
    unsigned char crypto_key[crypto_secretbox_KEYBYTES];
    int ret = crypto_pwhash(
        crypto_key, sizeof(crypto_key),
        key.c_str(), key.length(),
        salt,
        crypto_pwhash_OPSLIMIT_INTERACTIVE,
        crypto_pwhash_MEMLIMIT_INTERACTIVE,
        crypto_pwhash_ALG_DEFAULT
    );

    if (ret != 0) {
        sodium_memzero(crypto_key, sizeof(crypto_key));
        throw std::runtime_error("解密失败：密钥派生失败");
    }

    // 分配解密结果缓冲区
    size_t decrypted_content_len = encrypted_content_len - crypto_secretbox_MACBYTES;
    unsigned char* decrypted_buffer = static_cast<unsigned char*>(malloc(decrypted_content_len));
    if (!decrypted_buffer) {
        sodium_memzero(crypto_key, sizeof(crypto_key));
        throw std::runtime_error("解密失败：分配解密缓冲区内存不足");
    }

    // 核心解密操作
    if (crypto_secretbox_open_easy(
        decrypted_buffer,
        encrypted_content,
        encrypted_content_len,
        nonce,
        crypto_key) != 0) {
        free(decrypted_buffer);
        sodium_memzero(crypto_key, sizeof(crypto_key));
        throw std::runtime_error("解密失败：数据校验失败（可能密钥错误或数据损坏）");
    }

    // 安全清理敏感数据
    sodium_memzero(crypto_key, sizeof(crypto_key));

    // 更新 size 为解密后的数据大小
    size = static_cast<int>(decrypted_content_len);

    if (unpack_log_level >= 3) {
        printf("解密成功：\n"
            "  加密数据长度：%d 字节 | 解密后长度：%d 字节 | 解密算法：XSalsa20-Poly1305\n"
            "  加密内容结构：IV(24字节) + 盐值(16字节) + 加密数据(%zu字节 + 16字节MAC)\n",
            encrypted_size,
            size,
            decrypted_content_len);
    }

    return UniquePtr_uChar(decrypted_buffer);
}

UniquePtr_uChar MoeUnpack::_ztsd_decompression(const unsigned char* data, int& size) {
    // 校验输入数据
    if (!data) {
        throw std::runtime_error("ZSTD 解压失败：数据指针为空");
    }
    if (size <= 0) {
        throw std::runtime_error("ZSTD 解压失败：无效的压缩数据大小 [" + std::to_string(size) + "]");
    }

    // 保存压缩数据大小用于日志
    int compressed_size = size;

    // 获取解压后数据大小
    size_t decompressed_size = ZSTD_getFrameContentSize(data, static_cast<size_t>(size));

    if (decompressed_size == ZSTD_CONTENTSIZE_ERROR) {
        throw std::runtime_error("ZSTD 解压失败：不是有效的 ZSTD 压缩数据");
    }
    if (decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN) {
        throw std::runtime_error("ZSTD 解压失败：无法确定解压后数据大小（可能数据流过大）");
    }
    if (decompressed_size == 0) {
        throw std::runtime_error("ZSTD 解压失败：解压后数据大小为 0");
    }

    // 分配解压输出缓冲区
    unsigned char* decompressed_data = static_cast<unsigned char*>(malloc(decompressed_size));
    if (!decompressed_data) {
        throw std::runtime_error("ZSTD 解压失败：分配解压缓冲区内存失败（需要 " +
            std::to_string(decompressed_size) + " 字节）");
    }

    // 执行 ZSTD 解压
    size_t actual_decompressed_size = ZSTD_decompress(
        decompressed_data,      // 输出缓冲区
        decompressed_size,      // 输出缓冲区大小
        data,                   // 输入数据
        static_cast<size_t>(size) // 输入数据大小
    );

    // 校验解压结果
    if (ZSTD_isError(actual_decompressed_size)) {
        free(decompressed_data);
        std::string err_msg = "ZSTD 解压失败：" + std::string(ZSTD_getErrorName(actual_decompressed_size));
        throw std::runtime_error(err_msg);
    }

    // 验证解压后数据大小
    if (actual_decompressed_size != decompressed_size) {
        free(decompressed_data);
        throw std::runtime_error("ZSTD 解压失败：解压后大小与预期不符（预期：" +
            std::to_string(decompressed_size) + " 字节，实际：" +
            std::to_string(actual_decompressed_size) + " 字节）");
    }

    // 更新 size 为解压后大小
    size = static_cast<int>(actual_decompressed_size);

    // 封装为智能指针返回
    UniquePtr_uChar decompressed_ptr(decompressed_data);

    if (unpack_log_level >= 3) {
        printf("ZSTD 解压成功：\n"
            "  压缩后大小：%d 字节 | 解压后大小：%d 字节\n"
            "  解压缩比：%.2f:1\n",
            compressed_size,
            size,
            static_cast<float>(size) / static_cast<float>(compressed_size));
    }

    return decompressed_ptr;
}

bool MoeUnpack::_verify_data_integrity(const unsigned char* data, int size, const uint8_t* expected_hash) {
    // 初始化 libsodium
    if (sodium_init() == -1) {
        if (unpack_log_level >= 1) {
            throw std::runtime_error("校验失败：libsodium 初始化失败");
        }
        return false;
    }

    // 校验输入参数
    if (!data || size <= 0) {
        if (unpack_log_level >= 1) {
            throw std::runtime_error("校验失败：数据为空或大小无效");
        }
        return false;
    }
    if (!expected_hash) {
        if (unpack_log_level >= 1) {
            throw std::runtime_error("校验失败：期望哈希值为空");
        }
        return false;
    }

    // 计算实际SHA256哈希
    unsigned char actual_hash[crypto_hash_sha256_BYTES];
    crypto_hash_sha256(actual_hash, data, size);

    // 比较哈希值
    int result = sodium_memcmp(actual_hash, expected_hash, crypto_hash_sha256_BYTES);
    bool is_valid = (result == 0);

    if (unpack_log_level >= 2) {
        if (is_valid) {
            printf("数据完整性校验通过");
        }
        else {
            throw std::runtime_error("数据完整性校验失败：哈希值不匹配");

            // 输出哈希值用于调试
            if (unpack_log_level >= 3) {
                throw std::runtime_error("期望哈希值：");
                for (size_t i = 0; i < crypto_hash_sha256_BYTES; ++i) {
                    printf("%02x", expected_hash[i]);
                }
                printf("\n");

                printf("实际哈希值：");
                for (size_t i = 0; i < crypto_hash_sha256_BYTES; ++i) {
                    printf("%02x", actual_hash[i]);
                }
                printf("\n");
            }
        }
    }

    return is_valid;
}

void MoeUnpack::unpack_moe_header(unsigned char* data, MoeHeader& head_data) {
    if (data == nullptr) {
        throw std::runtime_error("解析MoeHeader失败：输入数据指针为空");
    }
    const size_t required_size = sizeof(MoeHeader);
    if (memcmp(data, "MOE", 3) != 0) {
        throw std::runtime_error("解析MoeHeader失败：不是MOE格式数据");
    }
    memcpy(&head_data, data, required_size);
    head_data.from_big_endian();

    if (!head_data.is_magic_valid()) {
        throw std::runtime_error("解析MoeHeader失败：魔数校验最终失败，数据可能损坏");
    }
    if (head_data.header_size != required_size) {
        throw std::runtime_error(
            "解析MoeHeader失败：头部大小不匹配，预期" + std::to_string(required_size) +
            "字节，实际" + std::to_string(head_data.header_size) + "字节"
        );
    }
    if (memcmp(head_data.version, MOE_PackVersion, 8) != 0) {
        throw std::runtime_error("解析MoeHeader失败：版本号不匹配");
    }
}

// 检测 MOE 文件版本: 返回 1 (V1), 2 (V2), -1 (无效)
inline int detect_moe_version(const unsigned char* data, size_t size) {
    if (size < 12) return -1;
    if (memcmp(data, "MOE", 3) != 0) return -1;
    if (memcmp(data + 4, MOE_PackVersion, 8) == 0) return 2;
    return -1;
}

// 将 V2 分块加密数据解密并重组为连续缓冲区
// data 指向 [stream_header(24)][salt(16)][chunks...]
// size: 输入为加密数据总大小, 输出为解密后总大小
inline UniquePtr_uChar _decrypt_v2_chunked(unsigned char* data, int& size,
                                            const MoeHeader& header,
                                            const std::string& key) {
    if (sodium_init() == -1) {
        throw std::runtime_error("分块解密失败：libsodium 初始化失败");
    }

    if (size < static_cast<int>(crypto_secretstream_xchacha20poly1305_HEADERBYTES
                                + crypto_pwhash_SALTBYTES)) {
        throw std::runtime_error("分块解密失败：数据长度不足");
    }

    unsigned char* stream_header = data;
    unsigned char* salt = data + crypto_secretstream_xchacha20poly1305_HEADERBYTES;

    unsigned char derived_key[crypto_secretstream_xchacha20poly1305_KEYBYTES];
    int ret = crypto_pwhash(
        derived_key, sizeof(derived_key),
        key.c_str(), key.length(),
        salt,
        crypto_pwhash_OPSLIMIT_INTERACTIVE,
        crypto_pwhash_MEMLIMIT_INTERACTIVE,
        crypto_pwhash_ALG_DEFAULT
    );
    if (ret != 0) {
        sodium_memzero(derived_key, sizeof(derived_key));
        throw std::runtime_error("分块解密失败：密钥派生失败");
    }

    crypto_secretstream_xchacha20poly1305_state state;
    ret = crypto_secretstream_xchacha20poly1305_init_pull(
        &state, stream_header, derived_key);
    sodium_memzero(derived_key, sizeof(derived_key));
    if (ret != 0) {
        throw std::runtime_error("分块解密失败：解密状态初始化失败");
    }

    unsigned char* output = static_cast<unsigned char*>(malloc(header.original_size));
    if (!output) {
        throw std::runtime_error("分块解密失败：内存分配失败");
    }

    unsigned char* in_ptr = data + crypto_secretstream_xchacha20poly1305_HEADERBYTES
                            + crypto_pwhash_SALTBYTES;
    unsigned char* out_ptr = output;
    size_t remaining_input = static_cast<size_t>(size)
        - crypto_secretstream_xchacha20poly1305_HEADERBYTES
        - crypto_pwhash_SALTBYTES;

    for (uint32_t i = 0; i < header.chunk_count && remaining_input > 0; i++) {
        uint64_t remaining_plain = static_cast<uint64_t>(header.original_size)
                                 - static_cast<uint64_t>(out_ptr - output);
        size_t expected_plain = (remaining_plain < header.chunk_size)
                              ? static_cast<size_t>(remaining_plain)
                              : header.chunk_size;
        size_t expected_cipher = expected_plain + crypto_secretstream_xchacha20poly1305_ABYTES;

        if (remaining_input < expected_cipher) {
            free(output);
            throw std::runtime_error("分块解密失败：数据不完整");
        }

        unsigned long long plain_len = 0;
        unsigned char tag = 0;
        ret = crypto_secretstream_xchacha20poly1305_pull(
            &state, out_ptr, &plain_len, &tag,
            in_ptr, expected_cipher,
            nullptr, 0);

        if (ret != 0) {
            free(output);
            throw std::runtime_error("分块解密失败：块解密错误（密钥错误或数据损坏）");
        }

        in_ptr += expected_cipher;
        out_ptr += plain_len;
        remaining_input -= expected_cipher;

        if (tag == crypto_secretstream_xchacha20poly1305_TAG_FINAL) {
            break;
        }
    }

    sodium_memzero(&state, sizeof(state));
    size = static_cast<int>(header.original_size);
    return UniquePtr_uChar(output);
}

// 内部结构: 解包处理结果
struct _UnpackResult {
    UniquePtr_uChar data;
    int size = 0;
    bool was_encrypted = false;
    bool was_compressed = false;
};

// 内部函数: 打开文件, 检测版本, 解析头部, 读取数据, 校验, 解密, 解压
// 返回处理后的原始数据, 各公开函数只需处理输出逻辑
inline _UnpackResult _unpack_process_file(const std::string& in_put, const std::string& _key) {
    namespace fs = std::filesystem;
    _UnpackResult result;

    if (!fs::exists(in_put)) {
        throw std::runtime_error("解包失败：输入文件不存在 - " + in_put);
    }

    if (MoeUnpack::unpack_log_level >= 1) {
        printf("开始解包文件: %s\n", in_put.c_str());
    }

    std::ifstream moe_file(in_put, std::ios::binary);
    if (!moe_file) {
        throw std::runtime_error("解包失败：无法打开文件 - " + in_put);
    }

    moe_file.seekg(0, std::ios::end);
    size_t file_size = moe_file.tellg();
    moe_file.seekg(0, std::ios::beg);

    // 读取前 12 字节检测版本
    unsigned char first_bytes[12];
    if (!moe_file.read(reinterpret_cast<char*>(first_bytes), 12)) {
        throw std::runtime_error("解包失败：读取文件头部失败");
    }

    int version = detect_moe_version(first_bytes, 12);
    if (version == -1) {
        throw std::runtime_error("解包失败：不是有效的MOE格式文件");
    }

    MoeHeader moe_header;
    std::vector<unsigned char> header_buffer(sizeof(MoeHeader));
    memcpy(header_buffer.data(), first_bytes, 12);
    if (!moe_file.read(reinterpret_cast<char*>(header_buffer.data()) + 12, sizeof(MoeHeader) - 12)) {
        throw std::runtime_error("解包失败：读取文件头部失败");
    }

    try {
        MoeUnpack::unpack_moe_header(header_buffer.data(), moe_header);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("解包失败：头部解析错误 - ") + e.what());
    }

    result.was_encrypted = moe_header.encrypted_on;
    result.was_compressed = moe_header.ztsd_on;

    if (MoeUnpack::unpack_log_level >= 2) {
        printf("头部解析成功：\n");
        printf("  原始大小: %u 字节\n", moe_header.original_size);
        printf("  压缩标志: %s\n", moe_header.ztsd_on ? "启用" : "禁用");
        printf("  加密标志: %s\n", moe_header.encrypted_on ? "启用" : "禁用");
        printf("  分块模式: %s\n", moe_header.chunk_count > 0 ? "启用" : "禁用");
    }

    size_t data_size = file_size - sizeof(MoeHeader);
    if (data_size == 0) {
        throw std::runtime_error("解包失败：数据大小为0");
    }
    unsigned char* raw_data = static_cast<unsigned char*>(malloc(data_size));
    if (!raw_data) {
        throw std::runtime_error("解包失败：分配数据内存失败");
    }
    if (!moe_file.read(reinterpret_cast<char*>(raw_data), data_size)) {
        free(raw_data);
        throw std::runtime_error("解包失败：读取文件数据失败");
    }
    moe_file.close();

    result.data = UniquePtr_uChar(raw_data);
    result.size = static_cast<int>(data_size);

    if (result.was_encrypted) {
        if (_key.empty()) {
            throw std::runtime_error("文件已加密但未提供解密密钥");
        }
        if (!MoeUnpack::_verify_data_integrity(result.data.get(), result.size,
                                    moe_header.check_data)) {
            throw std::runtime_error("数据完整性校验未通过");
        }
        if (moe_header.chunk_count > 0) {
            result.data = _decrypt_v2_chunked(result.data.get(), result.size,
                                               moe_header, _key);
        } else {
            result.data = MoeUnpack::_decrypt(result.data.get(), result.size, _key);
        }
    } else {
        if (!MoeUnpack::_verify_data_integrity(result.data.get(), result.size,
                                    moe_header.check_data)) {
            throw std::runtime_error("数据完整性校验未通过");
        }
    }

    if (result.was_compressed) {
        result.data = MoeUnpack::_ztsd_decompression(result.data.get(), result.size);
    }

    if (MoeUnpack::unpack_log_level >= 1) {
        printf("数据处理完成，最终大小: %d 字节\n", result.size);
    }

    return result;
}

std::string MoeUnpack::unpack(std::string in_put, std::string out_put,std::string _key="") {
    namespace fs = std::filesystem;

    _UnpackResult unpacked = _unpack_process_file(in_put, _key);
    UniquePtr_uChar current_data = std::move(unpacked.data);
    int current_size = unpacked.size;
    bool was_encrypted = unpacked.was_encrypted;
    bool was_compressed = unpacked.was_compressed;

    // 解压后立即检查数据头，此时应为KTX2头
    if (unpack_log_level >= 3) {
        printf("[调试] 解压后，数据前64字节 (应为KTX2头): ");
        for (int i = 0; i < 64 && i < current_size; ++i) {
            printf("%02X ", current_data.get()[i]);
        }
        printf("\n");
    }
#ifdef MOE_UNPACK_CHECK_DATA
    // 验证是否为有效的KTX数据(dds-ktx头部解析报错)
    //if (unpack_log_level >= 1) {
    //    printf("开始验证KTX数据...\n");
    //}
    //ddsktx_texture_info info = {};
    //if (!ddsktx_parse(&info, current_data.get(), current_size)) {
    //    throw std::runtime_error("解包失败：无效的KTX2数据格式");
    //}
    //if (info.width == 0 || info.height == 0) {
    //    throw std::runtime_error("解包失败：KTX纹理尺寸无效");
    //}
    //if (unpack_log_level >= 2) {
    //    printf("KTX数据验证成功：\n");
    //    printf("  尺寸: %dx%d\n", info.width, info.height);
    //    printf("  Mip级别: %d\n", info.num_mips);
    //    printf("  格式标识: %u\n", info.format);
    //    if (unpack_log_level >= 3) {
    //        const char* format_name = "未知";
    //        switch (info.format) {
    //        case DDSKTX_FORMAT_BC7: format_name = "BC7"; break;
    //        case DDSKTX_FORMAT_BC3: format_name = "BC3"; break;
    //        case DDSKTX_FORMAT_ETC2A: format_name = "ETC2_RGBA8"; break;
    //        case DDSKTX_FORMAT_ASTC4x4: format_name = "ASTC_4x4"; break;
    //        }
    //        printf("  格式: %s\n", format_name);
    //    }
    //}
    ////输出
    //fs::path input_path(in_put);
    //std::string stem = input_path.stem().string();
    //if (input_path.extension() == ".moe") {
    //    size_t pos = stem.rfind('.');
    //    if (pos != std::string::npos) {
    //        stem = stem.substr(0, pos);
    //    }
    //}
    //fs::path output_dir(out_put);
    //if (output_dir.empty()) {
    //    output_dir = input_path.parent_path();
    //}
    //if (!output_dir.empty() && !fs::exists(output_dir)) {
    //    if (unpack_log_level >= 2) {
    //        printf("创建输出目录: %s\n", output_dir.string().c_str());
    //    }
    //    if (!fs::create_directories(output_dir)) {
    //        throw std::runtime_error("解包失败：无法创建输出目录 - " + output_dir.string());
    //    }
    //}
    //fs::path output_path = output_dir / (stem + ".ktx2");
    //// 写入KTX文件
    //if (unpack_log_level >= 1) {
    //    printf("开始写入KTX文件: %s\n", output_path.string().c_str());
    //}
    //std::ofstream ktx_file(output_path, std::ios::binary);
    //if (!ktx_file) {
    //    throw std::runtime_error("解包失败：无法创建输出文件 - " + output_path.string());
    //}
    //ktx_file.write(reinterpret_cast<const char*>(current_data.get()), current_size);
    //ktx_file.close();
    //if (!ktx_file) {
    //    throw std::runtime_error("解包失败：写入KTX文件失败");
    //}
    //// 最终日志
    //if (unpack_log_level >= 1) {
    //    printf("解包成功完成！\n");
    //    printf("  输入文件: %s\n", in_put.c_str());
    //    printf("  输出文件: %s\n", output_path.string().c_str());
    //    printf("  纹理尺寸: %dx%d\n", info.width, info.height);
    //    printf("  输出大小: %d 字节\n", current_size);
    //    if (unpack_log_level >= 2) {
    //        printf("  处理步骤: ");
    //        if (moe_header.encrypted_on) printf("解密→");
    //        if (moe_header.ztsd_on) printf("解压→");
    //        printf("KTX验证→文件写入\n");
    //    }
    //}
    //return "SUCCESS";

    if (unpack_log_level >= 1) {
        printf("开始验证KTX数据(使用官方KTX库)...\n");
    }
    // 使用ktxTexture2_CreateFromMemory验证KTX数据
    ktxTexture2* texture = nullptr;
    ktxResult result = ktxTexture2_CreateFromMemory(
        current_data.get(),
        current_size,
        KTX_TEXTURE_CREATE_NO_FLAGS, // 不加载图像数据，只验证头部
        &texture
    );
    if (result != KTX_SUCCESS || texture == nullptr) {
        // 提供详细的错误信息
        const char* error_str = ktxErrorString(result);
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg),
            "解包失败：无效的KTX2数据格式 (错误码: %d, 错误信息: %s)",
            result, error_str ? error_str : "未知错误");
        throw std::runtime_error(error_msg);
    }

    // 从KTX纹理对象中获取信息
    uint32_t width = texture->baseWidth;
    uint32_t height = texture->baseHeight;
    uint32_t mipLevels = texture->numLevels;
    uint32_t format = texture->vkFormat;
    uint32_t supercompression = texture->supercompressionScheme;

    // 验证必要的纹理信息
    if (width == 0 || height == 0) {
        ktxTexture2_Destroy(texture);
        throw std::runtime_error("解包失败：KTX纹理尺寸无效");
    }

    if (unpack_log_level >= 2) {
        printf("KTX数据验证成功：\n");
        printf("  尺寸: %dx%d\n", width, height);
        printf("  Mip级别: %d\n", mipLevels);
        printf("  vkFormat: 0x%X\n", format);
        printf("  超级压缩方案: %u\n", supercompression);

        // 提供格式友好名称
        if (unpack_log_level >= 3) {
            const char* format_name = "未知";
            switch (format) {
            case 145: format_name = "VK_FORMAT_BC7_UNORM_BLOCK (BC7)"; break;
            case 131: format_name = "VK_FORMAT_BC3_UNORM_BLOCK (BC3/DXT5)"; break;
            case 147: format_name = "VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK (ETC2 RGBA8)"; break;
            case 181: format_name = "VK_FORMAT_ASTC_8x8_UNORM_BLOCK (ASTC 8x8)"; break;
            case 37: format_name = "VK_FORMAT_R8G8B8A8_UNORM (RGBA8)"; break;
            }
            printf("  格式: %s\n", format_name);

            // 超级压缩方案说明
            const char* supercomp_name = "未知";
            switch (supercompression) {
            case 0: supercomp_name = "无"; break;
            case 1: supercomp_name = "BasisLZ/ETC1S"; break;
            case 2: supercomp_name = "Zstandard"; break;
            case 3: supercomp_name = "ZLIB"; break;
            case 104: supercomp_name = "Basis Universal (UASTC)"; break;
            }
            printf("  超级压缩: %s\n", supercomp_name);
        }
    }
    // 销毁纹理对象（我们只需要验证，不需要保留）
    ktxTexture2_Destroy(texture);
    texture = nullptr;
#endif // MOE_UNPACK_CHECK_DATA


    fs::path input_path(in_put);
    std::string stem = input_path.stem().string();
    if (input_path.extension() == ".moe") {
        size_t pos = stem.rfind('.');
        if (pos != std::string::npos) {
            stem = stem.substr(0, pos);
        }
    }
    fs::path output_dir(out_put);
    if (output_dir.empty()) {
        output_dir = input_path.parent_path();
    }
    if (!output_dir.empty() && !fs::exists(output_dir)) {
        if (unpack_log_level >= 2) {
            printf("创建输出目录: %s\n", output_dir.string().c_str());
        }
        if (!fs::create_directories(output_dir)) {
            throw std::runtime_error("解包失败：无法创建输出目录 - " + output_dir.string());
        }
    }
    fs::path output_path = output_dir / (stem + ".ktx2");
    // 写入KTX文件
    if (unpack_log_level >= 1) {
        printf("开始写入KTX文件: %s\n", output_path.string().c_str());
    }
    std::ofstream ktx_file(output_path, std::ios::binary);
    if (!ktx_file) {
        throw std::runtime_error("解包失败：无法创建输出文件 - " + output_path.string());
    }
    ktx_file.write(reinterpret_cast<const char*>(current_data.get()), current_size);
    ktx_file.close();
    if (!ktx_file) {
        throw std::runtime_error("解包失败：写入KTX文件失败");
    }
    if (unpack_log_level >= 1) {
        printf("解包成功完成！\n");
        printf("  输入文件: %s\n", in_put.c_str());
        printf("  输出文件: %s\n", output_path.string().c_str());
#ifdef MOE_UNPACK_CHECK_DATA
        printf("  纹理尺寸: %dx%d\n", width, height);
#endif
        printf("  输出大小: %d 字节\n", current_size);
        if (unpack_log_level >= 2) {
            printf("  处理步骤: ");
            if (was_encrypted) printf("解密→");
            if (was_compressed) printf("解压→");
            printf("KTX验证→文件写入\n");
        }
    }
    return "SUCCESS";
}

unsigned char* MoeUnpack::unpack(std::string in_put, int& size, std::string _key="") {
    _UnpackResult result = _unpack_process_file(in_put, _key);
    size = result.size;
    return result.data.release();
}

std::string MoeUnpack::unpack_ex(std::string in_put, std::string out_put, std::string _key) {
    namespace fs = std::filesystem;

    _UnpackResult result = _unpack_process_file(in_put, _key);
    UniquePtr_uChar current_data = std::move(result.data);
    int current_size = result.size;
    bool was_encrypted = result.was_encrypted;
    bool was_compressed = result.was_compressed;

    // 输出文件（去掉.moe后缀，不添加KTX扩展名）
    fs::path input_path(in_put);
    std::string stem = input_path.stem().string();
    if (input_path.extension() == ".moe") {
        size_t pos = stem.rfind('.');
        if (pos != std::string::npos) {
            stem = stem.substr(0, pos);
        }
    }
    fs::path output_dir(out_put);
    if (output_dir.empty()) {
        output_dir = input_path.parent_path();
    }
    if (!output_dir.empty() && !fs::exists(output_dir)) {
        if (unpack_log_level >= 2) {
            printf("创建输出目录: %s\n", output_dir.string().c_str());
        }
        if (!fs::create_directories(output_dir)) {
            throw std::runtime_error("解包失败：无法创建输出目录 - " + output_dir.string());
        }
    }
    fs::path output_path = output_dir / stem; // 不添加扩展名

    if (unpack_log_level >= 1) {
        printf("开始写入输出文件: %s\n", output_path.string().c_str());
    }
    std::ofstream out_file(output_path, std::ios::binary);
    if (!out_file) {
        throw std::runtime_error("解包失败：无法创建输出文件 - " + output_path.string());
    }
    out_file.write(reinterpret_cast<const char*>(current_data.get()), current_size);
    out_file.close();
    if (!out_file) {
        throw std::runtime_error("解包失败：写入文件失败");
    }
    if (unpack_log_level >= 1) {
        printf("解包成功完成！\n");
        printf("  输入文件: %s\n", in_put.c_str());
        printf("  输出文件: %s\n", output_path.string().c_str());
        printf("  输出大小: %d 字节\n", current_size);
        if (unpack_log_level >= 2) {
            printf("  处理步骤: ");
            if (was_encrypted) printf("解密→");
            if (was_compressed) printf("解压→");
            printf("文件写入\n");
        }
    }
    return "SUCCESS";
}

unsigned char* MoeUnpack::unpack_ex(std::string in_put, int& size, std::string _key) {
    _UnpackResult result = _unpack_process_file(in_put, _key);
    size = result.size;
    return result.data.release();
}

#endif // MOE_UNPACK_IMPLEMENTATION