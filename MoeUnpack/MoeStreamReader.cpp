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
    if (memcmp(version, MOE_PackVersion, 8) == 0) return 2;
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

    file_.read(reinterpret_cast<char*>(&header_), sizeof(MoeHeader));
    if (!file_ || file_.gcount() != sizeof(MoeHeader)) {
        error_ = Error::HeaderParseFailed;
        return false;
    }

    if (!header_.is_magic_valid()) {
        error_ = Error::InvalidMagic;
        return false;
    }
    if (memcmp(header_.version, MOE_PackVersion, 8) != 0) {
        error_ = Error::UnsupportedVersion;
        return false;
    }

    header_.from_big_endian();

    if (header_.header_size != sizeof(MoeHeader)) {
        error_ = Error::HeaderParseFailed;
        return false;
    }

    // 空文件
    if (header_.original_size == 0 && header_.chunk_count == 0) {
        current_chunk_ = 0;
        current_byte_pos_ = 0;
        error_ = Error::None;
        return true;
    }

    if (header_.chunk_size == 0) {
        error_ = Error::HeaderParseFailed;
        return false;
    }

    // 读取 stream_header 和 salt, 缓存用于 seek 回退
    file_.read(reinterpret_cast<char*>(saved_stream_header_), sizeof(saved_stream_header_));
    if (!file_ || file_.gcount() != sizeof(saved_stream_header_)) {
        error_ = Error::HeaderParseFailed;
        return false;
    }

    file_.read(reinterpret_cast<char*>(saved_salt_), sizeof(saved_salt_));
    if (!file_ || file_.gcount() != sizeof(saved_salt_)) {
        error_ = Error::HeaderParseFailed;
        return false;
    }

    // 记录密文数据起始偏移
    data_start_offset_ = file_.tellg();

    if (!_derive_key_and_init(saved_stream_header_, saved_salt_)) {
        return false;
    }

    // 初始化 SHA-256, 将 stream_header 和 salt 计入哈希
    crypto_hash_sha256_init(&hash_state_);
    crypto_hash_sha256_update(&hash_state_, saved_stream_header_, sizeof(saved_stream_header_));
    crypto_hash_sha256_update(&hash_state_, saved_salt_, sizeof(saved_salt_));
    hash_initialized_ = true;
    hash_verified_ = false;

    current_chunk_ = 0;
    current_byte_pos_ = 0;
    chunk_buffer_pos_ = 0;
    chunk_buffer_size_ = 0;
    error_ = Error::None;
    return true;
}

bool MoeStreamReader::_derive_key_and_init(const unsigned char* stream_header,
                                            const unsigned char* salt) {
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

    ret = crypto_secretstream_xchacha20poly1305_init_pull(
        &state_, stream_header, derived_key_);

    if (ret != 0) {
        sodium_memzero(derived_key_, sizeof(derived_key_));
        error_ = Error::DecryptInitFailed;
        return false;
    }

    state_initialized_ = true;
    return true;
}

bool MoeStreamReader::_read_next_chunk() {
    if (current_chunk_ >= header_.chunk_count) return false;

    uint64_t remaining = static_cast<uint64_t>(header_.original_size)
                       - static_cast<uint64_t>(current_chunk_) * header_.chunk_size;
    size_t plain_size = (remaining < header_.chunk_size)
                      ? static_cast<size_t>(remaining)
                      : header_.chunk_size;
    size_t cipher_size = plain_size + CHUNK_ABYTES;

    std::vector<unsigned char> ciphertext(cipher_size);
    file_.read(reinterpret_cast<char*>(ciphertext.data()), cipher_size);
    if (!file_ || file_.gcount() != static_cast<std::streamsize>(cipher_size)) {
        error_ = Error::FileReadError;
        return false;
    }

    if (hash_initialized_ && !hash_verified_) {
        crypto_hash_sha256_update(&hash_state_, ciphertext.data(), cipher_size);
    }

    chunk_buffer_.resize(plain_size);
    unsigned long long plain_len = 0;
    unsigned char tag = 0;
    int ret = crypto_secretstream_xchacha20poly1305_pull(
        &state_,
        chunk_buffer_.data(), &plain_len, &tag,
        ciphertext.data(), cipher_size,
        nullptr, 0);

    if (ret != 0) {
        error_ = Error::DecryptionFailed;
        return false;
    }

    current_chunk_++;
    chunk_buffer_pos_ = 0;
    chunk_buffer_size_ = static_cast<size_t>(plain_len);

    if (current_chunk_ >= header_.chunk_count) {
        if (tag != crypto_secretstream_xchacha20poly1305_TAG_FINAL) {
            error_ = Error::DecryptionFailed;
            return false;
        }
        if (hash_initialized_ && !hash_verified_) {
            unsigned char computed_hash[crypto_hash_sha256_BYTES];
            crypto_hash_sha256_final(&hash_state_, computed_hash);
            hash_verified_ = true;
            if (sodium_memcmp(computed_hash, header_.check_data, crypto_hash_sha256_BYTES) != 0) {
                error_ = Error::IntegrityCheckFailed;
                return false;
            }
        }
    }

    return true;
}

size_t MoeStreamReader::read_chunk(void* buffer, size_t buffer_size) {
    if (is_eof()) return 0;
    if (!file_.is_open()) {
        error_ = Error::FileOpenFailed;
        return 0;
    }

    if (!_read_next_chunk()) return 0;

    if (buffer_size < chunk_buffer_size_) return 0;

    memcpy(buffer, chunk_buffer_.data(), chunk_buffer_size_);
    size_t result = chunk_buffer_size_;
    current_byte_pos_ += static_cast<uint32_t>(chunk_buffer_size_);
    chunk_buffer_pos_ = chunk_buffer_size_;
    return result;
}

size_t MoeStreamReader::read_bytes(void* buffer, size_t size) {
    if (!file_.is_open() || error_ != Error::None) return 0;
    if (is_eof()) return 0;

    unsigned char* out = static_cast<unsigned char*>(buffer);
    size_t total_read = 0;

    while (total_read < size) {
        if (chunk_buffer_pos_ >= chunk_buffer_size_) {
            if (!_read_next_chunk()) break;
        }

        size_t avail = chunk_buffer_size_ - chunk_buffer_pos_;
        size_t to_copy = (size - total_read < avail) ? (size - total_read) : avail;
        memcpy(out + total_read, chunk_buffer_.data() + chunk_buffer_pos_, to_copy);
        chunk_buffer_pos_ += to_copy;
        total_read += to_copy;
        current_byte_pos_ += static_cast<uint32_t>(to_copy);
    }

    return total_read;
}

bool MoeStreamReader::_reinit_crypto_state() {
    if (state_initialized_) {
        sodium_memzero(&state_, sizeof(state_));
        state_initialized_ = false;
    }

    // 复用 open 时缓存的 derived_key_
    int ret = crypto_secretstream_xchacha20poly1305_init_pull(
        &state_, saved_stream_header_, derived_key_);
    if (ret != 0) {
        error_ = Error::DecryptInitFailed;
        return false;
    }
    state_initialized_ = true;

    crypto_hash_sha256_init(&hash_state_);
    crypto_hash_sha256_update(&hash_state_, saved_stream_header_, sizeof(saved_stream_header_));
    crypto_hash_sha256_update(&hash_state_, saved_salt_, sizeof(saved_salt_));
    hash_verified_ = false;

    file_.clear();
    file_.seekg(data_start_offset_);

    current_chunk_ = 0;
    current_byte_pos_ = 0;
    chunk_buffer_pos_ = 0;
    chunk_buffer_size_ = 0;

    return true;
}

bool MoeStreamReader::seek_bytes(int64_t offset, int origin) {
    if (!file_.is_open() || error_ != Error::None) return false;
    if (header_.original_size == 0) return true;

    int64_t target;
    switch (origin) {
        case 0: target = offset; break;
        case 1: target = static_cast<int64_t>(current_byte_pos_) + offset; break;
        case 2: target = static_cast<int64_t>(header_.original_size) + offset; break;
        default: return false;
    }

    if (target < 0) target = 0;
    if (target > static_cast<int64_t>(header_.original_size))
        target = static_cast<int64_t>(header_.original_size);

    uint32_t target_pos = static_cast<uint32_t>(target);

    if (!_reinit_crypto_state()) return false;

    while (current_byte_pos_ < target_pos) {
        if (!_read_next_chunk()) return false;

        uint32_t needed = target_pos - current_byte_pos_;
        if (needed < chunk_buffer_size_) {
            chunk_buffer_pos_ = needed;
            current_byte_pos_ = target_pos;
            break;
        }
        current_byte_pos_ += static_cast<uint32_t>(chunk_buffer_size_);
        chunk_buffer_pos_ = chunk_buffer_size_;
    }

    return true;
}

uint32_t MoeStreamReader::tell_bytes() const {
    return current_byte_pos_;
}

bool MoeStreamReader::reset() {
    return seek_bytes(0, 0);
}

void MoeStreamReader::export_key_material(MoeKeyMaterial& out) const {
    memcpy(out.derived_key, derived_key_, sizeof(derived_key_));
    memcpy(out.stream_header, saved_stream_header_, sizeof(saved_stream_header_));
    memcpy(out.salt, saved_salt_, sizeof(saved_salt_));
    out.header = header_;
    out.data_start_offset = data_start_offset_;
    out.ready = true;
}

bool MoeStreamReader::open_with_cached_key(const char* file_path,
                                            const MoeKeyMaterial& km) {
    close();

    if (!km.ready) {
        error_ = Error::KeyDerivationFailed;
        return false;
    }

    file_path_ = file_path;
    password_  = "";  // 不需要明文密码了

    if (sodium_init() == -1) {
        error_ = Error::KeyDerivationFailed;
        return false;
    }

    file_.open(file_path, std::ios::binary);
    if (!file_) {
        error_ = Error::FileOpenFailed;
        return false;
    }

    file_.read(reinterpret_cast<char*>(&header_), sizeof(MoeHeader));
    if (!file_ || file_.gcount() != sizeof(MoeHeader)) {
        error_ = Error::HeaderParseFailed;
        return false;
    }

    if (!header_.is_magic_valid()) {
        error_ = Error::InvalidMagic;
        return false;
    }
    if (memcmp(header_.version, MOE_PackVersion, 8) != 0) {
        error_ = Error::UnsupportedVersion;
        return false;
    }

    header_.from_big_endian();

    if (header_.header_size != sizeof(MoeHeader)) {
        error_ = Error::HeaderParseFailed;
        return false;
    }

    if (header_.original_size == 0 && header_.chunk_count == 0) {
        current_chunk_ = 0;
        current_byte_pos_ = 0;
        error_ = Error::None;
        return true;
    }

    if (header_.chunk_size == 0) {
        error_ = Error::HeaderParseFailed;
        return false;
    }

    // 复制缓存的密钥材料
    memcpy(derived_key_, km.derived_key, sizeof(derived_key_));
    memcpy(saved_stream_header_, km.stream_header, sizeof(saved_stream_header_));
    memcpy(saved_salt_, km.salt, sizeof(saved_salt_));
    data_start_offset_ = km.data_start_offset;

    // 跳到密文数据起始位置
    file_.seekg(data_start_offset_);

    // 直接用缓存密钥初始化 stream cipher state，跳过 crypto_pwhash
    int ret = crypto_secretstream_xchacha20poly1305_init_pull(
        &state_, saved_stream_header_, derived_key_);
    if (ret != 0) {
        sodium_memzero(derived_key_, sizeof(derived_key_));
        error_ = Error::DecryptInitFailed;
        return false;
    }
    state_initialized_ = true;

    // 初始化 SHA-256 增量校验
    crypto_hash_sha256_init(&hash_state_);
    crypto_hash_sha256_update(&hash_state_, saved_stream_header_, sizeof(saved_stream_header_));
    crypto_hash_sha256_update(&hash_state_, saved_salt_, sizeof(saved_salt_));
    hash_initialized_ = true;
    hash_verified_ = false;

    current_chunk_ = 0;
    current_byte_pos_ = 0;
    chunk_buffer_pos_ = 0;
    chunk_buffer_size_ = 0;
    error_ = Error::None;
    return true;
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
    sodium_memzero(saved_stream_header_, sizeof(saved_stream_header_));
    sodium_memzero(saved_salt_, sizeof(saved_salt_));
    memset(&hash_state_, 0, sizeof(hash_state_));
    memset(header_.check_data, 0, sizeof(header_.check_data));
    hash_initialized_ = false;
    hash_verified_ = false;
    current_chunk_ = 0;
    current_byte_pos_ = 0;
    chunk_buffer_.clear();
    chunk_buffer_pos_ = 0;
    chunk_buffer_size_ = 0;
    data_start_offset_ = 0;
    error_ = Error::None;
}
