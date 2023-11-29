#pragma once

#include <vector>
#include <unordered_map>

#include <cstring>
#include <cerrno>
#include <cstdlib>
#include <cassert>

#include <sys/epoll.h>
#include <unistd.h>

#include "error.hpp"
#include "log.hpp"

#include "channel.hpp"

using epoll_event = struct epoll_event;
// typedef struct epoll_event epoll_event;

#define EVEBTSCAP 1024

class epoller
{
private:
    int _epfd;
    const static int _cap = EVEBTSCAP;
    epoll_event _evs[_cap];
    std::unordered_map<int, channel *> _channels;

public:
    epoller() : _epfd(-1) { create(); }
    ~epoller()
    {
        if (_epfd != -1)
            close(_epfd);
    }

private:
    bool has_channel(channel *chan) { return _channels.find(chan->get_fd()) == _channels.end(); }

public:
    bool update(channel *chan)
    {
        if (!has_channel(chan))
            return add(chan);
        else
            return mod(chan);
    }
    bool remove(channel *chan)
    {
        if (has_channel(chan))
            return del(chan);

        return true;
    }
    void wait(std::vector<channel *> &active_links)
    {
        int n = block_wait();
        for (int i = 0; i < n; ++i)
        {
            auto it = _channels.find(_evs[i].data.fd);
            assert(it != _channels.end());

            it->second->set_revents(_evs[i].events);
            active_links.push_back(it->second);
        }
    }

private:
    /////////////////////////////   create
#define SIZE 128 // epoll_create函数参数
    void create()
    {
        _epfd = epoll_create(SIZE);
        if (-1 == _epfd)
        {
            LOG(FATAL, "[epoll create failed][%d]: %s", errno, strerror(errno));
            exit(EPOLL_CREATE_ERR);
        }

        LOG(DEBUG, "epoll create successed");
    }

    /////////////////////////////   add
    bool add(channel *chan)
    {
        epoll_event ev;
        ev.events = chan->get_events();
        ev.data.fd = chan->get_fd();
        if (-1 == epoll_ctl(_epfd, EPOLL_CTL_ADD, chan->get_fd(), &ev))
        {
            LOG(ERROR, "[epoll add fd failed][fd : %d][%d]: %s", chan->get_fd(), errno, strerror(errno));
            return false;
        }

        LOG(DEBUG, "epoll add fd successed");
        return true;
    }

    /////////////////////////////   del
    bool del(channel *chan)
    {
        // epoll模型中的红黑树是以fd为key值的，当删除红黑树节点时，需要关心节点内容是什么吗？
        // 显然是不需要的
        // 所以这里的epoll_event* 是被忽略的，可以直接传空指针
        if (-1 == epoll_ctl(_epfd, EPOLL_CTL_DEL, chan->get_fd(), nullptr))
        {
            LOG(ERROR, "[epoll delete fd failed][%d]: %s", errno, strerror(errno));
            return false;
        }

        LOG(DEBUG, "epoll delete fd successed");
        return true;
    }

    /////////////////////////////   mod
    bool mod(channel *chan)
    {
        epoll_event ev;
        ev.data.fd = chan->get_fd();
        ev.events = chan->get_events();
        if (-1 == epoll_ctl(_epfd, EPOLL_CTL_MOD, chan->get_fd(), &ev))
        {
            LOG(ERROR, "[epoll modify event failed][%d]: %s", errno, strerror(errno));
            return false;
        }

        LOG(DEBUG, "epoll modify event successed");
        return true;
    }

    /////////////////////////////   block_wait  阻塞式等待
    int block_wait()
    {
        int n = epoll_wait(_epfd, _evs, _cap, -1);
        if (-1 == n)
        {
            LOG(ERROR, "[epoll wait error][%d]: %s", errno, strerror(errno));
            return -1; // exit ?
        }

        LOG(DEBUG, "epoll block wait block successed");
        return n;
    }
};
