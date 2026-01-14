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
用法: Pack [参数]

必要参数:
  -i / -in_put / -in       输入路径（待打包的文件/目录路径）
  -p / -platform           目标平台（支持：WINDOWS/LINUX/MACOS/IOS/ANDROID，大小写不敏感）

可选参数:
  -o / -out_put / -out     输出路径（默认：当前工作目录）
  -z / -ztsd_level / -zl   ZTSD压缩等级（默认：15，启用压缩）
  -k / -key                加密密钥（无此参数则禁用加密）
  -l / -log_level          日志等级（默认：1，等级越高输出越详细）

帮助参数:
  -h / -help               显示中文帮助信息
  -h/-help -en/-english    显示英文帮助信息

示例:
  Pack -i D:\src -p WINDOWS -o D:\dst -z 17 -k bleach -l 3
  Pack -in ./src -platform LINUX -zl 12 -key 123456
)"},
    {1, R"(
MoePack Pack Tool - English Help
===============================
Usage: Pack [Options]

Required Options:
  -i / -in_put / -in       Input path (file/directory to be packed)
  -p / -platform           Target platform (Supported: WINDOWS/LINUX/MACOS/IOS/ANDROID, case-insensitive)

Optional Options:
  -o / -out_put / -out     Output path (Default: Current working directory)
  -z / -ztsd_level / -zl   ZTSD compression level (Default: 15, compression enabled)
  -k / -key                Encryption key (No this option means encryption disabled)
  -l / -log_level          Log level (Default: 1, higher level for more details)

Help Options:
  -h / -help               Show Chinese help information
  -h/-help -en/-english    Show English help information

Examples:
  Pack -i D:\src -p WINDOWS -o D:\dst -z 17 -k bleach -l 3
  Pack -in ./src -platform LINUX -zl 12 -key 123456
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

// 参数处理器映射
struct CommandContext {
    std::string input_path;
    std::string output_path;
    MOE_Platform platform = MOE_Platform::NOP;
    int compression_level = 15;
    std::string encryption_key;
    int log_level = 1;
    bool show_help = false;
    bool english_help = false;

    // 验证必要参数
    bool validate() const {
        return !input_path.empty() && platform != MOE_Platform::NOP;
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