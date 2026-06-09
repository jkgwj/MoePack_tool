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
#include "MoeHeader.h"
#include "sodium.h"
#include <string>
#include <fstream>
#include <cstdint>
#include <vector>

/**
 * @brief 可跨 MoeStreamReader 实例共享的预派生密钥材料
 * @note Manager 持有的原型在创建时派生一次并缓存，后续 clone 复用，避免重复 Argon2id
 */
struct MoeKeyMaterial {
    unsigned char derived_key[crypto_secretstream_xchacha20poly1305_KEYBYTES];       ///< Argon2id 派生的 32 字节密钥
    unsigned char stream_header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];  ///< 加密流头部 (24 字节)
    unsigned char salt[crypto_pwhash_SALTBYTES];                                     ///< Argon2id 盐值 (16 字节)
    MoeHeader header;                                                                ///< 文件 MOE 头部
    std::streamoff data_start_offset = 0;                                            ///< 加密数据在文件中的起始偏移
    bool ready = false;                                                              ///< 密钥材料是否已就绪
};

/**
 * @brief MOE 流式解密读取器
 *
 * 用于分块加密的 .moe 文件（pack_ex_stream 生成），支持：
 * - 按块读取并解密 (read_chunk)
 * - 字节级跨块读取 (read_bytes)
 * - 流内随机定位 (seek_bytes)
 * - 密钥材料导出复用 (export_key_material / open_with_cached_key)
 * - 增量 SHA-256 完整性校验
 */
class MoeStreamReader {
public:
    enum class Error {
        None = 0,               ///< 无错误
        FileOpenFailed,         ///< 文件打开失败
        InvalidMagic,           ///< 魔数无效
        UnsupportedVersion,     ///< 不支持的版本
        HeaderParseFailed,      ///< 头部解析失败
        KeyDerivationFailed,    ///< 密钥派生失败 (Argon2id)
        DecryptInitFailed,      ///< 解密状态初始化失败
        DecryptionFailed,       ///< 解密失败（密钥错误或数据损坏）
        FileReadError,          ///< 文件读取错误
        IntegrityCheckFailed    ///< SHA-256 完整性校验失败
    };

    MoeStreamReader() = default;
    ~MoeStreamReader();

    MoeStreamReader(const MoeStreamReader&) = delete;
    MoeStreamReader& operator=(const MoeStreamReader&) = delete;

    /**
     * @brief 打开 .moe 文件并初始化解密状态
     * @param file_path .moe 文件路径
     * @param password 解密密码
     * @return bool 成功返回 true
     */
    bool open(const char* file_path, const char* password);

    /**
     * @brief 读取并解密下一块数据
     * @param buffer 输出缓冲区
     * @param buffer_size 缓冲区大小 (字节)
     * @return size_t 实际解密出的明文字节数，0 表示已读完或发生错误
     */
    size_t read_chunk(void* buffer, size_t buffer_size);

    /**
     * @brief 字节级读取，可跨块边界
     * @param buffer 输出缓冲区
     * @param size 要读取的字节数
     * @return size_t 实际读取的字节数，0 表示 EOF 或错误
     */
    size_t read_bytes(void* buffer, size_t size);

    /**
     * @brief 定位到解密流中的指定字节位置
     * @param offset 偏移量
     * @param origin 基准位置 (0=SEEK_SET, 1=SEEK_CUR, 2=SEEK_END)
     * @return bool 成功返回 true
     */
    bool seek_bytes(int64_t offset, int origin);

    /// 当前在解密流中的字节位置
    uint32_t tell_bytes() const;

    /// 重置读取位置到开头
    bool reset();

    /**
     * @brief 导出已派生的密钥材料，供其他实例复用
     * @param out [输出] 密钥材料结构体
     * @note 在 close 前调用，避免敏感数据被清零
     */
    void export_key_material(MoeKeyMaterial& out) const;

    /**
     * @brief 使用预派生密钥材料打开文件，跳过 Argon2id
     * @param file_path .moe 文件路径
     * @param km 预派生的密钥材料
     * @return bool 成功返回 true
     */
    bool open_with_cached_key(const char* file_path, const MoeKeyMaterial& km);

    /// 关闭文件并清零所有敏感数据
    void close();

    /**
     * @brief 静态工具：检测 .moe 文件是否为有效格式
     * @param file_path 文件路径
     * @return int 有效返回 1，无效返回 -1
     */
    static int is_valid_moe(const char* file_path);

    // -- 信息查询 --
    const MoeHeader& header()           const { return header_; }
    Error            error()            const { return error_; }
    bool             is_eof()           const { return current_byte_pos_ >= header_.original_size; }
    bool             is_open()          const { return file_.is_open(); }
    uint32_t         current_chunk()    const { return current_chunk_; }
    uint32_t         current_byte_pos() const { return current_byte_pos_; }

private:
    std::ifstream file_;
    MoeHeader     header_;
    unsigned char derived_key_[crypto_secretstream_xchacha20poly1305_KEYBYTES];
    crypto_secretstream_xchacha20poly1305_state state_;
    Error    error_ = Error::None;
    uint32_t current_chunk_ = 0;
    bool     state_initialized_ = false;
    std::string file_path_;
    std::string password_;

    std::vector<unsigned char> chunk_buffer_;   ///< 当前块解密缓冲区
    size_t   chunk_buffer_pos_ = 0;             ///< 当前块读取位置
    size_t   chunk_buffer_size_ = 0;            ///< 当前块有效数据大小
    uint32_t current_byte_pos_ = 0;             ///< 全局字节位置

    crypto_hash_sha256_state hash_state_;       ///< SHA-256 增量哈希状态
    bool     hash_initialized_ = false;
    bool     hash_verified_ = false;

    unsigned char saved_stream_header_[crypto_secretstream_xchacha20poly1305_HEADERBYTES]; ///< seek 回退用
    unsigned char saved_salt_[crypto_pwhash_SALTBYTES];                                    ///< seek 回退用
    std::streamoff data_start_offset_ = 0;                                                 ///< 加密数据起始偏移

    bool _derive_key_and_init(const unsigned char* stream_header,
                               const unsigned char* salt);
    bool _reinit_crypto_state();
    bool _read_next_chunk();
};
