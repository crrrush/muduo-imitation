#pragma once

#include <iostream>
#include <string>
#include <functional>

#include <cstring>
#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <cstdarg>
#include <ctime>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>

class epoller;
class eventloop;

class channel
{
    using evcb_t = std::function<void()>;

private:
    int _fd;
    eventloop *_loop;
    uint32_t _events;             // 当前已设置监控的事件
    uint32_t _revents;            // 已发生的事件
    evcb_t _read_event_callbcak;  // 读事件回调函数
    evcb_t _write_event_callbcak; // 写事件回调函数
    evcb_t _error_event_callbcak; // 异常事件回调函数
    evcb_t _close_event_callbcak; // 连接断开事件回调函数
    evcb_t _any_event_callbcak;   // 任意事件回调函数
public:
    channel(int fd, eventloop *loop) : _fd(fd), _loop(loop) {}
    // ~channel();

    int get_fd() const { return _fd; }

    int get_events() const { return _events; }

    int get_revents() const { return _revents; }

    void set_revents(const uint32_t &events) { _revents = events; }

    // 设置回调函数
    void set_read_event_callbcak(const evcb_t &cb) { _read_event_callbcak = cb; }
    void set_write_event_callbcak(const evcb_t &cb) { _write_event_callbcak = cb; }
    void set_error_event_callbcak(const evcb_t &cb) { _error_event_callbcak = cb; }
    void set_close_event_callbcak(const evcb_t &cb) { _close_event_callbcak = cb; }
    void set_any_event_callbcak(const evcb_t &cb) { _any_event_callbcak = cb; }

    // 是否监控了可读
    bool is_read_monitored() const { return _events & EPOLLIN; }
    // 是否监控了可写
    bool is_write_monitored() const { return _events & EPOLLOUT; }

    // 监控写事件
    void monitor_read_event()
    {
        _events |= EPOLLIN;
        update_events();
    }
    // 监控读事件
    void monitor_write_event()
    {
        _events |= EPOLLOUT;
        update_events();
    }

    // 取消监控写事件
    void cancel_monitor_read_event()
    {
        _events &= ~EPOLLIN;
        update_events();
    }
    // 取消监控读事件
    void cancel_monitor_write_event()
    {
        _events &= ~EPOLLOUT;
        update_events();
    }
    // 取消监控所有事件
    void cancel_monitor_all_event()
    {
        _events = 0;
        remove_events();
    }

    // 移除监控
    void remove_events();

    void update_events();

    // 事件处理,一旦触发了事件，就调用这个函数，由这个函数派发对应任务
    void handle_event() const
    {
        if ((_revents & EPOLLIN) || (_revents & EPOLLRDHUP) || (_revents & EPOLLPRI))
        {
            if (_read_event_callbcak)
                _read_event_callbcak();
            // 任意事件被触发都调用该函数（如果已经被设置过的话）
            if (_any_event_callbcak)
                _any_event_callbcak();
        }

        if (_revents & EPOLLOUT)
        {
            if (_write_event_callbcak)
                _write_event_callbcak();

            // 写完后再刷新活跃度
            // 任意事件被触发都调用该函数（如果已经被设置过的话）
            if (_any_event_callbcak)
                _any_event_callbcak();
        }
        else if (_revents & EPOLLERR)
        {
            // 任意事件被触发都调用该函数（如果已经被设置过的话）
            if (_any_event_callbcak)
                _any_event_callbcak();

            if (_error_event_callbcak)
                _error_event_callbcak();
        }
        else if (_revents & EPOLLHUP)
        {
            // 任意事件被触发都调用该函数（如果已经被设置过的话）
            if (_any_event_callbcak)
                _any_event_callbcak();

            if (_close_event_callbcak)
                _close_event_callbcak();
        }
    }
};

void channel::update_events() { _loop->update_events(this); }

void channel::remove_events() { _loop->remove_events(this); }
