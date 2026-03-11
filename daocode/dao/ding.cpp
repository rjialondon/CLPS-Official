#include "ding.hpp"
#include "wanwu.hpp"
#include "tun.hpp"
#include "xu.hpp"
#include "jiji.hpp"
#include <iostream>
#include <sstream>
#include <fstream>

ClpsVM::ClpsVM(const ClpsByteCode& bc, ClpsOutputFn out)
    : bc_(bc)
    , out_(out ? std::move(out) : [](const std::string& s){ std::cout << s << "\n"; })
{
    auto root = std::make_shared<TaiJiJieJie>("太极", 0, nullptr);
    scope_stack_.push_back(root);
}

void ClpsVM::emit(const std::string& s) { out_(s); }

// ── 池条目 → ClpsValue ───────────────────────────────────────────────────
ClpsValue ClpsVM::resolve_pool(uint16_t idx) const {
    if (idx == POOL_DOLLAR_I) {
        // $i：读取当前循环索引
        if (!loop_stack_.empty())
            return ClpsValue((int64_t)loop_stack_.back().iter_index);
        return ClpsValue((int64_t)0);
    }
    if (idx == POOL_NONE) return ClpsValue(); // 虚

    const PoolEntry& e = bc_.pool[idx];
    switch (e.type) {
        case PoolType::Name:
        case PoolType::StrLit:
            return ClpsValue(e.str);
        case PoolType::VarRef:
            // 从当前作用域向外查找
            for (auto it = scope_stack_.rbegin(); it != scope_stack_.rend(); ++it) {
                try { return (*it)->dui(e.str); } catch (...) {}
            }
            return ClpsValue(); // 未找到 → 虚
        case PoolType::Int64:
            return ClpsValue(e.ival);
        case PoolType::Float64:
            return ClpsValue(e.dval);
        case PoolType::Wx:
            return ClpsValue((int64_t)e.wx);
    }
    return ClpsValue();
}

// ── 值比较（复用解释器逻辑）─────────────────────────────────────────────
bool ClpsVM::compare_vals(const ClpsValue& lhs, const ClpsValue& rhs, uint8_t cmp) {
    double l=0, r=0; bool ln=false, rn=false;
    if (auto* iv = std::get_if<int64_t>(&lhs.data)) { l=(double)*iv; ln=true; }
    else if (auto* dv = std::get_if<double>(&lhs.data)) { l=*dv; ln=true; }
    if (auto* iv = std::get_if<int64_t>(&rhs.data)) { r=(double)*iv; rn=true; }
    else if (auto* dv = std::get_if<double>(&rhs.data)) { r=*dv; rn=true; }
    if (ln && rn) {
        switch (cmp) {
            case CMP_EQ:  return l==r; case CMP_NEQ: return l!=r;
            case CMP_GT:  return l>r;  case CMP_LT:  return l<r;
            case CMP_GTE: return l>=r; case CMP_LTE: return l<=r;
        }
    }
    auto* ls = std::get_if<std::string>(&lhs.data);
    auto* rs = std::get_if<std::string>(&rhs.data);
    if (ls && rs) {
        switch (cmp) {
            case CMP_EQ:  return *ls==*rs; case CMP_NEQ: return *ls!=*rs;
            case CMP_GT:  return *ls>*rs;  case CMP_LT:  return *ls<*rs;
            case CMP_GTE: return *ls>=*rs; case CMP_LTE: return *ls<=*rs;
        }
    }
    bool le = std::holds_alternative<std::monostate>(lhs.data);
    bool re = std::holds_alternative<std::monostate>(rhs.data);
    if (cmp==CMP_EQ)  return le && re;
    if (cmp==CMP_NEQ) return !(le && re);
    return false;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  单步执行（JIAN_BEG 递归调用）
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void ClpsVM::step() {
    uint8_t opcode = read_u8();

    switch ((Op)opcode) {

        // ── 蒙·行号标记 ─────────────────────────────────────────────
        case Op::LINE:
            cur_line_ = read_u16();
            break;

        // ── 坤·归元 ─────────────────────────────────────────────────
        case Op::KUN:
            cur()->kun();
            emit("[坤·归元] 结界资源已散化");
            break;

        // ── 坤·归列：截断列表到前 N 项 ───────────────────────────
        case Op::KUN_LIST: {
            uint16_t key_idx   = read_u16();
            uint16_t count_idx = read_u16();
            std::string key = bc_.pool[key_idx].str;
            ClpsValue cv = resolve_pool(count_idx);
            int64_t n = std::get<int64_t>(cv.data);
            cur()->kun_list(key, static_cast<size_t>(n));
            emit("[坤·归列] \"" + key + "\" 截断至 " + std::to_string(n) + " 项");
            break;
        }

        // ── 作用域管理 ──────────────────────────────────────────────
        case Op::SCOPE_PUSH: {
            uint16_t name_idx = read_u16();
            std::string name = bc_.pool[name_idx].str;
            auto child = std::make_shared<TaiJiJieJie>(name, (int64_t)scope_stack_.size(), nullptr);
            scope_stack_.push_back(child);
            break;
        }
        case Op::SCOPE_POP: {
            std::string name = cur()->name;
            scope_stack_.pop_back();
            emit("[内卦坍缩] 结界 \"" + name + "\" 已归元");
            break;
        }

        // ── 复·循环 ─────────────────────────────────────────────────
        case Op::LOOP_BEG: {
            uint8_t  ctype   = read_u8();
            uint16_t cnt_val = read_u16();
            uint32_t body_sz = read_u32();

            uint32_t count = 0;
            if (ctype == LOOP_LIT) {
                count = (uint32_t)cnt_val;
            } else {
                ClpsValue v = resolve_pool(cnt_val);
                if (auto* iv = std::get_if<int64_t>(&v.data)) count = (uint32_t)*iv;
            }

            if (count == 0) {
                // 跳过循环体（含 LOOP_END）
                ip_ += body_sz;
            } else {
                loop_stack_.push_back({ip_, count, 0});
            }
            break;
        }
        case Op::LOOP_END: {
            if (loop_stack_.empty()) break;
            auto& lf = loop_stack_.back();
            ++lf.iter_index;
            --lf.remaining;
            if (lf.remaining > 0) {
                ip_ = lf.ip_body_start; // 跳回循环体起始
            } else {
                loop_stack_.pop_back();
            }
            break;
        }

        // ── 变·爻变 ─────────────────────────────────────────────────
        case Op::BIAN: {
            uint16_t key_idx = read_u16();
            uint8_t  wx_val  = read_u8();
            std::string key  = bc_.pool[key_idx].str;
            static const WX wx_tab[] = {WX::Jin, WX::Shui, WX::Mu, WX::Huo, WX::Tu};
            cur()->bian(key, wx_tab[wx_val % 5]);
            emit("[变·爻变] \"" + key + "\" 五行已更新");
            break;
        }

        // ── 讼·断言 ─────────────────────────────────────────────────
        case Op::SONG_CMP: {
            uint8_t  cmp     = read_u8();
            uint16_t key_idx = read_u16();
            uint16_t val_idx = read_u16();
            std::string key  = bc_.pool[key_idx].str;
            ClpsValue lhs    = cur()->dui(key);
            ClpsValue rhs    = resolve_pool(val_idx);
            bool ok = compare_vals(lhs, rhs, cmp);
            static const char* cmp_strs[] = {"==","!=",">","<",">=","<="};
            if (ok) {
                emit(std::string("[讼·验] \"") + key + "\" " + cmp_strs[cmp] +
                     " " + rhs.repr() + " → 通过");
            } else {
                std::ostringstream oss;
                oss << "[讼·断] 断言失败: \"" << key << "\" (= "
                    << lhs.repr() << ") " << cmp_strs[cmp] << " " << rhs.repr();
                vm_throw(oss.str());
            }
            break;
        }

        // ── 家人·模块加载 ───────────────────────────────────────────
        case Op::JIAREN: {
            uint16_t name_idx = read_u16();
            uint16_t path_idx = read_u16();
            std::string mod_name = bc_.pool[name_idx].str;
            std::string filepath = bc_.pool[path_idx].str;

            std::ifstream f(filepath);
            if (!f) vm_throw("家人：无法打开文件 \"" + filepath + "\"");
            std::ostringstream ss; ss << f.rdbuf();
            std::string src = ss.str();

            ClpsLexer lexer(src);
            auto tokens = lexer.tokenize();
            ClpsCodegen cgen(tokens);
            auto child_bc = cgen.compile();

            ClpsVM child_vm(child_bc, out_);
            child_vm.run();
            child_vm.scope_stack_[0]->name = mod_name;
            modules_[mod_name] = child_vm.scope_stack_[0];
            emit("[家人·归] 模块 \"" + mod_name + "\" 已加载（" + filepath + "）");
            break;
        }

        // ── 师·分兵（异步启动）──────────────────────────────────────
        case Op::SHI_BEI: {
            uint16_t name_idx = read_u16();
            uint16_t path_idx = read_u16();
            std::string task_name = bc_.pool[name_idx].str;
            std::string filepath  = bc_.pool[path_idx].str;

            // 预读文件（在主线程中，避免并发文件访问竞争）
            std::ifstream f(filepath);
            if (!f) vm_throw("师·分兵：无法打开文件 \"" + filepath + "\"");
            std::ostringstream ss; ss << f.rdbuf();
            std::string src = ss.str();

            // 编译（在主线程中完成，字节码是只读的）
            ClpsLexer lexer(src);
            auto tokens = lexer.tokenize();
            ClpsCodegen cgen(tokens);
            auto child_bc = std::make_shared<ClpsByteCode>(cgen.compile());

            // 输出缓冲（每个子任务独立缓冲，合师时按序输出）
            auto buf = std::make_shared<std::vector<std::string>>();
            auto buf_out = [buf](const std::string& s){ buf->push_back(s); };

            // 异步执行
            auto fut = std::async(std::launch::async,
                [child_bc, buf_out, task_name]() mutable
                    -> std::shared_ptr<TaiJiJieJie>
                {
                    ClpsVM child_vm(*child_bc, buf_out);
                    child_vm.run();
                    auto scope = child_vm.scope_stack_[0];
                    scope->name = task_name;
                    return scope;
                });

            pending_tasks_.emplace(task_name, ShiTask{ std::move(fut) });
            pending_bufs_[task_name] = buf;

            emit("[师·分兵] 任务 \"" + task_name + "\" 已启动（" + filepath + "）");
            break;
        }

        // ── 师·合师（等待所有子任务）────────────────────────────────
        case Op::SHI_HE: {
            if (pending_tasks_.empty()) {
                emit("[师·合师] 无待命部队");
                break;
            }
            for (auto& [name, task] : pending_tasks_) {
                // 阻塞等待，获取子任务根结界
                auto scope = task.fut.get();
                // 按序输出缓冲内容
                if (auto it = pending_bufs_.find(name); it != pending_bufs_.end()) {
                    for (auto& line : *it->second) emit(line);
                }
                modules_[name] = scope;
                emit("[师·归] 任务 \"" + name + "\" 已完成");
            }
            pending_tasks_.clear();
            pending_bufs_.clear();
            break;
        }

        // ── 艮·条件门控 ─────────────────────────────────────────────
        case Op::GEN_CMP: {
            uint8_t  cmp     = read_u8();
            uint16_t key_idx = read_u16();
            uint16_t val_idx = read_u16();
            uint16_t skip    = read_u16();
            std::string key  = bc_.pool[key_idx].str;
            ClpsValue lhs    = cur()->dui(key);
            ClpsValue rhs    = resolve_pool(val_idx);
            if (!compare_vals(lhs, rhs, cmp)) {
                ip_ += skip; // 条件为假：跳过 if 块（及 GEN_ELSE 头，若有）
            }
            break;
        }

        // ── 否·天地否 — 跳过 else 块（条件为真时触发）────────────
        case Op::GEN_ELSE: {
            uint16_t else_sz = read_u16();
            ip_ += else_sz; // 条件已真，跳过 else 块
            break;
        }

        // ── 坎·安全运算 ─────────────────────────────────────────────
        case Op::KAN_OP: {
            uint8_t  op      = read_u8();
            uint16_t key_idx = read_u16();
            uint16_t val_idx = read_u16();
            std::string key  = bc_.pool[key_idx].str;
            ClpsValue val    = resolve_pool(val_idx);
            cur()->kan(key, (char)op, std::move(val));
            break;
        }

        // ── 巽·遍历 ─────────────────────────────────────────────────
        case Op::XUN: {
            uint16_t pat_idx = read_u16();
            std::optional<std::string> pat;
            if (pat_idx != POOL_NONE) pat = bc_.pool[pat_idx].str;
            ClpsMap result = cur()->xun(pat);
            emit("[巽·遍历] 共 " + std::to_string(result.size()) + " 项");
            for (auto& [k, v] : result) {
                std::ostringstream oss;
                oss << "  " << k << " = " << v.repr();
                emit(oss.str());
            }
            break;
        }

        // ── 震·算术 ─────────────────────────────────────────────────
        case Op::ZHEN_OP: {
            uint8_t  op      = read_u8();
            uint16_t key_idx = read_u16();
            uint16_t val_idx = read_u16();
            std::string key  = bc_.pool[key_idx].str;
            ClpsValue val    = resolve_pool(val_idx);
            cur()->zhen(key, (char)op, std::move(val));
            break;
        }

        // ── 震·库调用（有返回值）────────────────────────────────────
        case Op::ZHEN_CALL: {
            uint16_t res_idx = read_u16();
            uint16_t mod_idx = read_u16();
            uint16_t fn_idx  = read_u16();
            uint8_t  argc    = read_u8();
            std::vector<ClpsValue> args;
            for (int i = 0; i < argc; ++i)
                args.push_back(resolve_pool(read_u16()));
            std::string res_key  = bc_.pool[res_idx].str;
            std::string mod_name = bc_.pool[mod_idx].str;
            std::string fn_name  = bc_.pool[fn_idx].str;
            ClpsValue result = WanWuKu::instance().call(mod_name, fn_name, args);
            cur()->qian(res_key, std::move(result));
            break;
        }

        // ── 震·库调用（纯副作用）────────────────────────────────────
        case Op::ZHEN_SFX: {
            uint16_t mod_idx = read_u16();
            uint16_t fn_idx  = read_u16();
            uint8_t  argc    = read_u8();
            std::vector<ClpsValue> args;
            for (int i = 0; i < argc; ++i)
                args.push_back(resolve_pool(read_u16()));
            std::string mod_name = bc_.pool[mod_idx].str;
            std::string fn_name  = bc_.pool[fn_idx].str;
            WanWuKu::instance().call(mod_name, fn_name, args);
            break;
        }

        // ── 离·跨界引用 ─────────────────────────────────────────────
        case Op::LI: {
            uint16_t key_idx   = read_u16();
            uint16_t scope_idx = read_u16();
            uint16_t src_idx   = read_u16();
            std::string key        = bc_.pool[key_idx].str;
            std::string scope_name = bc_.pool[scope_idx].str;
            std::string src_key    = bc_.pool[src_idx].str;

            std::shared_ptr<TaiJiJieJie> src_jj;
            for (auto it = scope_stack_.rbegin(); it != scope_stack_.rend(); ++it) {
                if ((*it)->name == scope_name) { src_jj = *it; break; }
            }
            if (!src_jj) {
                auto it = modules_.find(scope_name);
                if (it != modules_.end()) src_jj = it->second;
            }
            if (!src_jj)
                vm_throw("离卦：找不到结界或模块 \"" + scope_name + "\"");
            cur()->li(key, src_jj, src_key);
            break;
        }

        // ── 兑·输出 ─────────────────────────────────────────────────
        // 兑卦·字面量兜底：key 不在作用域 → 以 key 本名直接输出
        case Op::DUI: {
            uint16_t key_idx = read_u16();
            std::string key  = bc_.pool[key_idx].str;
            auto* scope = cur().get();
            bool found = (scope->resources.count(key) > 0 ||
                          scope->refs.count(key) > 0);
            if (!found) {
                emit("[兑·显像] " + key);
                break;
            }
            ClpsValue val = scope->dui(key);
            std::ostringstream oss;
            oss << "[兑·显像] " << key << " => " << val.repr();
            emit(oss.str());
            break;
        }

        // ── 乾·存入 ─────────────────────────────────────────────────
        case Op::QIAN: {
            uint16_t key_idx = read_u16();
            uint16_t val_idx = read_u16();
            std::string key  = bc_.pool[key_idx].str;
            ClpsValue val    = resolve_pool(val_idx);
            cur()->qian(key, std::move(val));
            break;
        }

        case Op::QIAN_WX: {
            uint16_t key_idx = read_u16();
            uint16_t val_idx = read_u16();
            uint8_t  wx_val  = read_u8();
            std::string key  = bc_.pool[key_idx].str;
            ClpsValue val    = resolve_pool(val_idx);
            static const WX wx_tab[] = {WX::Jin, WX::Shui, WX::Mu, WX::Huo, WX::Tu};
            cur()->qian(key, std::move(val), wx_tab[wx_val % 5]);
            break;
        }

        // ── 恒·只读常量 ─────────────────────────────────────────────
        case Op::HENG: {
            uint16_t key_idx = read_u16();
            uint16_t val_idx = read_u16();
            std::string key  = bc_.pool[key_idx].str;
            ClpsValue val    = resolve_pool(val_idx);
            cur()->heng(key, std::move(val));
            emit("[恒·常] \"" + key + "\" 已锁为只读常量");
            break;
        }
        case Op::HENG_WX: {
            uint16_t key_idx = read_u16();
            uint16_t val_idx = read_u16();
            uint8_t  wx_val  = read_u8();
            std::string key  = bc_.pool[key_idx].str;
            ClpsValue val    = resolve_pool(val_idx);
            static const WX wx_tab[] = {WX::Jin, WX::Shui, WX::Mu, WX::Huo, WX::Tu};
            cur()->heng(key, std::move(val), wx_tab[wx_val % 5]);
            emit("[恒·常] \"" + key + "\" 已锁为只读常量（显式五行）");
            break;
        }

        // ── 蹇·错误恢复 ─────────────────────────────────────────────
        case Op::JIAN_BEG: {
            uint32_t try_sz    = read_u32();
            uint16_t catch_key = read_u16();
            uint32_t catch_sz  = read_u32();
            size_t   try_end   = ip_ + try_sz;
            size_t   after_all = try_end + catch_sz;

            std::string err_msg;
            bool threw = false;
            try {
                while (ip_ < try_end) step();
            } catch (const std::exception& e) {
                threw = true;
                err_msg = e.what();
                ip_ = try_end; // 跳到 catch 体起始
            }

            if (!threw) {
                ip_ = after_all; // 跳过 catch 体
            } else {
                if (catch_key != POOL_NONE) {
                    cur()->qian(bc_.pool[catch_key].str, ClpsValue(err_msg));
                    emit("[蹇·化] 捕获错误 → \"" + bc_.pool[catch_key].str + "\"：" + err_msg);
                } else {
                    emit("[蹇·化] 错误已化解：" + err_msg);
                }
                // ip_ = try_end → catch 体由主循环自然执行
            }
            break;
        }

        // ── 小畜·列表积累 ───────────────────────────────────────────
        case Op::XIAO_XU: {
            uint16_t key_idx = read_u16();
            uint16_t val_idx = read_u16();
            std::string key  = bc_.pool[key_idx].str;
            std::optional<ClpsValue> val;
            if (val_idx != POOL_NONE) val = resolve_pool(val_idx);
            cur()->xiao_xu(key, std::move(val));
            if (val) {
                emit("[小畜·积] \"" + key + "\" ← 一项");
            } else {
                emit("[小畜·辟] \"" + key + "\" 空列已辟");
            }
            break;
        }

        // ── 益·定义 ─────────────────────────────────────────────────
        case Op::YI_DEF: {
            uint16_t name_idx = read_u16();
            uint8_t  nparams  = read_u8();
            std::string name  = bc_.pool[name_idx].str;

            std::vector<std::string> params;
            for (uint8_t i = 0; i < nparams; ++i) {
                uint16_t pi = read_u16();
                params.push_back(bc_.pool[pi].str);
            }
            uint32_t body_sz = read_u32();

            size_t body_start = ip_;          // body 第一条指令
            size_t body_end   = ip_ + body_sz - 1; // YI_RET 的位置

            yi_defs_[name] = YiVmDef{params, body_start, body_end};
            ip_ += body_sz;                   // 跳过 body + YI_RET
            emit("[益·定] 子程序 \"" + name + "\" 已定义");
            break;
        }

        // ── 益·调用 ─────────────────────────────────────────────────
        case Op::YI_CALL: {
            uint16_t name_idx = read_u16();
            uint8_t  nargs    = read_u8();
            std::string name  = bc_.pool[name_idx].str;

            auto it = yi_defs_.find(name);
            if (it == yi_defs_.end())
                vm_throw("益卦：未定义的子程序 \"" + name + "\"");

            const YiVmDef& def = it->second;

            // 绑定参数（乾的语义）
            for (uint8_t i = 0; i < nargs; ++i) {
                uint16_t val_idx = read_u16();
                if (i < def.params.size()) {
                    cur()->qian(def.params[i], resolve_pool(val_idx));
                }
                // 多余参数静默丢弃
            }
            // 若调用时参数不足（定义有参，调用无参），跳过绑定
            for (uint8_t i = nargs; i < (uint8_t)def.params.size(); ++i) {
                // 参数不足时保持现有作用域值（调用者提前 乾 的兜底）
            }

            yi_call_stack_.push_back(ip_); // 保存返回地址
            ip_ = def.body_start;
            emit("[益·召] 子程序 \"" + name + "\" 开始");
            break;
        }

        // ── 益·返回 ─────────────────────────────────────────────────
        case Op::YI_RET: {
            if (yi_call_stack_.empty())
                vm_throw("益卦：YI_RET 无对应调用");
            // 取出返回地址，弹出栈
            std::string ret_name;
            // 找当前调用的子程序名（用于 emit）
            for (auto& [n, d] : yi_defs_) {
                if (ip_ - 1 == d.body_end) { ret_name = n; break; }
            }
            ip_ = yi_call_stack_.back();
            yi_call_stack_.pop_back();
            if (!ret_name.empty())
                emit("[益·归] 子程序 \"" + ret_name + "\" 结束");
            else
                emit("[益·归] 子程序返回");
            break;
        }

        default:
            vm_throw("VM：未知操作码 0x" +
                [opcode]{ std::ostringstream o; o << std::hex << (int)opcode; return o.str(); }());
        }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  主执行循环
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void ClpsVM::run() {
    auto add_line = [&](const std::string& msg) -> std::string {
        if (cur_line_ == 0) return msg;
        if (msg.rfind("[第", 0) == 0) return msg;
        return "[第" + std::to_string(cur_line_) + "行] " + msg;
    };

    try {
        while (ip_ < bc_.code.size()) step();
    } catch (const ClpsKezError& e) {
        throw ClpsKezError(add_line(e.what()));
    } catch (const ClpsError& e) {
        throw ClpsError(add_line(e.what()));
    } catch (const std::exception& e) {
        throw ClpsError(add_line(e.what()));
    }
}
