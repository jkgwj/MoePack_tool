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

// 流式解包读取器
// 用于逐块解密 v0.2.00 格式的 .moe 文件
// 每次 read_chunk() 从文件读取一块密文, 解密后返回明文
//
// 典型用法:
//   MoeStreamReader reader;
//   reader.open("audio.moe", "password");
//   while (!reader.is_eof()) {
//       size_t len = reader.read_chunk(buf, reader.header().chunk_size);
//       // 处理 buf[0..len-1] 中的解密数据
//   }
//   reader.close();
//
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
        FileReadError
    };

    MoeStreamReader() = default;
    ~MoeStreamReader();

    // 不可拷贝 
    MoeStreamReader(const MoeStreamReader&) = delete;
    MoeStreamReader& operator=(const MoeStreamReader&) = delete;

    // 打开 .moe 文件, 解析 MoeHeaderV2, 派生密钥, 初始化解密状态
    // 返回 true 表示成功, false 表示失败 (可通过 error() 获取错误原因)
    bool open(const char* file_path, const char* password);

    // 读取并解密下一块数据, 返回实际解密出的明文字节数, 0 表示已读完或发生错误
    // buffer_size 应不小于 header().chunk_size
    size_t read_chunk(void* buffer, size_t buffer_size);

    // 重置读取位置到文件开头 (关闭后重新打开, 重新派生密钥)
    bool reset();

    // 关闭文件并清零敏感数据
    void close();

    // 静态工具: 检测 .moe 文件版本, 返回 1 (V1), 2 (V2), -1 (无效)
    static int detect_version(const char* file_path);

    // 信息查询
    const MoeHeaderV2& header() const { return header_; }
    Error           error()        const { return error_; }
    bool            is_eof()       const { return current_chunk_ >= header_.chunk_count; }
    bool            is_open()      const { return file_.is_open(); }
    uint32_t        current_chunk() const { return current_chunk_; }

private:
    std::ifstream file_;
    MoeHeaderV2   header_;
    unsigned char derived_key_[32];  // crypto_secretstream_xchacha20poly1305_KEYBYTES
    crypto_secretstream_xchacha20poly1305_state state_;
    Error    error_ = Error::None;
    uint32_t current_chunk_ = 0;
    bool     state_initialized_ = false;
    std::string file_path_;
    std::string password_;

    bool _derive_key_and_init(const unsigned char* stream_header,
                               const unsigned char* salt);
};
