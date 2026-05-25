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
#include "MoeStreamReader.h"
#include "sodium.h"
#include <cstring>
#include <vector>

static const size_t CHUNK_ABYTES = crypto_secretstream_xchacha20poly1305_ABYTES;

int MoeStreamReader::detect_version(const char* file_path) {
    std::ifstream f(file_path, std::ios::binary);
    if (!f) return -1;

    char magic[4] = {};
    char version[8] = {};
    f.read(magic, 4);
    f.read(version, 8);

    if (!f || memcmp(magic, "MOE", 3) != 0) return -1;
    if (memcmp(version, MOE_PackVersion, 8)    == 0) return 1;
    if (memcmp(version, MOE_PackVersion_V2, 8) == 0) return 2;
    return -1;
}

MoeStreamReader::~MoeStreamReader() {
    close();
}

bool MoeStreamReader::open(const char* file_path, const char* password) {
    close();

    file_path_ = file_path;
    password_  = password ? password : "";

    if (sodium_init() == -1) {
        error_ = Error::KeyDerivationFailed;
        return false;
    }

    file_.open(file_path, std::ios::binary);
    if (!file_) {
        error_ = Error::FileOpenFailed;
        return false;
    }

    // 读取 98 字节 MoeHeaderV2
    file_.read(reinterpret_cast<char*>(&header_), sizeof(MoeHeaderV2));
    if (!file_ || file_.gcount() != sizeof(MoeHeaderV2)) {
        error_ = Error::HeaderParseFailed;
        return false;
    }

    if (!header_.is_magic_valid()) {
        error_ = Error::InvalidMagic;
        return false;
    }
    if (!header_.is_v2()) {
        error_ = Error::UnsupportedVersion;
        return false;
    }

    header_.from_big_endian();

    if (header_.header_size != sizeof(MoeHeaderV2)) {
        error_ = Error::HeaderParseFailed;
        return false;
    }

    // 空文件: original_size 和 chunk_count 均为 0
    if (header_.original_size == 0 && header_.chunk_count == 0) {
        current_chunk_ = 0;
        error_ = Error::None;
        return true;
    }

    if (header_.chunk_size == 0) {
        error_ = Error::HeaderParseFailed;
        return false;
    }

    // 读取 24 字节 stream_header
    unsigned char stream_header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
    file_.read(reinterpret_cast<char*>(stream_header), sizeof(stream_header));
    if (!file_ || file_.gcount() != sizeof(stream_header)) {
        error_ = Error::HeaderParseFailed;
        return false;
    }

    // 读取 16 字节 salt
    unsigned char salt[crypto_pwhash_SALTBYTES];
    file_.read(reinterpret_cast<char*>(salt), sizeof(salt));
    if (!file_ || file_.gcount() != sizeof(salt)) {
        error_ = Error::HeaderParseFailed;
        return false;
    }

    if (!_derive_key_and_init(stream_header, salt)) {
        return false;
    }

    sodium_memzero(stream_header, sizeof(stream_header));
    sodium_memzero(salt, sizeof(salt));

    current_chunk_ = 0;
    error_ = Error::None;
    return true;
}

bool MoeStreamReader::_derive_key_and_init(const unsigned char* stream_header,
                                            const unsigned char* salt) {
    // Argon2id 密钥派生, 参数与 V1 一致
    int ret = crypto_pwhash(
        derived_key_, sizeof(derived_key_),
        password_.c_str(), password_.length(),
        salt,
        crypto_pwhash_OPSLIMIT_INTERACTIVE,
        crypto_pwhash_MEMLIMIT_INTERACTIVE,
        crypto_pwhash_ALG_DEFAULT
    );
    if (ret != 0) {
        sodium_memzero(derived_key_, sizeof(derived_key_));
        error_ = Error::KeyDerivationFailed;
        return false;
    }

    // 从 stream_header 和派生密钥初始化解密状态
    ret = crypto_secretstream_xchacha20poly1305_init_pull(
        &state_, stream_header, derived_key_);
    sodium_memzero(derived_key_, sizeof(derived_key_));

    if (ret != 0) {
        error_ = Error::DecryptInitFailed;
        return false;
    }

    state_initialized_ = true;
    return true;
}

size_t MoeStreamReader::read_chunk(void* buffer, size_t buffer_size) {
    if (is_eof()) return 0;
    if (!file_.is_open()) {
        error_ = Error::FileOpenFailed;
        return 0;
    }

    // 计算本块明文大小
    uint64_t remaining = static_cast<uint64_t>(header_.original_size)
                       - static_cast<uint64_t>(current_chunk_) * header_.chunk_size;
    size_t plain_size = (remaining < header_.chunk_size)
                        ? static_cast<size_t>(remaining)
                        : header_.chunk_size;
    size_t cipher_size = plain_size + CHUNK_ABYTES;

    if (buffer_size < plain_size) {
        return 0;
    }

    // 读取一块密文
    std::vector<unsigned char> ciphertext(cipher_size);
    file_.read(reinterpret_cast<char*>(ciphertext.data()), cipher_size);
    if (!file_ || file_.gcount() != static_cast<std::streamsize>(cipher_size)) {
        error_ = Error::FileReadError;
        return 0;
    }

    // 解密
    unsigned long long plain_len = 0;
    unsigned char tag = 0;
    int ret = crypto_secretstream_xchacha20poly1305_pull(
        &state_,
        static_cast<unsigned char*>(buffer), &plain_len, &tag,
        ciphertext.data(), cipher_size,
        nullptr, 0);

    if (ret != 0) {
        error_ = Error::DecryptionFailed;
        return 0;
    }

    current_chunk_++;

    // 最后一块必须是 TAG_FINAL
    if (current_chunk_ >= header_.chunk_count
        && tag != crypto_secretstream_xchacha20poly1305_TAG_FINAL) {
        error_ = Error::DecryptionFailed;
        return 0;
    }

    return static_cast<size_t>(plain_len);
}

bool MoeStreamReader::reset() {
    close();
    return open(file_path_.c_str(), password_.c_str());
}

void MoeStreamReader::close() {
    if (file_.is_open()) {
        file_.close();
    }
    if (state_initialized_) {
        sodium_memzero(&state_, sizeof(state_));
        state_initialized_ = false;
    }
    sodium_memzero(derived_key_, sizeof(derived_key_));
    memset(header_.check_data, 0, sizeof(header_.check_data));
    current_chunk_ = 0;
    error_ = Error::None;
}
