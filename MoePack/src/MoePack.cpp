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
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#include "MoePack.h"
#include "sodium.h"
#include <cstring>
#include <cstdio>
#include <iomanip>

static const unsigned char DEFAULT_FIXED_SALT[crypto_pwhash_SALTBYTES] = {
    0x4D, 0x6F, 0x65, 0x41, 0x75, 0x64, 0x69, 0x6F,
    0x53, 0x61, 0x6C, 0x74, 0x56, 0x31, 0x2E, 0x30
};

static bool normalize_salt(const std::string& input, unsigned char out[crypto_pwhash_SALTBYTES]) {
    if (input.empty()) {
        memcpy(out, DEFAULT_FIXED_SALT, crypto_pwhash_SALTBYTES);
        return true;
    }
    if (input.size() > crypto_pwhash_SALTBYTES) {
        std::cerr << "错误: salt 长度超过 " << crypto_pwhash_SALTBYTES
                  << " 字节 (" << input.size() << " > " << crypto_pwhash_SALTBYTES << ")"
                  << std::endl;
        return false;
    }
    memset(out, 0, crypto_pwhash_SALTBYTES);
    memcpy(out, input.data(), input.size());
    return true;
}

MoePack::MoePack() {}
MoePack::~MoePack() {}

void MoePack::ztsd_compression(bool enable, int level) {
    pack_params.ztsd_on = enable;
    pack_params.ztsd_level = level;
}

void MoePack::encryption(bool enable, const std::string key) {
    pack_params.is_encryption = enable;
    pack_params.encryption_key = key;
}

void MoePack::use_fixed_salt(bool enable) {
    pack_params.use_fixed_salt = enable;
    pack_params.fixed_salt_value.clear();
}

void MoePack::use_fixed_salt(bool enable, const std::string& salt) {
    pack_params.use_fixed_salt = enable;
    pack_params.fixed_salt_value = salt;
}

UniquePtr_uChar MoePack::_load_image_to_src_format(const std::string& image_path, int& h, int& w) {
    if (image_path.empty()) {
        throw std::runtime_error("解码图片失败：路径为空");
    }

    h = 0;
    w = 0;

    int stb_format = _moe_format_to_stb_format(pack_params.src_image_format);
    if (stb_format == -1) {
        throw std::runtime_error("解码图片失败：不支持的图片格式 " + std::to_string((int)pack_params.src_image_format));
    }

    int original_channels = 0;
    unsigned char* raw_image_data = stbi_load(
        image_path.c_str(), &w, &h, &original_channels, stb_format);

    if (!raw_image_data) {
        std::string error_msg = "图片解码失败：" + image_path + " | 原因：" + stbi_failure_reason();
        throw std::runtime_error(error_msg);
    }

    if (w <= 0 || h <= 0) {
        free(raw_image_data);
        throw std::runtime_error("解码图片失败：" + image_path + " | 无效尺寸：" + std::to_string(w) + "x" + std::to_string(h));
    }

    UniquePtr_uChar image_data(raw_image_data);

    if (log_level >= 3) printf("图片解码成功：%s | 尺寸：%dx%d | 格式：%d\n",
        image_path.c_str(), w, h, (int)pack_params.src_image_format);
    return image_data;
}

UniquePtr_uChar MoePack::_src_format_to_ktx(const unsigned char* src_data, int& h, int& w, int& size) {
    if (!src_data) {
        throw std::runtime_error("转换 KTX 失败：源数据裸指针为空");
    }
    if (w <= 0 || h <= 0) {
        throw std::runtime_error("转换 KTX 失败：无效尺寸 " + std::to_string(w) + "x" + std::to_string(h));
    }

    ktxTexture2* ktx2_texture = nullptr;
    ktxTextureCreateInfo create_info = {};

    VkFormat target_format = VK_FORMAT_UNDEFINED;
    if (pack_params.texture_format != VK_FORMAT_UNDEFINED) {
        target_format = pack_params.texture_format;
    } else {
        target_format = GetGpuCompressionFormat(dst_platform);
    }

    if (target_format == VK_FORMAT_UNDEFINED) {
        throw std::runtime_error("转换 KTX 失败：目标 VK 格式未定义，请检查平台设置");
    }

    if (log_level >= 3) {
        std::cout << "目标 VK 格式: 0x" << std::hex << target_format << std::dec << std::endl;
    }

    create_info.vkFormat = VK_FORMAT_R8G8B8A8_UNORM;
    create_info.baseWidth = static_cast<uint32_t>(w);
    create_info.baseHeight = static_cast<uint32_t>(h);
    create_info.baseDepth = 1;
    create_info.numDimensions = pack_params.is_2D ? 2 : 3;
    create_info.numLevels = 1;
    create_info.numLayers = 1;
    create_info.numFaces = 1;
    create_info.isArray = KTX_FALSE;
    create_info.generateMipmaps = KTX_FALSE;

    ktxResult ktx_ret = ktxTexture2_Create(&create_info, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &ktx2_texture);
    if (ktx_ret != KTX_SUCCESS) {
        std::string err_msg = "转换 KTX 失败：创建 KTX2 对象失败 | 错误码："
            + std::string(ktxErrorString(ktx_ret))
            + " | VK 格式：0x" + std::to_string((int)target_format);
        throw std::runtime_error(err_msg);
    }

    size_t src_data_size = _calc_src_data_size(w, h);
    if (src_data_size == 0) {
        ktxTexture2_Destroy(ktx2_texture);
        throw std::runtime_error("转换 KTX 失败：源数据大小计算为 0（源格式：" + std::to_string((int)pack_params.src_image_format) + "）");
    }

    ktx_ret = ktxTexture_SetImageFromMemory(
        ktxTexture(ktx2_texture), 0, 0, 0, src_data, src_data_size);
    if (ktx_ret != KTX_SUCCESS) {
        std::string err_msg = "转换 KTX 失败：写入源数据到 KTX2 失败 | 错误码："
            + std::string(ktxErrorString(ktx_ret));
        ktxTexture2_Destroy(ktx2_texture);
        throw std::runtime_error(err_msg);
    }

    if (log_level >= 2) {
        std::cout << "开始 GPU 纹理压缩..." << std::endl;
    }

    ktx_ret = _compress_ktx_with_basis(ktx2_texture, target_format);
    if (ktx_ret != KTX_SUCCESS) {
        std::string err_msg = "转换 KTX 失败：Basis Universal 压缩失败 | 错误码："
            + std::string(ktxErrorString(ktx_ret))
            + " | 目标格式：0x" + std::to_string((int)target_format);
        ktxTexture2_Destroy(ktx2_texture);
        throw std::runtime_error(err_msg);
    }

    if (log_level >= 2) {
        std::cout << "GPU 纹理压缩完成" << std::endl;
    }

    unsigned char* ktx_raw_data = nullptr;
    size_t ktx_data_size = 0;
    ktx_ret = ktxTexture_WriteToMemory(ktxTexture(ktx2_texture), &ktx_raw_data, &ktx_data_size);

    ktxTexture2_Destroy(ktx2_texture);

    if (ktx_ret != KTX_SUCCESS) {
        if (ktx_raw_data) free(ktx_raw_data);
        std::string err_msg = "转换 KTX 失败：导出 KTX2 内存数据失败 | 错误码："
            + std::string(ktxErrorString(ktx_ret));
        throw std::runtime_error(err_msg);
    }

    if (!ktx_raw_data || ktx_data_size == 0) {
        if (ktx_raw_data) free(ktx_raw_data);
        throw std::runtime_error("转换 KTX 失败：导出的 KTX2 数据为空");
    }

    UniquePtr_uChar ktx_data(ktx_raw_data);
    size = static_cast<int>(ktx_data_size);

    std::string format_name = "未知格式";
    switch (target_format) {
        case VK_FORMAT_BC7_UNORM_BLOCK: format_name = "BC7"; break;
        case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK: format_name = "ETC2_RGBA8"; break;
        case VK_FORMAT_ASTC_8x8_UNORM_BLOCK: format_name = "ASTC_8x8"; break;
        case VK_FORMAT_ASTC_6x6_UNORM_BLOCK: format_name = "ASTC_6x6"; break;
        default: break;
    }

    if (log_level >= 2) {
        printf("KTX2 转换成功：\n"
            "  尺寸：%dx%d | 纹理类型：%s | MIP级别：%u\n"
            "  目标格式：%s (0x%X) | 源格式：%d | 数据大小：%zu 字节\n",
            w, h, pack_params.is_2D ? "2D" : "3D", 1,
            format_name.c_str(), (int)target_format,
            (int)pack_params.src_image_format, ktx_data_size);
    }

    return ktx_data;
}

UniquePtr_uChar MoePack::_ztsd_compression(const unsigned char* ktx_data, int& size) {
    if (!ktx_data) {
        throw std::runtime_error("ZSTD 压缩失败：KTX 数据指针为空");
    }
    if (size <= 0) {
        throw std::runtime_error("ZSTD 压缩失败：无效的 KTX 数据大小 [" + std::to_string(size) + "]");
    }

    int original_size = size;

    size_t max_compressed_size = ZSTD_compressBound(static_cast<size_t>(size));
    if (max_compressed_size == 0) {
        throw std::runtime_error("ZSTD 压缩失败：计算最大压缩缓冲区大小失败");
    }

    unsigned char* compressed_data = static_cast<unsigned char*>(malloc(max_compressed_size));
    if (!compressed_data) {
        throw std::runtime_error("ZSTD 压缩失败：分配压缩缓冲区内存失败（需要 "
            + std::to_string(max_compressed_size) + " 字节）");
    }

    size_t compressed_size = ZSTD_compress(
        compressed_data, max_compressed_size,
        ktx_data, static_cast<size_t>(size),
        pack_params.ztsd_level);

    if (ZSTD_isError(compressed_size)) {
        free(compressed_data);
        std::string err_msg = "ZSTD 压缩失败：" + std::string(ZSTD_getErrorName(compressed_size));
        throw std::runtime_error(err_msg);
    }

    size = static_cast<int>(compressed_size);
    if (size <= 0) {
        free(compressed_data);
        throw std::runtime_error("ZSTD 压缩失败：压缩后数据大小为 0");
    }

    UniquePtr_uChar compressed_ptr(compressed_data);

    if (log_level >= 3) {
        printf("ZSTD 压缩成功：\n"
            "  原始大小：%d 字节 | 压缩后大小：%d 字节 | 压缩等级：%d\n"
            "  压缩比：%.2f:1\n",
            original_size, size, pack_params.ztsd_level,
            static_cast<float>(original_size) / static_cast<float>(size));
    }

    return compressed_ptr;
}

UniquePtr_uChar MoePack::_encrypt(unsigned char* data, int& size, std::string key) {
    if (sodium_init() == -1) {
        throw std::runtime_error("加密失败：libsodium 初始化失败");
    }

    if (!data) {
        throw std::runtime_error("加密失败：待加密数据指针为空");
    }
    if (size <= 0) {
        throw std::runtime_error("加密失败：待加密数据长度无效");
    }

    int original_size = size;

    if (key.empty()) {
        key = pack_params.encryption_key;
        if (key.empty()) {
            throw std::runtime_error("加密失败：密钥为空");
        }
    }

    unsigned char crypto_key[crypto_secretbox_KEYBYTES];
    unsigned char salt[crypto_pwhash_SALTBYTES];
    randombytes_buf(salt, sizeof(salt));

    int ret = crypto_pwhash(
        crypto_key, sizeof(crypto_key),
        key.c_str(), key.length(),
        salt,
        crypto_pwhash_OPSLIMIT_INTERACTIVE,
        crypto_pwhash_MEMLIMIT_INTERACTIVE,
        crypto_pwhash_ALG_DEFAULT);
    if (ret != 0) {
        sodium_memzero(crypto_key, sizeof(crypto_key));
        throw std::runtime_error("加密失败：密钥派生失败");
    }

    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    randombytes_buf(nonce, sizeof(nonce));

    size_t encrypted_content_len = static_cast<size_t>(size) + crypto_secretbox_MACBYTES;
    size_t total_encrypted_len = sizeof(nonce) + sizeof(salt) + encrypted_content_len;

    unsigned char* encrypted_buffer = static_cast<unsigned char*>(malloc(total_encrypted_len));
    if (!encrypted_buffer) {
        sodium_memzero(crypto_key, sizeof(crypto_key));
        throw std::runtime_error("加密失败：分配加密缓冲区内存不足");
    }

    unsigned char* encrypted_content = encrypted_buffer + sizeof(nonce) + sizeof(salt);
    crypto_secretbox_easy(encrypted_content, data,
        static_cast<unsigned long long>(size), nonce, crypto_key);

    memcpy(encrypted_buffer, nonce, sizeof(nonce));
    memcpy(encrypted_buffer + sizeof(nonce), salt, sizeof(salt));

    sodium_memzero(crypto_key, sizeof(crypto_key));
    sodium_memzero(nonce, sizeof(nonce));
    sodium_memzero(salt, sizeof(salt));

    size = static_cast<int>(total_encrypted_len);

    if (log_level >= 3) {
        printf("加密成功：\n"
            "  原始数据长度：%d 字节 | 加密后总长度：%d 字节 | 加密算法：XSalsa20-Poly1305\n"
            "  加密内容结构：IV(24字节) + 盐值(16字节) + 加密数据(%zu字节 + 16字节MAC)\n",
            original_size, size, static_cast<size_t>(original_size));
    }

    return UniquePtr_uChar(encrypted_buffer);
}

std::string MoePack::pack(std::string in_path, const std::string out_path) {
    using namespace std;
    vector<std::string> in_srcdata_paths;
    vector<std::string> in_srcdata_names;
    int64_t src_data_size = 0;
    int64_t packed_data_size = 0;
    std::wstring out_file_path;

    pack_params.texture_format = GetGpuCompressionFormat(dst_platform);

    if (!is_directory_path(out_path, true)) {
        out_file_path = GetParentDirFromPath(out_path);
    } else {
        out_file_path = utf8ToWstring(out_path);
    }

    if (is_directory_path(in_path)) {
        GetFileDirEx_NoExtension(in_path, in_srcdata_paths, in_srcdata_names);
        if (log_level >= 2) cout << "检测到输入路径为文件夹，自动获取符合格式的图片\n";
    } else {
        in_srcdata_paths.push_back(in_path);
        std::string processed_path = wstringToUtf8(RemoveFileExtension(utf8ToWstring(in_path)));
        size_t last_sep_pos = processed_path.find_last_of("\\/");
        if (last_sep_pos != std::string::npos) {
            processed_path = processed_path.substr(last_sep_pos + 1);
        }
        in_srcdata_names.push_back(processed_path);
    }

    if (log_level >= 0) cout << "共计 " << in_srcdata_paths.size() << " 个文件待处理。（正在工作中，请不要关闭程序）\n";

    if (!fs::exists(wstringToUtf8(out_file_path))) {
        if (!fs::create_directories(wstringToUtf8(out_file_path))) {
            throw std::runtime_error("创建输出目录失败：" + wstringToUtf8(out_file_path));
        }
        if (log_level >= 2) cout << "已创建输出目录：" << wstringToUtf8(out_file_path) << endl;
    }

    for (size_t i = 0; i < in_srcdata_paths.size(); ++i) {
        std::string path = in_srcdata_paths[i];
        int size = 0;
        UniquePtr_uChar out_data;

        if (path.empty()) {
            cout << "警告：路径为空，跳过处理。\n";
            continue;
        }

        if (!fs::exists(path)) {
            cout << "警告：文件不存在，跳过处理：" << path << endl;
            continue;
        }

        try {
            if (log_level >= 2) {
                src_data_size = fs::file_size(path);
            }

            int img_h = 0;
            int img_w = 0;
            if (log_level >= 1) {
                cout << "[" << (i + 1) << "/" << in_srcdata_paths.size() << "] 开始解码图片: "
                    << fs::path(path).filename().string() << " (请耐心等待)";
                std::flush(cout);
            }
            out_data = _load_image_to_src_format(path, img_h, img_w);
            if (log_level >= 1) {
                cout << "-> 解码成功 [" << img_w << "x" << img_h << "]" << endl;
            }

            if (log_level >= 1) {
                cout << "     开始转换为KTX2格式(请耐心等待)";
                std::flush(cout);
            }
            out_data = _src_format_to_ktx(out_data.get(), img_h, img_w, size);
            if (log_level >= 1) {
                cout << "-> 转换成功 [" << size << "字节]" << endl;
            }

            if (pack_params.ztsd_on) {
                if (log_level >= 1) {
                    cout << "     开始ZSTD压缩(请耐心等待)";
                    std::flush(cout);
                }
                out_data = _ztsd_compression(out_data.get(), size);
                if (log_level >= 1) {
                    cout << "-> 压缩成功 [" << size << "字节]" << endl;
                }
            }

            if (pack_params.is_encryption) {
                if (log_level >= 1) {
                    cout << "     开始加密数据(请耐心等待)";
                    std::flush(cout);
                }
                out_data = _encrypt(out_data.get(), size, pack_params.encryption_key);
                if (log_level >= 1) {
                    cout << "-> 加密成功 [" << size << "字节]" << endl;
                }
            }

            MoeHeader moe_header;
            moe_header.ztsd_on = pack_params.ztsd_on ? 1 : 0;
            moe_header.encrypted_on = pack_params.is_encryption ? 1 : 0;
            moe_header.original_size = static_cast<uint32_t>(size);

            if (size > 0 && out_data.get()) {
                unsigned char hash[crypto_hash_sha256_BYTES];
                crypto_hash_sha256(hash, out_data.get(), size);
                moe_header.set_check_data(hash, crypto_hash_sha256_BYTES);
            }

            moe_header.to_big_endian();

            std::wstring out_file_full_path = out_file_path + L"/" +
                utf8ToWstring(in_srcdata_names[i]) + L".moe";

            if (fs::exists(wstringToUtf8(out_file_full_path))) {
                if (log_level >= 2) {
                    cout << "     注意：输出文件已存在，将覆盖" << endl;
                }
            }

            if (log_level >= 1) {
                cout << "     开始写入输出文件：" << in_srcdata_names[i] << ".moe (请耐心等待)";
                std::flush(cout);
            }
            bool write_success = write_data_to_dir(
                out_file_full_path, out_data.get(), static_cast<size_t>(size), &moe_header);

            if (!write_success) {
                throw std::runtime_error("写入文件失败：" + wstringToUtf8(out_file_full_path));
            }

            if (log_level >= 1) {
                cout << "-> 写入成功" << endl;
            }

            moe_header.from_big_endian();

            if (log_level >= 1) {
                packed_data_size = fs::file_size(wstringToUtf8(out_file_full_path));

                cout << "->封包完成：" << endl;
                cout << "    输出文件: " << in_srcdata_names[i] << ".moe" << endl;
                cout << "    输出大小: " << packed_data_size << " 字节" << endl;
                cout << "    头部大小: " << sizeof(MoeHeader) << " 字节" << endl;
                cout << "    数据大小: " << size << " 字节" << endl;

                if (log_level >= 2 && src_data_size > 0) {
                    float ratio = static_cast<float>(packed_data_size) / static_cast<float>(src_data_size);
                    cout << "    原始大小: " << src_data_size << " 字节" << endl;
                    cout << "    压缩率: " << std::fixed << std::setprecision(2) << ratio << endl;
                }

                if (log_level >= 3) {
                    cout << "    处理步骤: 解码→KTX2转换";
                    if (pack_params.ztsd_on) cout << "→ZSTD压缩";
                    if (pack_params.is_encryption) cout << "→加密";
                    cout << "→写入文件" << endl;
                }
            }

            out_data.reset();

        } catch (const std::exception& e) {
            cerr << "错误：处理文件 " << path << " 时发生异常：" << e.what() << endl;
            if (log_level >= 1) {
                cout << "  文件 " << fs::path(path).filename().string() << " 处理失败，已跳过" << endl;
            }
            continue;
        }

        if (log_level >= 1) {
            cout << "----------------------------------------" << endl;
            cout << "已完成 " << (i + 1) << " / " << in_srcdata_paths.size()
                << " 个文件的封包。" << endl;
            cout << "----------------------------------------" << endl;
        }
    }

    if (log_level >= 0) {
        if (in_srcdata_paths.size() > 0) {
            cout << endl << " 已完成所有文件的封包处理。" << endl;
            cout << "  总计处理: " << in_srcdata_paths.size() << " 个文件" << endl;
            cout << "  输出目录: " << wstringToUtf8(out_file_path) << endl;

            if (log_level >= 2) {
                cout << "  文件格式: .moe (MOE_ARC格式)" << endl;
                cout << "  头部大小: " << sizeof(MoeHeader) << " 字节" << endl;
                cout << "  版本号: " << MOE_PackVersion << endl;
            }
        } else {
            cout << "警告：没有找到可处理的文件。" << endl;
        }
    }

    return "SUCCESS";
}

size_t MoePack::_calc_src_data_size(int width, int height) {
    if (width <= 0 || height <= 0) {
        throw std::runtime_error(
            "计算源数据大小失败：无效尺寸 [" + std::to_string(width) + "x" + std::to_string(height) + "]");
    }

    size_t pixel_byte_size = 0;
    switch (pack_params.src_image_format) {
    case RGB888:      pixel_byte_size = 3; break;
    case RGBA8888:    pixel_byte_size = 4; break;
    case L8:          pixel_byte_size = 1; break;
    case LA88:        pixel_byte_size = 2; break;
    case RGBA32F:     pixel_byte_size = 16; break;
    default:
        pixel_byte_size = 4;
        printf("警告：未识别的源图像格式（枚举值：%d），默认按 RGBA8888 计算\n",
            (int)pack_params.src_image_format);
        break;
    }

    size_t total_size = static_cast<size_t>(width) * static_cast<size_t>(height) * pixel_byte_size;

    if (total_size == 0) {
        throw std::runtime_error(
            "计算源数据大小失败：总字节数为0（格式：" + std::to_string((int)pack_params.src_image_format) + "）");
    }

    return total_size;
}

void MoePack::_set_basis_compression_params(CompressionParams& params, VkFormat target_format) {
    params = CompressionParams();

    switch (target_format) {
    case VK_FORMAT_BC7_UNORM_BLOCK:
        params.uastcFlags = KTX_PACK_UASTC_LEVEL_DEFAULT | KTX_PACK_UASTC_FAVOR_BC7_ERROR;
        if (log_level >= 3) {
            std::cout << "  压缩参数：目标格式 BC7，优化 BC7 错误率" << std::endl;
        }
        break;

    case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
        params.uastcFlags = KTX_PACK_UASTC_LEVEL_DEFAULT | KTX_PACK_UASTC_ETC1_FASTER_HINTS;
        if (log_level >= 3) {
            std::cout << "  压缩参数：目标格式 ETC2，优化 ETC1 转换速度" << std::endl;
        }
        break;

    case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
    case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
        params.uastcFlags = KTX_PACK_UASTC_LEVEL_DEFAULT;
        if (log_level >= 3) {
            std::cout << "  压缩参数：目标格式 ASTC，使用默认设置" << std::endl;
        }
        break;

    default:
        params.uastcFlags = KTX_PACK_UASTC_LEVEL_DEFAULT;
        if (log_level >= 2) {
            std::cout << "  警告：未知目标格式，使用默认压缩参数" << std::endl;
        }
        break;
    }

    if (pack_params.ztsd_on) {
        int zstd_level = pack_params.ztsd_level;

        ktx_pack_uastc_flags uastc_level = KTX_PACK_UASTC_LEVEL_DEFAULT;
        if (zstd_level <= 3) {
            uastc_level = KTX_PACK_UASTC_LEVEL_FASTEST;
        } else if (zstd_level <= 9) {
            uastc_level = KTX_PACK_UASTC_LEVEL_FASTER;
        } else if (zstd_level <= 15) {
            uastc_level = KTX_PACK_UASTC_LEVEL_DEFAULT;
        } else if (zstd_level <= 18) {
            uastc_level = KTX_PACK_UASTC_LEVEL_SLOWER;
        } else {
            uastc_level = KTX_PACK_UASTC_LEVEL_VERYSLOW;
        }

        params.uastcFlags = (params.uastcFlags & ~KTX_PACK_UASTC_LEVEL_MASK) | uastc_level;

        if (zstd_level > 15) {
            params.uastcRDO = KTX_TRUE;
            params.uastcRDOQualityScalar = 1.0f + (zstd_level - 15) * 0.1f;
            if (params.uastcRDOQualityScalar > 2.0f) params.uastcRDOQualityScalar = 2.0f;
        }

        if (log_level >= 3) {
            std::cout << "  基于ZSTD级别(" << zstd_level << ")调整参数：" << std::endl;
            std::cout << "    - UASTC级别: ";
            switch (uastc_level) {
            case KTX_PACK_UASTC_LEVEL_FASTEST: std::cout << "最快"; break;
            case KTX_PACK_UASTC_LEVEL_FASTER: std::cout << "较快"; break;
            case KTX_PACK_UASTC_LEVEL_DEFAULT: std::cout << "默认"; break;
            case KTX_PACK_UASTC_LEVEL_SLOWER: std::cout << "较慢"; break;
            case KTX_PACK_UASTC_LEVEL_VERYSLOW: std::cout << "最慢"; break;
            default: std::cout << "未知"; break;
            }
            std::cout << std::endl;
            std::cout << "    - UASTC RDO: " << (params.uastcRDO ? "启用" : "禁用") << std::endl;
            if (params.uastcRDO) {
                std::cout << "    - RDO质量标量: " << params.uastcRDOQualityScalar << std::endl;
            }
        }
    }

    if (log_level >= 3) {
        std::cout << "  最终压缩参数：" << std::endl;
        std::cout << "    - 线程数: " << params.threadCount << std::endl;
        std::cout << "    - UASTC标志: 0x" << std::hex << params.uastcFlags << std::dec << std::endl;
        std::cout << "    - 使用UASTC RDO: " << (params.uastcRDO ? "是" : "否") << std::endl;
        if (params.uastcRDO) {
            std::cout << "    - RDO质量标量: " << params.uastcRDOQualityScalar << std::endl;
        }
    }
}

ktxResult MoePack::_compress_ktx_with_basis(ktxTexture2* texture, VkFormat target_format) {
    if (!texture) {
        return KTX_INVALID_VALUE;
    }

    CompressionParams basis_params;
    _set_basis_compression_params(basis_params, target_format);

    ktxBasisParams ktx_params = {};
    ktx_params.structSize = sizeof(ktxBasisParams);

    ktx_params.uastc = basis_params.uastc;
    ktx_params.threadCount = basis_params.threadCount;
    ktx_params.uastcFlags = basis_params.uastcFlags;
    ktx_params.uastcRDO = basis_params.uastcRDO;
    ktx_params.uastcRDOQualityScalar = basis_params.uastcRDOQualityScalar;

    ktx_params.normalMap = KTX_FALSE;
    ktx_params.preSwizzle = KTX_FALSE;
    ktx_params.noEndpointRDO = KTX_FALSE;
    ktx_params.noSelectorRDO = KTX_FALSE;

    if (ktx_params.uastcRDO) {
        ktx_params.uastcRDODictSize = 4096;
        ktx_params.uastcRDOMaxSmoothBlockErrorScale = 10.0f;
        ktx_params.uastcRDOMaxSmoothBlockStdDev = 18.0f;
        ktx_params.uastcRDODontFavorSimplerModes = KTX_FALSE;
        ktx_params.uastcRDONoMultithreading = KTX_FALSE;
    }

    if (log_level >= 2) {
        std::cout << "Basis Universal 压缩参数：" << std::endl;
        std::cout << "  模式: UASTC" << std::endl;
        std::cout << "  线程数: " << ktx_params.threadCount << std::endl;

        ktx_uint32_t level = ktx_params.uastcFlags & KTX_PACK_UASTC_LEVEL_MASK;
        std::cout << "  压缩级别: ";
        switch (level) {
        case KTX_PACK_UASTC_LEVEL_FASTEST: std::cout << "最快 (43.45dB)"; break;
        case KTX_PACK_UASTC_LEVEL_FASTER: std::cout << "较快 (46.49dB)"; break;
        case KTX_PACK_UASTC_LEVEL_DEFAULT: std::cout << "默认 (47.47dB)"; break;
        case KTX_PACK_UASTC_LEVEL_SLOWER: std::cout << "较慢 (48.01dB)"; break;
        case KTX_PACK_UASTC_LEVEL_VERYSLOW: std::cout << "最慢 (48.24dB)"; break;
        default: std::cout << "未知"; break;
        }
        std::cout << std::endl;
        std::cout << "  UASTC RDO: " << (ktx_params.uastcRDO ? "启用" : "禁用") << std::endl;
        if (ktx_params.uastcRDO) {
            std::cout << "    - 质量标量: " << ktx_params.uastcRDOQualityScalar << std::endl;
            std::cout << "    - 字典大小: " << ktx_params.uastcRDODictSize << std::endl;
            std::cout << "    - 平滑块错误比例: " << ktx_params.uastcRDOMaxSmoothBlockErrorScale << std::endl;
            std::cout << "    - 平滑块标准差: " << ktx_params.uastcRDOMaxSmoothBlockStdDev << std::endl;
            std::cout << "    - 偏爱简单模式: " << (!ktx_params.uastcRDODontFavorSimplerModes ? "是" : "否") << std::endl;
        }

        if (ktx_params.uastcFlags & KTX_PACK_UASTC_FAVOR_BC7_ERROR) {
            std::cout << "  优化标志: 优化BC7错误率" << std::endl;
        } else if (ktx_params.uastcFlags & KTX_PACK_UASTC_ETC1_FASTER_HINTS) {
            std::cout << "  优化标志: 优化ETC1转换速度" << std::endl;
        }
    }

    ktxResult result = ktxTexture2_CompressBasisEx(texture, &ktx_params);

    if (result != KTX_SUCCESS) {
        if (log_level >= 1) {
            std::cerr << "Basis Universal 压缩失败：" << std::endl;
            std::cerr << "  错误码: " << result << " (" << ktxErrorString(result) << ")" << std::endl;
        }
        return result;
    }

    if (log_level >= 2) {
        std::cout << "Basis Universal 压缩成功完成" << std::endl;
    }

    if (log_level >= 3) {
        std::cout << "Basis压缩后信息:" << std::endl;
        std::cout << "  原始vkFormat: 0x" << std::hex << texture->vkFormat << std::dec
            << " (应在此刻检查是否为UNDEFINED)" << std::endl;
        std::cout << "  超级压缩方案: " << texture->supercompressionScheme << std::endl;
    }

    if (log_level >= 2) {
        std::cout << "开始将Basis格式转码为目标GPU格式 (0x" << std::hex << target_format << std::dec << ")..." << std::endl;
    }

    ktxResult ktx_ret = ktxTexture2_TranscodeBasis(texture, VkFormatToKtxTranscodeFmt(target_format), 0);
    if (ktx_ret != KTX_SUCCESS) {
        std::string err_msg = "转换 KTX 失败：Basis转码失败 | 错误码："
            + std::string(ktxErrorString(ktx_ret))
            + " | 目标格式：0x" + std::to_string((int)target_format);
        ktxTexture2_Destroy(texture);
        throw std::runtime_error(err_msg);
    }

    if (log_level >= 2) {
        std::cout << "Basis转码成功完成。" << std::endl;
        std::cout << "  转码后vkFormat: 0x" << std::hex << texture->vkFormat << std::dec << std::endl;
    }

    return KTX_SUCCESS;
}

std::string MoePack::pack_ex(std::string in_path, std::string out_path) {
    using namespace std;
    vector<std::string> in_srcdata_paths;
    vector<std::string> in_srcdata_names;
    std::wstring out_file_path;

    if (!is_directory_path(out_path, true)) {
        out_file_path = GetParentDirFromPath(out_path);
    } else {
        out_file_path = utf8ToWstring(out_path);
    }

    if (is_directory_path(in_path)) {
        GetFileDirEx_NoExtension(in_path, in_srcdata_paths, in_srcdata_names);
        if (log_level >= 2) cout << "检测到输入路径为文件夹，自动获取文件\n";
    } else {
        in_srcdata_paths.push_back(in_path);
        std::string processed_path = wstringToUtf8(RemoveFileExtension(utf8ToWstring(in_path)));
        size_t last_sep_pos = processed_path.find_last_of("\\/");
        if (last_sep_pos != std::string::npos) {
            processed_path = processed_path.substr(last_sep_pos + 1);
        }
        in_srcdata_names.push_back(processed_path);
    }

    if (log_level >= 0) cout << "共计 " << in_srcdata_paths.size() << " 个文件待处理。（正在工作中，请不要关闭程序）\n";

    if (!fs::exists(wstringToUtf8(out_file_path))) {
        if (!fs::create_directories(wstringToUtf8(out_file_path))) {
            throw std::runtime_error("创建输出目录失败：" + wstringToUtf8(out_file_path));
        }
        if (log_level >= 2) cout << "已创建输出目录：" << wstringToUtf8(out_file_path) << endl;
    }

    for (size_t i = 0; i < in_srcdata_paths.size(); ++i) {
        std::string path = in_srcdata_paths[i];
        int size = 0;
        UniquePtr_uChar out_data;

        if (path.empty()) {
            cout << "警告：路径为空，跳过处理。\n";
            continue;
        }

        if (!fs::exists(path)) {
            cout << "警告：文件不存在，跳过处理：" << path << endl;
            continue;
        }

        try {
            if (log_level >= 1) {
                cout << "[" << (i + 1) << "/" << in_srcdata_paths.size() << "] 开始读取文件: "
                    << fs::path(path).filename().string() << " (请耐心等待)";
                std::flush(cout);
            }
            size_t file_size_val = fs::file_size(path);
            if (file_size_val == 0) {
                cout << "  警告：文件大小为0，跳过处理：" << path << endl;
                continue;
            }
            unsigned char* raw_file_data = static_cast<unsigned char*>(malloc(file_size_val));
            if (!raw_file_data) {
                throw std::runtime_error("读取文件失败：内存分配失败 - " + path);
            }
            std::ifstream file(path, std::ios::binary);
            if (!file.read(reinterpret_cast<char*>(raw_file_data), file_size_val)) {
                free(raw_file_data);
                throw std::runtime_error("读取文件失败：无法读取文件 - " + path);
            }
            file.close();
            out_data = UniquePtr_uChar(raw_file_data);
            size = static_cast<int>(file_size_val);
            if (log_level >= 1) {
                cout << "-> 读取成功 [" << size << "字节]" << endl;
            }

            if (pack_params.ztsd_on) {
                if (log_level >= 1) {
                    cout << "     开始ZSTD压缩(请耐心等待)";
                    std::flush(cout);
                }
                out_data = _ztsd_compression(out_data.get(), size);
                if (log_level >= 1) {
                    cout << "-> 压缩成功 [" << size << "字节]" << endl;
                }
            }

            if (pack_params.is_encryption) {
                if (log_level >= 1) {
                    cout << "     开始加密数据(请耐心等待)";
                    std::flush(cout);
                }
                out_data = _encrypt(out_data.get(), size, pack_params.encryption_key);
                if (log_level >= 1) {
                    cout << "-> 加密成功 [" << size << "字节]" << endl;
                }
            }

            MoeHeader moe_header;
            moe_header.ztsd_on = pack_params.ztsd_on ? 1 : 0;
            moe_header.encrypted_on = pack_params.is_encryption ? 1 : 0;
            moe_header.original_size = static_cast<uint32_t>(size);

            if (size > 0 && out_data.get()) {
                unsigned char hash[crypto_hash_sha256_BYTES];
                crypto_hash_sha256(hash, out_data.get(), size);
                moe_header.set_check_data(hash, crypto_hash_sha256_BYTES);
            }

            moe_header.to_big_endian();

            std::wstring out_file_full_path = out_file_path + L"/" +
                utf8ToWstring(in_srcdata_names[i]) + L".moe";

            if (fs::exists(wstringToUtf8(out_file_full_path))) {
                if (log_level >= 2) {
                    cout << "     注意：输出文件已存在，将覆盖" << endl;
                }
            }

            if (log_level >= 1) {
                cout << "     开始写入输出文件：" << in_srcdata_names[i] << ".moe (请耐心等待)";
                std::flush(cout);
            }
            bool write_success = write_data_to_dir(
                out_file_full_path, out_data.get(), static_cast<size_t>(size), &moe_header);

            if (!write_success) {
                throw std::runtime_error("写入文件失败：" + wstringToUtf8(out_file_full_path));
            }

            if (log_level >= 1) {
                cout << "-> 写入成功" << endl;
            }

            if (log_level >= 1) {
                int64_t packed_data_size = fs::file_size(wstringToUtf8(out_file_full_path));

                cout << "->封包完成：" << endl;
                cout << "    输出文件: " << in_srcdata_names[i] << ".moe" << endl;
                cout << "    输出大小: " << packed_data_size << " 字节" << endl;
                cout << "    头部大小: " << sizeof(MoeHeader) << " 字节" << endl;
                cout << "    数据大小: " << size << " 字节" << endl;

                if (log_level >= 3) {
                    cout << "    处理步骤: 读取文件";
                    if (pack_params.ztsd_on) cout << "→ZSTD压缩";
                    if (pack_params.is_encryption) cout << "→加密";
                    cout << "→写入文件" << endl;
                }
            }

            out_data.reset();
        } catch (const std::exception& e) {
            cerr << "错误：处理文件 " << path << " 时发生异常：" << e.what() << endl;
            if (log_level >= 1) {
                cout << "  文件 " << fs::path(path).filename().string() << " 处理失败，已跳过" << endl;
            }
            continue;
        }

        if (log_level >= 1) {
            cout << "----------------------------------------" << endl;
            cout << "已完成 " << (i + 1) << " / " << in_srcdata_paths.size()
                << " 个文件的封包。" << endl;
            cout << "----------------------------------------" << endl;
        }
    }

    if (log_level >= 0) {
        if (in_srcdata_paths.size() > 0) {
            cout << endl << " 已完成所有文件的封包处理。" << endl;
            cout << "  总计处理: " << in_srcdata_paths.size() << " 个文件" << endl;
            cout << "  输出目录: " << wstringToUtf8(out_file_path) << endl;
        } else {
            cout << "警告：没有找到可处理的文件。" << endl;
        }
    }

    return "SUCCESS";
}

UniquePtr_uChar MoePack::_encrypt_stream(unsigned char* data, int size, std::string key,
                                          uint32_t chunk_size, uint32_t& out_chunk_count) {
    if (sodium_init() == -1) {
        throw std::runtime_error("分块加密失败：libsodium 初始化失败");
    }
    if (!data) {
        throw std::runtime_error("分块加密失败：待加密数据指针为空");
    }
    if (size <= 0) {
        throw std::runtime_error("分块加密失败：待加密数据长度无效");
    }
    if (chunk_size == 0) {
        throw std::runtime_error("分块加密失败：块大小不能为 0");
    }

    if (key.empty()) {
        key = pack_params.encryption_key;
        if (key.empty()) {
            throw std::runtime_error("分块加密失败：密钥为空");
        }
    }

    unsigned char crypto_key[crypto_secretstream_xchacha20poly1305_KEYBYTES];
    unsigned char salt[crypto_pwhash_SALTBYTES];
    if (pack_params.use_fixed_salt) {
        if (!normalize_salt(pack_params.fixed_salt_value, salt)) {
            throw std::runtime_error("分块加密失败：固定 salt 无效");
        }
        if (log_level >= 1) {
            std::cout << "  使用固定 salt: ";
            for (int i = 0; i < (int)sizeof(salt); i++) printf("%02x", salt[i]);
            std::cout << std::endl;
        }
    } else {
        randombytes_buf(salt, sizeof(salt));
    }

    int ret = crypto_pwhash(
        crypto_key, sizeof(crypto_key),
        key.c_str(), key.length(),
        salt,
        crypto_pwhash_OPSLIMIT_INTERACTIVE,
        crypto_pwhash_MEMLIMIT_INTERACTIVE,
        crypto_pwhash_ALG_DEFAULT);
    if (ret != 0) {
        sodium_memzero(crypto_key, sizeof(crypto_key));
        throw std::runtime_error("分块加密失败：密钥派生失败");
    }

    crypto_secretstream_xchacha20poly1305_state state;
    unsigned char stream_header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
    crypto_secretstream_xchacha20poly1305_init_push(&state, stream_header, crypto_key);
    sodium_memzero(crypto_key, sizeof(crypto_key));

    out_chunk_count = (static_cast<uint32_t>(size) + chunk_size - 1) / chunk_size;

    size_t abytes = crypto_secretstream_xchacha20poly1305_ABYTES;
    size_t total_size = sizeof(stream_header) + sizeof(salt);
    uint32_t remaining = static_cast<uint32_t>(size);
    for (uint32_t i = 0; i < out_chunk_count; i++) {
        uint32_t this_plain = (remaining < chunk_size) ? remaining : chunk_size;
        total_size += this_plain + abytes;
        remaining -= this_plain;
    }

    unsigned char* encrypted_buffer = static_cast<unsigned char*>(malloc(total_size));
    if (!encrypted_buffer) {
        sodium_memzero(&state, sizeof(state));
        throw std::runtime_error("分块加密失败：分配加密缓冲区内存不足");
    }

    memcpy(encrypted_buffer, stream_header, sizeof(stream_header));
    memcpy(encrypted_buffer + sizeof(stream_header), salt, sizeof(salt));
    sodium_memzero(salt, sizeof(salt));

    unsigned char* out_ptr = encrypted_buffer + sizeof(stream_header) + sizeof(salt);
    unsigned char* in_ptr  = data;
    remaining = static_cast<uint32_t>(size);

    for (uint32_t i = 0; i < out_chunk_count; i++) {
        uint32_t this_plain = (remaining < chunk_size) ? remaining : chunk_size;
        bool is_final = (i == out_chunk_count - 1);
        unsigned char tag = is_final
            ? crypto_secretstream_xchacha20poly1305_TAG_FINAL
            : crypto_secretstream_xchacha20poly1305_TAG_MESSAGE;

        unsigned long long out_len = 0;
        crypto_secretstream_xchacha20poly1305_push(
            &state, out_ptr, &out_len,
            in_ptr, this_plain,
            nullptr, 0, tag);

        out_ptr   += out_len;
        in_ptr    += this_plain;
        remaining -= this_plain;
    }

    sodium_memzero(&state, sizeof(state));

    return UniquePtr_uChar(encrypted_buffer);
}

std::string MoePack::pack_ex_stream(std::string in_path, std::string out_path,
                                     uint32_t chunk_size) {
    using namespace std;

    if (!fs::exists(in_path)) {
        throw std::runtime_error("输入文件不存在：" + in_path);
    }

    size_t file_size_val = fs::file_size(in_path);
    if (file_size_val == 0) {
        throw std::runtime_error("文件大小为0：" + in_path);
    }

    unsigned char* raw_file_data = static_cast<unsigned char*>(malloc(file_size_val));
    if (!raw_file_data) {
        throw std::runtime_error("内存分配失败：" + in_path);
    }
    std::ifstream file(in_path, std::ios::binary);
    if (!file.read(reinterpret_cast<char*>(raw_file_data), file_size_val)) {
        free(raw_file_data);
        throw std::runtime_error("读取文件失败：" + in_path);
    }
    file.close();

    UniquePtr_uChar file_data(raw_file_data);
    int data_size = static_cast<int>(file_size_val);

    MOE_Pack_AudioFormat audio_fmt = _detect_audio_format(raw_file_data, file_size_val);

    if (log_level >= 1) {
        cout << "分块加密打包: " << fs::path(in_path).filename().string()
             << " (块大小: " << chunk_size << " 字节)" << endl;
    }

    uint32_t chunk_count = 0;
    UniquePtr_uChar encrypted = _encrypt_stream(raw_file_data, data_size,
                                                  pack_params.encryption_key,
                                                  chunk_size, chunk_count);

    MoeHeader header;
    header.chunk_size   = chunk_size;
    header.chunk_count  = chunk_count;
    header.audio_format = static_cast<uint32_t>(audio_fmt);
    header.original_size = static_cast<uint32_t>(file_size_val);

    size_t encrypted_size = crypto_secretstream_xchacha20poly1305_HEADERBYTES
                          + crypto_pwhash_SALTBYTES
                          + file_size_val
                          + static_cast<size_t>(chunk_count) * crypto_secretstream_xchacha20poly1305_ABYTES;

    {
        unsigned char hash[crypto_hash_sha256_BYTES];
        crypto_hash_sha256(hash, encrypted.get(), encrypted_size);
        header.set_check_data(hash, crypto_hash_sha256_BYTES);
    }

    header.to_big_endian();

    std::wstring out_file_path;
    if (!is_directory_path(out_path, true)) {
        out_file_path = utf8ToWstring(out_path);
    } else {
        std::string name = wstringToUtf8(
            RemoveFileExtension(utf8ToWstring(in_path)));
        size_t sep = name.find_last_of("\\/");
        if (sep != std::string::npos) name = name.substr(sep + 1);
        out_file_path = utf8ToWstring(out_path) + L"/" + utf8ToWstring(name) + L".moe";
    }

    fs::path fs_path(out_file_path);
    fs::create_directories(fs_path.parent_path());

    FILE* fp = nullptr;
#if defined(_WIN32) || defined(_WIN64)
    fp = _wfopen(fs_path.c_str(), L"wb");
#else
    fp = fopen(fs_path.c_str(), "wb");
#endif
    if (!fp) {
        throw std::runtime_error("无法创建输出文件：" + wstringToUtf8(out_file_path));
    }

    fwrite(&header, sizeof(MoeHeader), 1, fp);
    fwrite(encrypted.get(), 1, encrypted_size, fp);
    fclose(fp);

    if (log_level >= 1) {
        int64_t out_size = fs::file_size(fs_path);
        cout << "  分块加密完成: " << wstringToUtf8(out_file_path) << endl;
        cout << "    原始大小: " << file_size_val << " 字节" << endl;
        cout << "    块数: " << chunk_count << endl;
        cout << "    音频格式: " << (int)audio_fmt << endl;
        cout << "    输出大小: " << out_size << " 字节" << endl;
    }

    return "SUCCESS";
}
