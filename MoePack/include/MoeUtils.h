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
#include <string>
#include <vector>
#include <filesystem>
#include "MoeTypes.h"
#include "MoeHeader.h"



/**
 * @brief UTF-8 字符串转宽字符串 (Windows)
 * @param utf8Str UTF-8 编码字符串
 * @return std::wstring 宽字符串
 */
std::wstring utf8ToWstring(const std::string& utf8Str);

/**
 * @brief 宽字符串转 UTF-8 字符串 (Windows)
 * @param wstr 宽字符串
 * @return std::string UTF-8 编码字符串
 */
std::string wstringToUtf8(const std::wstring& wstr);

/**
 * @brief 移除宽字符文件名的扩展名（最后一个后缀）
 * @param fileNameW 宽字符文件名
 * @return std::wstring 无扩展名的文件名
 */
std::wstring RemoveFileExtension(const std::wstring& fileNameW);

/**
 * @brief 获取目录下指定类型文件列表
 * @param path 目录路径 (UTF-8)
 * @param files [输出] 文件完整路径列表
 * @param fileName [输出] 文件名列表
 * @param fileType 文件类型过滤（如 ".png"，空字符串表示所有文件）
 */
void GetFileDirEx(std::string& path, std::vector<std::string>& files,
                  std::vector<std::string>& fileName, std::string fileType = "");

/**
 * @brief 获取目录下指定类型文件列表（文件名去除扩展名）
 * @param path 目录路径 (UTF-8)
 * @param files [输出] 文件完整路径列表
 * @param fileName [输出] 文件名列表（已去除扩展名）
 * @param fileType 文件类型过滤
 */
void GetFileDirEx_NoExtension(std::string& path, std::vector<std::string>& files,
                              std::vector<std::string>& fileName, std::string fileType = "");

/**
 * @brief 从完整路径中提取父目录
 * @param FullPath 完整路径 (UTF-8)
 * @return std::wstring 父目录宽字符路径
 */
std::wstring GetParentDirFromPath(const std::string& FullPath);

/**
 * @brief 检测路径是否为文件夹
 * @param path_str 路径字符串
 * @param no_exists 是否跳过存在性检查（默认 false）
 * @return bool 是文件夹返回 true
 */
bool is_directory_path(const std::string& path_str, bool no_exists = false);

/**
 * @brief 将数据写入文件
 * @param wpath 输出文件路径（宽字符）
 * @param data 数据指针
 * @param size 数据大小 (字节)
 * @return bool 写入成功返回 true
 */
bool write_data_to_dir(const std::wstring& wpath, const unsigned char* data, size_t size);

/**
 * @brief 将 MOE 头部 + 数据写入文件
 * @param wpath 输出文件路径（宽字符）
 * @param data 数据指针
 * @param size 数据大小 (字节)
 * @param header MOE 文件头部指针（可为 nullptr）
 * @return bool 写入成功返回 true
 */
bool write_data_to_dir(const std::wstring& wpath, const unsigned char* data, size_t size,
                       const MoeHeader* header);
