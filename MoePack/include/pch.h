#pragma once
#include <vulkan/vulkan.h>
#include <filesystem>  
#include <io.h>
#include <Windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <utility>

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

namespace fs = std::filesystem;

// 平台枚举
enum MOE_Platform
{
	WINDOWS,
	LINUX,
	MACOS,
	IOS,
	ANDROID,
    NOP
}; 

// 图像格式枚举
enum MOE_ImageFormat
{
	RGB888=0,             //rgb无透明通道
	RGBA8888,             //rgba8888
	L8,                   //单通道8位灰度图
	LA88,                 //8位灰度图+8位Alpha
	RGBA32F               //RGBA 32位浮点数
};

/**
 * @brief 根据平台获取默认的 GPU 压缩纹理格式（Vulkan 符号）
 * @param platform 目标平台枚举
 * @return 对应平台的 Vulkan 压缩格式枚举，默认返回通用的 ETC2 压缩格式
 */
static inline VkFormat GetGpuCompressionFormat(MOE_Platform platform)
{
    switch (platform)
    {
    case WINDOWS:
        // Windows：BC7
        return VK_FORMAT_BC7_UNORM_BLOCK;

    case LINUX:
        // Linux：ETC2 RGBA8
        return VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;

    case MACOS:
        // macOS：ASTC 8x8
        return VK_FORMAT_ASTC_8x8_UNORM_BLOCK;

    case IOS:
        // iOS：ASTC 6x6
        return VK_FORMAT_ASTC_6x6_UNORM_BLOCK;

    case ANDROID:
        // Android：ETC2 RGBA8
        return VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;

    default:
        // 确保兼容性
        return VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
    }
}
/**
 * @brief 将 Vulkan 压缩格式（VkFormat）转换为 KTX 转码格式（ktx_transcode_fmt_e）
 * @param vkFmt Vulkan 格式枚举（来自 GetGpuCompressionFormat 的返回值）
 * @return 匹配的 KTX 转码格式，默认返回 KTX_TTF_NOSELECTION
 */
static inline ktx_transcode_fmt_e VkFormatToKtxTranscodeFmt(VkFormat vkFmt)
{
    switch (vkFmt)
    {
        // Windows：BC7 → KTX_TTF_BC7_RGBA
    case VK_FORMAT_BC7_UNORM_BLOCK:
        return KTX_TTF_BC7_RGBA;

        // Linux/Android：ETC2 RGBA8 → KTX_TTF_ETC2_RGBA
    case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
        return KTX_TTF_ETC2_RGBA; 


    // macOS：ASTC 8x8 → KTX_TTF_ASTC_4x4_RGBA
    case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
        return KTX_TTF_ASTC_4x4_RGBA;

        // iOS：ASTC 6x6 → KTX_TTF_ASTC_4x4_RGBA
    case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
        return KTX_TTF_ASTC_4x4_RGBA;

    default:
        return KTX_TTF_NOSELECTION;
    }
}

//路径预处理辅助函数
using namespace std;

// UTF-8转换函数
inline static std::wstring utf8ToWstring(const string & utf8Str) {
            if (utf8Str.empty()) return L"";
            int len = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, nullptr, 0);
            wchar_t* buffer = new wchar_t[len];
            MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, buffer, len);
            wstring wstr(buffer);
            delete[] buffer;
            return wstr;
        }
inline static std::string wstringToUtf8(const wstring & wstr) {
            if (wstr.empty()) return "";
            int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
            char* buffer = new char[len];
            WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, buffer, len, nullptr, nullptr);
            string utf8Str(buffer);
            delete[] buffer;
            return utf8Str;
        }
// 移除宽字符文件名的扩展名（只去掉最后一个后缀）
inline static std::wstring RemoveFileExtension(const std::wstring& fileNameW) {
    // 找到最后一个 '.' 的位置
    size_t dotPos = fileNameW.find_last_of(L'.');
    if (dotPos == std::wstring::npos || dotPos == 0) {
        return fileNameW; // 无后缀/隐藏文件，返回原名称
    }
    // 截取
    return fileNameW.substr(0, dotPos);
}
//获取目录下指定类型文件列表
inline static void GetFileDirEx(string & path, vector<string>&files, vector<string>&fileName, string fileType = "")
        {
            intptr_t hFile = 0;
            struct _wfinddata_t fileInfoW;
            wstring wSearchPath = utf8ToWstring(path); // UTF-8转宽字符

            if (!wSearchPath.empty() && wSearchPath.back() != L'\\' && wSearchPath.back() != L'/') {
                wSearchPath += L"\\";
            }
            wSearchPath += L"*";
            if (!fileType.empty()) {
                wSearchPath += utf8ToWstring(fileType);
            }

            if ((hFile = _wfindfirst(wSearchPath.c_str(), &fileInfoW)) != -1) {
                do {
                    wstring fileNameW = fileInfoW.name;
                    if (fileNameW == L"." || fileNameW == L"..") continue; // 过滤无效目录
                    if (!(fileInfoW.attrib & _A_SUBDIR)) { // 只保留文件
                        // 拼接完整路径
                        wstring wFullPath = utf8ToWstring(path);
                        if (!wFullPath.empty() && wFullPath.back() != L'\\' && wFullPath.back() != L'/') {
                            wFullPath += L"\\";
                        }
                        wFullPath += fileNameW;
                        // 宽字符转UTF-8，适配控制台输出
                        files.push_back(wstringToUtf8(wFullPath));
                        fileName.push_back(wstringToUtf8(fileNameW));
                    }
                } while (_wfindnext(hFile, &fileInfoW) == 0);
                _findclose(hFile);
            }
        }
inline static void GetFileDirEx_NoExtension(string& path, vector<string>& files, vector<string>& fileName, string fileType = "") {
    intptr_t hFile = 0;
    struct _wfinddata_t fileInfoW;
    std::wstring wSearchPath = utf8ToWstring(path); // UTF-8转宽字符

    if (!wSearchPath.empty() && wSearchPath.back() != L'\\' && wSearchPath.back() != L'/') {
        wSearchPath += L"\\";
    }
    wSearchPath += L"*";
    if (!fileType.empty()) {
        wSearchPath += utf8ToWstring(fileType);
    }

    if ((hFile = _wfindfirst(wSearchPath.c_str(), &fileInfoW)) != -1) {
        do {
            std::wstring fileNameW = fileInfoW.name;
            if (fileNameW == L"." || fileNameW == L"..") continue; // 过滤无效目录
            if (!(fileInfoW.attrib & _A_SUBDIR)) { // 只保留文件
                // 拼接完整路径（原逻辑不变）
                std::wstring wFullPath = utf8ToWstring(path);
                if (!wFullPath.empty() && wFullPath.back() != L'\\' && wFullPath.back() != L'/') {
                    wFullPath += L"\\";
                }
                wFullPath += fileNameW;
                files.push_back(wstringToUtf8(wFullPath));

                // 移除文件名后缀后再存入fileName向量
                std::wstring fileNameWithoutExt = RemoveFileExtension(fileNameW);
                fileName.push_back(wstringToUtf8(fileNameWithoutExt));
            }
        } while (_wfindnext(hFile, &fileInfoW) == 0);
        _findclose(hFile);
    }
}
//获取当前路径的目录
inline static std::wstring GetParentDirFromPath(const std::string& FullPath) {
    if (FullPath.empty()) {
        return L"";
    }
    std::wstring wFullPath = utf8ToWstring(FullPath);
    size_t lastSepPos = wFullPath.find_last_of(L"\\/");

    if (lastSepPos == std::wstring::npos) {
        return L".";
    }
    if (lastSepPos == 0) {
        return wFullPath.substr(0, 1);
    }
    if (lastSepPos == wFullPath.length() - 1) {
        return wFullPath.substr(0, wFullPath.length() - 1);
    }
    return wFullPath.substr(0, lastSepPos);
}
// 检测路径是否为文件夹的函数
inline static bool is_directory_path(const std::string& path_str, bool no_exists = false/*忽视存在检查*/) {
    try {
        fs::path target_path = path_str;
        if (!no_exists) {
            if (!fs::exists(target_path)) {
                std::cerr << "错误：路径 \"" << path_str << "\" 不存在！" << std::endl;
                return false;
            }
            return fs::is_directory(target_path);
        }
        std::string trimmed_path = path_str;
        while (!trimmed_path.empty() && (trimmed_path.back() == '\\' || trimmed_path.back() == '/')) {
            trimmed_path.pop_back();
        }
        fs::path trimmed_fs_path = trimmed_path;
        bool has_extension = !trimmed_fs_path.extension().empty();
        bool ends_with_sep = !path_str.empty() && (path_str.back() == '\\' || path_str.back() == '/');

        return ends_with_sep || !has_extension;

    }
    catch (const fs::filesystem_error& e) {
        std::cerr << "文件系统错误：" << e.what() << std::endl;
        return false;
    }
}
// 写入数据
inline static bool write_data_to_dir(const std::wstring& wpath, const unsigned char* data, size_t size) {
    // 宽字符路径转标准filesystem路径
    fs::path fs_path(wpath);

    try {
        fs::create_directories(fs_path.parent_path());
        FILE* fp = nullptr;
#if defined(_WIN32) || defined(_WIN64)
        fp = _wfopen(fs_path.c_str(), L"wb"); // Windows宽字符
#else
        fp = fopen(fs_path.c_str(), "wb");    // Linux/macOS UTF-8窄字符
#endif

        if (!fp) return false;

        // 写入数据+校验完整性
        size_t written = fwrite(data, 1, size, fp);
        fclose(fp);
        return written == size;
    }
    catch (const fs::filesystem_error&) {
        return false;
    }
}
inline static bool write_data_to_dir(const std::wstring& wpath,const unsigned char* data,size_t size,const MoeHeader* header = nullptr) {
    // 宽字符路径转标准filesystem路径
    fs::path fs_path(wpath);

    try {
        fs::create_directories(fs_path.parent_path());

        // 打开文件进行写入
        FILE* fp = nullptr;
#if defined(_WIN32) || defined(_WIN64)
        fp = _wfopen(fs_path.c_str(), L"wb"); // Windows宽字符
#else
        fp = fopen(fs_path.c_str(), "wb");    // Linux/macOS UTF-8窄字符
#endif

        if (!fp) {
            return false;
        }

        bool success = true;

        // 如果有头部，先写入头部
        if (header) {
            size_t header_size = sizeof(MoeHeader);
            size_t written_header = fwrite(header, 1, header_size, fp);
            if (written_header != header_size) {
                success = false;
            }
        }

        // 然后写入主体数据
        if (success && data && size > 0) {
            size_t written_data = fwrite(data, 1, size, fp);
            if (written_data != size) {
                success = false;
            }
        }

        fclose(fp);
        return success;
    }
    catch (const fs::filesystem_error&) {
        return false;
    }
}
