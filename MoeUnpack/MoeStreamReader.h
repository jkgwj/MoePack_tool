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

class MoeStreamReader {
public:
    enum class Error {
        None = 0,
        FileOpenFailed,
        InvalidMagic,
        UnsupportedVersion,
        HeaderParseFailed,
        KeyDerivationFailed,
        DecryptInitFailed,
        DecryptionFailed,
        FileReadError,
        IntegrityCheckFailed
    };

    MoeStreamReader() = default;
    ~MoeStreamReader();

    MoeStreamReader(const MoeStreamReader&) = delete;
    MoeStreamReader& operator=(const MoeStreamReader&) = delete;

    // 打开 .moe 文件, 解析 MoeHeader, 派生密钥, 初始化解密状态, 校验完整性
    bool open(const char* file_path, const char* password);

    // 读取并解密下一块数据, 返回实际解密出的明文字节数, 0 表示已读完或发生错误
    size_t read_chunk(void* buffer, size_t buffer_size);

    // 字节级读取, 可跨块边界, 返回实际读取的字节数, 0 表示 EOF 或错误
    size_t read_bytes(void* buffer, size_t size);

    // 定位到解密流中的指定字节位置, origin: 0=SEEK_SET, 1=SEEK_CUR, 2=SEEK_END
    bool seek_bytes(int64_t offset, int origin);

    // 当前在解密流中的字节位置
    uint32_t tell_bytes() const;

    // 重置读取位置到开头
    bool reset();

    // 关闭文件并清零敏感数据
    void close();

    // 静态工具: 检测 .moe 文件是否为有效格式
    static int detect_version(const char* file_path);

    // 信息查询
    const MoeHeader& header()    const { return header_; }
    Error            error()     const { return error_; }
    bool             is_eof()    const { return current_byte_pos_ >= header_.original_size; }
    bool             is_open()   const { return file_.is_open(); }
    uint32_t         current_chunk() const { return current_chunk_; }
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

    // 字节级读取状态
    std::vector<unsigned char> chunk_buffer_;
    size_t   chunk_buffer_pos_ = 0;
    size_t   chunk_buffer_size_ = 0;
    uint32_t current_byte_pos_ = 0;

    // SHA-256 增量校验
    crypto_hash_sha256_state hash_state_;
    bool     hash_initialized_ = false;
    bool     hash_verified_ = false;

    // 用于 seek 回退时重新初始化
    unsigned char saved_stream_header_[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
    unsigned char saved_salt_[crypto_pwhash_SALTBYTES];
    std::streamoff data_start_offset_ = 0;

    bool _derive_key_and_init(const unsigned char* stream_header,
                               const unsigned char* salt);
    bool _reinit_crypto_state();
    bool _read_next_chunk();
};
