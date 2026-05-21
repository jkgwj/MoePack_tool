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
#include"cmd.h"
// -1 没有对应的命令输入
// 0  解包命令(unpack)
// 1  封包命令(pack)
// 2  非图片封包(pack_ex)
// 3  非图片解包(unpack_ex)
int is_pack_command = 1;

int main() {
	SetConsoleOutputCP(65001); // 设置控制台编码为UTF-8
	SetConsoleCP(65001);
    std::cout << "============ MoePack 打包工具 ============" << std::endl;
    std::cout << "可用命令: pack | pack_ex | unpack | unpack_ex" << std::endl;
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
            // 识别命令类型
            if (args[0] == "pack_ex" || args[0] == "Pack_ex" || args[0] == "PACK_EX") {
                is_pack_command = 2;
                args.erase(args.begin());
            }
            else if (args[0] == "unpack_ex" || args[0] == "Unpack_ex" || args[0] == "UNPACK_EX") {
                is_pack_command = 3;
                args.erase(args.begin());
            }
            else if (args[0] == "Pack" || args[0] == "pack" || args[0] == "PACK") {
                is_pack_command = 1;
                args.erase(args.begin());
            }
            else if (args[0] == "Unpack" || args[0] == "unpack" || args[0] == "UNPACK") {
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

// 主命令处理函数
void cmd(const std::vector<std::string>& args) {
    CommandContext ctx;
    bool has_error = false;

    // 解析参数
    for (size_t i = 0; i < args.size(); ++i) {
        std::string param = normalize_param(args[i]);

        if (param == "-h") {
            ctx.show_help = true;
        }
        else if (param == "-en") {
            ctx.english_help = true;
        }
        else if (param == "-i" || param == "-o" || param == "-p" ||
            param == "-z" || param == "-k" || param == "-l") {
            // 需要参数的选项
            if (i + 1 >= args.size() || args[i + 1][0] == '-') {
                cout << "错误: 参数 " << args[i] << " 需要一个值" << std::endl;
                has_error = true;
                continue;
            }

            std::string value = args[++i];

            if (param == "-i") {//输入路径命令处理
                ctx.input_path = value;
            }
            else if (param == "-o") {//输出路径命令处理
                ctx.output_path = value;
            }
            else if (param == "-p") {//目标平台设置命令处理
                ctx.platform = ParsePlatform(value);
                if (ctx.platform == MOE_Platform::NOP) {
                    cout << "错误: 无效的目标平台 " << value << std::endl;
                    has_error = true;
                }
            }
            else if (param == "-z") {//压缩等级命令处理
                try {
                    ctx.compression_level = std::stoi(value);
                    ctx.compression_set = true;
                    if (ctx.compression_level < 0) {
                        ctx.compression_level = 15;
                    }
                }
                catch (...) {
                    std::cout << "错误: 无效的压缩等级 " << value << std::endl;
                    has_error = true;
                }
            }
            else if (param == "-k") {//加密密钥命令处理
                ctx.encryption_key = value;
            }
            else if (param == "-l") {//日志等级命令处理
                try {
                    ctx.log_level = std::stoi(value);
                }
                catch (...) {
                    std::cout << "错误: 无效的日志等级 " << value << std::endl;
                    has_error = true;
                }
            }
        }
        else if (param[0] == '-') {// 未知参数
            std::cout << "警告: 未知参数 " << args[i] << std::endl;
        }
    }

    // 处理帮助信息
    if (ctx.show_help || has_error) {
        int lang = ctx.english_help ? 1 : 0;
        auto it = g_help_info.find(lang);
        if (it != g_help_info.end()) {
            std::cout << it->second << std::endl;
        }
        return;
    }

    CmdType cmd_type = static_cast<CmdType>(is_pack_command);

    // 验证必要参数
    if (!ctx.validate(cmd_type)) {
        std::cout << "错误: 缺少必要参数" << std::endl;
        std::cout << g_help_info.at(0) << std::endl;
        return;
    }

    // pack_ex 默认不压缩（用户可通过 -z 显式启用）
    if (cmd_type == CMD_PACK_EX && !ctx.compression_set) {
        ctx.compression_level = 0;
    }

    // 创建MoePack对象并执行
    MoePack packer;
    packer.set_log_level(ctx.log_level);
    MoeUnpack::set_unpack_log_level(ctx.log_level);
    packer.ztsd_compression(ctx.compression_level > 0, ctx.compression_level);
    packer.encryption(!ctx.encryption_key.empty(), ctx.encryption_key);

    if (!has_error) {
        // pack_ex/unpack_ex 不需要平台参数
        if (cmd_type == CMD_PACK || cmd_type == CMD_PACK_EX) {
            packer.set_dst_platform(ctx.platform);
        }

        if (cmd_type == CMD_PACK) {
            cout << packer.pack(ctx.input_path, ctx.output_path);
        }
        else if (cmd_type == CMD_UNPACK) {
            cout << MoeUnpack::unpack(ctx.input_path, ctx.output_path, ctx.encryption_key);
        }
        else if (cmd_type == CMD_PACK_EX) {
            cout << packer.pack_ex(ctx.input_path, ctx.output_path);
        }
        else if (cmd_type == CMD_UNPACK_EX) {
            cout << MoeUnpack::unpack_ex(ctx.input_path, ctx.output_path, ctx.encryption_key);
        }
    }
}