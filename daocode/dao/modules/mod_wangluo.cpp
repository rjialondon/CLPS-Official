// mod_wangluo.cpp — 道·码万物库子模块
#include "../wanwu.hpp"
#include "mod_helpers.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <thread>
#include <mutex>
#include <map>
#include <atomic>
#include <deque>

static void reg_di_shui_bi() {
    auto& ku = WanWuKu::instance();

    // ── 发信 "host" port "消息" ─────────────────────────────
    // TCP 客户端：连接 host:port，发送消息（以 \n 结尾），关闭连接
    // 返回 None（副作用）
    ku.reg("地水比卦", "发信", WX::Huo, [](const std::vector<ClpsValue>& args) {
        if (args.size() < 3)
            throw ClpsError("地水比卦·发信：需要 host port 消息 三个参数");

        std::string host = to_str(args[0]);
        int         port = static_cast<int>(to_int(args[1], 8888));
        std::string msg  = to_str(args[2]);

        int sock = ::socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0)
            throw ClpsError("地水比卦·发信：socket 创建失败");

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(static_cast<uint16_t>(port));
        if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
            ::close(sock);
            throw ClpsError("地水比卦·发信：无效的 host 地址 " + host);
        }

        if (::connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
            ::close(sock);
            throw ClpsError("地水比卦·发信：连接失败 " + host + ":" + std::to_string(port));
        }

        // 消息以换行结尾，对方用 readline 接收
        msg += "\n";
        ::send(sock, msg.c_str(), msg.size(), 0);
        ::close(sock);

        return ClpsValue{};  // None
    });

    // ── 纳信 port ──────────────────────────────────────────
    // TCP 服务端：绑定 port，等待一个连接，读取一行消息，关闭，返回字符串
    // 阻塞直到收到消息——应在 师 子任务里调用，主层不阻塞
    ku.reg("地水比卦", "纳信", WX::Huo, [](const std::vector<ClpsValue>& args) {
        int port = args.empty() ? 8888 : static_cast<int>(to_int(args[0], 8888));

        int srv = ::socket(AF_INET, SOCK_STREAM, 0);
        if (srv < 0)
            throw ClpsError("地水比卦·纳信：socket 创建失败");

        // SO_REUSEADDR 避免 TIME_WAIT 卡住
        int yes = 1;
        ::setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_port        = htons(static_cast<uint16_t>(port));
        addr.sin_addr.s_addr = INADDR_ANY;

        if (::bind(srv, (sockaddr*)&addr, sizeof(addr)) < 0) {
            ::close(srv);
            throw ClpsError("地水比卦·纳信：bind 失败，端口 " + std::to_string(port));
        }
        ::listen(srv, 5);

        // 等待一个连接
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client = ::accept(srv, (sockaddr*)&client_addr, &client_len);
        ::close(srv);   // 只收一次，立即关闭监听端

        if (client < 0)
            throw ClpsError("地水比卦·纳信：accept 失败");

        // 读取直到换行或连接关闭
        std::string result;
        char buf[256];
        while (true) {
            ssize_t n = ::recv(client, buf, sizeof(buf) - 1, 0);
            if (n <= 0) break;
            buf[n] = '\0';
            result += buf;
            // 遇到换行即截止
            auto nl = result.find('\n');
            if (nl != std::string::npos) {
                result = result.substr(0, nl);
                break;
            }
        }
        ::close(client);

        return ClpsValue(result);
    });
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  【地天泰卦】— 泰卦(11)，流处理/管道
//  卦义：天地交而万物通；小往大来，吉亨
//  实现：字符串↔列表互化，查找，替换
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

struct LvParsedUrl {
    std::string host;
    int         port = 80;
    std::string path;
    bool        valid = false;
};

static LvParsedUrl lv_parse_url(const std::string& url) {
    LvParsedUrl r;
    if (url.size() < 7 || url.substr(0, 7) != "http://") return r;
    std::string rest = url.substr(7);
    size_t slash = rest.find('/');
    std::string host_part = (slash != std::string::npos) ? rest.substr(0, slash) : rest;
    r.path = (slash != std::string::npos) ? rest.substr(slash) : "/";
    size_t colon = host_part.find(':');
    if (colon != std::string::npos) {
        r.host = host_part.substr(0, colon);
        r.port = std::stoi(host_part.substr(colon + 1));
    } else {
        r.host = host_part;
        r.port = 80;
    }
    r.valid = !r.host.empty();
    return r;
}

// 内核：执行一次 HTTP 请求，返回完整响应字符串（含状态行+头部+体）
static std::string lv_do_request(const std::string& method,
                                  const std::string& url,
                                  const std::string& body = "",
                                  const std::string& content_type = "text/plain") {
    auto pu = lv_parse_url(url);
    if (!pu.valid) return "ERROR: 无效URL（仅支持 http://）";

    // DNS 解析 + 连接
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    std::string port_str = std::to_string(pu.port);
    if (::getaddrinfo(pu.host.c_str(), port_str.c_str(), &hints, &res) != 0 || !res)
        return "ERROR: DNS解析失败 " + pu.host;

    int sock = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) { ::freeaddrinfo(res); return "ERROR: socket创建失败"; }

    // 连接超时 10 秒
    struct timeval tv{ .tv_sec = 10, .tv_usec = 0 };
    ::setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    if (::connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
        ::freeaddrinfo(res); ::close(sock);
        return "ERROR: 连接失败 " + pu.host + ":" + port_str;
    }
    ::freeaddrinfo(res);

    // 构造 HTTP/1.0 请求（Connection:close，读完即结束）
    std::string req = method + " " + pu.path + " HTTP/1.0\r\n";
    req += "Host: " + pu.host + "\r\n";
    req += "User-Agent: CLPS/1.0 (道·码·旅卦)\r\n";
    req += "Accept: */*\r\n";
    req += "Connection: close\r\n";
    if (!body.empty()) {
        req += "Content-Type: " + content_type + "\r\n";
        req += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    }
    req += "\r\n";
    req += body;

    ::send(sock, req.c_str(), req.size(), 0);

    // 读取完整响应
    std::string response;
    char buf[8192];
    ssize_t n;
    while ((n = ::recv(sock, buf, sizeof(buf), 0)) > 0)
        response.append(buf, static_cast<size_t>(n));
    ::close(sock);
    return response;
}

// 从完整响应提取状态码
static int64_t lv_status_code(const std::string& resp) {
    // "HTTP/1.x NNN ..."
    size_t sp1 = resp.find(' ');
    if (sp1 == std::string::npos) return -1;
    size_t sp2 = resp.find(' ', sp1 + 1);
    std::string code_s = resp.substr(sp1 + 1,
        sp2 == std::string::npos ? std::string::npos : sp2 - sp1 - 1);
    try { return std::stoll(code_s); } catch (...) { return -1; }
}

// 从完整响应提取体（\r\n\r\n 之后）
static std::string lv_body(const std::string& resp) {
    size_t pos = resp.find("\r\n\r\n");
    if (pos == std::string::npos) return resp;
    return resp.substr(pos + 4);
}

static void reg_huo_shan_lv() {
    auto& ku = WanWuKu::instance();

    // 访 "url" → 响应体（GET，常用）
    ku.reg("火山旅卦", "访", WX::Huo, [](const std::vector<ClpsValue>& a) {
        std::string url = a.empty() ? "" : to_str(a[0]);
        return ClpsValue(lv_body(lv_do_request("GET", url)));
    });

    // 全访 "url" → 完整响应（含头部，调试用）
    ku.reg("火山旅卦", "全访", WX::Huo, [](const std::vector<ClpsValue>& a) {
        std::string url = a.empty() ? "" : to_str(a[0]);
        return ClpsValue(lv_do_request("GET", url));
    });

    // 投 "url" "body" ["content-type"] → 响应体（POST）
    ku.reg("火山旅卦", "投", WX::Huo, [](const std::vector<ClpsValue>& a) {
        std::string url  = a.size() >= 1 ? to_str(a[0]) : "";
        std::string body = a.size() >= 2 ? to_str(a[1]) : "";
        std::string ct   = a.size() >= 3 ? to_str(a[2]) : "application/x-www-form-urlencoded";
        return ClpsValue(lv_body(lv_do_request("POST", url, body, ct)));
    });

    // 取码 "完整响应" → HTTP 状态码（整数）
    ku.reg("火山旅卦", "取码", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string resp = a.empty() ? "" : to_str(a[0]);
        return ClpsValue(lv_status_code(resp));
    });

    // 取体 "完整响应" → 响应体（去除头部）
    ku.reg("火山旅卦", "取体", WX::Huo, [](const std::vector<ClpsValue>& a) {
        std::string resp = a.empty() ? "" : to_str(a[0]);
        return ClpsValue(lv_body(resp));
    });
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  【火水未济卦】— 未济(64)，HTTP 服务器
//  卦义：火在水上，未济。君子以慎辨物居方。
//  水火不交，持续待命，永远"未完成"——守护进程
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

struct WeiJiServer {
    int listen_fd = -1;   // 监听套接字（持久）
    int client_fd = -1;   // 当前连接（待请→回应 成对使用）
};

static std::mutex g_weiji_registry_mx;
static std::unordered_map<std::string, std::shared_ptr<WeiJiServer>> g_weiji_servers;

static std::shared_ptr<WeiJiServer> get_weiji(const std::string& name) {
    std::lock_guard<std::mutex> lg(g_weiji_registry_mx);
    auto it = g_weiji_servers.find(name);
    if (it != g_weiji_servers.end()) return it->second;
    auto s = std::make_shared<WeiJiServer>();
    g_weiji_servers[name] = s;
    return s;
}

// 从原始 HTTP 请求读取 "METHOD path\n[body]"
static std::string weiji_parse_request(const std::string& raw) {
    // 第一行：METHOD path HTTP/x.x
    size_t line_end = raw.find("\r\n");
    std::string first_line = raw.substr(0, line_end == std::string::npos ? raw.size() : line_end);
    size_t sp1 = first_line.find(' ');
    std::string method = (sp1 != std::string::npos) ? first_line.substr(0, sp1) : first_line;
    size_t sp2 = first_line.find(' ', sp1 + 1);
    std::string path = (sp1 != std::string::npos)
        ? first_line.substr(sp1 + 1, sp2 == std::string::npos ? std::string::npos : sp2 - sp1 - 1)
        : "/";
    // 体（\r\n\r\n 之后）
    std::string body;
    size_t hdr_end = raw.find("\r\n\r\n");
    if (hdr_end != std::string::npos) body = raw.substr(hdr_end + 4);
    return method + "\n" + path + "\n" + body;
}

static void reg_huo_shui_weiji() {
    auto& ku = WanWuKu::instance();

    // 启服 "名" port → 实际监听端口（建立监听套接字）
    ku.reg("火水未济卦", "启服", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string name = a.size() >= 1 ? to_str(a[0]) : "default";
        int port = a.size() >= 2 ? static_cast<int>(to_int(a[1], 8080)) : 8080;

        auto srv = get_weiji(name);
        if (srv->listen_fd >= 0) return ClpsValue(static_cast<int64_t>(port)); // 已启动

        int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) throw ClpsError("火水未济卦·启服：socket 创建失败");

        int yes = 1;
        ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port        = htons(static_cast<uint16_t>(port));

        if (::bind(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
            ::close(fd);
            throw ClpsError("火水未济卦·启服：bind 失败，端口 " + std::to_string(port));
        }
        ::listen(fd, 16);
        srv->listen_fd = fd;
        return ClpsValue(static_cast<int64_t>(port));
    });

    // 待请 "名" → "METHOD\npath\nbody"（阻塞等待下一个 HTTP 请求）
    ku.reg("火水未济卦", "待请", WX::Huo, [](const std::vector<ClpsValue>& a) {
        std::string name = a.empty() ? "default" : to_str(a[0]);
        auto srv = get_weiji(name);
        if (srv->listen_fd < 0) throw ClpsError("火水未济卦·待请：服务未启动，请先调用 启服");

        // 关闭上一次未关闭的连接
        if (srv->client_fd >= 0) { ::close(srv->client_fd); srv->client_fd = -1; }

        int client = ::accept(srv->listen_fd, nullptr, nullptr);
        if (client < 0) throw ClpsError("火水未济卦·待请：accept 失败");
        srv->client_fd = client;

        // 读取完整请求（读到 \r\n\r\n 或超时）
        struct timeval tv{ .tv_sec = 5, .tv_usec = 0 };
        ::setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        std::string raw;
        char buf[4096];
        ssize_t n;
        while ((n = ::recv(client, buf, sizeof(buf), 0)) > 0) {
            raw.append(buf, static_cast<size_t>(n));
            if (raw.find("\r\n\r\n") != std::string::npos) break;
        }
        return ClpsValue(weiji_parse_request(raw));
    });

    // 回应 "名" status_code "body" ["content-type"] → 1=成功
    ku.reg("火水未济卦", "回应", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string name   = a.size() >= 1 ? to_str(a[0]) : "default";
        int64_t     status = a.size() >= 2 ? to_int(a[1], 200) : 200;
        std::string body   = a.size() >= 3 ? to_str(a[2]) : "";
        std::string ct     = a.size() >= 4 ? to_str(a[3]) : "text/plain; charset=utf-8";

        auto srv = get_weiji(name);
        if (srv->client_fd < 0) throw ClpsError("火水未济卦·回应：无待回应的连接");

        // 状态短语
        std::string phrase = (status == 200) ? "OK"
            : (status == 201) ? "Created"
            : (status == 400) ? "Bad Request"
            : (status == 404) ? "Not Found"
            : (status == 500) ? "Internal Server Error" : "OK";

        std::string resp = "HTTP/1.0 " + std::to_string(status) + " " + phrase + "\r\n";
        resp += "Content-Type: " + ct + "\r\n";
        resp += "Content-Length: " + std::to_string(body.size()) + "\r\n";
        resp += "Connection: close\r\n";
        resp += "\r\n";
        resp += body;

        ::send(srv->client_fd, resp.c_str(), resp.size(), 0);
        ::close(srv->client_fd);
        srv->client_fd = -1;
        return ClpsValue(int64_t(1));
    });

    // 停服 "名" → 关闭监听套接字
    ku.reg("火水未济卦", "停服", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string name = a.empty() ? "default" : to_str(a[0]);
        auto srv = get_weiji(name);
        if (srv->client_fd >= 0) { ::close(srv->client_fd); srv->client_fd = -1; }
        if (srv->listen_fd >= 0) { ::close(srv->listen_fd); srv->listen_fd = -1; }
        std::lock_guard<std::mutex> lg(g_weiji_registry_mx);
        g_weiji_servers.erase(name);
        return ClpsValue(int64_t(1));
    });
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  观(20) 【地风观卦】— 监控/日志
//  坤下巽上，风行地上，无处不达，仰观俯察
//
//  观察 level msg  → void     level: "信"|"警"|"危"
//  立标 name       → void     记录命名计时器起点
//  量时 name       → int      返回自立标起毫秒数
//  统计            → string   各级日志条数摘要
//  清观            → void     清空日志缓冲
//  回看 N          → string   最近 N 条日志拼接

static std::shared_ptr<ClpsList> jie_split(const std::string& s, char sep) {
    auto lst = std::make_shared<ClpsList>();
    std::string part;
    for (char c : s) {
        if (c == sep) { if (!part.empty()) { lst->push_back(ClpsValue(part)); part.clear(); } }
        else           { part.push_back(c); }
    }
    if (!part.empty()) lst->push_back(ClpsValue(part));
    return lst;
}

static void reg_lei_shui_jie() {
    auto& ku = WanWuKu::instance();

    // 解查 "a=1&b=2&c=hello%20world" → ClpsList ["a=1", "b=2", "c=hello world"]
    // 解析 URL query string，返回 "key=value" 对列表（值已 URL 解码）
    ku.reg("雷水解卦", "解查", WX::Mu, [](const std::vector<ClpsValue>& a) {
        std::string qs = a.empty() ? "" : to_str(a[0]);
        auto lst = std::make_shared<ClpsList>();
        // split by &
        std::string part;
        auto push_pair = [&](const std::string& p) {
            if (p.empty()) return;
            auto eq = p.find('=');
            std::string k, v;
            if (eq == std::string::npos) { k = url_decode(p); v = ""; }
            else { k = url_decode(p.substr(0, eq)); v = url_decode(p.substr(eq+1)); }
            lst->push_back(ClpsValue(k + "=" + v));
        };
        for (char c : qs) {
            if (c == '&') { push_pair(part); part.clear(); }
            else           { part.push_back(c); }
        }
        push_pair(part);
        return ClpsValue(lst);
    });

    // 解路 "http://host:port/path?q=1#frag" → "/path"
    // 从完整 URL 提取路径部分（不含查询字符串）
    ku.reg("雷水解卦", "解路", WX::Huo, [](const std::vector<ClpsValue>& a) {
        std::string url = a.empty() ? "" : to_str(a[0]);
        // skip scheme://host:port
        size_t pos = 0;
        if (url.find("://") != std::string::npos) {
            pos = url.find("://") + 3;
            pos = url.find('/', pos);
            if (pos == std::string::npos) return ClpsValue(std::string("/"));
        }
        // strip query and fragment
        size_t qpos = url.find('?', pos);
        size_t hpos = url.find('#', pos);
        size_t end  = std::min(qpos, hpos);
        return ClpsValue(url.substr(pos, end == std::string::npos ? std::string::npos : end - pos));
    });

    // 解头 "Content-Type: text/html\r\nAccept: */*" → ClpsList ["Content-Type=text/html","Accept=*/*"]
    // 解析 HTTP 请求头，返回 "key=value" 对列表（冒号分割，值去首尾空格）
    ku.reg("雷水解卦", "解头", WX::Mu, [](const std::vector<ClpsValue>& a) {
        std::string hdrs = a.empty() ? "" : to_str(a[0]);
        auto lst = std::make_shared<ClpsList>();
        // split by \n or \r\n
        std::string line;
        for (size_t i = 0; i <= hdrs.size(); ++i) {
            char c = (i < hdrs.size()) ? hdrs[i] : '\n';
            if (c == '\r') continue;
            if (c == '\n') {
                if (!line.empty()) {
                    auto col = line.find(':');
                    if (col != std::string::npos) {
                        std::string k = line.substr(0, col);
                        std::string v = line.substr(col+1);
                        // trim leading/trailing spaces from v
                        size_t vs = v.find_first_not_of(' ');
                        size_t ve = v.find_last_not_of(' ');
                        if (vs != std::string::npos) v = v.substr(vs, ve-vs+1);
                        else v = "";
                        lst->push_back(ClpsValue(k + "=" + v));
                    }
                    line.clear();
                }
            } else { line.push_back(c); }
        }
        return ClpsValue(lst);
    });

    // 解分 "a,b,c" "," → ClpsList ["a","b","c"]
    // 按分隔符分割字符串，返回列表（去空项）
    ku.reg("雷水解卦", "解分", WX::Mu, [](const std::vector<ClpsValue>& a) {
        std::string s   = a.size() >= 1 ? to_str(a[0]) : "";
        std::string sep = a.size() >= 2 ? to_str(a[1]) : ",";
        char c = sep.empty() ? ',' : sep[0];
        return ClpsValue(jie_split(s, c));
    });

    // 解请 "METHOD\npath\nbody" N → 第 N 段（0=方法, 1=路径, 2=请求体）
    // 专为 火水未济卦·待请 的返回值设计：方便从请求字符串中提取各部分
    ku.reg("雷水解卦", "解请", WX::Huo, [](const std::vector<ClpsValue>& a) -> ClpsValue {
        std::string raw = a.size() >= 1 ? to_str(a[0]) : "";
        int idx = a.size() >= 2 ? (int)to_int(a[1], 0) : 0;
        size_t start = 0;
        for (int i = 0; i < idx; ++i) {
            size_t nl = raw.find('\n', start);
            if (nl == std::string::npos) return ClpsValue();
            start = nl + 1;
        }
        size_t end = raw.find('\n', start);
        return ClpsValue(raw.substr(start, end == std::string::npos
            ? std::string::npos : end - start));
    });

    // 解余 "str" N → 从第 N 个 \n 之后到字符串末尾（保留后续所有 \n）
    // 专用：从 待请 返回值中提取完整请求体（body 可含 \n）
    ku.reg("雷水解卦", "解余", WX::Huo, [](const std::vector<ClpsValue>& a) -> ClpsValue {
        std::string raw = a.size() >= 1 ? to_str(a[0]) : "";
        int idx = a.size() >= 2 ? (int)to_int(a[1], 0) : 0;
        size_t start = 0;
        for (int i = 0; i < idx; ++i) {
            size_t nl = raw.find('\n', start);
            if (nl == std::string::npos) return ClpsValue();
            start = nl + 1;
        }
        return ClpsValue(raw.substr(start));
    });

    // 解参 "key" "a=1&b=2&c=3" → value string（从查询字符串取指定key的值）
    ku.reg("雷水解卦", "解参", WX::Huo, [](const std::vector<ClpsValue>& a) {
        std::string key = a.size() >= 1 ? to_str(a[0]) : "";
        std::string qs  = a.size() >= 2 ? to_str(a[1]) : "";
        std::string part;
        auto check = [&](const std::string& p) -> std::string {
            if (p.empty()) return "";
            auto eq = p.find('=');
            std::string k = url_decode(eq == std::string::npos ? p : p.substr(0, eq));
            if (k == key) return url_decode(eq == std::string::npos ? "" : p.substr(eq+1));
            return "";
        };
        for (char c2 : qs) {
            if (c2 == '&') {
                auto v = check(part); part.clear();
                if (!v.empty() || part.find('=') != std::string::npos) {
                    if (!v.empty()) return ClpsValue(v);
                }
            } else { part.push_back(c2); }
        }
        auto v = check(part);
        return ClpsValue(v);
    });
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  【乾为天卦】— 乾(1)，后台子进程管理
//  卦义：天行健，君子以自强不息。
//  乾=天=力=进程；不息=后台常驻；自强=独立执行
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━


// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  【天火同人卦】— 同人(13)，CLPS握手协议
//  卦义：同人于野，亨；利涉大川，利君子贞。
//  同类相遇，共识相认，降低防御，高效交互。
//
//  协议：双方互发暗号 "CLPS/同人/v1\n"
//        双方都收到暗号 → 同人（同类，共识）
//        任一方收到其他内容 → 外（外来，需拜山贴）
//
//  函数：
//    同人·牌               → 返回本实例身份牌（字符串）
//    同人·握 "host" port   → 主动握手，返回 "同人" 或 "外"
//    同人·听 port          → 侦听一次握手，返回 "同人" 或 "外"
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

#define CLPS_TONGREN_MAGIC "CLPS/同人/v1\n"

static void reg_tian_huo_tong_ren() {
    auto& ku = WanWuKu::instance();

    // 同人·牌 → 本实例身份牌
    // 格式："CLPS/同人/v1"（去掉\n的可读版本）
    ku.reg("天火同人卦", "同人·牌", WX::Huo, [](const std::vector<ClpsValue>&) {
        return ClpsValue(std::string("CLPS/同人/v1"));
    });

    // 同人·握 "host" port → 主动连接，互发暗号，返回 "同人" 或 "外"
    ku.reg("天火同人卦", "同人·握", WX::Huo, [](const std::vector<ClpsValue>& a) {
        if (a.size() < 2) throw ClpsError("天火同人卦·同人·握：需要 host port");
        std::string host = to_str(a[0]);
        int port = (int)to_int(a[1]);

        int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) throw ClpsError("同人·握：socket 失败");

        // 设置超时（3秒）
        struct timeval tv{ 3, 0 };
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port   = htons((uint16_t)port);
        struct hostent* he = gethostbyname(host.c_str());
        if (!he) { ::close(fd); throw ClpsError("同人·握：无法解析 " + host); }
        memcpy(&addr.sin_addr, he->h_addr, he->h_length);

        if (::connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            ::close(fd);
            return ClpsValue(std::string("外")); // 连不上→外
        }

        // 发暗号
        const char* magic = CLPS_TONGREN_MAGIC;
        ::send(fd, magic, strlen(magic), 0);

        // 收对方暗号
        char buf[64] = {};
        int n = (int)::recv(fd, buf, sizeof(buf)-1, 0);
        ::close(fd);

        if (n > 0 && std::string(buf, n) == std::string(CLPS_TONGREN_MAGIC))
            return ClpsValue(std::string("同人"));
        return ClpsValue(std::string("外"));
    });

    // 同人·听 port → 侦听一次握手，返回 "同人" 或 "外"
    ku.reg("天火同人卦", "同人·听", WX::Huo, [](const std::vector<ClpsValue>& a) {
        if (a.empty()) throw ClpsError("天火同人卦·同人·听：需要 port");
        int port = (int)to_int(a[0]);

        int srv = ::socket(AF_INET, SOCK_STREAM, 0);
        if (srv < 0) throw ClpsError("同人·听：socket 失败");

        int opt = 1;
        setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        struct timeval tv{ 30, 0 }; // 最多等30秒
        setsockopt(srv, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        struct sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port        = htons((uint16_t)port);

        if (::bind(srv, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            ::close(srv);
            throw ClpsError("同人·听：bind 失败，端口 " + std::to_string(port));
        }
        ::listen(srv, 1);

        int cli = ::accept(srv, nullptr, nullptr);
        ::close(srv);
        if (cli < 0) return ClpsValue(std::string("外"));

        // 收对方暗号
        char buf[64] = {};
        int n = (int)::recv(cli, buf, sizeof(buf)-1, 0);

        // 回发暗号
        const char* magic = CLPS_TONGREN_MAGIC;
        ::send(cli, magic, strlen(magic), 0);
        ::close(cli);

        if (n > 0 && std::string(buf, n) == std::string(CLPS_TONGREN_MAGIC))
            return ClpsValue(std::string("同人"));
        return ClpsValue(std::string("外"));
    });
}

void init_mod_wangluo() {
    reg_di_shui_bi();
    reg_huo_shan_lv();
    reg_huo_shui_weiji();
    reg_lei_shui_jie();
    reg_tian_huo_tong_ren();
}
