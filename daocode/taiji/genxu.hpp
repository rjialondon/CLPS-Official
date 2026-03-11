#pragma once
/*
 * gen_xu_lock.hpp  — 艮需并发锁（纯C++，零Python依赖）
 * GIL 的 release/acquire 由 Python 绑定层负责
 */

#include <mutex>
#include <condition_variable>
#include <memory>
#include <functional>

struct GenXuLock {
    std::shared_ptr<std::mutex>              m;
    std::shared_ptr<std::condition_variable> cv;
    std::shared_ptr<bool>                    locked_flag;

    GenXuLock()
        : m(std::make_shared<std::mutex>()),
          cv(std::make_shared<std::condition_variable>()),
          locked_flag(std::make_shared<bool>(false))
    {}

    // gen() — 加锁（纯C++，不涉及GIL）
    void gen() {
        auto _m  = m;
        auto _cv = cv;
        auto _lf = locked_flag;
        std::unique_lock<std::mutex> lk(*_m);
        _cv->wait(lk, [&]{ return !*_lf; });
        *_lf = true;
    }

    // po() — 解锁
    void po() {
        std::unique_lock<std::mutex> lk(*m);
        *locked_flag = false;
        cv->notify_one();
    }

    // gen_compute(fn) — 原子：加锁 + 执行 fn + 解锁，全程持锁
    // 调用方（Python绑定层）负责在调用前释放GIL，fn 内不得调用任何 Python API
    void gen_compute(std::function<void()> fn) {
        std::unique_lock<std::mutex> lk(*m);
        cv->wait(lk, [&]{ return !*locked_flag; });
        *locked_flag = true;
        fn();
        *locked_flag = false;
        cv->notify_one();
    }
};
