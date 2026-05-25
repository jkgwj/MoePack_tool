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
#include <memory>
#include<cassert>
#include<cstdint>
#include<cstring>
#include <stdexcept>

template<typename T>
struct FreeDeleter {
    void operator()(T* ptr) const {
        if (ptr) free(ptr);
    }
};
using UniquePtr_uChar = std::unique_ptr<unsigned char, FreeDeleter<unsigned char>>;

static const char MOE_PackVersion[8] = { 'v','0','.','2','.','0','0','\0' };

namespace MOE_Endian {
    inline bool is_host_little_endian() {
        union {
            uint32_t i;
            uint8_t  c[4];
        } test = { 0x01020304 };
        return test.c[0] == 0x04;
    }

    inline uint32_t htobe32(uint32_t host32) {
        if (is_host_little_endian()) {
            return ((host32 & 0x000000FF) << 24) |
                ((host32 & 0x0000FF00) << 8) |
                ((host32 & 0x00FF0000) >> 8) |
                ((host32 & 0xFF000000) >> 24);
        }
        return host32;
    }

    inline uint64_t htobe64(uint64_t host64) {
        if (is_host_little_endian()) {
            return ((host64 & 0x00000000000000FFULL) << 56) |
                ((host64 & 0x000000000000FF00ULL) << 40) |
                ((host64 & 0x0000000000FF0000ULL) << 24) |
                ((host64 & 0x00000000FF000000ULL) << 8) |
                ((host64 & 0x000000FF00000000ULL) >> 8) |
                ((host64 & 0x0000FF0000000000ULL) >> 24) |
                ((host64 & 0x00FF000000000000ULL) >> 40) |
                ((host64 & 0xFF00000000000000ULL) >> 56);
        }
        return host64;
    }

    inline uint32_t betoh32(uint32_t be32) {
        return htobe32(be32);
    }

    inline uint64_t betoh64(uint64_t be64) {
        return htobe64(be64);
    }
}

#pragma pack(push, 1)

struct MoeHeader
{
    char     magic[4];
    char     version[8];
    uint32_t header_size;
    uint8_t  ztsd_on;
    uint8_t  encrypted_on;
    uint8_t  check_data[32];
    uint32_t chunk_size;
    uint32_t chunk_count;
    uint32_t audio_format;
    uint32_t original_size;
    uint8_t  reserved[32];

    MoeHeader() {
        memcpy(magic, "MOE\0", 4);
        memcpy(version, MOE_PackVersion, 8);
        header_size = sizeof(MoeHeader);
        ztsd_on = 1;
        encrypted_on = 0;
        memset(check_data, 0, sizeof(check_data));
        chunk_size = 0;
        chunk_count = 0;
        audio_format = 0;
        original_size = 0;
        memset(reserved, 0, sizeof(reserved));
    }

    MoeHeader(const MoeHeader& other) {
        memcpy(this, &other, sizeof(MoeHeader));
    }

    void set_check_data(const uint8_t* data, size_t size) {
        if (size > sizeof(check_data)) size = sizeof(check_data);
        memcpy(check_data, data, size);
    }

    void to_big_endian() {
        header_size  = MOE_Endian::htobe32(header_size);
        chunk_size   = MOE_Endian::htobe32(chunk_size);
        chunk_count  = MOE_Endian::htobe32(chunk_count);
        audio_format = MOE_Endian::htobe32(audio_format);
        original_size = MOE_Endian::htobe32(original_size);
    }

    void from_big_endian() {
        header_size  = MOE_Endian::betoh32(header_size);
        chunk_size   = MOE_Endian::betoh32(chunk_size);
        chunk_count  = MOE_Endian::betoh32(chunk_count);
        audio_format = MOE_Endian::betoh32(audio_format);
        original_size = MOE_Endian::betoh32(original_size);
    }

    bool is_magic_valid() const {
        return memcmp(magic, "MOE", 3) == 0;
    }
};

static_assert(sizeof(MoeHeader) == 98, u8"MoeHeader 大小错误！请检查#pragma pack和字段定义");
#pragma pack(pop)

enum class MOE_Pack_AudioFormat : uint32_t {
    UNKNOWN = 0,
    WAV     = 1,
    FLAC    = 2,
    MP3     = 3,
    VORBIS  = 4
};

inline MOE_Pack_AudioFormat _detect_audio_format(const unsigned char* data, size_t size) {
    if (size >= 12 && memcmp(data, "RIFF", 4) == 0
        && memcmp(data + 8, "WAVE", 4) == 0)
        return MOE_Pack_AudioFormat::WAV;
    if (size >= 4 && memcmp(data, "fLaC", 4) == 0)
        return MOE_Pack_AudioFormat::FLAC;
    if (size >= 4 && memcmp(data, "OggS", 4) == 0)
        return MOE_Pack_AudioFormat::VORBIS;
    if (size >= 2 && data[0] == 0xFF && (data[1] & 0xE0) == 0xE0)
        return MOE_Pack_AudioFormat::MP3;
    return MOE_Pack_AudioFormat::UNKNOWN;
}
