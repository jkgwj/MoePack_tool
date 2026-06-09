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
#include "cmd.h"
#include <Windows.h>
#include <iostream>

const std::map<int, std::string> g_help_info = {
    {0, R"(
MoePack 打包工具 - 中文帮助
============================

【命令说明】
  pack            - 图片资源封包：解码图片→KTX2 GPU纹理压缩→ZSTD压缩(可选)→加密(可选)
                    适用场景：纹理/贴图等图像资源，会根据目标平台自动选择最优GPU压缩格式
                    需要 -p 指定目标平台
  pack_ex         - 非图片资源封包：直接读取原始文件→ZSTD压缩(可选)→加密(可选)
                    适用场景：音频(MP3/FLAC/OGG)、视频、文本等任意二进制文件
                    不进行任何解码/转码，只做压缩+加密+添加MOE文件头
                    无需 -p 参数
  pack_ex_stream  - 流式加密封包：读取原始文件→分块加密(无压缩)→输出.moe
                    适用场景：需要流式播放的大音频文件
                    自动检测音频格式(WAV/FLAC/MP3/VORBIS)写入文件头
                    使用 -s 指定自定义块大小(默认64KB)
                    无需 -p、-z 参数(不支持压缩)
  unpack          - 图片资源解包：解密(可选)→解压(可选)→KTX2格式验证→输出.ktx2文件
                    适用场景：解包由 pack 命令生成的 .moe 文件
  unpack_ex       - 非图片资源解包：解密(可选)→解压(可选)→输出原始文件(无扩展名)
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
  -s / -stream             流式加密块大小(字节)，仅 pack_ex_stream 使用（默认65536即64KB）
  -fs / -fixed_salt       使用固定 salt（仅 pack_ex_stream，可选指定 salt，空则使用默认常量）

【帮助参数】
  -h / -help               显示中文帮助
  -h/-help -en/-english    显示英文帮助

【示例】
  pack            -i D:\textures -p WINDOWS -o D:\output -z 17 -k mykey -l 2
  pack_ex         -i D:\audio\bgm.mp3 -o D:\output -k mykey
  pack_ex_stream  -i D:\audio\bgm.mp3 -o D:\output -k mykey -s 131072 -fs
  unpack          -i D:\output\texture.moe -o D:\unpacked -k mykey
  unpack_ex       -i D:\output\bgm.moe -o D:\unpacked -k mykey
)"},
    {1, R"(
MoePack Pack Tool - English Help
=================================

[Command Overview]
  pack            - Image resource packing: decode→KTX2 GPU compression→ZSTD(opt)→encrypt(opt)
                    Use case: textures/sprites, auto-selects optimal GPU format per platform
                    Requires -p for target platform
  pack_ex         - Generic resource packing: raw file→ZSTD(opt)→encrypt(opt)
                    Use case: audio(MP3/FLAC/OGG), video, text, any binary files
                    NO decoding/transcoding, only compression+encryption+MOE header
                    No -p required
  pack_ex_stream  - Streaming encryption pack: raw file→chunked encrypt(no compress)→.moe
                    Use case: large audio files for streaming playback
                    Auto-detects audio format (WAV/FLAC/MP3/VORBIS) stored in header
                    Use -s to set custom chunk size (default 64KB)
                    No -p or -z needed (compression not supported)
  unpack          - Image resource unpacking: decrypt(opt)→decompress(opt)→KTX2 validate→.ktx2
                    Use case: unpack .moe files created by the pack command
  unpack_ex       - Generic resource unpacking: decrypt(opt)→decompress(opt)→raw file (no ext)
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
  -s / -stream             Chunk size in bytes for pack_ex_stream (default 65536 = 64KB)
  -fs / -fixed_salt       Use fixed salt (pack_ex_stream only, optional salt string, empty for default)

[Help]
  -h / -help               Show Chinese help
  -h/-help -en/-english    Show English help

[Examples]
  pack            -i D:\textures -p WINDOWS -o D:\output -z 17 -k mykey -l 2
  pack_ex         -i D:\audio\bgm.mp3 -o D:\output -k mykey
  pack_ex_stream  -i D:\audio\bgm.mp3 -o D:\output -k mykey -s 131072 -fs
  unpack          -i D:\output\texture.moe -o D:\unpacked -k mykey
  unpack_ex       -i D:\output\bgm.moe -o D:\unpacked -k mykey
)"}
};

const std::map<std::string, std::string> g_param_alias = {
    {"-in_put", "-i"}, {"-in", "-i"},
    {"-out_put", "-o"}, {"-out", "-o"},
    {"-platform", "-p"},
    {"-ztsd_level", "-z"}, {"-zl", "-z"},
    {"-key", "-k"},
    {"-log_level", "-l"},
    {"-help", "-h"},
    {"-english", "-en"}, {"-en", "-en"},
    {"-stream", "-s"}, {"-s", "-s"},
    {"-fixed_salt", "-fs"}, {"-fs", "-fs"}
};

std::string normalize_param(const std::string& param) {
    auto it = g_param_alias.find(param);
    if (it != g_param_alias.end()) {
        return it->second;
    }
    return param;
}

int is_pack_command = 1;

int main() {
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
    std::cout << "============ MoePack 打包工具 ============" << std::endl;
    std::cout << "可用命令: pack | pack_ex | pack_ex_stream | unpack | unpack_ex" << std::endl;
    std::cout << "输入命令+参数 (输入 'exit' 退出, 输入 -h 查看详细帮助):" << std::endl;

    std::string input;
    while (true) {
        std::getline(std::cin, input);
        if (input == "exit" || input == "quit") {
            break;
        }
        if (input.empty()) {
            continue;
        }
        std::vector<std::string> args = split_command(input);
        if (!args.empty()) {
            if (args[0] == "pack_ex" || args[0] == "Pack_ex" || args[0] == "PACK_EX") {
                is_pack_command = 2;
                args.erase(args.begin());
            } else if (args[0] == "unpack_ex" || args[0] == "Unpack_ex" || args[0] == "UNPACK_EX") {
                is_pack_command = 3;
                args.erase(args.begin());
            } else if (args[0] == "pack_ex_stream" || args[0] == "Pack_ex_stream" || args[0] == "PACK_EX_STREAM") {
                is_pack_command = 4;
                args.erase(args.begin());
            } else if (args[0] == "Pack" || args[0] == "pack" || args[0] == "PACK") {
                is_pack_command = 1;
                args.erase(args.begin());
            } else if (args[0] == "Unpack" || args[0] == "unpack" || args[0] == "UNPACK") {
                is_pack_command = 0;
                args.erase(args.begin());
            }
        }
        if (args.empty()) {
            std::cout << "请输入参数 (输入 -h 查看帮助)" << std::endl;
            continue;
        }
        cmd(args);
    }

    return 0;
}

void cmd(const std::vector<std::string>& args) {
    CommandContext ctx;
    bool has_error = false;

    for (size_t i = 0; i < args.size(); ++i) {
        std::string param = normalize_param(args[i]);

        if (param == "-h") {
            ctx.show_help = true;
        } else if (param == "-en") {
            ctx.english_help = true;
        } else if (param == "-i" || param == "-o" || param == "-p" ||
            param == "-z" || param == "-k" || param == "-l" || param == "-s" ||
            param == "-fs") {
            if (i + 1 >= args.size() || args[i + 1][0] == '-') {
                std::cout << "错误: 参数 " << args[i] << " 需要一个值" << std::endl;
                has_error = true;
                continue;
            }

            std::string value = args[++i];

            if (param == "-i") {
                ctx.input_path = value;
            } else if (param == "-o") {
                ctx.output_path = value;
            } else if (param == "-p") {
                ctx.platform = ParsePlatform(value);
                if (ctx.platform == MOE_Platform::NOP) {
                    std::cout << "错误: 无效的目标平台 " << value << std::endl;
                    has_error = true;
                }
            } else if (param == "-z") {
                try {
                    ctx.compression_level = std::stoi(value);
                    ctx.compression_set = true;
                    if (ctx.compression_level < 0) ctx.compression_level = 15;
                } catch (...) {
                    std::cout << "错误: 无效的压缩等级 " << value << std::endl;
                    has_error = true;
                }
            } else if (param == "-k") {
                ctx.encryption_key = value;
            } else if (param == "-l") {
                try {
                    ctx.log_level = std::stoi(value);
                } catch (...) {
                    std::cout << "错误: 无效的日志等级 " << value << std::endl;
                    has_error = true;
                }
            } else if (param == "-s") {
                try {
                    int cs = std::stoi(value);
                    if (cs <= 0) cs = 65536;
                    ctx.chunk_size = static_cast<uint32_t>(cs);
                } catch (...) {
                    std::cout << "错误: 无效的块大小 " << value << std::endl;
                    has_error = true;
                }
            } else if (param == "-fs") {
                ctx.use_fixed_salt = true;
                if (i + 1 < args.size() && args[i + 1][0] != '-') {
                    ctx.fixed_salt_value = args[i + 1];
                    i++;
                }
            }
        } else if (param[0] == '-') {
            std::cout << "警告: 未知参数 " << args[i] << std::endl;
        }
    }

    if (ctx.show_help || has_error) {
        int lang = ctx.english_help ? 1 : 0;
        auto it = g_help_info.find(lang);
        if (it != g_help_info.end()) {
            std::cout << it->second << std::endl;
        }
        return;
    }

    CmdType cmd_type = static_cast<CmdType>(is_pack_command);

    if (!ctx.validate(cmd_type)) {
        std::cout << "错误: 缺少必要参数" << std::endl;
        std::cout << g_help_info.at(0) << std::endl;
        return;
    }

    if (cmd_type == CMD_PACK_EX && !ctx.compression_set) {
        ctx.compression_level = 0;
    }

    MoePack packer;
    packer.set_log_level(ctx.log_level);
    MoeUnpack::set_unpack_log_level(ctx.log_level);
    packer.ztsd_compression(ctx.compression_level > 0, ctx.compression_level);
    packer.encryption(!ctx.encryption_key.empty(), ctx.encryption_key);

    if (ctx.use_fixed_salt) {
        if (ctx.fixed_salt_value.empty()) {
            packer.use_fixed_salt(true);
        } else {
            packer.use_fixed_salt(true, ctx.fixed_salt_value);
        }
    }

    if (!has_error) {
        if (cmd_type == CMD_PACK) {
            packer.set_dst_platform(ctx.platform);
        }

        if (cmd_type == CMD_PACK_EX_STREAM) {
            packer.ztsd_compression(false, 0);
            std::cout << packer.pack_ex_stream(ctx.input_path, ctx.output_path, ctx.chunk_size);
        } else if (cmd_type == CMD_PACK) {
            std::cout << packer.pack(ctx.input_path, ctx.output_path);
        } else if (cmd_type == CMD_UNPACK) {
            std::cout << MoeUnpack::unpack(ctx.input_path, ctx.output_path, ctx.encryption_key);
        } else if (cmd_type == CMD_PACK_EX) {
            std::cout << packer.pack_ex(ctx.input_path, ctx.output_path);
        } else if (cmd_type == CMD_UNPACK_EX) {
            std::cout << MoeUnpack::unpack_ex(ctx.input_path, ctx.output_path, ctx.encryption_key);
        }
    }
}
