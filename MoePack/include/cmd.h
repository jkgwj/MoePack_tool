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
#include"MoePack.h"
#define MOE_UNPACK_IMPLEMENTATION
#define MOE_UNPACK_CHECK_DATA
#include"../../MoeUnpack/MoeUnpack.h"
#include<map>
#include<sstream>


// 平台字符串转枚举
inline MOE_Platform ParsePlatform(const std::string& plat_str) {
    std::string lower_plat = plat_str;
    std::transform(lower_plat.begin(), lower_plat.end(), lower_plat.begin(), ::tolower);
    if (lower_plat == "windows") return MOE_Platform::WINDOWS;
    if (lower_plat == "linux") return MOE_Platform::LINUX;
    if (lower_plat == "macos") return MOE_Platform::MACOS;
    if (lower_plat == "ios") return MOE_Platform::IOS;
    if (lower_plat == "android") return MOE_Platform::ANDROID;

	return NOP; //没有对应平台支持或者你的输入有误
}

// 全局帮助信息存储（0=中，1=英）
const map<int, string> g_help_info = {
    {0, R"(
MoePack 打包工具 - 中文帮助
============================

【命令说明】
  pack      - 图片资源封包：解码图片→KTX2 GPU纹理压缩→ZSTD压缩(可选)→加密(可选)
              适用场景：纹理/贴图等图像资源，会根据目标平台自动选择最优GPU压缩格式
              需要 -p 指定目标平台
  pack_ex   - 非图片资源封包：直接读取原始文件→ZSTD压缩(可选)→加密(可选)
              适用场景：音频(MP3/FLAC/OGG)、视频、文本等任意二进制文件
              不进行任何解码/转码，只做压缩+加密+添加MOE文件头
              无需 -p 参数
  unpack    - 图片资源解包：解密(可选)→解压(可选)→KTX2格式验证→输出.ktx2文件
              适用场景：解包由 pack 命令生成的 .moe 文件
  unpack_ex - 非图片资源解包：解密(可选)→解压(可选)→输出原始文件(无扩展名)
              适用场景：解包由 pack_ex 命令生成的 .moe 文件
              不进行KTX2验证，直接还原原始二进制数据

【参数说明】
  -i / -in_put / -in       输入路径（待打包的文件/目录路径，或需要解包的.moe文件）
  -o / -out_put / -out     输出路径（默认：当前工作目录）
  -p / -platform           目标平台（仅 pack 命令需要）
                           支持：WINDOWS / LINUX / MACOS / IOS / ANDROID（大小写不敏感）
  -z / -ztsd_level / -zl   ZSTD压缩等级（pack默认15；pack_ex默认0即不压缩，设为非0值启用压缩）
  -k / -key                加密密钥（不提供则禁用加密，解包时如有加密必须指定）
  -l / -log_level          日志等级（0=简洁 / 1=正常 / 2=详细 / 3=调试）

【帮助参数】
  -h / -help               显示中文帮助
  -h/-help -en/-english    显示英文帮助

【示例】
  pack      -i D:\textures -p WINDOWS -o D:\output -z 17 -k mykey -l 2
  pack_ex   -i D:\audio\bgm.mp3 -o D:\output -k mykey
  unpack    -i D:\output\texture.moe -o D:\unpacked -k mykey
  unpack_ex -i D:\output\bgm.moe -o D:\unpacked -k mykey
)"},
    {1, R"(
MoePack Pack Tool - English Help
=================================

[Command Overview]
  pack      - Image resource packing: decode→KTX2 GPU compression→ZSTD(opt)→encrypt(opt)
              Use case: textures/sprites, auto-selects optimal GPU format per platform
              Requires -p for target platform
  pack_ex   - Generic resource packing: raw file→ZSTD(opt)→encrypt(opt)
              Use case: audio(MP3/FLAC/OGG), video, text, any binary files
              NO decoding/transcoding, only compression+encryption+MOE header
              No -p required
  unpack    - Image resource unpacking: decrypt(opt)→decompress(opt)→KTX2 validate→.ktx2
              Use case: unpack .moe files created by the pack command
  unpack_ex - Generic resource unpacking: decrypt(opt)→decompress(opt)→raw file (no ext)
              Use case: unpack .moe files created by the pack_ex command
              No KTX2 validation, restores original binary data directly

[Options]
  -i / -in_put / -in       Input path (file/directory to pack, or .moe file to unpack)
  -o / -out_put / -out     Output path (Default: current working directory)
  -p / -platform           Target platform (pack command only)
                           Supported: WINDOWS / LINUX / MACOS / IOS / ANDROID
  -z / -ztsd_level / -zl   ZSTD compression level (pack default 15; pack_ex default 0=off, set >0 to enable)
  -k / -key                Encryption key (no key = no encryption; required if encrypted)
  -l / -log_level          Log level (0=minimal / 1=normal / 2=verbose / 3=debug)

[Help]
  -h / -help               Show Chinese help
  -h/-help -en/-english    Show English help

[Examples]
  pack      -i D:\textures -p WINDOWS -o D:\output -z 17 -k mykey -l 2
  pack_ex   -i D:\audio\bgm.mp3 -o D:\output -k mykey
  unpack    -i D:\output\texture.moe -o D:\unpacked -k mykey
  unpack_ex -i D:\output\bgm.moe -o D:\unpacked -k mykey
)"}
};

// 参数别名映射
const std::map<std::string, std::string> g_param_alias = {
    {"-in_put", "-i"}, {"-in", "-i"},
    {"-out_put", "-o"}, {"-out", "-o"},
    {"-platform", "-p"},
    {"-ztsd_level", "-z"}, {"-zl", "-z"},
    {"-key", "-k"},
    {"-log_level", "-l"},
    {"-help", "-h"},
    {"-english", "-en"}, {"-en", "-en"}
};

// 命令类型枚举
enum CmdType {
    CMD_NONE = -1,
    CMD_UNPACK = 0,
    CMD_PACK = 1,
    CMD_PACK_EX = 2,
    CMD_UNPACK_EX = 3
};

// 参数处理器映射
struct CommandContext {
    std::string input_path;
    std::string output_path;
    MOE_Platform platform = MOE_Platform::NOP;
    int compression_level = 15;
    bool compression_set = false; // 用户是否显式指定了 -z 参数
    std::string encryption_key;
    int log_level = 1;
    bool show_help = false;
    bool english_help = false;

    // 验证必要参数（pack 需要 platform，其他命令不需要）
    bool validate(CmdType cmd_type = CMD_PACK) const {
        if (input_path.empty()) return false;
        if (cmd_type == CMD_PACK && platform == MOE_Platform::NOP) return false;
        return true;
    }
};

// 命令切割函数
inline std::vector<std::string> split_command(const std::string& cmd) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream iss(cmd);

    while (iss >> token) {
        tokens.push_back(token);
    }

    return tokens;
}

// 标准化参数名
inline std::string normalize_param(const std::string& param) {
    auto it = g_param_alias.find(param);
    if (it != g_param_alias.end()) {
        return it->second;
    }
    return param;
}

// 主命令处理函数
void cmd(const std::vector<std::string>& args);