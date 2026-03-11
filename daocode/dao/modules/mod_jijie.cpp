// mod_jijie.cpp — 道·码万物库子模块
#include "../wanwu.hpp"
#include "mod_helpers.hpp"
#include <algorithm>
#include <set>
#include <unordered_set>
#include <numeric>
#include <sstream>
#include <mutex>
#include <deque>

static void reg_di_tian_tai() {
    auto& ku = WanWuKu::instance();

    // 行流 "文字" — 按 \n 切分，返回列表（过滤空行）
    ku.reg("地天泰卦", "行流", WX::Mu, [](const std::vector<ClpsValue>& args) {
        std::string text = args.empty() ? "" : to_str(args[0]);
        auto list = std::make_shared<ClpsList>();
        std::istringstream ss(text);
        std::string line;
        while (std::getline(ss, line)) {
            // 去掉末尾 \r
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!line.empty()) list->push_back(ClpsValue(line));
        }
        return ClpsValue(std::move(list));
    });

    // 劈 "文字" "分隔符" — 按分隔符切分，返回列表
    ku.reg("地天泰卦", "劈", WX::Mu, [](const std::vector<ClpsValue>& args) {
        std::string text = args.size() >= 1 ? to_str(args[0]) : "";
        std::string sep  = args.size() >= 2 ? to_str(args[1]) : " ";
        auto list = std::make_shared<ClpsList>();
        if (sep.empty()) {
            // 无分隔符：每字符一项
            size_t i = 0;
            while (i < text.size()) {
                unsigned char c = text[i];
                size_t w = (c < 0x80) ? 1 : (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 4;
                list->push_back(ClpsValue(text.substr(i, w)));
                i += w;
            }
        } else {
            size_t pos = 0, found;
            while ((found = text.find(sep, pos)) != std::string::npos) {
                list->push_back(ClpsValue(text.substr(pos, found - pos)));
                pos = found + sep.size();
            }
            list->push_back(ClpsValue(text.substr(pos)));
        }
        return ClpsValue(std::move(list));
    });

    // 汇 list "分隔符" — 列表合成字符串
    // 参数：ClpsList（IDENT 查词典传入），分隔符字符串
    ku.reg("地天泰卦", "汇", WX::Huo, [](const std::vector<ClpsValue>& args) {
        if (args.empty()) return ClpsValue(std::string(""));
        std::string sep = args.size() >= 2 ? to_str(args[1]) : "";
        // 第一个参数可以是列表
        if (auto* lp = std::get_if<std::shared_ptr<ClpsList>>(&args[0].data)) {
            std::string result;
            for (size_t i = 0; i < (*lp)->size(); ++i) {
                if (i > 0) result += sep;
                result += to_str((**lp)[i]);
            }
            return ClpsValue(result);
        }
        // 非列表直接转字符串
        return ClpsValue(to_str(args[0]));
    });

    // 含 "文字" "子串" — 包含则返回 1，否则返回 0
    ku.reg("地天泰卦", "含", WX::Jin, [](const std::vector<ClpsValue>& args) {
        std::string text = args.size() >= 1 ? to_str(args[0]) : "";
        std::string sub  = args.size() >= 2 ? to_str(args[1]) : "";
        return ClpsValue(static_cast<int64_t>(text.find(sub) != std::string::npos ? 1 : 0));
    });

    // 换 "文字" "旧" "新" — 替换所有出现
    ku.reg("地天泰卦", "换", WX::Huo, [](const std::vector<ClpsValue>& args) {
        std::string text = args.size() >= 1 ? to_str(args[0]) : "";
        std::string from = args.size() >= 2 ? to_str(args[1]) : "";
        std::string to_s = args.size() >= 3 ? to_str(args[2]) : "";
        if (from.empty()) return ClpsValue(text);
        size_t pos = 0;
        while ((pos = text.find(from, pos)) != std::string::npos) {
            text.replace(pos, from.size(), to_s);
            pos += to_s.size();
        }
        return ClpsValue(text);
    });
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  【天泽履卦】— 履卦(10)，系统调用
//  卦义：上天下泽，履；君子以辩上下，定民志
//  实现：popen / getenv / getpid
//  原则：履虎尾不咥人 — 甲方程序表里如一声明系统调用意图，
//        OS（乙方）决定是否放行
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

static std::optional<double> to_num(const ClpsValue& v) {
    if (auto* p = std::get_if<int64_t>(&v.data)) return static_cast<double>(*p);
    if (auto* p = std::get_if<double>  (&v.data)) return *p;
    return std::nullopt;
}

static void reg_da_you_huo_tian() {
    auto& ku = WanWuKu::instance();

    // 总 list — 求所有数值元素之和
    // 若全为整数返回金（int），含浮点返回水（double）
    ku.reg("火天大有卦", "总", WX::Jin, [](const std::vector<ClpsValue>& args) {
        if (args.empty()) return ClpsValue(int64_t(0));
        auto* lp = std::get_if<std::shared_ptr<ClpsList>>(&args[0].data);
        if (!lp) return ClpsValue(int64_t(0));
        int64_t sum_i = 0;
        double  sum_d = 0.0;
        bool    has_float = false;
        for (auto& v : **lp) {
            if (auto n = to_num(v)) {
                if (std::get_if<double>(&v.data)) has_float = true;
                sum_i += std::get_if<int64_t>(&v.data) ? *std::get_if<int64_t>(&v.data) : 0;
                sum_d += *n;
            }
        }
        return has_float ? ClpsValue(sum_d) : ClpsValue(sum_i);
    });

    // 极大 list — 最大数值
    ku.reg("火天大有卦", "极大", WX::Jin, [](const std::vector<ClpsValue>& args) {
        if (args.empty()) return ClpsValue(int64_t(0));
        auto* lp = std::get_if<std::shared_ptr<ClpsList>>(&args[0].data);
        if (!lp || (*lp)->empty()) return ClpsValue(int64_t(0));
        double mx = std::numeric_limits<double>::lowest();
        bool   has_float = false, found = false;
        for (auto& v : **lp) {
            if (auto n = to_num(v)) {
                if (*n > mx) mx = *n;
                if (std::get_if<double>(&v.data)) has_float = true;
                found = true;
            }
        }
        if (!found) return ClpsValue(int64_t(0));
        return has_float ? ClpsValue(mx)
                         : ClpsValue(static_cast<int64_t>(mx));
    });

    // 极小 list — 最小数值
    ku.reg("火天大有卦", "极小", WX::Jin, [](const std::vector<ClpsValue>& args) {
        if (args.empty()) return ClpsValue(int64_t(0));
        auto* lp = std::get_if<std::shared_ptr<ClpsList>>(&args[0].data);
        if (!lp || (*lp)->empty()) return ClpsValue(int64_t(0));
        double mn = std::numeric_limits<double>::max();
        bool   has_float = false, found = false;
        for (auto& v : **lp) {
            if (auto n = to_num(v)) {
                if (*n < mn) mn = *n;
                if (std::get_if<double>(&v.data)) has_float = true;
                found = true;
            }
        }
        if (!found) return ClpsValue(int64_t(0));
        return has_float ? ClpsValue(mn)
                         : ClpsValue(static_cast<int64_t>(mn));
    });

    // 均值 list — 算术平均（返回水·double）
    ku.reg("火天大有卦", "均值", WX::Shui, [](const std::vector<ClpsValue>& args) {
        if (args.empty()) return ClpsValue(0.0);
        auto* lp = std::get_if<std::shared_ptr<ClpsList>>(&args[0].data);
        if (!lp || (*lp)->empty()) return ClpsValue(0.0);
        double sum = 0.0;
        int64_t cnt = 0;
        for (auto& v : **lp) {
            if (auto n = to_num(v)) { sum += *n; ++cnt; }
        }
        return ClpsValue(cnt > 0 ? sum / cnt : 0.0);
    });

    // 积 list — 连乘（累积积）
    ku.reg("火天大有卦", "积", WX::Jin, [](const std::vector<ClpsValue>& args) {
        if (args.empty()) return ClpsValue(int64_t(1));
        auto* lp = std::get_if<std::shared_ptr<ClpsList>>(&args[0].data);
        if (!lp || (*lp)->empty()) return ClpsValue(int64_t(1));
        int64_t prod_i = 1;
        double  prod_d = 1.0;
        bool    has_float = false;
        for (auto& v : **lp) {
            if (auto n = to_num(v)) {
                if (std::get_if<double>(&v.data)) has_float = true;
                if (!has_float) prod_i *= std::get_if<int64_t>(&v.data)
                                          ? *std::get_if<int64_t>(&v.data) : 1;
                prod_d *= *n;
            }
        }
        return has_float ? ClpsValue(prod_d) : ClpsValue(prod_i);
    });
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  【山泽损卦】— 损(41)，滑动窗口统计
//  卦义：损下益上，其道上行。损之，又损，以至无为。
//  函数：窗/入/数/和/均/最小/最大/方差/列/清
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

struct SunWindow {
    std::mutex           mx;
    std::deque<double>   buf;
    size_t               cap;  // 0 = 无限

    explicit SunWindow(size_t c = 0) : cap(c) {}

    void push(double v) {
        std::lock_guard<std::mutex> lk(mx);
        buf.push_back(v);
        if (cap > 0 && buf.size() > cap) buf.pop_front();
    }
    int64_t count() {
        std::lock_guard<std::mutex> lk(mx);
        return (int64_t)buf.size();
    }
    double sum_v() {
        std::lock_guard<std::mutex> lk(mx);
        double s = 0; for (auto x : buf) s += x; return s;
    }
    double mean_v() {
        std::lock_guard<std::mutex> lk(mx);
        if (buf.empty()) return 0.0;
        double s = 0; for (auto x : buf) s += x; return s / buf.size();
    }
    double min_v() {
        std::lock_guard<std::mutex> lk(mx);
        if (buf.empty()) return 0.0;
        return *std::min_element(buf.begin(), buf.end());
    }
    double max_v() {
        std::lock_guard<std::mutex> lk(mx);
        if (buf.empty()) return 0.0;
        return *std::max_element(buf.begin(), buf.end());
    }
    double variance_v() {
        std::lock_guard<std::mutex> lk(mx);
        if (buf.empty()) return 0.0;
        double m = 0; for (auto x : buf) m += x; m /= buf.size();
        double v = 0; for (auto x : buf) v += (x - m) * (x - m);
        return v / buf.size();
    }
    std::shared_ptr<ClpsList> tail_n(int64_t n) {
        std::lock_guard<std::mutex> lk(mx);
        auto lst = std::make_shared<ClpsList>();
        auto it = buf.begin();
        if (n > 0 && (size_t)n < buf.size()) it = buf.end() - n;
        for (; it != buf.end(); ++it) lst->push_back(ClpsValue(*it));
        return lst;
    }
    void clear_data() {
        std::lock_guard<std::mutex> lk(mx);
        buf.clear();
    }
};

static std::mutex g_sun_mx;
static std::unordered_map<std::string, std::shared_ptr<SunWindow>> g_sun_windows;

static std::shared_ptr<SunWindow> sun_get(const std::string& name) {
    std::lock_guard<std::mutex> lk(g_sun_mx);
    auto it = g_sun_windows.find(name);
    if (it != g_sun_windows.end()) return it->second;
    auto w = std::make_shared<SunWindow>(0);
    g_sun_windows[name] = w;
    return w;
}

static void reg_shan_ze_sun() {
    auto& ku = WanWuKu::instance();

    // 窗 "name" [cap] → 1  建立滑动窗口（cap=0 无限）
    ku.reg("山泽损卦", "窗", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string name = a.size()>=1 ? to_str(a[0]) : "w";
        size_t cap = a.size()>=2 ? (size_t)to_int(a[1]) : 0;
        std::lock_guard<std::mutex> lk(g_sun_mx);
        g_sun_windows[name] = std::make_shared<SunWindow>(cap);
        return ClpsValue(int64_t(1));
    });

    // 入 "name" val → count  推入一个值
    ku.reg("山泽损卦", "入", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string name = a.size()>=1 ? to_str(a[0]) : "w";
        double val = a.size()>=2 ? to_double(a[1]) : 0.0;
        auto w = sun_get(name);
        w->push(val);
        return ClpsValue(w->count());
    });

    // 数 "name" → count
    ku.reg("山泽损卦", "数", WX::Jin, [](const std::vector<ClpsValue>& a) {
        return ClpsValue(sun_get(a.empty() ? "w" : to_str(a[0]))->count());
    });

    // 和 "name" → sum
    ku.reg("山泽损卦", "和", WX::Shui, [](const std::vector<ClpsValue>& a) {
        return ClpsValue(sun_get(a.empty() ? "w" : to_str(a[0]))->sum_v());
    });

    // 均 "name" → mean
    ku.reg("山泽损卦", "均", WX::Shui, [](const std::vector<ClpsValue>& a) {
        return ClpsValue(sun_get(a.empty() ? "w" : to_str(a[0]))->mean_v());
    });

    // 最小 "name" → min
    ku.reg("山泽损卦", "最小", WX::Shui, [](const std::vector<ClpsValue>& a) {
        return ClpsValue(sun_get(a.empty() ? "w" : to_str(a[0]))->min_v());
    });

    // 最大 "name" → max
    ku.reg("山泽损卦", "最大", WX::Shui, [](const std::vector<ClpsValue>& a) {
        return ClpsValue(sun_get(a.empty() ? "w" : to_str(a[0]))->max_v());
    });

    // 方差 "name" → variance
    ku.reg("山泽损卦", "方差", WX::Shui, [](const std::vector<ClpsValue>& a) {
        return ClpsValue(sun_get(a.empty() ? "w" : to_str(a[0]))->variance_v());
    });

    // 列 "name" [n] → ClpsList（最近n个，n=0全部）
    ku.reg("山泽损卦", "列", WX::Mu, [](const std::vector<ClpsValue>& a) {
        std::string name = a.size()>=1 ? to_str(a[0]) : "w";
        int64_t n = a.size()>=2 ? to_int(a[1]) : 0;
        auto lst = sun_get(name)->tail_n(n);
        return ClpsValue(lst);
    });

    // 清 "name" → 1  清空窗口
    ku.reg("山泽损卦", "清", WX::Jin, [](const std::vector<ClpsValue>& a) {
        sun_get(a.empty() ? "w" : to_str(a[0]))->clear_data();
        return ClpsValue(int64_t(1));
    });
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  【风雷益卦】— 益(42)，集合操作
//  卦义：益，利有攸往，利涉大川。
//  益=增益=集合的扩充与提炼；并交差 = 益损之道
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// 将 ClpsValue 转为有序字符串 set（去重比较用）
static std::set<std::string> clps_to_set(const ClpsValue& v) {
    std::set<std::string> s;
    if (auto* lst = std::get_if<std::shared_ptr<ClpsList>>(&v.data)) {
        for (auto& item : **lst) s.insert(to_str(item));
    } else {
        std::string sv = to_str(v);
        if (!sv.empty()) s.insert(sv);
    }
    return s;
}

static void reg_feng_lei_yi() {
    auto& ku = WanWuKu::instance();

    // 并 list1 list2 → ClpsList  集合并（去重）
    ku.reg("风雷益卦", "并", WX::Mu, [](const std::vector<ClpsValue>& a) {
        auto s1 = a.size()>=1 ? clps_to_set(a[0]) : std::set<std::string>{};
        auto s2 = a.size()>=2 ? clps_to_set(a[1]) : std::set<std::string>{};
        auto lst = std::make_shared<ClpsList>();
        std::set<std::string> merged;
        for (auto& x : s1) merged.insert(x);
        for (auto& x : s2) merged.insert(x);
        for (auto& x : merged) lst->push_back(ClpsValue(x));
        return ClpsValue(lst);
    });

    // 交 list1 list2 → ClpsList  集合交
    ku.reg("风雷益卦", "交", WX::Mu, [](const std::vector<ClpsValue>& a) {
        auto s1 = a.size()>=1 ? clps_to_set(a[0]) : std::set<std::string>{};
        auto s2 = a.size()>=2 ? clps_to_set(a[1]) : std::set<std::string>{};
        auto lst = std::make_shared<ClpsList>();
        for (auto& x : s1) if (s2.count(x)) lst->push_back(ClpsValue(x));
        return ClpsValue(lst);
    });

    // 差 list1 list2 → ClpsList  集合差（在list1但不在list2）
    ku.reg("风雷益卦", "差", WX::Mu, [](const std::vector<ClpsValue>& a) {
        auto s1 = a.size()>=1 ? clps_to_set(a[0]) : std::set<std::string>{};
        auto s2 = a.size()>=2 ? clps_to_set(a[1]) : std::set<std::string>{};
        auto lst = std::make_shared<ClpsList>();
        for (auto& x : s1) if (!s2.count(x)) lst->push_back(ClpsValue(x));
        return ClpsValue(lst);
    });

    // 含 list elem → 1=含有 0=无
    ku.reg("风雷益卦", "含", WX::Jin, [](const std::vector<ClpsValue>& a) {
        auto s = a.size()>=1 ? clps_to_set(a[0]) : std::set<std::string>{};
        std::string elem = a.size()>=2 ? to_str(a[1]) : "";
        return ClpsValue(int64_t(s.count(elem) ? 1 : 0));
    });

    // 去重 list → ClpsList  去除重复元素（保序）
    ku.reg("风雷益卦", "去重", WX::Mu, [](const std::vector<ClpsValue>& a) {
        auto lst = std::make_shared<ClpsList>();
        if (a.empty()) return ClpsValue(lst);
        std::unordered_set<std::string> seen;
        if (auto* l = std::get_if<std::shared_ptr<ClpsList>>(&a[0].data)) {
            for (auto& item : **l) {
                std::string sv = to_str(item);
                if (seen.insert(sv).second) lst->push_back(item);
            }
        }
        return ClpsValue(lst);
    });

    // 子集 list1 list2 → 1=list1是list2子集 0=否
    ku.reg("风雷益卦", "子集", WX::Jin, [](const std::vector<ClpsValue>& a) {
        auto s1 = a.size()>=1 ? clps_to_set(a[0]) : std::set<std::string>{};
        auto s2 = a.size()>=2 ? clps_to_set(a[1]) : std::set<std::string>{};
        for (auto& x : s1) if (!s2.count(x)) return ClpsValue(int64_t(0));
        return ClpsValue(int64_t(1));
    });

    // 个数 list → count（支持 ClpsList 或 string）
    ku.reg("风雷益卦", "个数", WX::Jin, [](const std::vector<ClpsValue>& a) {
        if (a.empty()) return ClpsValue(int64_t(0));
        if (auto* l = std::get_if<std::shared_ptr<ClpsList>>(&a[0].data))
            return ClpsValue(int64_t((*l)->size()));
        return ClpsValue(int64_t(0));
    });

    // 排序 list [asc] → ClpsList（升序，asc默认1，0=降序）
    ku.reg("风雷益卦", "排序", WX::Mu, [](const std::vector<ClpsValue>& a) {
        auto lst = std::make_shared<ClpsList>();
        if (a.empty()) return ClpsValue(lst);
        bool asc = a.size() < 2 || to_int(a[1]) != 0;
        if (auto* l = std::get_if<std::shared_ptr<ClpsList>>(&a[0].data)) {
            std::vector<std::string> v;
            for (auto& item : **l) v.push_back(to_str(item));
            std::sort(v.begin(), v.end());
            if (!asc) std::reverse(v.begin(), v.end());
            for (auto& s : v) lst->push_back(ClpsValue(s));
        }
        return ClpsValue(lst);
    });
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  【巽为风卦】— 巽(57)，数据管道/列表变换
//  卦义：随风，巽；君子以申命行事。
//  巽=风=渗透=变换；数据如风，流经管道而改形
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

static void reg_xun_feng() {
    auto& ku = WanWuKu::instance();

    // 取 list n → 前n个元素
    ku.reg("巽为风卦", "取", WX::Mu, [](const std::vector<ClpsValue>& a) {
        auto lst = std::make_shared<ClpsList>();
        if (a.empty()) return ClpsValue(lst);
        auto src = xun_get_list(a[0]);
        int64_t n = a.size()>=2 ? to_int(a[1]) : (int64_t)src->size();
        if (n<0) n=0; if (n>(int64_t)src->size()) n=(int64_t)src->size();
        for (int64_t i=0;i<n;i++) lst->push_back((*src)[i]);
        return ClpsValue(lst);
    });

    // 跳 list n → 跳过前n个
    ku.reg("巽为风卦", "跳", WX::Mu, [](const std::vector<ClpsValue>& a) {
        auto lst = std::make_shared<ClpsList>();
        if (a.empty()) return ClpsValue(lst);
        auto src = xun_get_list(a[0]);
        int64_t n = a.size()>=2 ? to_int(a[1]) : 0;
        if (n<0) n=0; if (n>(int64_t)src->size()) n=(int64_t)src->size();
        for (size_t i=(size_t)n;i<src->size();i++) lst->push_back((*src)[i]);
        return ClpsValue(lst);
    });

    // 反 list → 反转列表
    ku.reg("巽为风卦", "反", WX::Mu, [](const std::vector<ClpsValue>& a) {
        auto src = a.empty() ? std::make_shared<ClpsList>() : xun_get_list(a[0]);
        auto lst = std::make_shared<ClpsList>(*src);
        std::reverse(lst->begin(), lst->end());
        return ClpsValue(lst);
    });

    // 截 list start end → list[start:end)
    ku.reg("巽为风卦", "截", WX::Mu, [](const std::vector<ClpsValue>& a) {
        auto lst = std::make_shared<ClpsList>();
        if (a.empty()) return ClpsValue(lst);
        auto src = xun_get_list(a[0]);
        int64_t start = a.size()>=2 ? to_int(a[1]) : 0;
        int64_t end   = a.size()>=3 ? to_int(a[2]) : (int64_t)src->size();
        if (start<0) start=0; if (end>(int64_t)src->size()) end=(int64_t)src->size();
        for (int64_t i=start;i<end;i++) lst->push_back((*src)[i]);
        return ClpsValue(lst);
    });

    // 连 list1 list2 → 拼接两个列表
    ku.reg("巽为风卦", "连", WX::Mu, [](const std::vector<ClpsValue>& a) {
        auto lst = std::make_shared<ClpsList>();
        for (auto& arg : a) {
            if (auto* l = std::get_if<std::shared_ptr<ClpsList>>(&arg.data))
                for (auto& item : **l) lst->push_back(item);
        }
        return ClpsValue(lst);
    });

    // 扁 list → 展平一层（元素为list则展开，否则保留）
    ku.reg("巽为风卦", "扁", WX::Mu, [](const std::vector<ClpsValue>& a) {
        auto lst = std::make_shared<ClpsList>();
        if (a.empty()) return ClpsValue(lst);
        auto src = xun_get_list(a[0]);
        for (auto& item : *src) {
            if (auto* sub = std::get_if<std::shared_ptr<ClpsList>>(&item.data))
                for (auto& x : **sub) lst->push_back(x);
            else lst->push_back(item);
        }
        return ClpsValue(lst);
    });

    // 去空 list → 过滤掉空字符串和None
    ku.reg("巽为风卦", "去空", WX::Mu, [](const std::vector<ClpsValue>& a) {
        auto lst = std::make_shared<ClpsList>();
        if (a.empty()) return ClpsValue(lst);
        auto src = xun_get_list(a[0]);
        for (auto& item : *src) {
            bool empty = std::holds_alternative<std::monostate>(item.data);
            if (!empty) {
                if (auto* s = std::get_if<std::string>(&item.data)) empty = s->empty();
            }
            if (!empty) lst->push_back(item);
        }
        return ClpsValue(lst);
    });

    // 映加 list n → 所有数值元素加n（非数值保留原值）
    ku.reg("巽为风卦", "映加", WX::Mu, [](const std::vector<ClpsValue>& a) {
        auto lst = std::make_shared<ClpsList>();
        if (a.empty()) return ClpsValue(lst);
        auto src = xun_get_list(a[0]);
        double n = a.size()>=2 ? to_double(a[1]) : 0.0;
        bool is_int = !a.empty() && a.size()>=2 && std::holds_alternative<int64_t>(a[1].data);
        for (auto& item : *src) {
            if (auto* iv = std::get_if<int64_t>(&item.data))
                lst->push_back(ClpsValue(is_int ? ClpsValue(*iv + to_int(a[1])) : ClpsValue(*iv + n)));
            else if (auto* dv = std::get_if<double>(&item.data))
                lst->push_back(ClpsValue(*dv + n));
            else lst->push_back(item);
        }
        return ClpsValue(lst);
    });

    // 映乘 list n → 所有数值元素乘n
    ku.reg("巽为风卦", "映乘", WX::Mu, [](const std::vector<ClpsValue>& a) {
        auto lst = std::make_shared<ClpsList>();
        if (a.empty()) return ClpsValue(lst);
        auto src = xun_get_list(a[0]);
        double n = a.size()>=2 ? to_double(a[1]) : 1.0;
        for (auto& item : *src) {
            if (auto* iv = std::get_if<int64_t>(&item.data)) lst->push_back(ClpsValue((double)(*iv) * n));
            else if (auto* dv = std::get_if<double>(&item.data)) lst->push_back(ClpsValue(*dv * n));
            else lst->push_back(item);
        }
        return ClpsValue(lst);
    });

    // 范围滤 list min max → 只保留 min<=val<=max 的数值元素
    ku.reg("巽为风卦", "范围滤", WX::Mu, [](const std::vector<ClpsValue>& a) {
        auto lst = std::make_shared<ClpsList>();
        if (a.empty()) return ClpsValue(lst);
        auto src = xun_get_list(a[0]);
        double lo = a.size()>=2 ? to_double(a[1]) : -1e300;
        double hi = a.size()>=3 ? to_double(a[2]) : 1e300;
        for (auto& item : *src) {
            double v = 0; bool ok = false;
            if (auto* iv = std::get_if<int64_t>(&item.data)) { v=(double)*iv; ok=true; }
            else if (auto* dv = std::get_if<double>(&item.data)) { v=*dv; ok=true; }
            if (ok && v >= lo && v <= hi) lst->push_back(item);
        }
        return ClpsValue(lst);
    });

    // 下标 list → ClpsList of "i:val"  带下标枚举
    ku.reg("巽为风卦", "下标", WX::Mu, [](const std::vector<ClpsValue>& a) {
        auto lst = std::make_shared<ClpsList>();
        if (a.empty()) return ClpsValue(lst);
        auto src = xun_get_list(a[0]);
        for (size_t i=0;i<src->size();i++)
            lst->push_back(ClpsValue(std::to_string(i) + ":" + to_str((*src)[i])));
        return ClpsValue(lst);
    });
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  【风山渐卦】— 渐(53)，序列生成器
//  卦义：山上有木，渐；君子以居贤德善俗。
//  渐=渐进=步步为营；序列即渐进之道的数学映射
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━



static void reg_feng_shan_jian() {
    auto& ku = WanWuKu::instance();

    // 步 start end step → ClpsList 整数范围 [start, end)，步长step
    ku.reg("风山渐卦", "步", WX::Mu, [](const std::vector<ClpsValue>& a) {
        int64_t start = a.size()>=1 ? to_int(a[0]) : 0;
        int64_t end   = a.size()>=2 ? to_int(a[1]) : 0;
        int64_t step  = a.size()>=3 ? to_int(a[2]) : 1;
        if (step == 0) step = 1;
        auto lst = std::make_shared<ClpsList>();
        if (step > 0) for (int64_t i=start; i<end; i+=step) lst->push_back(ClpsValue(i));
        else for (int64_t i=start; i>end; i+=step) lst->push_back(ClpsValue(i));
        return ClpsValue(lst);
    });

    // 重 val n → 重复val共n次
    ku.reg("风山渐卦", "重", WX::Mu, [](const std::vector<ClpsValue>& a) {
        ClpsValue val = a.size()>=1 ? a[0] : ClpsValue{};
        int64_t n = a.size()>=2 ? to_int(a[1]) : 0;
        auto lst = std::make_shared<ClpsList>();
        for (int64_t i=0;i<n;i++) lst->push_back(val);
        return ClpsValue(lst);
    });

    // 等差 start n step → 等差数列（前n项）
    ku.reg("风山渐卦", "等差", WX::Mu, [](const std::vector<ClpsValue>& a) {
        double start = a.size()>=1 ? to_double(a[0]) : 0.0;
        int64_t n    = a.size()>=2 ? to_int(a[1]) : 0;
        double step  = a.size()>=3 ? to_double(a[2]) : 1.0;
        auto lst = std::make_shared<ClpsList>();
        for (int64_t i=0;i<n;i++) lst->push_back(ClpsValue(start + i * step));
        return ClpsValue(lst);
    });

    // 等比 start n ratio → 等比数列（前n项）
    ku.reg("风山渐卦", "等比", WX::Mu, [](const std::vector<ClpsValue>& a) {
        double start = a.size()>=1 ? to_double(a[0]) : 1.0;
        int64_t n    = a.size()>=2 ? to_int(a[1]) : 0;
        double ratio = a.size()>=3 ? to_double(a[2]) : 2.0;
        auto lst = std::make_shared<ClpsList>();
        double v = start;
        for (int64_t i=0;i<n;i++) { lst->push_back(ClpsValue(v)); v *= ratio; }
        return ClpsValue(lst);
    });

    // 斐 n → 前n个斐波那契数（int序列）
    ku.reg("风山渐卦", "斐", WX::Mu, [](const std::vector<ClpsValue>& a) {
        int64_t n = a.empty() ? 10 : to_int(a[0]);
        auto lst = std::make_shared<ClpsList>();
        int64_t a_=0, b_=1;
        for (int64_t i=0;i<n;i++) { lst->push_back(ClpsValue(a_)); int64_t t=a_+b_; a_=b_; b_=t; }
        return ClpsValue(lst);
    });

    // 素数 n → 前n个素数
    ku.reg("风山渐卦", "素数", WX::Mu, [](const std::vector<ClpsValue>& a) {
        int64_t n = a.empty() ? 10 : to_int(a[0]);
        auto lst = std::make_shared<ClpsList>();
        auto is_prime = [](int64_t x) {
            if (x < 2) return false;
            for (int64_t i=2; i*i<=x; i++) if (x%i==0) return false;
            return true;
        };
        for (int64_t x=2; (int64_t)lst->size()<n; x++) if (is_prime(x)) lst->push_back(ClpsValue(x));
        return ClpsValue(lst);
    });

    // 轮 list n → 循环取list中的元素共n个
    ku.reg("风山渐卦", "轮", WX::Mu, [](const std::vector<ClpsValue>& a) {
        auto lst_out = std::make_shared<ClpsList>();
        if (a.empty()) return ClpsValue(lst_out);
        auto src = xun_get_list(a[0]);
        if (src->empty()) return ClpsValue(lst_out);
        int64_t n = a.size()>=2 ? to_int(a[1]) : (int64_t)src->size();
        for (int64_t i=0;i<n;i++) lst_out->push_back((*src)[i % src->size()]);
        return ClpsValue(lst_out);
    });

    // 积 list → 所有数值元素乘积
    ku.reg("风山渐卦", "积", WX::Shui, [](const std::vector<ClpsValue>& a) {
        if (a.empty()) return ClpsValue(int64_t(1));
        auto src = xun_get_list(a[0]);
        double prod = 1.0; bool all_int = true;
        int64_t iprod = 1;
        for (auto& item : *src) {
            if (auto* iv = std::get_if<int64_t>(&item.data)) { iprod *= *iv; prod *= *iv; }
            else if (auto* dv = std::get_if<double>(&item.data)) { prod *= *dv; all_int = false; }
        }
        return all_int ? ClpsValue(iprod) : ClpsValue(prod);
    });

    // 累和 list → ClpsList 累积和（prefix sum）
    ku.reg("风山渐卦", "累和", WX::Mu, [](const std::vector<ClpsValue>& a) {
        auto lst = std::make_shared<ClpsList>();
        if (a.empty()) return ClpsValue(lst);
        auto src = xun_get_list(a[0]);
        double running = 0.0;
        for (auto& item : *src) {
            running += to_double(item);
            lst->push_back(ClpsValue(running));
        }
        return ClpsValue(lst);
    });
}

void init_mod_jijie() {
    reg_di_tian_tai();
    reg_da_you_huo_tian();
    reg_shan_ze_sun();
    reg_feng_lei_yi();
    reg_xun_feng();
    reg_feng_shan_jian();
}
