// mod_xitong.cpp — 道·码万物库子模块
#include "../wanwu.hpp"
#include "mod_helpers.hpp"

#include <chrono>
#include <thread>
#include <cstdlib>
#include <fstream>
#include <cstring>
#include <ctime>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <mutex>
#include <map>
#include <functional>

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  【泽雷随卦】— 随(17)，天干地支·二十四节气
//  卦义：随时之义大矣哉。泽中有雷，随；君子以向晦入宴息。
//  CLPS原生时间：天干地支·月建·时辰·节气·五行
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// ── 天干地支名表
static const char* const GZ_GAN[10]      = {"甲","乙","丙","丁","戊","己","庚","辛","壬","癸"};
static const char* const GZ_ZHI[12]      = {"子","丑","寅","卯","辰","巳","午","未","申","酉","戌","亥"};
static const char* const GZ_SHI_CHEN[24] = {
    "子时","丑时","丑时","寅时","寅时","卯时","卯时",
    "辰时","辰时","巳时","巳时","午时","午时","未时","未时",
    "申时","申时","酉时","酉时","戌时","戌时","亥时","亥时","子时"
};

// ── 二十四节气表（月份1-12，每月2个，按时间顺序）
struct GzJieQi { int month; int day; const char* name; };
static const GzJieQi GZ_JIE_QI[24] = {
    {1, 6,"小寒"}, {1,20,"大寒"},
    {2, 4,"立春"}, {2,19,"雨水"},
    {3, 6,"惊蛰"}, {3,21,"春分"},
    {4, 5,"清明"}, {4,20,"谷雨"},
    {5, 6,"立夏"}, {5,21,"小满"},
    {6, 6,"芒种"}, {6,21,"夏至"},
    {7, 7,"小暑"}, {7,23,"大暑"},
    {8, 8,"立秋"}, {8,23,"处暑"},
    {9, 8,"白露"}, {9,23,"秋分"},
    {10,8,"寒露"}, {10,23,"霜降"},
    {11,7,"立冬"}, {11,22,"小雪"},
    {12,7,"大雪"}, {12,22,"冬至"},
};

// ── 月建：12个"节"的月日 + 对应地支索引
//   小寒→丑(1), 立春→寅(2), 惊蛰→卯(3), 清明→辰(4),
//   立夏→巳(5), 芒种→午(6), 小暑→未(7), 立秋→申(8),
//   白露→酉(9), 寒露→戌(10), 立冬→亥(11), 大雪→子(0)
static const int GZ_JIE_MONTH[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12};
static const int GZ_JIE_DAY[12]   = {6, 4, 6, 5, 6, 6, 7, 8, 8, 8, 7, 7};
static const int GZ_JIE_ZHI[12]   = {1, 2, 3, 4, 5, 6, 7, 8, 9,10,11, 0};

// ── 当前月建地支索引（0=子…11=亥）
static int gz_yue_jian(int month, int day) {
    for (int i = 11; i >= 0; i--) {
        if (month > GZ_JIE_MONTH[i] ||
            (month == GZ_JIE_MONTH[i] && day >= GZ_JIE_DAY[i]))
            return GZ_JIE_ZHI[i];
    }
    return 0; // 子月（1月5日前，大雪后、小寒前）
}

// ── 当前最近节气（已过或今日）
static const char* gz_current_jieqi(int month, int day) {
    for (int i = 23; i >= 0; i--) {
        const GzJieQi& jq = GZ_JIE_QI[i];
        if (month > jq.month || (month == jq.month && day >= jq.day))
            return jq.name;
    }
    return "冬至"; // 1月1-5日，冬至后小寒前
}

// ── 地支索引 → 五行名（中文）
static const char* gz_zhi_to_wx_str(int zhi) {
    switch (zhi) {
        case 0: case 11: return "水"; // 子亥
        case 2: case 3:  return "木"; // 寅卯
        case 5: case 6:  return "火"; // 巳午
        case 8: case 9:  return "金"; // 申酉
        default:         return "土"; // 辰戌丑未
    }
}

static void reg_ze_lei_sui() {
    auto& ku = WanWuKu::instance();

    // 问天·年 → 干支年（如"丙午年"）
    ku.reg("泽雷随卦", "问天·年", WX::Huo, [](const std::vector<ClpsValue>&) {
        time_t t = time(nullptr); struct tm tm{}; localtime_r(&t, &tm);
        int year = tm.tm_year + 1900;
        int gan  = ((year - 4) % 10 + 10) % 10;
        int zhi  = ((year - 4) % 12 + 12) % 12;
        return ClpsValue(std::string(GZ_GAN[gan]) + GZ_ZHI[zhi] + "年");
    });

    // 问天·月 → 月建（如"卯月"）
    ku.reg("泽雷随卦", "问天·月", WX::Huo, [](const std::vector<ClpsValue>&) {
        time_t t = time(nullptr); struct tm tm{}; localtime_r(&t, &tm);
        int zhi = gz_yue_jian(tm.tm_mon + 1, tm.tm_mday);
        return ClpsValue(std::string(GZ_ZHI[zhi]) + "月");
    });

    // 问天·时 → 时辰（如"午时"）
    ku.reg("泽雷随卦", "问天·时", WX::Huo, [](const std::vector<ClpsValue>&) {
        time_t t = time(nullptr); struct tm tm{}; localtime_r(&t, &tm);
        return ClpsValue(std::string(GZ_SHI_CHEN[tm.tm_hour]));
    });

    // 问天·气 → 当前最近节气（如"春分"）
    ku.reg("泽雷随卦", "问天·气", WX::Huo, [](const std::vector<ClpsValue>&) {
        time_t t = time(nullptr); struct tm tm{}; localtime_r(&t, &tm);
        return ClpsValue(std::string(gz_current_jieqi(tm.tm_mon + 1, tm.tm_mday)));
    });

    // 问天·行 → 当前月建五行（如"木"）
    ku.reg("泽雷随卦", "问天·行", WX::Huo, [](const std::vector<ClpsValue>&) {
        time_t t = time(nullptr); struct tm tm{}; localtime_r(&t, &tm);
        int zhi = gz_yue_jian(tm.tm_mon + 1, tm.tm_mday);
        return ClpsValue(std::string(gz_zhi_to_wx_str(zhi)));
    });

    // 干支·年 N → 干支年字符串（公历年转换，如 干支·年 2026 → "丙午年"）
    ku.reg("泽雷随卦", "干支·年", WX::Huo, [](const std::vector<ClpsValue>& a) {
        int year = (int)to_int(a.empty() ? ClpsValue{} : a[0]);
        int gan  = ((year - 4) % 10 + 10) % 10;
        int zhi  = ((year - 4) % 12 + 12) % 12;
        return ClpsValue(std::string(GZ_GAN[gan]) + GZ_ZHI[zhi] + "年");
    });

    // 干支·行 "甲辰" → 五行名（天干五行，如"木"）
    ku.reg("泽雷随卦", "干支·行", WX::Huo, [](const std::vector<ClpsValue>& a) {
        if (a.empty()) throw ClpsError("干支·行：需要干支字符串");
        std::string gz = to_str(a[0]);
        if (gz.empty()) throw ClpsError("干支·行：空字符串");
        // 取第一个字（天干），UTF-8中文字符3字节
        std::string gan3 = gz.substr(0, 3);
        static const char* GAN_WX[10] = {"木","木","火","火","土","土","金","金","水","水"};
        for (int i = 0; i < 10; i++) {
            if (gan3 == GZ_GAN[i]) return ClpsValue(std::string(GAN_WX[i]));
        }
        return ClpsValue(std::string("未知"));
    });

    // ──────────────────────────────────────────────────────
    // 微观时间单位（古法时制·刹那层）
    // 来源：《俱舍论》佛教微观 + 《时宪历》官方历法
    //
    // 以毫秒为锚点，定义六层时间原子：
    //   刹那 =      1 ms  (佛教最小时间单位，计算机最小精度)
    //   弹指 =    100 ms  (弹指之间，人体感知基准)
    //   须臾 =   1000 ms  (1秒，一呼一吸)
    //   古分 =  60000 ms  (与现代分钟完全一致)
    //   刻   = 900000 ms  (15分钟，四刻一时辰)
    //   时辰 = 7200000 ms (2小时，已由问天·时辰覆盖)
    // ──────────────────────────────────────────────────────

    // 时制·刹那 → 1 (ms基准值)
    ku.reg("泽雷随卦", "时制·刹那", WX::Jin, [](const std::vector<ClpsValue>&) {
        return ClpsValue(int64_t(1));
    });
    // 时制·弹指 → 100
    ku.reg("泽雷随卦", "时制·弹指", WX::Jin, [](const std::vector<ClpsValue>&) {
        return ClpsValue(int64_t(100));
    });
    // 时制·须臾 → 1000
    ku.reg("泽雷随卦", "时制·须臾", WX::Jin, [](const std::vector<ClpsValue>&) {
        return ClpsValue(int64_t(1000));
    });
    // 时制·古分 → 60000
    ku.reg("泽雷随卦", "时制·古分", WX::Jin, [](const std::vector<ClpsValue>&) {
        return ClpsValue(int64_t(60000));
    });
    // 时制·刻 → 900000
    ku.reg("泽雷随卦", "时制·刻", WX::Jin, [](const std::vector<ClpsValue>&) {
        return ClpsValue(int64_t(900000));
    });

    // 问天·刹那 → 当前Unix时间戳（毫秒，与刹那同精度）
    ku.reg("泽雷随卦", "问天·刹那", WX::Jin, [](const std::vector<ClpsValue>&) {
        auto now = std::chrono::system_clock::now();
        return ClpsValue(static_cast<int64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count()));
    });

    // 候 N "单位" → 休眠 N 个指定单位
    // 单位字符串：刹那/弹指/须臾/古分/刻
    // 例：候 3 "须臾" = 休眠3秒
    ku.reg("泽雷随卦", "候", WX::Tu, [](const std::vector<ClpsValue>& a) {
        int64_t n    = a.size() >= 1 ? to_int(a[0]) : 1;
        std::string u = a.size() >= 2 ? to_str(a[1]) : "须臾";
        int64_t ms;
        if      (u == "刹那") ms = 1;
        else if (u == "弹指") ms = 100;
        else if (u == "须臾") ms = 1000;
        else if (u == "古分") ms = 60000;
        else if (u == "刻")   ms = 900000;
        else throw ClpsError("候：未知时间单位「" + u + "」，可用：刹那·弹指·须臾·古分·刻");
        std::this_thread::sleep_for(std::chrono::milliseconds(n * ms));
        return ClpsValue{};
    });
}

static void reg_si_shi() {
    auto& ku = WanWuKu::instance();

    // 斗转星移 — 休眠 N 毫秒（默认 1000ms）
    ku.reg("四时革卦", "斗转星移", WX::Tu, [](const std::vector<ClpsValue>& args) {
        int64_t ms = args.empty() ? 1000 : to_int(args[0], 1000);
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        return ClpsValue{};  // None
    });

    // 问天时 — 当前 Unix 时间戳（毫秒精度）→ 整数
    ku.reg("四时革卦", "问天时", WX::Jin, [](const std::vector<ClpsValue>&) {
        auto now = std::chrono::system_clock::now();
        auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(
                       now.time_since_epoch()).count();
        return ClpsValue(static_cast<int64_t>(ms));
    });
}

static void reg_tian_ze_lv() {
    auto& ku = WanWuKu::instance();

    // 执令 "shell命令" — 执行命令，捕获 stdout，返回字符串
    // 履虎尾：可以执行任意命令，OS 决定权限是否足够
    ku.reg("天泽履卦", "执令", WX::Huo, [](const std::vector<ClpsValue>& args) {
        if (args.empty())
            throw ClpsError("天泽履卦·执令：需要命令字符串");
        std::string cmd = to_str(args[0]);
        FILE* fp = ::popen(cmd.c_str(), "r");
        if (!fp)
            throw ClpsError("天泽履卦·执令：无法执行命令 " + cmd);
        std::string result;
        char buf[256];
        while (::fgets(buf, sizeof(buf), fp))
            result += buf;
        ::pclose(fp);
        // 去掉末尾换行
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
            result.pop_back();
        return ClpsValue(result);
    });

    // 环境 "VAR_NAME" — 读取环境变量，返回字符串（不存在则返回空串）
    ku.reg("天泽履卦", "环境", WX::Huo, [](const std::vector<ClpsValue>& args) {
        if (args.empty()) return ClpsValue(std::string(""));
        std::string var = to_str(args[0]);
        const char* val = ::getenv(var.c_str());
        return ClpsValue(val ? std::string(val) : std::string(""));
    });

    // 进程号 — 返回当前进程 PID（整数）
    ku.reg("天泽履卦", "进程号", WX::Jin, [](const std::vector<ClpsValue>&) {
        return ClpsValue(static_cast<int64_t>(::getpid()));
    });
}

static void reg_qian_tian() {
    auto& ku = WanWuKu::instance();

    // 后台 "cmd" → pid（fork+exec，非阻塞）
    ku.reg("乾为天卦", "后台", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string cmd = a.empty() ? "" : to_str(a[0]);
        pid_t pid = fork();
        if (pid == 0) {
            // 子进程：关闭 stdin/stdout/stderr，避免干扰父进程输出
            for (int fd = 0; fd < 3; fd++) close(fd);
            execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
            _exit(127);
        }
        if (pid < 0) throw std::runtime_error("乾卦·后台：fork失败");
        return ClpsValue(static_cast<int64_t>(pid));
    });

    // 等 pid → 退出码（阻塞等待进程结束）
    ku.reg("乾为天卦", "等", WX::Jin, [](const std::vector<ClpsValue>& a) {
        pid_t pid = (pid_t)to_int(a.empty() ? ClpsValue{} : a[0]);
        int status = 0;
        if (waitpid(pid, &status, 0) < 0) return ClpsValue(int64_t(-1));
        return ClpsValue(static_cast<int64_t>(WIFEXITED(status) ? WEXITSTATUS(status) : -1));
    });

    // 在 pid → 1=运行中 0=已结束（非阻塞查询）
    ku.reg("乾为天卦", "在", WX::Jin, [](const std::vector<ClpsValue>& a) {
        pid_t pid = (pid_t)to_int(a.empty() ? ClpsValue{} : a[0]);
        int status = 0;
        int ret = waitpid(pid, &status, WNOHANG);
        return ClpsValue(static_cast<int64_t>(ret == 0 ? 1 : 0));
    });

    // 杀 pid → 1=成功 0=失败（发送 SIGTERM）
    ku.reg("乾为天卦", "杀", WX::Jin, [](const std::vector<ClpsValue>& a) {
        pid_t pid = (pid_t)to_int(a.empty() ? ClpsValue{} : a[0]);
        return ClpsValue(static_cast<int64_t>(::kill(pid, SIGTERM) == 0 ? 1 : 0));
    });

    // 执 "cmd" → stdout 输出（阻塞，与履卦执令功能互补：乾返回完整stderr+stdout）
    // 通过重定向 2>&1 同时捕获 stderr
    ku.reg("乾为天卦", "执", WX::Huo, [](const std::vector<ClpsValue>& a) {
        std::string cmd = (a.empty() ? "" : to_str(a[0])) + " 2>&1";
        FILE* fp = ::popen(cmd.c_str(), "r");
        if (!fp) throw std::runtime_error("乾卦·执：popen失败: " + cmd);
        std::string out;
        char buf[512];
        while (::fgets(buf, sizeof(buf), fp)) out += buf;
        int code = ::pclose(fp);
        // 追加退出码（非0时附加提示）
        if (WIFEXITED(code) && WEXITSTATUS(code) != 0) {
            // 去掉末尾换行再附加
            while (!out.empty() && (out.back()=='\n'||out.back()=='\r')) out.pop_back();
        } else {
            while (!out.empty() && (out.back()=='\n'||out.back()=='\r')) out.pop_back();
        }
        return ClpsValue(out);
    });

    // 此进程号 → 当前进程 PID
    ku.reg("乾为天卦", "此进程号", WX::Jin, [](const std::vector<ClpsValue>&) {
        return ClpsValue(static_cast<int64_t>(::getpid()));
    });
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  【震为雷卦】— 震(51)，定时器/心跳
//  卦义：洊雷，震；君子以恐惧修省。
//  震=雷=动=节律；定时器即宇宙节律在程序中的映射
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

#include <atomic>

// 命名定时器：每个定时器记录创建时刻，支持到期查询
struct ZhenTimer {
    std::chrono::steady_clock::time_point start;
    int64_t interval_ms;   // 0 = 单次；>0 = 周期（取模）
    std::atomic<int64_t>  fired_count{0};  // 到期次数（周期模式）

    explicit ZhenTimer(int64_t ms, bool periodic = false)
        : start(std::chrono::steady_clock::now()),
          interval_ms(periodic ? ms : 0) {}

    // 距创建已过去 ms 数
    int64_t elapsed_ms() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
    }

    // 单次：是否已过 delay_ms
    bool expired_once(int64_t delay_ms) const {
        return elapsed_ms() >= delay_ms;
    }

    // 周期：自上次已过 interval_ms（轮询更新）
    bool tick() {
        if (interval_ms <= 0) return false;
        int64_t el = elapsed_ms();
        int64_t expected = fired_count.load() + 1;
        if (el >= expected * interval_ms) {
            fired_count.store(expected);
            return true;
        }
        return false;
    }
};

static std::mutex g_zhen_mx;
static std::unordered_map<std::string, std::shared_ptr<ZhenTimer>> g_zhen_timers;


static void reg_zhen_lei() {
    auto& ku = WanWuKu::instance();

    // 起 "name" ms → 1  建立单次定时器（ms 毫秒后到期）
    ku.reg("震为雷卦", "起", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string name = a.size() >= 1 ? to_str(a[0]) : "default";
        int64_t ms       = a.size() >= 2 ? to_int(a[1]) : 1000;
        std::lock_guard<std::mutex> lk(g_zhen_mx);
        g_zhen_timers[name] = std::make_shared<ZhenTimer>(ms, false);
        return ClpsValue(int64_t(1));
    });

    // 周 "name" ms → 1  建立周期定时器（每 ms 毫秒一次）
    ku.reg("震为雷卦", "周", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string name = a.size() >= 1 ? to_str(a[0]) : "default";
        int64_t ms       = a.size() >= 2 ? to_int(a[1]) : 1000;
        std::lock_guard<std::mutex> lk(g_zhen_mx);
        g_zhen_timers[name] = std::make_shared<ZhenTimer>(ms, true);
        return ClpsValue(int64_t(1));
    });

    // 到 "name" ms → 1=到期 0=未到（单次判断）
    ku.reg("震为雷卦", "到", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string name = a.size() >= 1 ? to_str(a[0]) : "default";
        int64_t ms       = a.size() >= 2 ? to_int(a[1]) : 0;
        std::shared_ptr<ZhenTimer> t;
        { std::lock_guard<std::mutex> lk(g_zhen_mx); auto it = g_zhen_timers.find(name); if (it != g_zhen_timers.end()) t = it->second; }
        if (!t) return ClpsValue(int64_t(0));
        return ClpsValue(int64_t(t->expired_once(ms) ? 1 : 0));
    });

    // 滴 "name" → 1=本轮到期 0=未到（周期tick，每次只触发一次）
    ku.reg("震为雷卦", "滴", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string name = a.size() >= 1 ? to_str(a[0]) : "default";
        std::shared_ptr<ZhenTimer> t;
        { std::lock_guard<std::mutex> lk(g_zhen_mx); auto it = g_zhen_timers.find(name); if (it != g_zhen_timers.end()) t = it->second; }
        if (!t) return ClpsValue(int64_t(0));
        return ClpsValue(int64_t(t->tick() ? 1 : 0));
    });

    // 歇 ms → ms（阻塞睡眠，释放GIL等价已在顶层；纯副作用）
    ku.reg("震为雷卦", "歇", WX::Jin, [](const std::vector<ClpsValue>& a) {
        int64_t ms = a.empty() ? 0 : to_int(a[0]);
        if (ms > 0) std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        return ClpsValue(ms);
    });

    // 距 "name" → elapsed_ms（自定时器起已过毫秒数）
    ku.reg("震为雷卦", "距", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string name = a.size() >= 1 ? to_str(a[0]) : "default";
        std::shared_ptr<ZhenTimer> t;
        { std::lock_guard<std::mutex> lk(g_zhen_mx); auto it = g_zhen_timers.find(name); if (it != g_zhen_timers.end()) t = it->second; }
        if (!t) return ClpsValue(int64_t(-1));
        return ClpsValue(t->elapsed_ms());
    });

    // 次 "name" → fired_count（周期定时器已触发次数）
    ku.reg("震为雷卦", "次", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string name = a.size() >= 1 ? to_str(a[0]) : "default";
        std::shared_ptr<ZhenTimer> t;
        { std::lock_guard<std::mutex> lk(g_zhen_mx); auto it = g_zhen_timers.find(name); if (it != g_zhen_timers.end()) t = it->second; }
        if (!t) return ClpsValue(int64_t(0));
        return ClpsValue(t->fired_count.load());
    });

    // 撤 "name" → 1  删除定时器
    ku.reg("震为雷卦", "撤", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string name = a.size() >= 1 ? to_str(a[0]) : "default";
        std::lock_guard<std::mutex> lk(g_zhen_mx);
        return ClpsValue(int64_t(g_zhen_timers.erase(name) > 0 ? 1 : 0));
    });
}

void init_mod_xitong() {
    reg_si_shi();
    reg_tian_ze_lv();
    reg_qian_tian();
    reg_zhen_lei();
    reg_ze_lei_sui();   // 泽雷随卦·天干地支·二十四节气
}
