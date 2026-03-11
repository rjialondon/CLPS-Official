// mod_shujuku.cpp — 道·码万物库子模块
#include "../wanwu.hpp"
#include "mod_helpers.hpp"
#include <sqlite3.h>
#include <sstream>
#include <mutex>

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

struct DiZeLin {
    std::mutex mx;
    sqlite3* db = nullptr;
    std::string path;
};
static DiZeLin g_lin;

static void lin_exec_noret(const std::string& sql) {
    if (!g_lin.db) throw ClpsError("地泽临：数据库未开启，请先调用「开库」");
    char* errmsg = nullptr;
    int rc = sqlite3_exec(g_lin.db, sql.c_str(), nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        std::string msg = errmsg ? errmsg : "未知错误";
        sqlite3_free(errmsg);
        throw ClpsError("地泽临：SQL 执行错误: " + msg);
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  归妹(54) 【雷泽归妹卦】— Promise / Future
//  兑下震上，震动而兑悦纳，有期而归
//
//  许诺 name       → void    创建命名 Promise（待兑）
//  履约 name value → void    兑现 Promise（通知等待方）
//  失信 name msg   → void    拒绝 Promise（抛出错误）
//  候约 name [ms]  → value   等待 Promise 兑现（超时返回 None）
//  查约 name       → int     0=待 1=已兑 2=已拒
//  清约            → void    清空所有 Promise
static void reg_di_ze_lin() {
    auto& ku = WanWuKu::instance();

    // 开库 path → void
    ku.reg("地泽临卦", "开库", WX::Tu, [](const std::vector<ClpsValue>& a) {
        std::string path = a.empty() ? ":memory:" : to_str(a[0]);
        std::lock_guard<std::mutex> lk(g_lin.mx);
        if (g_lin.db) { sqlite3_close(g_lin.db); g_lin.db = nullptr; }
        int rc = sqlite3_open(path.c_str(), &g_lin.db);
        if (rc != SQLITE_OK) {
            std::string msg = sqlite3_errmsg(g_lin.db);
            sqlite3_close(g_lin.db); g_lin.db = nullptr;
            throw ClpsError("地泽临：无法打开数据库 \"" + path + "\": " + msg);
        }
        g_lin.path = path;
        return ClpsValue{};
    });

    // 执行 sql → int（受影响行数）
    ku.reg("地泽临卦", "执行", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string sql = a.empty() ? "" : to_str(a[0]);
        std::lock_guard<std::mutex> lk(g_lin.mx);
        lin_exec_noret(sql);
        int changes = sqlite3_changes(g_lin.db);
        return ClpsValue(int64_t(changes));
    });

    // 查询 sql → list（tab 分隔的行字符串列表）
    ku.reg("地泽临卦", "查询", WX::Mu, [](const std::vector<ClpsValue>& a) {
        std::string sql = a.empty() ? "" : to_str(a[0]);
        std::lock_guard<std::mutex> lk(g_lin.mx);
        if (!g_lin.db) throw ClpsError("地泽临：数据库未开启");

        auto rows = std::make_shared<ClpsList>();
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(g_lin.db, sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
            throw ClpsError("地泽临：查询准备失败: " + std::string(sqlite3_errmsg(g_lin.db)));

        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            int ncols = sqlite3_column_count(stmt);
            std::ostringstream oss;
            for (int c = 0; c < ncols; ++c) {
                if (c > 0) oss << "\t";
                const char* txt = (const char*)sqlite3_column_text(stmt, c);
                oss << (txt ? txt : "NULL");
            }
            rows->push_back(ClpsValue(oss.str()));
        }
        sqlite3_finalize(stmt);
        if (rc != SQLITE_DONE)
            throw ClpsError("地泽临：查询执行错误: " + std::string(sqlite3_errmsg(g_lin.db)));

        return ClpsValue(rows);
    });

    // 关库 → void
    ku.reg("地泽临卦", "关库", WX::Tu, [](const std::vector<ClpsValue>&) {
        std::lock_guard<std::mutex> lk(g_lin.mx);
        if (g_lin.db) { sqlite3_close(g_lin.db); g_lin.db = nullptr; }
        return ClpsValue{};
    });

    // 事务 → void（BEGIN）
    ku.reg("地泽临卦", "事务", WX::Tu, [](const std::vector<ClpsValue>&) {
        std::lock_guard<std::mutex> lk(g_lin.mx);
        lin_exec_noret("BEGIN TRANSACTION");
        return ClpsValue{};
    });

    // 提交 → void
    ku.reg("地泽临卦", "提交", WX::Tu, [](const std::vector<ClpsValue>&) {
        std::lock_guard<std::mutex> lk(g_lin.mx);
        lin_exec_noret("COMMIT");
        return ClpsValue{};
    });

    // 回滚 → void
    ku.reg("地泽临卦", "回滚", WX::Tu, [](const std::vector<ClpsValue>&) {
        std::lock_guard<std::mutex> lk(g_lin.mx);
        lin_exec_noret("ROLLBACK");
        return ClpsValue{};
    });
}


void init_mod_shujuku() {
    reg_di_ze_lin();
}
