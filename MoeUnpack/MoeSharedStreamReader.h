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
#include <vector>

/**
 * @brief 轻量流式解密读取器
 *
 * 与 MoeStreamReader 的区别：
 * - 不持有密码，不执行 Argon2id 密钥派生
 * - 密钥由上层（如 AudioDataManager）统一管理
 * - 内部保存一份密钥拷贝仅用于 seek 时重新初始化解密状态
 * - 适用于固定 salt 场景：派生一次密钥，所有资源实例复用
 */
class MoeSharedStreamReader {
public:
    enum class Error {
        None = 0,               ///< 无错误
        FileOpenFailed,         ///< 文件打开失败
        InvalidMagic,           ///< 魔数无效
        UnsupportedVersion,     ///< 不支持的版本
        HeaderParseFailed,      ///< 头部解析失败
        DecryptInitFailed,      ///< 解密状态初始化失败
        DecryptionFailed,       ///< 解密失败（密钥错误或数据损坏）
        FileReadError,          ///< 文件读取错误
        IntegrityCheckFailed    ///< SHA-256 完整性校验失败
    };

    MoeSharedStreamReader() = default;
    ~MoeSharedStreamReader();

    MoeSharedStreamReader(const MoeSharedStreamReader&) = delete;
    MoeSharedStreamReader& operator=(const MoeSharedStreamReader&) = delete;

    /**
     * @brief 从 .moe 文件头读取 salt 并派生密钥 (Argon2id)
     * @param file_path .moe 文件路径
     * @param password 解密密码
     * @param derived_key_out [输出] 32 字节派生密钥
     * @return bool 成功返回 true
     * @note 用于固定 salt 场景：派生一次，所有实例复用
     */
    static bool derive_key(const char* file_path, const char* password,
                           unsigned char derived_key_out[crypto_secretstream_xchacha20poly1305_KEYBYTES]);

    /**
     * @brief 使用外部已派生的密钥打开 .moe 文件
     * @param file_path .moe 文件路径
     * @param derived_key 32 字节已派生密钥（调用者管理生命周期）
     * @return bool 成功返回 true
     * @note 跳过 Argon2id 密钥派生，直接初始化解密状态
     */
    bool open(const char* file_path,
              const unsigned char derived_key[crypto_secretstream_xchacha20poly1305_KEYBYTES]);

    /**
     * @brief 字节级读取，可跨块边界
     * @param buffer 输出缓冲区
     * @param size 要读取的字节数
     * @return size_t 实际读取的字节数
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

    /// 关闭文件并清零所有敏感数据
    void close();

    // -- 信息查询 --
    const MoeHeader& header()           const { return header_; }
    Error            error()            const { return error_; }
    bool             is_eof()           const { return current_byte_pos_ >= header_.original_size; }
    bool             is_open()          const { return file_.is_open(); }
    uint32_t         current_byte_pos() const { return current_byte_pos_; }

private:
    std::ifstream file_;
    MoeHeader     header_;

    unsigned char derived_key_[crypto_secretstream_xchacha20poly1305_KEYBYTES]; ///< 外部派生密钥的本地拷贝（用于 seek 重初始化）

    crypto_secretstream_xchacha20poly1305_state state_;
    bool     state_initialized_ = false;
    Error    error_ = Error::None;

    std::vector<unsigned char> chunk_buffer_;   ///< 当前块解密缓冲区
    size_t   chunk_buffer_pos_ = 0;             ///< 当前块读取位置
    size_t   chunk_buffer_size_ = 0;            ///< 当前块有效数据大小
    uint32_t current_byte_pos_ = 0;             ///< 全局字节位置
    uint32_t current_chunk_ = 0;                ///< 当前块索引

    crypto_hash_sha256_state hash_state_;       ///< SHA-256 增量哈希状态
    bool hash_initialized_ = false;
    bool hash_verified_ = false;

    unsigned char saved_stream_header_[crypto_secretstream_xchacha20poly1305_HEADERBYTES]; ///< seek 回退用
    unsigned char saved_salt_[crypto_pwhash_SALTBYTES];                                    ///< seek 回退用

    bool _read_next_chunk();
    bool _reinit_crypto_state();
};
