#include"cmd.h"

int main() {
	SetConsoleOutputCP(65001); // 设置控制台编码为UTF-8
	SetConsoleCP(65001);
    std::cout << "============ MoePack 打包工具 ============" << std::endl;
    std::cout << "输入命令 (输入 'exit' 退出,输入 -h 查看帮助):" << std::endl;

    MoeUnpack::set_unpack_log_level(3);
    MoeUnpack::unpack("D:\\project\\cpp\\MoePack\\test\\dst1\\AI_FD_e05c.moe", "D:\\project\\cpp\\MoePack\\test\\dst1","bleach");

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
        if (!args.empty() && (args[0] == "Pack" || args[0] == "pack")) {
            args.erase(args.begin());
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
                    
                    has_error = true;
                }
            }
            else if (param == "-z") {//压缩等级命令处理
                try {
                    ctx.compression_level = std::stoi(value);
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

    // 验证必要参数
    if (!ctx.validate()) {
        std::cout << "错误: 缺少必要参数" << std::endl;
        std::cout << g_help_info.at(0) << std::endl;
        return;
    }

    // 创建MoePack对象并执行
    MoePack packer;
    packer.set_dst_platform(ctx.platform);
    packer.set_log_level(ctx.log_level);
    packer.ztsd_compression(ctx.compression_level > 0, ctx.compression_level);
    packer.encryption(!ctx.encryption_key.empty(), ctx.encryption_key);

    if(!has_error)cout<< packer.pack(ctx.input_path, ctx.output_path);
}