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
#include "MoeUtils.h"
#include <Windows.h>
#include <io.h>
#include <iostream>
#include <fstream>

std::wstring utf8ToWstring(const std::string& utf8Str) {
    if (utf8Str.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, nullptr, 0);
    wchar_t* buffer = new wchar_t[len];
    MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, buffer, len);
    std::wstring wstr(buffer);
    delete[] buffer;
    return wstr;
}

std::string wstringToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    char* buffer = new char[len];
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, buffer, len, nullptr, nullptr);
    std::string utf8Str(buffer);
    delete[] buffer;
    return utf8Str;
}

std::wstring RemoveFileExtension(const std::wstring& fileNameW) {
    size_t dotPos = fileNameW.find_last_of(L'.');
    if (dotPos == std::wstring::npos || dotPos == 0) {
        return fileNameW;
    }
    return fileNameW.substr(0, dotPos);
}

void GetFileDirEx(std::string& path, std::vector<std::string>& files,
                  std::vector<std::string>& fileName, std::string fileType) {
    intptr_t hFile = 0;
    struct _wfinddata_t fileInfoW;
    std::wstring wSearchPath = utf8ToWstring(path);

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
            if (fileNameW == L"." || fileNameW == L"..") continue;
            if (!(fileInfoW.attrib & _A_SUBDIR)) {
                std::wstring wFullPath = utf8ToWstring(path);
                if (!wFullPath.empty() && wFullPath.back() != L'\\' && wFullPath.back() != L'/') {
                    wFullPath += L"\\";
                }
                wFullPath += fileNameW;
                files.push_back(wstringToUtf8(wFullPath));
                fileName.push_back(wstringToUtf8(fileNameW));
            }
        } while (_wfindnext(hFile, &fileInfoW) == 0);
        _findclose(hFile);
    }
}

void GetFileDirEx_NoExtension(std::string& path, std::vector<std::string>& files,
                              std::vector<std::string>& fileName, std::string fileType) {
    intptr_t hFile = 0;
    struct _wfinddata_t fileInfoW;
    std::wstring wSearchPath = utf8ToWstring(path);

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
            if (fileNameW == L"." || fileNameW == L"..") continue;
            if (!(fileInfoW.attrib & _A_SUBDIR)) {
                std::wstring wFullPath = utf8ToWstring(path);
                if (!wFullPath.empty() && wFullPath.back() != L'\\' && wFullPath.back() != L'/') {
                    wFullPath += L"\\";
                }
                wFullPath += fileNameW;
                files.push_back(wstringToUtf8(wFullPath));
                std::wstring fileNameWithoutExt = RemoveFileExtension(fileNameW);
                fileName.push_back(wstringToUtf8(fileNameWithoutExt));
            }
        } while (_wfindnext(hFile, &fileInfoW) == 0);
        _findclose(hFile);
    }
}

std::wstring GetParentDirFromPath(const std::string& FullPath) {
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

bool is_directory_path(const std::string& path_str, bool no_exists) {
    try {
        namespace fs = std::filesystem;
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
    catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "文件系统错误：" << e.what() << std::endl;
        return false;
    }
}

bool write_data_to_dir(const std::wstring& wpath, const unsigned char* data, size_t size) {
    namespace fs = std::filesystem;
    fs::path fs_path(wpath);

    try {
        fs::create_directories(fs_path.parent_path());
        FILE* fp = nullptr;
#if defined(_WIN32) || defined(_WIN64)
        fp = _wfopen(fs_path.c_str(), L"wb");
#else
        fp = fopen(fs_path.c_str(), "wb");
#endif

        if (!fp) return false;

        size_t written = fwrite(data, 1, size, fp);
        fclose(fp);
        return written == size;
    }
    catch (const fs::filesystem_error&) {
        return false;
    }
}

bool write_data_to_dir(const std::wstring& wpath, const unsigned char* data, size_t size,
                       const MoeHeader* header) {
    namespace fs = std::filesystem;
    fs::path fs_path(wpath);

    try {
        fs::create_directories(fs_path.parent_path());

        FILE* fp = nullptr;
#if defined(_WIN32) || defined(_WIN64)
        fp = _wfopen(fs_path.c_str(), L"wb");
#else
        fp = fopen(fs_path.c_str(), "wb");
#endif

        if (!fp) {
            return false;
        }

        bool success = true;

        if (header) {
            size_t header_size = sizeof(MoeHeader);
            size_t written_header = fwrite(header, 1, header_size, fp);
            if (written_header != header_size) {
                success = false;
            }
        }

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
