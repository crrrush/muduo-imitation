#pragma once

#include <iostream>
#include <string>

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

#include "error.hpp"
#include "log.hpp"

typedef struct sockaddr_in sockaddr_in;

// 创建并使用TCP套接字
class tcp_sock
{
#define DEFAULTBACKLOG 32

private:
    int _listensock;

    // 错误码，为支持ET模式下调用者想一次性将连接全部获取
    // int _accept_errno;
    // int _send_errno;
    // int _recv_errno;

public:
    // tcp_sock() : _listensock(-1), _errno(-1) {}
    // tcp_sock() : _listensock(-1) {}

    explicit tcp_sock(const int &sockfd = -1) : _listensock(sockfd) {}
    // 如果想在构造时就创建网络套接字
    // tcp_sock(bool create/*为了与另一个构造函数区分*/, const uint16_t &port, const std::string &ip = "0.0.0.0") : _listensock(-1) { create_server(port, ip); }
    explicit tcp_sock(const uint16_t &port, const std::string &ip = "0.0.0.0") : _listensock(-1) { create_server(port, ip); }

    ~tcp_sock()
    {
        if (_listensock != -1)
            close(_listensock);
    }

public:
    void close_()
    {
        close(_listensock);
        _listensock = -1;
    }

    void socket_()
    {
        _listensock = socket(AF_INET, SOCK_STREAM, 0); // 创建TCP套接字
        if (-1 == _listensock)
        {
            LOG(FATAL, "[create socket failed][%d]: %s", errno, strerror(errno));
            exit(SOCK_ERR);
        }

        LOG(DEBUG, "[create socket successed][socket descriptor:%d]", _listensock);
    }

    void bind_(const uint16_t &port, const std::string &ip = "0.0.0.0") const
    {
        // 绑定（本主机任意）ip地址和特定端口号
        sockaddr_in sa;
        memset(&sa, 0, sizeof(sa));
        sa.sin_family = AF_INET;   // 协议号 TCP
        sa.sin_port = htons(port); // 绑定某一个固定端口

        // sa.sin_addr.s_addr = INADDR_ANY; // 本机的任意IP
        sa.sin_addr.s_addr = inet_addr(ip.c_str());

        if (-1 == bind(_listensock, (const struct sockaddr *)&sa, sizeof(sa)))
        {
            LOG(FATAL, "[bind port %d failed][%d]: %s", port, errno, strerror(errno));
            exit(BIND_ERR);
        }
        LOG(DEBUG, "[bind port %d successed]", port);
    }

    void connect_(const uint16_t &port, const std::string &ip) const
    {
        // 连接（目标主机）ip地址和特定端口号
        sockaddr_in sa;
        memset(&sa, 0, sizeof(sa));
        sa.sin_family = AF_INET;   // 协议号 TCP
        sa.sin_port = htons(port); // 绑定某一个固定端口

        sa.sin_addr.s_addr = inet_addr(ip.c_str());

        if (-1 == connect(_listensock, (const struct sockaddr *)&sa, sizeof(sa)))
        {
            LOG(FATAL, "[connect failed][%d]: %s", errno, strerror(errno));
            exit(BIND_ERR);
        }
        LOG(DEBUG, "bind successed");
    }

    void listen_(int backlog = DEFAULTBACKLOG) const
    {
        // 设置套接字为监听状态
        if (-1 == listen(_listensock, backlog))
        {
            LOG(FATAL, "[set listen failed][%d]: %s", errno, strerror(errno));
            exit(LISTEN_ERR);
        }
        LOG(DEBUG, "[set listen successed]");
    }

    // int accept_(std::string *clientip_, uint16_t *clientport_)
    int accept_()
    {
        // 从TCP套接字中获取连接
        sockaddr_in link;
        socklen_t len = sizeof(link);
        memset(&link, 0, len);
        int fd = accept(_listensock, (struct sockaddr *)&link, &len);
        if (-1 == fd)
        {
            // // 将错误码带出，为支持ET模式下调用者想一次性将连接全部获取
            // _accept_errno = errno;

            // -1 一定就是有问题吗？
            // 读写条件尚未满足也是返回-1，此时错误码是EAGAIN 或者 EWOULDBLOCK
            // if(errno == EAGAIN)
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                LOG(WARNING, "[accept conditions were not met][%d:%s]", errno, strerror(errno)); // 读取条件尚不满足
                return 0;
            }
            else if (errno == EINTR)
            {
                LOG(WARNING, "[accept were interrupted][%d:%s]", errno, strerror(errno)); // 读取过程中被信号中断 退出再读一次
                return 0;
            }
            else
                LOG(ERROR, "[accept link failed][%d:%s]", errno, strerror(errno));
        }
        else
        {
            // *clientip_ = inet_ntoa(link.sin_addr);
            // *clientport_ = ntohs(link.sin_port);

            // LOG(DEBUG, "[accept link successed][fd:%d]", fd);
        }

        return fd;
    }

    ssize_t send_(const int &fd, const void *buf, const size_t &len, const int &flags) const
    {
        ssize_t n = send(fd, buf, len, flags);
        // 事实上，这里对端退出（不论正常，还是异常）时，这里都会返回0！！！ 所以要采取对端挂断事件关心的处理，还是这里作特殊判断呢？

        if (-1 == n)
        {
            // // 将错误码带出，为支持ET模式下调用者想一次性将连接全部获取
            // _send_errno = errno;

            // -1 一定就是有问题吗？
            // 读写条件尚未满足也是返回-1，此时错误码是EAGAIN 或者 EWOULDBLOCK
            // if(errno == EAGAIN)
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                LOG(WARNING, "[send conditions were not met][%d:%s]", errno, strerror(errno)); // 读取条件尚不满足
                return 0;
            }
            else if (errno == EINTR)
            {
                LOG(WARNING, "[send were interrupted][%d:%s]", errno, strerror(errno)); // 读取过程中被信号中断 退出再读一次
                return 0;
            }
            else
                LOG(ERROR, "[send data failed][%d:%s]", errno, strerror(errno));
            
        }

        return n;
    }

    ssize_t send_nonblock(const int &fd, const void *buf, const size_t &len) const { return send_(fd, buf, len, MSG_DONTWAIT); }

    ssize_t send_nonblock(const void *buf, const size_t &len) const { return send_(_listensock, buf, len, MSG_DONTWAIT); }

    ssize_t recv_(const int &fd, void *buf, const size_t &len, const int &flags) const
    {
        ssize_t n = recv(fd, buf, len, flags);
        // 事实上，这里对端退出（不论正常，还是异常）时，这里都会返回0！！！所以要采取对端挂断事件关心的处理，还是这里作特殊判断呢？

        if (-1 == n)
        {
            // // 将错误码带出，为支持ET模式下调用者想一次性将连接全部获取
            // _recv_errno = errno;

            // -1 一定就是有问题吗？
            // 读写条件尚未满足也是返回-1，此时错误码是EAGAIN 或者 EWOULDBLOCK
            // if(errno == EAGAIN)
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                LOG(WARNING, "[recv conditions were not met][%d:%s]", errno, strerror(errno)); // 读取条件尚不满足
                return 0;
            }
            else if (errno == EINTR)
            {
                LOG(WARNING, "[recv were interrupted][%d:%s]", errno, strerror(errno)); // 读取过程中被信号中断 退出再读一次
                return 0;
            }
            else
                LOG(ERROR, "[recv data failed][%d:%s]", errno, strerror(errno));

        }

        // LOG(DEBUG,"[recv is called][fd:%d][n : %d]", fd, n);

        return n;
    }

    ssize_t recv_nonblcok(const int &fd, void *buf, const size_t &len) const { return recv_(fd, buf, len, MSG_DONTWAIT); }

    ssize_t recv_nonblcok(void *buf, const size_t &len) const { return recv_(_listensock, buf, len, MSG_DONTWAIT); }

    // void create_server(const uint16_t &port, bool block = true, const std::string &ip = "0.0.0.0")
    void create_server(const uint16_t &port, const std::string &ip = "0.0.0.0")
    {
        socket_();
        // if (!block) set_nonblock(_listensock); // 设置非阻塞读取
        set_reuse(_listensock); // 设置地址和端口复用
        bind_(port, ip);
        listen_();
    }

    void create_client(const uint16_t &port, const std::string &ip)
    {
        socket_();
        connect_(port, ip);
    }

    int get_fd() const { return _listensock; }

    // int accept_err() const { return _errno; }

    // 设置地址和端口复用
    void set_reuse(int fd) const
    {
        int opt = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    }

    static bool set_nonblock(int fd)
    {
        int fl = fcntl(fd, F_GETFL);
        if (fl < 0)
            return false;

        fcntl(fd, F_SETFL, fl | O_NONBLOCK);
        return true;
    }
};
