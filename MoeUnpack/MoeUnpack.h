#pragma once
#include<string>
#include<fstream>
#include<filesystem>
#include<vector>
#include"MoeHeader.h"
#ifdef MOE_UNPACK_CHECK_DATA
#include"ktx.h"
#endif
#include"zstd.h"
#include"sodium.h"

namespace MoeUnpack{
	int unpack_log_level = 1; //默认日志等级
	//设置日志等级
	void set_unpack_log_level(int level) { unpack_log_level = level; }

	/**
    * @brief 解密函数
    *
    * @param data 待解密数据指针
    * @param size 待解密数据大小（输入时是加密数据大小，输出时是解密后数据大小）
    * @param key 解密密钥
    * @return UniquePtr_uChar 解密后的数据智能指针（不对外返回）
    * @throws std::runtime_error 解密失败时抛出异常
    */
    UniquePtr_uChar _decrypt(unsigned char* data, int& size, std::string key);
    /**
    * @brief ZSTD解压函数
    *
    * @param data 待解压数据指针
    * @param size 待解压数据大小（输入时是压缩数据大小，输出时是解压后数据大小）
    * @return UniquePtr_uChar 解压后的数据智能指针（不对外返回）
    * @throws std::runtime_error 解压失败时抛出异常
    */
    UniquePtr_uChar _ztsd_decompression(const unsigned char* data, int& size);
    /**
    * @brief 校验数据完整性（验证SHA256哈希）
    *
    * @param data 待校验数据指针
    * @param size 待校验数据大小
    * @param expected_hash 期望的SHA256哈希值
    * @return bool 校验通过返回true，否则返回false
    */
    bool _verify_data_integrity(const unsigned char* data, int size, const uint8_t* expected_hash);

    /**
    * @brief 解析MOE格式文件头部数据
    *
    * @details 从原始字节数据中解析MOE格式文件头部
    *
    * @param data 原始MOE头部字节数据指针
    * @param head_data 输出参数，解析后的MOE头部结构体（会覆盖原有值）
    * @throw std::runtime_error 当输入指针为空、魔数非法、头部大小不匹配时抛出异常
    */
    void unpack_moe_header(unsigned char* data, MoeHeader& head_data);

    /**
    * @brief 解包函数
    *
    */
    std::string unpack(std::string in_put/*某个文件的绝对路径*/, std::string out_put,std::string _key);
}

#ifdef MOE_UNPACK_IMPLEMENTATION

UniquePtr_uChar MoeUnpack::_decrypt(unsigned char* data, int& size, std::string key) {
    // 初始化 libsodium
    if (sodium_init() == -1) {
        throw std::runtime_error("解密失败：libsodium 初始化失败");
    }

    // 校验输入数据
    if (!data) {
        throw std::runtime_error("解密失败：待解密数据指针为空");
    }
    if (size <= 0) {
        throw std::runtime_error("解密失败：待解密数据长度无效");
    }

    // 保存加密数据大小用于日志
    int encrypted_size = size;

    // 检查数据长度是否足够包含 IV 和盐值
    size_t min_required_size = crypto_secretbox_NONCEBYTES + crypto_pwhash_SALTBYTES + crypto_secretbox_MACBYTES;
    if (static_cast<size_t>(size) < min_required_size) {
        throw std::runtime_error("解密失败：数据长度不足，无法解析加密格式");
    }

    // 解析加密数据格式：IV(24字节) + 盐值(16字节) + 加密数据
    unsigned char* nonce = data;  // 前24字节为IV
    unsigned char* salt = data + crypto_secretbox_NONCEBYTES;  // 接着16字节为盐值
    unsigned char* encrypted_content = data + crypto_secretbox_NONCEBYTES + crypto_pwhash_SALTBYTES;  // 剩余为加密内容
    size_t encrypted_content_len = static_cast<size_t>(size) - crypto_secretbox_NONCEBYTES - crypto_pwhash_SALTBYTES;

    // 密钥派生（使用相同的盐值）
    unsigned char crypto_key[crypto_secretbox_KEYBYTES];
    int ret = crypto_pwhash(
        crypto_key, sizeof(crypto_key),
        key.c_str(), key.length(),
        salt,
        crypto_pwhash_OPSLIMIT_INTERACTIVE,
        crypto_pwhash_MEMLIMIT_INTERACTIVE,
        crypto_pwhash_ALG_DEFAULT
    );

    if (ret != 0) {
        sodium_memzero(crypto_key, sizeof(crypto_key));
        throw std::runtime_error("解密失败：密钥派生失败");
    }

    // 分配解密结果缓冲区
    size_t decrypted_content_len = encrypted_content_len - crypto_secretbox_MACBYTES;
    unsigned char* decrypted_buffer = static_cast<unsigned char*>(malloc(decrypted_content_len));
    if (!decrypted_buffer) {
        sodium_memzero(crypto_key, sizeof(crypto_key));
        throw std::runtime_error("解密失败：分配解密缓冲区内存不足");
    }

    // 核心解密操作
    if (crypto_secretbox_open_easy(
        decrypted_buffer,
        encrypted_content,
        encrypted_content_len,
        nonce,
        crypto_key) != 0) {
        free(decrypted_buffer);
        sodium_memzero(crypto_key, sizeof(crypto_key));
        throw std::runtime_error("解密失败：数据校验失败（可能密钥错误或数据损坏）");
    }

    // 安全清理敏感数据
    sodium_memzero(crypto_key, sizeof(crypto_key));

    // 更新 size 为解密后的数据大小
    size = static_cast<int>(decrypted_content_len);

    if (unpack_log_level >= 3) {
        printf("解密成功：\n"
            "  加密数据长度：%d 字节 | 解密后长度：%d 字节 | 解密算法：XSalsa20-Poly1305\n"
            "  加密内容结构：IV(24字节) + 盐值(16字节) + 加密数据(%zu字节 + 16字节MAC)\n",
            encrypted_size,
            size,
            decrypted_content_len);
    }

    return UniquePtr_uChar(decrypted_buffer);
}

UniquePtr_uChar MoeUnpack::_ztsd_decompression(const unsigned char* data, int& size) {
    // 校验输入数据
    if (!data) {
        throw std::runtime_error("ZSTD 解压失败：数据指针为空");
    }
    if (size <= 0) {
        throw std::runtime_error("ZSTD 解压失败：无效的压缩数据大小 [" + std::to_string(size) + "]");
    }

    // 保存压缩数据大小用于日志
    int compressed_size = size;

    // 获取解压后数据大小
    size_t decompressed_size = ZSTD_getFrameContentSize(data, static_cast<size_t>(size));

    if (decompressed_size == ZSTD_CONTENTSIZE_ERROR) {
        throw std::runtime_error("ZSTD 解压失败：不是有效的 ZSTD 压缩数据");
    }
    if (decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN) {
        throw std::runtime_error("ZSTD 解压失败：无法确定解压后数据大小（可能数据流过大）");
    }
    if (decompressed_size == 0) {
        throw std::runtime_error("ZSTD 解压失败：解压后数据大小为 0");
    }

    // 分配解压输出缓冲区
    unsigned char* decompressed_data = static_cast<unsigned char*>(malloc(decompressed_size));
    if (!decompressed_data) {
        throw std::runtime_error("ZSTD 解压失败：分配解压缓冲区内存失败（需要 " +
            std::to_string(decompressed_size) + " 字节）");
    }

    // 执行 ZSTD 解压
    size_t actual_decompressed_size = ZSTD_decompress(
        decompressed_data,      // 输出缓冲区
        decompressed_size,      // 输出缓冲区大小
        data,                   // 输入数据
        static_cast<size_t>(size) // 输入数据大小
    );

    // 校验解压结果
    if (ZSTD_isError(actual_decompressed_size)) {
        free(decompressed_data);
        std::string err_msg = "ZSTD 解压失败：" + std::string(ZSTD_getErrorName(actual_decompressed_size));
        throw std::runtime_error(err_msg);
    }

    // 验证解压后数据大小
    if (actual_decompressed_size != decompressed_size) {
        free(decompressed_data);
        throw std::runtime_error("ZSTD 解压失败：解压后大小与预期不符（预期：" +
            std::to_string(decompressed_size) + " 字节，实际：" +
            std::to_string(actual_decompressed_size) + " 字节）");
    }

    // 更新 size 为解压后大小
    size = static_cast<int>(actual_decompressed_size);

    // 封装为智能指针返回
    UniquePtr_uChar decompressed_ptr(decompressed_data);

    if (unpack_log_level >= 3) {
        printf("ZSTD 解压成功：\n"
            "  压缩后大小：%d 字节 | 解压后大小：%d 字节\n"
            "  解压缩比：%.2f:1\n",
            compressed_size,
            size,
            static_cast<float>(size) / static_cast<float>(compressed_size));
    }

    return decompressed_ptr;
}

bool MoeUnpack::_verify_data_integrity(const unsigned char* data, int size, const uint8_t* expected_hash) {
    // 初始化 libsodium
    if (sodium_init() == -1) {
        if (unpack_log_level >= 1) {
            throw std::runtime_error("校验失败：libsodium 初始化失败");
        }
        return false;
    }

    // 校验输入参数
    if (!data || size <= 0) {
        if (unpack_log_level >= 1) {
            throw std::runtime_error("校验失败：数据为空或大小无效");
        }
        return false;
    }
    if (!expected_hash) {
        if (unpack_log_level >= 1) {
            throw std::runtime_error("校验失败：期望哈希值为空");
        }
        return false;
    }

    // 计算实际SHA256哈希
    unsigned char actual_hash[crypto_hash_sha256_BYTES];
    crypto_hash_sha256(actual_hash, data, size);

    // 比较哈希值
    int result = sodium_memcmp(actual_hash, expected_hash, crypto_hash_sha256_BYTES);
    bool is_valid = (result == 0);

    if (unpack_log_level >= 2) {
        if (is_valid) {
            printf("数据完整性校验通过");
        }
        else {
            throw std::runtime_error("数据完整性校验失败：哈希值不匹配");

            // 输出哈希值用于调试
            if (unpack_log_level >= 3) {
                throw std::runtime_error("期望哈希值：");
                for (size_t i = 0; i < crypto_hash_sha256_BYTES; ++i) {
                    printf("%02x", expected_hash[i]);
                }
                printf("\n");

                printf("实际哈希值：");
                for (size_t i = 0; i < crypto_hash_sha256_BYTES; ++i) {
                    printf("%02x", actual_hash[i]);
                }
                printf("\n");
            }
        }
    }

    return is_valid;
}

void MoeUnpack::unpack_moe_header(unsigned char* data, MoeHeader& head_data) {
    if (data == nullptr) {
        throw std::runtime_error("解析MoeHeader失败：输入数据指针为空");
    }
    const size_t required_size = sizeof(MoeHeader); // 90字节
    if (memcmp(data, "MOE", 3) != 0) {
        throw std::runtime_error("解析MoeHeader失败：不是MOE格式数据");
    }
    memcpy(&head_data, data, required_size);
    // 结构体中多字节字段（header_size、data_size）以大端存储，需转为当前主机序
    head_data.from_big_endian();

    // 校验（避免拷贝/字节序转换后数据异常）
    if (!head_data.is_magic_valid()) {
        throw std::runtime_error("解析MoeHeader失败：魔数校验最终失败，数据可能损坏");
    }
    //头部大小校验（确保解析的头部大小与定义一致）
    if (head_data.header_size != required_size) {
        throw std::runtime_error(
            "解析MoeHeader失败：头部大小不匹配，预期" + std::to_string(required_size) +
            "字节，实际" + std::to_string(head_data.header_size) + "字节"
        );
    }

    if (memcmp(head_data.version, MOE_PackVersion, 8) != 0) {
        throw std::runtime_error("解析MoeHeader失败：版本号不匹配(会尝试启用兼容模式)");
    }
}

std::string MoeUnpack::unpack(std::string in_put, std::string out_put,std::string _key) {
    MoeHeader moe_header;
    namespace fs = std::filesystem;

    // 验证输入文件
    if (!fs::exists(in_put)) {
        throw std::runtime_error("解包失败：输入文件不存在 - " + in_put);
    }

    if (unpack_log_level >= 1) {
        printf("开始解包文件: %s\n", in_put.c_str());
    }

    // 读取文件
    std::ifstream moe_file(in_put, std::ios::binary);
    if (!moe_file) {
        throw std::runtime_error("解包失败：无法打开文件 - " + in_put);
    }

    moe_file.seekg(0, std::ios::end);
    size_t file_size = moe_file.tellg();
    moe_file.seekg(0, std::ios::beg);

    // 解析头部
    std::vector<unsigned char> header_buffer(sizeof(MoeHeader));
    if (!moe_file.read(reinterpret_cast<char*>(header_buffer.data()), sizeof(MoeHeader))) {
        throw std::runtime_error("解包失败：读取文件头部失败");
    }

    try {
        unpack_moe_header(header_buffer.data(), moe_header);
    }
    catch (const std::exception& e) {
        throw std::runtime_error(std::string("解包失败：头部解析错误 - ") + e.what());
    }

    // 头部信息日志
    if (unpack_log_level >= 2) {
        printf("头部解析成功：\n");
        printf("  数据大小: %llu 字节\n", static_cast<unsigned long long>(moe_header.data_size));
        printf("  压缩标志: %s\n", moe_header.ztsd_on ? "启用" : "禁用");
        printf("  加密标志: %s\n", moe_header.encrypted_on ? "启用" : "禁用");
        if (unpack_log_level >= 3) {
            printf("  校验数据(前8字节): ");
            for (int i = 0; i < 8 && i < sizeof(moe_header.check_data); ++i) {
                printf("%02x", moe_header.check_data[i]);
            }
            printf("\n");
        }
    }

    // 验证文件大小
    size_t expected_total_size = sizeof(MoeHeader) + moe_header.data_size;
    if (file_size != expected_total_size) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg),
            "解包失败：文件大小不匹配（预期：%zu 字节，实际：%zu 字节）",
            expected_total_size, file_size);
        throw std::runtime_error(error_msg);
    }

    // 读取数据部分
    size_t data_size = static_cast<size_t>(moe_header.data_size);
    if (data_size == 0) {
        throw std::runtime_error("解包失败：数据大小为0");
    }
    unsigned char* raw_data = static_cast<unsigned char*>(malloc(data_size));
    if (!raw_data) {
        throw std::runtime_error("解包失败：分配数据内存失败");
    }
    if (!moe_file.read(reinterpret_cast<char*>(raw_data), data_size)) {
        free(raw_data);
        throw std::runtime_error("解包失败：读取文件数据失败");
    }
    moe_file.close();

    // 封装为智能指针
    UniquePtr_uChar current_data(raw_data);
    int current_size = static_cast<int>(data_size);

    // 数据完整性校验
    if (unpack_log_level >= 1) {
        printf("开始数据完整性校验...\n");
    }

    try {
        if (!_verify_data_integrity(current_data.get(), current_size, moe_header.check_data)) {
            throw std::runtime_error("数据完整性校验未通过");
        }
        if (unpack_log_level >= 1) {
            printf("数据完整性校验通过\n");
        }
    }
    catch (const std::exception& e) {
        throw std::runtime_error(std::string("解包失败：") + e.what());
    }

    // 解密
    if (moe_header.encrypted_on) {
        if (unpack_log_level >= 1) {
            printf("开始解密数据...\n");
        }

        try {
            std::string decryption_key = _key; 
            if (decryption_key.empty()) {
                throw std::runtime_error("文件已加密但未提供解密密钥");
            }

            current_data = _decrypt(current_data.get(), current_size, decryption_key);

            if (unpack_log_level >= 1) {
                printf("解密完成，数据大小: %d 字节\n", current_size);
            }
        }
        catch (const std::exception& e) {
            throw std::runtime_error(std::string("解包失败：解密过程异常 - ") + e.what());
        }
    }

    // 解压
    if (moe_header.ztsd_on) {
        if (unpack_log_level >= 1) {
            printf("开始解压数据...\n");
        }
        try {
            current_data = _ztsd_decompression(current_data.get(), current_size);

            if (unpack_log_level >= 1) {
                printf("解压完成，数据大小: %d 字节\n", current_size);
            }
        }
        catch (const std::exception& e) {
            throw std::runtime_error(std::string("解包失败：解压过程异常 - ") + e.what());
        }
    }
    // 解压后立即检查数据头，此时应为KTX2头
    if (unpack_log_level >= 3) {
        printf("[调试] 解压后，数据前64字节 (应为KTX2头): ");
        for (int i = 0; i < 64 && i < current_size; ++i) {
            printf("%02X ", current_data.get()[i]);
        }
        printf("\n");
    }
#ifdef MOE_UNPACK_CHECK_DATA
    // 验证是否为有效的KTX数据(dds-ktx头部解析报错)
    //if (unpack_log_level >= 1) {
    //    printf("开始验证KTX数据...\n");
    //}
    //ddsktx_texture_info info = {};
    //if (!ddsktx_parse(&info, current_data.get(), current_size)) {
    //    throw std::runtime_error("解包失败：无效的KTX2数据格式");
    //}
    //if (info.width == 0 || info.height == 0) {
    //    throw std::runtime_error("解包失败：KTX纹理尺寸无效");
    //}
    //if (unpack_log_level >= 2) {
    //    printf("KTX数据验证成功：\n");
    //    printf("  尺寸: %dx%d\n", info.width, info.height);
    //    printf("  Mip级别: %d\n", info.num_mips);
    //    printf("  格式标识: %u\n", info.format);
    //    if (unpack_log_level >= 3) {
    //        const char* format_name = "未知";
    //        switch (info.format) {
    //        case DDSKTX_FORMAT_BC7: format_name = "BC7"; break;
    //        case DDSKTX_FORMAT_BC3: format_name = "BC3"; break;
    //        case DDSKTX_FORMAT_ETC2A: format_name = "ETC2_RGBA8"; break;
    //        case DDSKTX_FORMAT_ASTC4x4: format_name = "ASTC_4x4"; break;
    //        }
    //        printf("  格式: %s\n", format_name);
    //    }
    //}
    ////输出
    //fs::path input_path(in_put);
    //std::string stem = input_path.stem().string();
    //if (input_path.extension() == ".moe") {
    //    size_t pos = stem.rfind('.');
    //    if (pos != std::string::npos) {
    //        stem = stem.substr(0, pos);
    //    }
    //}
    //fs::path output_dir(out_put);
    //if (output_dir.empty()) {
    //    output_dir = input_path.parent_path();
    //}
    //if (!output_dir.empty() && !fs::exists(output_dir)) {
    //    if (unpack_log_level >= 2) {
    //        printf("创建输出目录: %s\n", output_dir.string().c_str());
    //    }
    //    if (!fs::create_directories(output_dir)) {
    //        throw std::runtime_error("解包失败：无法创建输出目录 - " + output_dir.string());
    //    }
    //}
    //fs::path output_path = output_dir / (stem + ".ktx2");
    //// 写入KTX文件
    //if (unpack_log_level >= 1) {
    //    printf("开始写入KTX文件: %s\n", output_path.string().c_str());
    //}
    //std::ofstream ktx_file(output_path, std::ios::binary);
    //if (!ktx_file) {
    //    throw std::runtime_error("解包失败：无法创建输出文件 - " + output_path.string());
    //}
    //ktx_file.write(reinterpret_cast<const char*>(current_data.get()), current_size);
    //ktx_file.close();
    //if (!ktx_file) {
    //    throw std::runtime_error("解包失败：写入KTX文件失败");
    //}
    //// 最终日志
    //if (unpack_log_level >= 1) {
    //    printf("解包成功完成！\n");
    //    printf("  输入文件: %s\n", in_put.c_str());
    //    printf("  输出文件: %s\n", output_path.string().c_str());
    //    printf("  纹理尺寸: %dx%d\n", info.width, info.height);
    //    printf("  输出大小: %d 字节\n", current_size);
    //    if (unpack_log_level >= 2) {
    //        printf("  处理步骤: ");
    //        if (moe_header.encrypted_on) printf("解密→");
    //        if (moe_header.ztsd_on) printf("解压→");
    //        printf("KTX验证→文件写入\n");
    //    }
    //}
    //return "SUCCESS";

    if (unpack_log_level >= 1) {
        printf("开始验证KTX数据(使用官方KTX库)...\n");
    }
    // 使用ktxTexture2_CreateFromMemory验证KTX数据
    ktxTexture2* texture = nullptr;
    ktxResult result = ktxTexture2_CreateFromMemory(
        current_data.get(),
        current_size,
        KTX_TEXTURE_CREATE_NO_FLAGS, // 不加载图像数据，只验证头部
        &texture
    );
    if (result != KTX_SUCCESS || texture == nullptr) {
        // 提供详细的错误信息
        const char* error_str = ktxErrorString(result);
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg),
            "解包失败：无效的KTX2数据格式 (错误码: %d, 错误信息: %s)",
            result, error_str ? error_str : "未知错误");
        throw std::runtime_error(error_msg);
    }

    // 从KTX纹理对象中获取信息
    uint32_t width = texture->baseWidth;
    uint32_t height = texture->baseHeight;
    uint32_t mipLevels = texture->numLevels;
    uint32_t format = texture->vkFormat;
    uint32_t supercompression = texture->supercompressionScheme;

    // 验证必要的纹理信息
    if (width == 0 || height == 0) {
        ktxTexture2_Destroy(texture);
        throw std::runtime_error("解包失败：KTX纹理尺寸无效");
    }

    if (unpack_log_level >= 2) {
        printf("KTX数据验证成功：\n");
        printf("  尺寸: %dx%d\n", width, height);
        printf("  Mip级别: %d\n", mipLevels);
        printf("  vkFormat: 0x%X\n", format);
        printf("  超级压缩方案: %u\n", supercompression);

        // 提供格式友好名称
        if (unpack_log_level >= 3) {
            const char* format_name = "未知";
            switch (format) {
            case 145: format_name = "VK_FORMAT_BC7_UNORM_BLOCK (BC7)"; break;
            case 131: format_name = "VK_FORMAT_BC3_UNORM_BLOCK (BC3/DXT5)"; break;
            case 147: format_name = "VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK (ETC2 RGBA8)"; break;
            case 181: format_name = "VK_FORMAT_ASTC_8x8_UNORM_BLOCK (ASTC 8x8)"; break;
            case 37: format_name = "VK_FORMAT_R8G8B8A8_UNORM (RGBA8)"; break;
            }
            printf("  格式: %s\n", format_name);

            // 超级压缩方案说明
            const char* supercomp_name = "未知";
            switch (supercompression) {
            case 0: supercomp_name = "无"; break;
            case 1: supercomp_name = "BasisLZ/ETC1S"; break;
            case 2: supercomp_name = "Zstandard"; break;
            case 3: supercomp_name = "ZLIB"; break;
            case 104: supercomp_name = "Basis Universal (UASTC)"; break;
            }
            printf("  超级压缩: %s\n", supercomp_name);
        }
    }
    // 销毁纹理对象（我们只需要验证，不需要保留）
    ktxTexture2_Destroy(texture);
    texture = nullptr;
#endif // MOE_UNPACK_CHECK_DATA


    fs::path input_path(in_put);
    std::string stem = input_path.stem().string();
    if (input_path.extension() == ".moe") {
        size_t pos = stem.rfind('.');
        if (pos != std::string::npos) {
            stem = stem.substr(0, pos);
        }
    }
    fs::path output_dir(out_put);
    if (output_dir.empty()) {
        output_dir = input_path.parent_path();
    }
    if (!output_dir.empty() && !fs::exists(output_dir)) {
        if (unpack_log_level >= 2) {
            printf("创建输出目录: %s\n", output_dir.string().c_str());
        }
        if (!fs::create_directories(output_dir)) {
            throw std::runtime_error("解包失败：无法创建输出目录 - " + output_dir.string());
        }
    }
    fs::path output_path = output_dir / (stem + ".ktx2");
    // 写入KTX文件
    if (unpack_log_level >= 1) {
        printf("开始写入KTX文件: %s\n", output_path.string().c_str());
    }
    std::ofstream ktx_file(output_path, std::ios::binary);
    if (!ktx_file) {
        throw std::runtime_error("解包失败：无法创建输出文件 - " + output_path.string());
    }
    ktx_file.write(reinterpret_cast<const char*>(current_data.get()), current_size);
    ktx_file.close();
    if (!ktx_file) {
        throw std::runtime_error("解包失败：写入KTX文件失败");
    }
    if (unpack_log_level >= 1) {
        printf("解包成功完成！\n");
        printf("  输入文件: %s\n", in_put.c_str());
        printf("  输出文件: %s\n", output_path.string().c_str());
#ifdef MOE_UNPACK_CHECK_DATA
        printf("  纹理尺寸: %dx%d\n", width, height);
#endif
        printf("  输出大小: %d 字节\n", current_size);
        if (unpack_log_level >= 2) {
            printf("  处理步骤: ");
            if (moe_header.encrypted_on) printf("解密→");
            if (moe_header.ztsd_on) printf("解压→");
            printf("KTX验证→文件写入\n");
        }
    }
    return "SUCCESS";
}

#endif // MOE_UNPACK_IMPLEMENTATION