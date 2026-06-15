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
#include "MoePack.h"
#include "MoeUnpack.h"
#include <map>
#include <sstream>

/**
 * @brief 平台字符串转枚举（大小写不敏感）
 * @param plat_str 平台名称字符串
 * @return MOE_Platform 对应平台枚举，无法匹配返回 NOP
 */
inline MOE_Platform ParsePlatform(const std::string& plat_str) {
    std::string lower_plat = plat_str;
    std::transform(lower_plat.begin(), lower_plat.end(), lower_plat.begin(), ::tolower);
    if (lower_plat == "windows") return MOE_Platform::WINDOWS;
    if (lower_plat == "linux")   return MOE_Platform::LINUX;
    if (lower_plat == "macos")   return MOE_Platform::MACOS;
    if (lower_plat == "ios")     return MOE_Platform::IOS;
    if (lower_plat == "android") return MOE_Platform::ANDROID;
    return NOP;
}

/**
 * @brief 命令类型枚举
 */
enum CmdType {
    CMD_NONE = -1,          ///< 无命令
    CMD_UNPACK = 0,         ///< 图片解包 (unpack)
    CMD_PACK = 1,           ///< 图片封包 (pack)
    CMD_PACK_EX = 2,        ///< 通用封包 (pack_ex)
    CMD_UNPACK_EX = 3,      ///< 通用解包 (unpack_ex)
    CMD_PACK_EX_STREAM = 4  ///< 流式加密封包 (pack_ex_stream)
};

/**
 * @brief CLI 命令上下文：保存所有解析后的命令行参数
 */
struct CommandContext {
    std::string input_path;                     ///< 输入路径（文件或目录）
    std::string output_path;                    ///< 输出路径（文件或目录）
    MOE_Platform platform = MOE_Platform::NOP;  ///< 目标平台（仅 pack 命令需要）
    int compression_level = 15;                 ///< ZSTD 压缩等级
    bool compression_set = false;               ///< 用户是否显式指定了 -z 参数
    std::string encryption_key;                 ///< 加密密钥
    int log_level = 1;                          ///< 日志等级
    bool show_help = false;                     ///< 是否显示帮助
    bool english_help = false;                  ///< 是否显示英文帮助
    uint32_t chunk_size = 65536;                ///< 流式加密块大小（默认 64KB）
    bool use_fixed_salt = false;                ///< 是否使用固定 salt
    std::string fixed_salt_value;               ///< 自定义固定 salt 值

    /**
     * @brief 验证必要参数
     * @param cmd_type 命令类型（pack 命令需要 platform 参数）
     * @return bool 参数齐全返回 true
     */
    bool validate(CmdType cmd_type = CMD_PACK) const {
        if (input_path.empty()) return false;
        if (cmd_type == CMD_PACK && platform == MOE_Platform::NOP) return false;
        return true;
    }
};

/**
 * @brief 将输入字符串按空格分割为 token 列表
 * @param cmd 输入命令行字符串
 * @return std::vector<std::string> token 列表
 */
inline std::vector<std::string> split_command(const std::string& cmd) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream iss(cmd);
    while (iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

/**
 * @brief 将参数别名标准化为规范形式
 * @param param 输入参数名
 * @return std::string 标准化后的参数名
 * @note 例如 "-in" → "-i", "-ztsd_level" → "-z"
 */
inline std::string normalize_param(const std::string& param);

/**
 * @brief 主命令处理函数：解析参数并执行对应的打包/解包操作
 * @param args 命令行参数 token 列表
 */
void cmd(const std::vector<std::string>& args);
