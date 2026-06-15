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
#include <cassert>
#include <cstdint>
#include <cstring>
#include <stdexcept>

/**
 * @brief 自定义删除器：使用 free() 释放 malloc 分配的内存
 * @tparam T 指针类型
 */
template<typename T>
struct FreeDeleter {
    void operator()(T* ptr) const {
        if (ptr) free(ptr);
    }
};

/// std::unique_ptr 封装 malloc 分配的内存，自动调用 free() 释放
using UniquePtr_uChar = std::unique_ptr<unsigned char, FreeDeleter<unsigned char>>;

/// MOE 文件格式版本号常量
static const char MOE_PackVersion[8] = { 'v','0','.','2','.','0','0','\0' };

/**
 * @brief 字节序转换工具（大端 ↔ 小端）
 * @note MOE 文件头中的多字节整数以大端序存储，跨平台兼容
 */
namespace MOE_Endian {
    /// 检测当前主机是否为小端序
    inline bool is_host_little_endian() {
        union {
            uint32_t i;
            uint8_t  c[4];
        } test = { 0x01020304 };
        return test.c[0] == 0x04;
    }

    /// 主机序 → 大端序 (32位)
    inline uint32_t htobe32(uint32_t host32) {
        if (is_host_little_endian()) {
            return ((host32 & 0x000000FF) << 24) |
                ((host32 & 0x0000FF00) << 8) |
                ((host32 & 0x00FF0000) >> 8) |
                ((host32 & 0xFF000000) >> 24);
        }
        return host32;
    }

    /// 主机序 → 大端序 (64位)
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

    /// 大端序 → 主机序 (32位)
    inline uint32_t betoh32(uint32_t be32) {
        return htobe32(be32);
    }

    /// 大端序 → 主机序 (64位)
    inline uint64_t betoh64(uint64_t be64) {
        return htobe64(be64);
    }
}

#pragma pack(push, 1)

/**
 * @brief MOE 文件格式头部结构体（98字节，大端序存储多字节字段）
 * @note 位于每个 .moe 文件开头，描述文件元数据和数据布局
 */
struct MoeHeader {
    char     magic[4];          ///< 魔数标识 "MOE\0"
    char     version[8];        ///< 版本号字符串 "v0.2.00\0"
    uint32_t header_size;       ///< 头部总大小 (字节，固定 98)
    uint8_t  ztsd_on;           ///< 是否启用 ZSTD 压缩 (1=压缩, 0=未压缩)
    uint8_t  encrypted_on;      ///< 是否启用加密 (1=加密, 0=未加密)
    uint8_t  check_data[32];    ///< 数据完整性校验 SHA-256 哈希值
    uint32_t chunk_size;        ///< 分块加密每块大小 (字节，0=未分块)
    uint32_t chunk_count;       ///< 分块加密总块数 (0=未分块)
    uint32_t audio_format;      ///< 音频格式标识，参见 MOE_Pack_AudioFormat
    uint32_t original_size;     ///< 原始数据（压缩前/加密前）大小 (字节)
    uint8_t  reserved[32];      ///< 保留字段（未来扩展用，当前填 0）

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

    /// 设置完整性校验哈希值
    void set_check_data(const uint8_t* data, size_t size) {
        if (size > sizeof(check_data)) size = sizeof(check_data);
        memcpy(check_data, data, size);
    }

    /// 将多字节字段转换为主机序 → 大端序（写入文件前调用）
    void to_big_endian() {
        header_size  = MOE_Endian::htobe32(header_size);
        chunk_size   = MOE_Endian::htobe32(chunk_size);
        chunk_count  = MOE_Endian::htobe32(chunk_count);
        audio_format = MOE_Endian::htobe32(audio_format);
        original_size = MOE_Endian::htobe32(original_size);
    }

    /// 将多字节字段转换为大端序 → 主机序（读取文件后调用）
    void from_big_endian() {
        header_size  = MOE_Endian::betoh32(header_size);
        chunk_size   = MOE_Endian::betoh32(chunk_size);
        chunk_count  = MOE_Endian::betoh32(chunk_count);
        audio_format = MOE_Endian::betoh32(audio_format);
        original_size = MOE_Endian::betoh32(original_size);
    }

    /// 校验魔数是否有效
    bool is_magic_valid() const {
        return memcmp(magic, "MOE", 3) == 0;
    }
};

static_assert(sizeof(MoeHeader) == 98, "MoeHeader 大小错误，请检查 #pragma pack 和字段定义");
#pragma pack(pop)

/**
 * @brief MOE 音频格式枚举
 * @note 由打包工具自动检测，写入 MoeHeader.audio_format
 */
enum class MOE_Pack_AudioFormat : uint32_t {
    UNKNOWN = 0,    ///< 未知/未检测
    WAV     = 1,    ///< RIFF WAVE 格式
    FLAC    = 2,    ///< FLAC 无损压缩格式
    MP3     = 3,    ///< MPEG Audio Layer 3
    VORBIS  = 4     ///< Ogg Vorbis 格式
};

/**
 * @brief 通过文件头魔数检测音频格式
 * @param data 文件头部数据指针（至少 12 字节）
 * @param size 数据大小
 * @return MOE_Pack_AudioFormat 检测到的音频格式，无法识别返回 UNKNOWN
 */
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
