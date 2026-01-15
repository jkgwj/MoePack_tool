#include "sodium.h"
#include "MoePack.h"



MoePack::MoePack()
{
}
MoePack::~MoePack()
{
}

void MoePack::ztsd_compression(bool enable, int level)
{
    pack_params.ztsd_on = enable;
    pack_params.ztsd_level = level;
}
void MoePack::encryption(bool enable, const std::string key)
{
    pack_params.is_encryption = enable;
    pack_params.encryption_key = key;
}

UniquePtr_uChar MoePack::_load_image_to_src_format(const std::string& image_path, int& h, int& w) {
    //  图片路径不能为空
    if (image_path.empty()) {
        throw std::runtime_error("解码图片失败：路径为空");
    }

    //  重置输出参数
    h = 0;
    w = 0;

    //  根据 src_image_format 获取 stb_image 对应的格式参数
    int stb_format = _moe_format_to_stb_format(pack_params.src_image_format);
    if (stb_format == -1) {
        throw std::runtime_error("解码图片失败：不支持的图片格式 " + (int)pack_params.src_image_format);
    }

    // 接收 stbi_load 返回的原始通道数
    int original_channels = 0;

    // 调用 stbi_load 加载图片并转换为指定格式
    // stbi_load 参数说明：&w(宽度)、&h(高度)、&original_channels(原始通道)、stb_format(目标格式)
    unsigned char* raw_image_data = stbi_load(
        image_path.c_str(),  // 图片路径
        &w,                  // 输出：宽度
        &h,                  // 输出：高度
        &original_channels,  // 输出：原始通道数
        stb_format           // 目标格式
    );

    // 错误处理：加载失败则抛异常
    if (!raw_image_data) {
        std::string error_msg = "图片解码失败：" + image_path + " | 原因：" + stbi_failure_reason();
        throw std::runtime_error(error_msg);
    }

    // 校验图片尺寸：宽高不能为0
    if (w <= 0 || h <= 0) {
        free(raw_image_data); // 释放资源
        throw std::runtime_error("解码图片失败：" + image_path + " | 无效尺寸：" + std::to_string(w) + "x" + std::to_string(h));
    }

    // 将裸指针封装为智能指针，移交所有权
    UniquePtr_uChar image_data(raw_image_data);

    // 打印解码日志
    if(log_level>=3)printf("图片解码成功：%s | 尺寸：%dx%d | 格式：%d\n",
        image_path.c_str(), w, h, (int)pack_params.src_image_format);
    return image_data;
}

UniquePtr_uChar MoePack::_src_format_to_ktx(const unsigned char* src_data, int& h, int& w, int& size) {
    // 校验数据
    if (!src_data) {
        throw std::runtime_error("转换 KTX 失败：源数据裸指针为空");
    }
    if (w <= 0 || h <= 0) {
        throw std::runtime_error("转换 KTX 失败：无效尺寸 " + std::to_string(w) + "x" + std::to_string(h));
    }

    // 初始化 KTX 创建参数
    ktxTexture2* ktx2_texture = nullptr;
    ktxTextureCreateInfo create_info = {}; // 清空结构体，避免脏值

    // 确定目标 VK 格式
    VkFormat target_format = VK_FORMAT_UNDEFINED;
    // 优先保留用户指定的GPU压缩格式，否则用目标平台默认
    if (pack_params.texture_format != VK_FORMAT_UNDEFINED) {
        target_format = pack_params.texture_format;
    }
    else {
        target_format = GetGpuCompressionFormat(dst_platform);
    }

    // 检查目标格式是否有效
    if (target_format == VK_FORMAT_UNDEFINED) {
        throw std::runtime_error("转换 KTX 失败：目标 VK 格式未定义，请检查平台设置");
    }

    if (log_level >= 3) {
        std::cout << "目标 VK 格式: 0x" << std::hex << target_format << std::dec << std::endl;
    }

    // 填充创建信息
    // 注意：先以 RGBA8888 格式创建，后续再压缩为 GPU 格式
    create_info.vkFormat = VK_FORMAT_R8G8B8A8_UNORM;        // 临时使用未压缩格式，压缩后会转换
    create_info.baseWidth = static_cast<uint32_t>(w);       // 宽度
    create_info.baseHeight = static_cast<uint32_t>(h);      // 高度
    create_info.baseDepth = 1;                              // 深度（2D纹理固定为1）
    create_info.numDimensions = pack_params.is_2D ? 2 : 3;  // 2D/3D 纹理（默认2D）
    create_info.numLevels = 1;                              // MIP暂时不支持，强制设为1级
    create_info.numLayers = 1;                              // 单层纹理
    create_info.numFaces = 1;                               // 非立方体贴图
    create_info.isArray = KTX_FALSE;                        // 非数组纹理
    create_info.generateMipmaps = KTX_FALSE;                // MIP暂时不支持，强制关闭自动生成

    // 创建 KTX2 纹理对象
    ktxResult ktx_ret = ktxTexture2_Create(
        &create_info,
        KTX_TEXTURE_CREATE_ALLOC_STORAGE,
        &ktx2_texture
    );
    
    if (ktx_ret != KTX_SUCCESS) {
        std::string err_msg = "转换 KTX 失败：创建 KTX2 对象失败 | 错误码："
            + std::string(ktxErrorString(ktx_ret))
            + " | VK 格式：0x" + std::to_string((int)target_format);
        throw std::runtime_error(err_msg);
    }

    // 计算源数据大小
    size_t src_data_size = _calc_src_data_size(w, h);
    if (src_data_size == 0) {
        ktxTexture2_Destroy(ktx2_texture);
        throw std::runtime_error("转换 KTX 失败：源数据大小计算为 0（源格式：" + std::to_string((int)pack_params.src_image_format) + "）");
    }

    // 写入源数据到 KTX2 的 Level 0 层级
    ktx_ret = ktxTexture_SetImageFromMemory(
        ktxTexture(ktx2_texture),
        0,          // MIP Level 0
        0,          // 图层索引
        0,          // 立方体贴图面索引
        src_data,   // 源数据裸指针
        src_data_size // 源数据总大小
    );
    
    if (ktx_ret != KTX_SUCCESS) {
        std::string err_msg = "转换 KTX 失败：写入源数据到 KTX2 失败 | 错误码："
            + std::string(ktxErrorString(ktx_ret));
        ktxTexture2_Destroy(ktx2_texture);
        throw std::runtime_error(err_msg);
    }

    // 使用 Basis Universal 进行 GPU 纹理压缩
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

    // 导出 KTX2 数据到内存
    unsigned char* ktx_raw_data = nullptr;
    size_t ktx_data_size = 0;
    ktx_ret = ktxTexture_WriteToMemory(
        ktxTexture(ktx2_texture),
        &ktx_raw_data,
        &ktx_data_size
    );
    
    // 销毁 KTX2 对象
    ktxTexture2_Destroy(ktx2_texture);

    // 数据大小校验，避免返回空的纹理数据
    if (ktx_ret != KTX_SUCCESS) {
        if (ktx_raw_data) {
            free(ktx_raw_data);
        }
        std::string err_msg = "转换 KTX 失败：导出 KTX2 内存数据失败 | 错误码："
            + std::string(ktxErrorString(ktx_ret));
        throw std::runtime_error(err_msg);
    }

    if (!ktx_raw_data || ktx_data_size == 0) {
        if (ktx_raw_data) {
            free(ktx_raw_data);
        }
        throw std::runtime_error("转换 KTX 失败：导出的 KTX2 数据为空");
    }

    // 封装为智能指针返回
    UniquePtr_uChar ktx_data(ktx_raw_data);
    size = static_cast<int>(ktx_data_size);
    
    // 获取格式名称用于日志
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
            w, h,
            pack_params.is_2D ? "2D" : "3D",
            1,
            format_name.c_str(),
            (int)target_format,
            (int)pack_params.src_image_format,
            ktx_data_size);
    }

    return ktx_data;
}

UniquePtr_uChar MoePack::_ztsd_compression(const unsigned char* ktx_data, int& size) {
    // 校验
    if (!ktx_data) {
        throw std::runtime_error("ZSTD 压缩失败：KTX 数据指针为空");
    }
    if (size <= 0) {
        throw std::runtime_error("ZSTD 压缩失败：无效的 KTX 数据大小 [" + std::to_string(size) + "]");
    }

    // 保存原始大小用于日志
    int original_size = size;

    // 计算压缩缓冲区最大所需大小
    size_t max_compressed_size = ZSTD_compressBound(static_cast<size_t>(size));
    if (max_compressed_size == 0) {
        throw std::runtime_error("ZSTD 压缩失败：计算最大压缩缓冲区大小失败");
    }

    // 分配压缩输出缓冲区
    unsigned char* compressed_data = static_cast<unsigned char*>(malloc(max_compressed_size));
    if (!compressed_data) {
        throw std::runtime_error("ZSTD 压缩失败：分配压缩缓冲区内存失败（需要 " +
            std::to_string(max_compressed_size) + " 字节）");
    }

    // 执行 ZSTD 压缩
    size_t compressed_size = ZSTD_compress(
        compressed_data,        // 输出缓冲区
        max_compressed_size,    // 输出缓冲区大小
        ktx_data,               // 输入数据
        static_cast<size_t>(size), // 输入数据大小
        pack_params.ztsd_level  // 压缩等级
    );

    // 校验压缩结果
    if (ZSTD_isError(compressed_size)) {
        free(compressed_data);
        std::string err_msg = "ZSTD 压缩失败：" + std::string(ZSTD_getErrorName(compressed_size));
        throw std::runtime_error(err_msg);
    }

    // 更新 size 为压缩后大小
    size = static_cast<int>(compressed_size);
    if (size <= 0) {
        free(compressed_data);
        throw std::runtime_error("ZSTD 压缩失败：压缩后数据大小为 0");
    }

    // 封装为智能指针返回
    UniquePtr_uChar compressed_ptr(compressed_data);

    if (log_level >= 3) {
        printf("ZSTD 压缩成功：\n"
            "  原始大小：%d 字节 | 压缩后大小：%d 字节 | 压缩等级：%d\n"
            "  压缩比：%.2f:1\n",
            original_size,
            size,
            pack_params.ztsd_level,
            static_cast<float>(original_size) / static_cast<float>(size));
    }

    return compressed_ptr;
}

UniquePtr_uChar MoePack::_encrypt(unsigned char* data, int& size, std::string key) {
    // 初始化 libsodium
    if (sodium_init() == -1) {
        throw std::runtime_error("加密失败：libsodium 初始化失败");
    }

    // 校验 
    if (!data) {
        throw std::runtime_error("加密失败：待加密数据指针为空");
    }
    if (size <= 0) {
        throw std::runtime_error("加密失败：待加密数据长度无效");
    }

    // 保存原始大小用于日志
    int original_size = size;

    // 优先使用 pack_params 中配置的密钥
    if (key.empty()) {
        key = pack_params.encryption_key;
        if (key.empty()) {
            throw std::runtime_error("加密失败：密钥为空");
        }
    }

    // 密钥派生
    unsigned char crypto_key[crypto_secretbox_KEYBYTES];
    unsigned char salt[crypto_pwhash_SALTBYTES];
    randombytes_buf(salt, sizeof(salt));

    // 使用 Argon2id 算法派生密钥
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
        throw std::runtime_error("加密失败：密钥派生失败");
    }

    // 生成随机 IV（24字节）
    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    randombytes_buf(nonce, sizeof(nonce));

    // 计算加密后数据总长度 
    size_t encrypted_content_len = static_cast<size_t>(size) + crypto_secretbox_MACBYTES;
    size_t total_encrypted_len = sizeof(nonce) + sizeof(salt) + encrypted_content_len;

    // 分配加密结果缓冲区
    unsigned char* encrypted_buffer = static_cast<unsigned char*>(malloc(total_encrypted_len));
    if (!encrypted_buffer) {
        sodium_memzero(crypto_key, sizeof(crypto_key));
        throw std::runtime_error("加密失败：分配加密缓冲区内存不足");
    }

    // 核心加密操作
    unsigned char* encrypted_content = encrypted_buffer + sizeof(nonce) + sizeof(salt);
    crypto_secretbox_easy(
        encrypted_content,
        data,
        static_cast<unsigned long long>(size),
        nonce,
        crypto_key
    );

    // 拼接 IV + 盐值 + 加密内容
    memcpy(encrypted_buffer, nonce, sizeof(nonce));
    memcpy(encrypted_buffer + sizeof(nonce), salt, sizeof(salt));

    // 安全清理敏感数据
    sodium_memzero(crypto_key, sizeof(crypto_key));
    sodium_memzero(nonce, sizeof(nonce));
    sodium_memzero(salt, sizeof(salt));

    // 更新 size 为加密后的总大小
    size = static_cast<int>(total_encrypted_len);

    if (log_level >= 3) {
        printf("加密成功：\n"
            "  原始数据长度：%d 字节 | 加密后总长度：%d 字节 | 加密算法：XSalsa20-Poly1305\n"
            "  加密内容结构：IV(24字节) + 盐值(16字节) + 加密数据(%zu字节 + 16字节MAC)\n",
            original_size,
            size,
            static_cast<size_t>(original_size));
    }

    return UniquePtr_uChar(encrypted_buffer);
}

std::string MoePack::pack(std::string in_path, const std::string out_path) {
    vector<std::string> in_srcdata_paths; // 输入的图片路径列表
    vector<std::string> in_srcdata_names; // 输入的图片文件名列表(保存时用)保存时后缀名为.moe
    int64_t src_data_size = 0; // 源数据大小
    int64_t packed_data_size = 0; // 封包后数据大小
    std::wstring out_file_path; // 输出文件夹路径(宽字符)
    pack_params.texture_format = GetGpuCompressionFormat(dst_platform); // 设置目标纹理格式为目标平台默认

    // 确定输出目录路径
    if (!is_directory_path(out_path, 1)) {
        out_file_path = GetParentDirFromPath(out_path);
    }
    else {
        out_file_path = utf8ToWstring(out_path);
    }

    // 检测输入是否是文件夹路径
    if (is_directory_path(in_path)) {
        GetFileDirEx_NoExtension(in_path, in_srcdata_paths, in_srcdata_names);
        if (log_level >= 2) cout << "检测到输入路径为文件夹，自动获取符合格式的图片\n";
    }
    else {
        in_srcdata_paths.push_back(in_path);
        std::string processed_path = wstringToUtf8(RemoveFileExtension(utf8ToWstring(in_path)));
        size_t last_sep_pos = processed_path.find_last_of("\\/"); 
        if (last_sep_pos != std::string::npos) {
            processed_path = processed_path.substr(last_sep_pos + 1);
        }
        in_srcdata_names.push_back(processed_path);
    }

    if (log_level >= 0) cout << "共计 " << in_srcdata_paths.size() << " 个文件待处理。（正在工作中，请不要关闭程序）\n";

    // 检查输出目录是否存在，不存在则创建
    if (!fs::exists(wstringToUtf8(out_file_path))) {
        if (!fs::create_directories(wstringToUtf8(out_file_path))) {
            throw std::runtime_error("创建输出目录失败：" + wstringToUtf8(out_file_path));
        }
        if (log_level >= 2) cout << "已创建输出目录：" << wstringToUtf8(out_file_path) << endl;
    }

    // 开始封包处理
    for (size_t i = 0; i < in_srcdata_paths.size(); ++i) {
        std::string path = in_srcdata_paths[i]; // 当前处理的图片路径
        int size = 0; // 当前数据大小，会在每个处理步骤中更新
        UniquePtr_uChar out_data; // 当前处理的数据指针

        if (path.empty()) {
            cout << "警告：路径为空，跳过处理。\n";
            continue;
        }

        // 检查输入文件是否存在
        if (!fs::exists(path)) {
            cout << "警告：文件不存在，跳过处理：" << path << endl;
            continue;
        }

        try {
            // 获取源文件大小
            if (log_level >= 2) {
                src_data_size = fs::file_size(path);
            }

            // 解码图片
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

            // 转换为KTX2格式
            if (log_level >= 1) {
                cout << "     开始转换为KTX2格式(请耐心等待)";
                std::flush(cout);
            }
            out_data = _src_format_to_ktx(out_data.get(), img_h, img_w, size);
            if (log_level >= 1) {
                cout << "-> 转换成功 [" << size << "字节]" << endl;
            }

            // ZSTD压缩（如果启用）
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

            // 加密（如果启用）
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

            // 创建 MoeHeader 文件头
            MoeHeader moe_header;

            // 设置 MoeHeader 参数
            moe_header.ztsd_on = pack_params.ztsd_on ? 1 : 0;
            moe_header.encrypted_on = pack_params.is_encryption ? 1 : 0;
            moe_header.set_data_size(static_cast<uint64_t>(size));

            // 计算校验数据（使用SHA256哈希）
            if (size > 0 && out_data.get()) {
                // 计算SHA256哈希值
                unsigned char hash[crypto_hash_sha256_BYTES];
                crypto_hash_sha256(hash, out_data.get(), size);

                // 设置校验数据到头部
                moe_header.set_check_data(hash, crypto_hash_sha256_BYTES);
            }

            // 将头部字段转为大端字节序
            moe_header.to_big_endian();

            // 准备输出文件路径
            std::wstring out_file_full_path = out_file_path + L"/" +
                utf8ToWstring(in_srcdata_names[i]) + L".moe";

            // 检查文件是否已存在
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
                out_file_full_path,
                out_data.get(),
                static_cast<size_t>(size),
                &moe_header 
            );

            if (!write_success) {
                throw std::runtime_error("写入文件失败：" + wstringToUtf8(out_file_full_path));
            }

            if (log_level >= 1) {
                cout << "-> 写入成功" << endl;
            }

            moe_header.from_big_endian();

            // 记录处理结果和统计信息
            if (log_level >= 1) {
                packed_data_size = fs::file_size(wstringToUtf8(out_file_full_path));

                cout << "->封包完成：" << endl;
                cout << "    输出文件: " << in_srcdata_names[i] << ".moe" << endl;
                cout << "    输出大小: " << packed_data_size << " 字节" << endl;
                cout << "    头部大小: " << sizeof(MoeHeader) << " 字节" << endl;
                cout << "    数据大小: " << size << " 字节" << endl;
                
                if (log_level >= 2 && src_data_size > 0) {
                    // 计算压缩率：封包后总大小 / 原始文件大小
                    float ratio = static_cast<float>(packed_data_size) / static_cast<float>(src_data_size);
                    cout << "    原始大小: " << src_data_size << " 字节" << endl;
                    cout << "    压缩率: " << std::fixed << std::setprecision(2) << ratio << endl;
                }

                // 输出处理步骤
                if (log_level >= 3) {
                    cout << "    处理步骤: 解码→KTX2转换";
                    if (pack_params.ztsd_on) cout << "→ZSTD压缩";
                    if (pack_params.is_encryption) cout << "→加密";
                    cout << "→写入文件" << endl;
                }
            }

            // 清理数据
            out_data.reset();

        }
        catch (const std::exception& e) {
            // 异常处理：记录错误并继续处理下一个文件
            cerr << "错误：处理文件 " << path << " 时发生异常：" << e.what() << endl;
            if (log_level >= 1) {
                cout << "  文件 " << fs::path(path).filename().string() << " 处理失败，已跳过" << endl;
            }
            continue; // 跳过当前文件，继续处理下一个
        }

        // 显示进度
        if (log_level >= 1) {
            cout << "----------------------------------------" << endl;
            cout << "已完成 " << (i + 1) << " / " << in_srcdata_paths.size()
                << " 个文件的封包。" << endl;
            cout << "----------------------------------------" << endl;
        }
    }

    // 最终统计
    if (log_level >= 0) {
        if (in_srcdata_paths.size() > 0) {
            cout << endl << " 已完成所有文件的封包处理。" << endl;
            cout << "  总计处理: " << in_srcdata_paths.size() << " 个文件" << endl;
            cout << "  输出目录: " << wstringToUtf8(out_file_path) << endl;

            // 输出格式信息
            if (log_level >= 2) {
                cout << "  文件格式: .moe (MOE_ARC格式)" << endl;
                cout << "  头部大小: " << sizeof(MoeHeader) << " 字节" << endl;
                cout << "  版本号: " << MOE_PackVersion << endl;
            }
        }
        else {
            cout << "警告：没有找到可处理的文件。" << endl;
        }
    }

    return "SUCCESS";
}

size_t MoePack::_calc_src_data_size(int width, int height)
{
    // 校验宽高
    if (width <= 0 || height <= 0) {
        throw std::runtime_error(
            "计算源数据大小失败：无效尺寸 [" + std::to_string(width) + "x" + std::to_string(height) + "]"
        );
    }

    // 根据 MOE_ImageFormat 枚举，匹配单像素字节数
    size_t pixel_byte_size = 0;
    switch (pack_params.src_image_format)
    {
    case RGB888:      // RGB无透明：3字节/像素（R8+G8+B8）
        pixel_byte_size = 3;
        break;

    case RGBA8888:    // RGBA带透明：4字节/像素（R8+G8+B8+A8）
        pixel_byte_size = 4;
        break;

    case L8:          // 单通道灰度：1字节/像素（L8）
        pixel_byte_size = 1;
        break;

    case LA88:        // 灰度+Alpha：2字节/像素（L8+A8）
        pixel_byte_size = 2;
        break;

    case RGBA32F:     // RGBA 32位浮点数：16字节/像素（R32F+G32F+B32F+A32F）
        pixel_byte_size = 16; // 4通道 × 4字节（float）= 16字节
        break;

    default:          //默认按 RGBA8888 计算
        pixel_byte_size = 4;
        printf("警告：未识别的源图像格式（枚举值：%d），默认按 RGBA8888 计算\n",
            (int)pack_params.src_image_format);
        break;
    }

    // 计算总字节数
    size_t total_size = static_cast<size_t>(width) * static_cast<size_t>(height) * pixel_byte_size;

    // 错误信息输出
    if (total_size == 0) {
        throw std::runtime_error(
            "计算源数据大小失败：总字节数为0（格式：" + std::to_string((int)pack_params.src_image_format) + "）"
        );
    }

    return total_size;
}

void MoePack::_set_basis_compression_params(CompressionParams& params, VkFormat target_format) {
    // 重置参数为默认值
    params = CompressionParams();

    // 根据目标格式设置优化标志
    switch (target_format) {
    case VK_FORMAT_BC7_UNORM_BLOCK:       
        // 针对BC7优化
        params.uastcFlags = KTX_PACK_UASTC_LEVEL_DEFAULT | KTX_PACK_UASTC_FAVOR_BC7_ERROR;
        if (log_level >= 3) {
            std::cout << "  压缩参数：目标格式 BC7，优化 BC7 错误率" << std::endl;
        }
        break;

    case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK: 
        // 针对ETC2优化
        params.uastcFlags = KTX_PACK_UASTC_LEVEL_DEFAULT | KTX_PACK_UASTC_ETC1_FASTER_HINTS;
        if (log_level >= 3) {
            std::cout << "  压缩参数：目标格式 ETC2，优化 ETC1 转换速度" << std::endl;
        }
        break;

    case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:    
    case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:    
        // 针对ASTC优化
        params.uastcFlags = KTX_PACK_UASTC_LEVEL_DEFAULT;
        if (log_level >= 3) {
            std::cout << "  压缩参数：目标格式 ASTC，使用默认设置" << std::endl;
        }
        break;

    default:
        // 未知格式/暂时不支持格式，使用默认参数
        params.uastcFlags = KTX_PACK_UASTC_LEVEL_DEFAULT;
        if (log_level >= 2) {
            std::cout << "  警告：未知目标格式，使用默认压缩参数" << std::endl;
        }
        break;
    }

    // 根据 ZSTD 压缩级别调整参数（如果有）
    if (pack_params.ztsd_on) {
        int zstd_level = pack_params.ztsd_level;

        // 调整 UASTC 压缩级别
        ktx_pack_uastc_flags uastc_level = KTX_PACK_UASTC_LEVEL_DEFAULT;
        if (zstd_level <= 3) {
            uastc_level = KTX_PACK_UASTC_LEVEL_FASTEST;
        }
        else if (zstd_level <= 9) {
            uastc_level = KTX_PACK_UASTC_LEVEL_FASTER;
        }
        else if (zstd_level <= 15) {
            uastc_level = KTX_PACK_UASTC_LEVEL_DEFAULT;
        }
        else if (zstd_level <= 18) {
            uastc_level = KTX_PACK_UASTC_LEVEL_SLOWER;
        }
        else {
            uastc_level = KTX_PACK_UASTC_LEVEL_VERYSLOW;
        }

        // 保留其他标志位，只更新级别
        params.uastcFlags = (params.uastcFlags & ~KTX_PACK_UASTC_LEVEL_MASK) | uastc_level;

        // 对于高压缩级别，启用UASTC RDO以进一步减小文件大小
        if (zstd_level > 15) {
            params.uastcRDO = KTX_TRUE;
            // 简单映射：zstd级别越高，RDO质量标量越大（压缩率更高，质量略低）
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
    // 记录最终参数
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

    // 设置 Basis 压缩参数
    CompressionParams basis_params;
    _set_basis_compression_params(basis_params, target_format);

    // 创建并初始化 ktxBasisParams
    ktxBasisParams ktx_params = {};
    ktx_params.structSize = sizeof(ktxBasisParams);

    // 只设置我们关心的参数，其他使用默认值
    ktx_params.uastc = basis_params.uastc;
    ktx_params.threadCount = basis_params.threadCount;
    ktx_params.uastcFlags = basis_params.uastcFlags;
    ktx_params.uastcRDO = basis_params.uastcRDO;
    ktx_params.uastcRDOQualityScalar = basis_params.uastcRDOQualityScalar;


    // 设置其他常用参数的默认值
    ktx_params.normalMap = KTX_FALSE;           // 不是法线贴图
    ktx_params.preSwizzle = KTX_FALSE;          // 不预交换通道
    ktx_params.noEndpointRDO = KTX_FALSE;       // 启用端点RDO
    ktx_params.noSelectorRDO = KTX_FALSE;       // 启用选择器RDO

    // 对于UASTC RDO，设置一些合理的默认值
    if (ktx_params.uastcRDO) {
        ktx_params.uastcRDODictSize = 4096;               // 默认字典大小
        ktx_params.uastcRDOMaxSmoothBlockErrorScale = 10.0f;  // 默认平滑块错误比例
        ktx_params.uastcRDOMaxSmoothBlockStdDev = 18.0f;  // 默认平滑块标准差
        ktx_params.uastcRDODontFavorSimplerModes = KTX_FALSE; // 允许偏爱简单模式
        ktx_params.uastcRDONoMultithreading = KTX_FALSE;  // 启用RDO多线程
    }

    // 调试日志：打印压缩参数
    if (log_level >= 2) {
        std::cout << "Basis Universal 压缩参数：" << std::endl;
        std::cout << "  模式: UASTC" << std::endl;
        std::cout << "  线程数: " << ktx_params.threadCount << std::endl;

        // 显示压缩级别
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

        // 显示优化标志
        if (ktx_params.uastcFlags & KTX_PACK_UASTC_FAVOR_BC7_ERROR) {
            std::cout << "  优化标志: 优化BC7错误率" << std::endl;
        }
        else if (ktx_params.uastcFlags & KTX_PACK_UASTC_ETC1_FASTER_HINTS) {
            std::cout << "  优化标志: 优化ETC1转换速度" << std::endl;
        }

        std::cout << "  UASTC RDO: " << (ktx_params.uastcRDO ? "启用" : "禁用") << std::endl;
        if (ktx_params.uastcRDO) {
            std::cout << "    - 质量标量: " << ktx_params.uastcRDOQualityScalar << std::endl;
        }
    }

    // 执行 Basis 压缩
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
    //执行Basis转码到目标GPU格式 
    if(log_level>=3){
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
