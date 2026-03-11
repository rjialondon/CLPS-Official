/*
 * clpsd — 道·码 (CLPS) 编译器/解释器入口
 *
 * 流水线 A（解释）：词法分析 → 需卦静态检查 → 扁平坍缩法执行
 * 流水线 B（编译）：词法分析 → 需卦静态检查 → 代码生成 → .clps 字节码
 * 流水线 C（VM）：  加载 .clps → 字节码虚拟机执行
 *
 * 用法：
 *   clpsd <file.dao>                 # 检查 + 解释执行
 *   clpsd --compile <file.dao>       # 编译为 .clps 字节码文件
 *   clpsd --bytecode <file.clps>     # 用 VM 执行字节码
 *   clpsd --check <file.dao>         # 仅静态检查（不执行）
 *   clpsd --tokens <file.dao>        # 仅输出词元（调试词法）
 *   clpsd --no-check <file.dao>      # 跳过静态检查直接执行
 */

#include "tun.hpp"
#include "xu.hpp"
#include "kun.hpp"
#include "jiji.hpp"
#include "ma.hpp"
#include "ding.hpp"
#include "wanwu.hpp"
#include "dui.hpp"
#include "xisui.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstring>

static std::string read_file(const char* path) {
    std::ifstream f(path);
    if (!f) {
        std::cerr << "clpsd: 无法打开文件 " << path << "\n";
        std::exit(1);
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

int main(int argc, char** argv) {
    if (argc < 2) {
        // 无参数 → REPL（不退出，继续）
    }

    // 洗髓丹·启动自检（能力探测，布丁已就位）
    xi_sui_dan(); // 结果保留供未来路径切换使用

    // 初始化万物库（四卦先行）
    wan_wu_ku_init();

    bool dump_tokens   = false;
    bool check_only    = false;
    bool skip_check    = false;
    bool repl_mode     = false;
    bool compile_mode  = false;  // --compile: .dao → .clps
    bool bytecode_mode = false;  // --bytecode: 直接运行 .clps
    const char* path = nullptr;
    std::vector<std::string> user_args; // 转发给 .dao 程序的用户参数

    for (int i = 1; i < argc; ++i) {
        if      (strcmp(argv[i], "--tokens")   == 0) dump_tokens   = true;
        else if (strcmp(argv[i], "--check")    == 0) check_only    = true;
        else if (strcmp(argv[i], "--no-check") == 0) skip_check    = true;
        else if (strcmp(argv[i], "--repl")     == 0) repl_mode     = true;
        else if (strcmp(argv[i], "--compile")  == 0) compile_mode  = true;
        else if (strcmp(argv[i], "--bytecode") == 0) bytecode_mode = true;
        else if (path == nullptr)                    path = argv[i];
        else                                         user_args.emplace_back(argv[i]);
    }

    wan_wu_ku_set_args(user_args);

    // 无参数 或 --repl → 进入 REPL
    if (repl_mode || (!path && !dump_tokens && !check_only)) {
        run_repl();
        return 0;
    }

    if (!path) { std::cerr << "clpsd: 缺少文件名\n"; return 1; }

    // ──────────────────────────────────────────────────────
    // 流水线 C：直接运行字节码（--bytecode file.clps）
    // ──────────────────────────────────────────────────────
    if (bytecode_mode) {
        std::ifstream bf(path, std::ios::binary);
        if (!bf) { std::cerr << "clpsd: 无法打开字节码文件 " << path << "\n"; return 1; }
        std::vector<uint8_t> raw((std::istreambuf_iterator<char>(bf)),
                                  std::istreambuf_iterator<char>());
        try {
            auto bc = ClpsByteCode::deserialize(raw);
            ClpsVM vm(bc);
            vm.run();
        } catch (const std::exception& e) {
            std::cout.flush();
            std::cerr << "[VM错误] " << e.what() << "\n";
            return 1;
        }
        return 0;
    }

    std::string src = read_file(path);
    std::string filename(path);

    // ──────────────────────────────────────────────────────
    // 第一段：词法分析
    // ──────────────────────────────────────────────────────
    ClpsLexer lexer(src);
    std::vector<Token> tokens;
    try {
        tokens = lexer.tokenize();
    } catch (const ClpsLexError& e) {
        std::cerr << "[词法错误] " << filename
                  << " 第" << e.line << "行: " << e.what() << "\n";
        return 1;
    }

    if (dump_tokens) {
        for (auto& t : tokens) {
            std::cout << "  [" << tk_name(t.type) << "]";
            if (!t.text.empty()) std::cout << " \"" << t.text << "\"";
            if (t.type == TK::INT)   std::cout << " = " << t.ival;
            if (t.type == TK::FLOAT) std::cout << " = " << t.dval;
            std::cout << "  (行" << t.line << ")\n";
        }
        return 0;
    }

    // ──────────────────────────────────────────────────────
    // 第二段：需卦静态五行检查
    // ──────────────────────────────────────────────────────
    if (!skip_check) {
        ClpsChecker checker(tokens);
        auto diags = checker.check();

        if (!diags.empty()) {
            print_diags(diags, filename);
        }

        if (diags_has_error(diags)) {
            std::cerr << "\n\033[1;31m[需卦] 天机已现，执行中止。修正上述错误后方可运行。\033[0m\n";
            return 2;
        }

        if (check_only) {
            if (diags.empty()) {
                std::cerr << "\033[1;32m[需卦] 五行和谐，未见克制冲突。\033[0m\n";
            }
            return 0;
        }
    }

    // ──────────────────────────────────────────────────────
    // 流水线 B：代码生成（--compile file.dao → file.clps）
    // ──────────────────────────────────────────────────────
    if (compile_mode) {
        try {
            ClpsCodegen cgen(tokens);
            auto bc = cgen.compile();
            auto raw = bc.serialize();

            // 输出文件名：把 .dao 替换为 .clps
            std::string out_path = filename;
            auto dot = out_path.rfind('.');
            if (dot != std::string::npos) out_path = out_path.substr(0, dot);
            out_path += ".clps";

            std::ofstream of(out_path, std::ios::binary);
            if (!of) { std::cerr << "clpsd: 无法写入 " << out_path << "\n"; return 1; }
            of.write((const char*)raw.data(), raw.size());

            std::cerr << "\033[1;32m[既济] 编译完成：" << out_path
                      << "（常量池 " << bc.pool.size() << " 条，字节码 "
                      << bc.code.size() << " 字节）\033[0m\n";
        } catch (const ClpsCodegenError& e) {
            std::cerr << "[代码生成错误] 第" << e.line << "行: " << e.what() << "\n";
            return 1;
        }
        return 0;
    }

    // ──────────────────────────────────────────────────────
    // 第三段：扁平坍缩法执行
    // ──────────────────────────────────────────────────────
    ClpsParser parser(tokens);
    try {
        parser.run();
    } catch (const ClpsParseError& e) {
        std::cout.flush();
        std::cerr << "[解析错误] 第" << e.line << "行: " << e.what() << "\n";
        return 1;
    } catch (const ClpsKezError& e) {
        std::cout.flush();
        std::cerr << "[天道阻断·运行时] " << e.what() << "\n";
        return 1;
    } catch (const ClpsError& e) {
        std::cout.flush();
        std::cerr << "[CLPS错误] " << e.what() << "\n";
        return 1;
    }

    return 0;
}
