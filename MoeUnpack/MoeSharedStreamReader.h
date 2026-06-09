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

// 轻量流式解密读取器，不派生、不持有密钥
// 用于固定 salt 场景，密钥由上层（AudioDataManager）统一管理
// 内部保存一份密钥拷贝仅用于 seek 时重新初始化 crypto state
class MoeSharedStreamReader {
public:
    enum class Error {
        None = 0,
        FileOpenFailed,
        InvalidMagic,
        UnsupportedVersion,
        HeaderParseFailed,
        DecryptInitFailed,
        DecryptionFailed,
        FileReadError,
        IntegrityCheckFailed
    };

    MoeSharedStreamReader() = default;
    ~MoeSharedStreamReader();

    MoeSharedStreamReader(const MoeSharedStreamReader&) = delete;
    MoeSharedStreamReader& operator=(const MoeSharedStreamReader&) = delete;

    // 从 .moe 文件头读取 salt 并派生密钥（仅 Argon2id，不初始化解密流）
    // 用于固定 salt 场景：派生一次，所有 MoeAudioStreamShare 资源复用
    // 成功返回 true 并将 32 字节密钥写入 derived_key_out
    static bool derive_key(const char* file_path, const char* password,
                           unsigned char derived_key_out[crypto_secretstream_xchacha20poly1305_KEYBYTES]);

    // 使用外部已派生的密钥打开 .moe 文件，跳过 Argon2id 密钥派生
    // derived_key 由上层传入，生命周期需覆盖本实例使用期间
    bool open(const char* file_path,
              const unsigned char derived_key[crypto_secretstream_xchacha20poly1305_KEYBYTES]);

    size_t   read_bytes(void* buffer, size_t size);
    bool     seek_bytes(int64_t offset, int origin);
    uint32_t tell_bytes() const;
    bool     reset();
    void     close();

    const MoeHeader& header()           const { return header_; }
    Error            error()            const { return error_; }
    bool             is_eof()           const { return current_byte_pos_ >= header_.original_size; }
    bool             is_open()          const { return file_.is_open(); }
    uint32_t         current_byte_pos() const { return current_byte_pos_; }

private:
    std::ifstream file_;
    MoeHeader     header_;

    // 外部派生密钥的本地拷贝，仅用于 seek 时 _reinit_crypto_state
    unsigned char derived_key_[crypto_secretstream_xchacha20poly1305_KEYBYTES];

    crypto_secretstream_xchacha20poly1305_state state_;
    bool     state_initialized_ = false;
    Error    error_ = Error::None;

    // 字节级读取缓冲
    std::vector<unsigned char> chunk_buffer_;
    size_t   chunk_buffer_pos_ = 0;
    size_t   chunk_buffer_size_ = 0;
    uint32_t current_byte_pos_ = 0;
    uint32_t current_chunk_ = 0;

    // SHA-256 增量校验
    crypto_hash_sha256_state hash_state_;
    bool hash_initialized_ = false;
    bool hash_verified_ = false;

    // seek 回退用（从文件读取，不派生）
    unsigned char saved_stream_header_[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
    unsigned char saved_salt_[crypto_pwhash_SALTBYTES];

    bool _read_next_chunk();
    bool _reinit_crypto_state();
};
