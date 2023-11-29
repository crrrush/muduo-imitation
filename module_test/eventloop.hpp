#pragma once

#include <thread>
#include <mutex>

#include <sys/eventfd.h>

#include "channel.hpp"
#include "epoller.hpp"

class eventloop
{
    using taskf_t = std::function<void()>;

private:
    std::thread::id _thread_id;
    int _evfd;                           // eventfd唤醒IO事件监控有可能导致的阻塞
    std::unique_ptr<channel> _evfd_chan; //_evfd对应的事件
    epoller _epo;                        // 进行所有描述符的事件监控
    std::vector<taskf_t> _tasks;
    std::mutex _mutex_task;

public:
    void run_all_task()
    {
        std::vector<taskf_t> tasks;
        {
            std::unique_lock<std::mutex> lock(_mutex_task);
            tasks.swap(_tasks);
        }
        for (const auto &f : tasks)
            f();
    }

    static int create_eventfd()
    {
        // int eventfd(unsigned int initval, int flags);
        int evfd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
        if (evfd == -1)
        {
            LOG(FATAL, "[eventfd create failed][%d]: %s", errno, strerror(errno));
            exit(EVENTFD_CREATE_ERR);
        }

        return evfd;
    }

    void read_eventfd()
    {
        uint64_t val = 0;
        if (-1 == read(_evfd, &val, sizeof(val)))
        {
            // 被信号打断，读取条件不满足则返回
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
            {
                LOG(WARNING, "[read eventfd error][%d : %s]", errno, strerror(errno));
                return;
            }

            LOG(FATAL, "[read eventfd error][%d : %s]", errno, strerror(errno));
            exit(EVENTFD_READ_ERR);
        }
    }

    void write_eventfd()
    {
        uint64_t val = 1;
        if (-1 == write(_evfd, &val, sizeof(val)))
        {
            // 被信号打断，读取条件不满足则返回
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
            {
                LOG(WARNING, "[read eventfd error][%d : %s]", errno, strerror(errno));
                return;
            }

            LOG(FATAL, "[read eventfd error][%d : %s]", errno, strerror(errno));
            exit(EVENTFD_READ_ERR);
        }
    }

public:
    eventloop(/* args */) : _thread_id(std::this_thread::get_id()), _evfd(create_eventfd()), _evfd_chan(new channel(_evfd, this))
    {
        _evfd_chan->set_read_event_callbcak(std::bind(&eventloop::read_eventfd, this)); // 设置读事件处理函数
        _evfd_chan->monitor_read_event();                                               // 设置监控读事件
    }
    // ~eventloop();

    // 判断当前线程是否是eventloop对应的线程
    bool is_in_loop() { return _thread_id == std::this_thread::get_id(); }

    // 将操作压入任务池
    void push_in_loop(const taskf_t &cb)
    {
        {
            std::unique_lock<std::mutex> lock(_mutex_task);
            _tasks.push_back(cb);
        }
        write_eventfd();
    }

    // 判断将要执行的任务是否处于当前线程，如果是则执行，否则就压入对应任务池
    void run_in_loop(const taskf_t &cb)
    {
        if (is_in_loop())
            cb();
        else
            push_in_loop(cb);
    }

    // 添加修改描述符的事件监控
    void update_events(channel *chan) { _epo.update(chan); }

    // 移除描述符的监控
    void remove_events(channel *chan) { _epo.remove(chan); }

    // 三步走： 事件监控--就绪事件处理--执行任务
    void start()
    {
        // 1. 事件监控
        std::vector<channel *> active_links;
        _epo.wait(active_links);

        // 2. 就绪事件处理
        for (const auto &e : active_links)
            e->handle_event();

        // 3. 执行任务
        run_all_task();
    }
};