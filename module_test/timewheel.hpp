#pragma once

#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>

#include <sys/timerfd.h>

#define SECWHEELCAP 60
#define DEFAULTTICK 0

using timefunc_t = std::function<void()>;
using rmfunc_t = std::function<void()>;

class time_task_t
{
public:
    time_task_t(const uint64_t &id, const uint32_t &time, const timefunc_t func) : _id(id), _timeout(time), _is_cancle(true), _callback(func) {}
    ~time_task_t()
    {
        if (_is_cancle)
            _callback();

        _release();
    }

    uint64_t id() { return _id; }

    uint32_t timeout() { return _timeout; }

    void set_release(const rmfunc_t &rm) { _release = rm; }

    void cancle() { _is_cancle = false; }

private:
    uint64_t _id;      // 自身id
    uint32_t _timeout; // 延时时间
    bool _is_cancle;   // 取消执行该任务

    timefunc_t _callback; // 任务回调函数，对象析构时自动执行

    rmfunc_t _release; // 提供管理此对象的容器，此对象析构时，自动清理容器的资源
};

class timewheel
{
private:
    typedef std::shared_ptr<time_task_t> ttsp_t;
    typedef std::weak_ptr<time_task_t> ttwp_t;

    void remove_ttwp(const uint64_t &id)
    {
        auto it = _ttmap.find(id);
        if (it != _ttmap.end())
            _ttmap.erase(it);
    }

public:
    timewheel() : _tick(DEFAULTTICK), _capacity(SECWHEELCAP), _wheel(SECWHEELCAP) {}
    ~timewheel() {}

    void add_timed_task(const uint64_t &id, const uint32_t &sec, const timefunc_t func)
    {
        ttsp_t tsp(new time_task_t(id, sec, func));
        size_t pos = (_tick + sec) % _capacity;                         // 循环队列的访问  但是sec超过cap呢？bug
        tsp->set_release(std::bind(&timewheel::remove_ttwp, this, id)); // 设置清理map资源的函数
        _wheel[pos].push_back(tsp);
        _ttmap[id] = ttwp_t(tsp);
    }

    void tick_tock()
    {
        // 经过一个时间间隔 执行在此时刻到时的任务
        _tick = (_tick + 1) % _capacity;
        _wheel[_tick].clear(); // 调用此时间片指向任务的shared_ptr的析构
    }

    void refresh(const uint64_t &id)
    {
        if (!is_task_exist(id))
            return;

        ttsp_t tsp = _ttmap[id].lock();
        size_t pos = (_tick + tsp->timeout()) % _capacity; // 循环队列的访问  但是sec超过cap呢？bug
        _wheel[pos].push_back(tsp);
    }

    void cancle_task(const uint64_t &id)
    {
        if (!is_task_exist(id))
            return;

        ttsp_t tsp = _ttmap[id].lock();
        if (tsp)
            tsp->cancle();
    }

    bool is_task_exist(const uint64_t &id) { return _ttmap.end() != _ttmap.find(id); }

private:
    size_t _tick;                            // 时间跳针，每个一个时间单位一跳
    size_t _capacity;                        // 跳针走一圈的跳数
    std::vector<std::vector<ttsp_t>> _wheel; // 循环队列
    std::unordered_map<uint64_t, ttwp_t> _ttmap;
};