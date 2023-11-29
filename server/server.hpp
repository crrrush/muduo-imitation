#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <functional>
#include <typeinfo>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>

#include <cstring>
#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <cassert>

#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>

class any_t;
class channel;
class epoller;
class eventloop;
class timewheel;
class connection;
class loop_thread;
class connection_manager;
class buffer_t;

typedef struct sockaddr_in sockaddr_in;

using epoll_event = struct epoll_event;
// typedef struct epoll_event epoll_event;

using conn_ptr = std::shared_ptr<connection>;
using any_ptr = any_t *;
using buf_ptr = buffer_t *;
// using chan_ptr = std::shared_ptr<channel>;
using chan_ptr = channel *;
using loop_ptr = eventloop *;
using loop_thread_ptr = loop_thread *;
// using conn_manager_ptr = std::unique_ptr<connection_manager>;

using evcb_t = std::function<void()>;
using timefunc_t = std::function<void()>;
using rmfunc_t = std::function<void()>;
using taskf_t = std::function<void()>; // eventloop线程池任务

enum ERR
{
    USAGE_ERR = 1,
    SOCK_ERR,
    BIND_ERR,
    LISTEN_ERR,
    CONNECT_ERR,
    ACCEPT_ERR,
    EPOLL_CREATE_ERR,
    SOCK_SET_NONBLOCK_ERR,
    SOCK_READ_MINITOR_ERR,
    EVENTFD_CREATE_ERR,
    EVENTFD_READ_ERR,
    EVENTFD_WRITE_ERR,
    EVENTFD_READ_MINITOR_ERR,
    TIMERFD_CREATE_ERR,
    TIMERFD_READ_ERR,
    TIMERFD_WRITE_ERR,
    TIMERFD_READ_MINITOR_ERR
};

enum level
{
    DEBUG,
    WARNING,
    ERROR,
    FATAL
};

#define DEFAULT_LEVEL WARNING

const char *level_str(level lv)
{
    switch (lv)
    {
    case DEBUG:
        return "DEBUG";
        break;
    case WARNING:
        return "WARNING";
        break;
    case ERROR:
        return "ERROR";
        break;
    case FATAL:
        return "FATAL";
        break;
    default:
        return nullptr;
        break;
    }
}

#define LOG(lv, format, ...)                                                                                                                           \
    do                                                                                                                                                 \
    {                                                                                                                                                  \
        if (lv < DEFAULT_LEVEL)                                                                                                                        \
            break;                                                                                                                                     \
        time_t t = time(nullptr);                                                                                                                      \
        struct tm *lt = localtime(&t);                                                                                                                 \
        char tmbuf[32] = {0};                                                                                                                          \
        strftime(tmbuf, 31, "%H:%M:%S", lt);                                                                                                           \
        fprintf(stdout, "[pid:0X%X][%s %s:%d][%s] " format "\n", pthread_self(), tmbuf, __FILE__, __LINE__, level_str((enum level)lv), ##__VA_ARGS__); \
    } while (0)

class any_t
{
private:
    class holder
    {
    public:
        virtual ~holder() {}
        virtual const std::type_info &type() = 0;
        virtual holder *clone() = 0;
    };

    template <class T>
    class placeholder : public holder
    {
    public:
        placeholder(const T &val) : _val(val) {}
        // placeholder(const placeholder &other) : _val(other._val) {}
        virtual const std::type_info &type() { return typeid(_val); }
        virtual holder *clone() { return new placeholder(_val); }

    public:
        T _val;
    };

private:
    holder *_content;

public:
    any_t() : _content(nullptr) {}

    ~any_t() { delete _content; }

    template <class T>
    any_t(const T &val) : _content(new placeholder<T>(val)) {}

    any_t(const any_t &other) : _content(other._content ? other._content->clone() : nullptr) {}

    template <class T>
    any_t &operator=(const T &val)
    {
        any_t(val).swap(*this);
        return *this;
    }

    any_t &operator=(const any_t &other)
    {
        any_t(other).swap(*this);
        return *this;
    }

    any_t &swap(any_t &other)
    {
        std::swap(_content, other._content);
        return *this;
    }

    template <class T>
    T *get()
    {
        assert(typeid(T) == _content->type());
        return &(((placeholder<T> *)_content)->_val);
    }
};

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

class buffer_t
{
#define DEFAULTBUFFERSIZE 1024
#define DEFAULTPOS 0
public:
    buffer_t(size_t buffersize = DEFAULTBUFFERSIZE) : _buffer(buffersize), _read_pos(DEFAULTPOS), _write_pos(DEFAULTPOS) {}
    // ~buffer_t() {}

    // _buffer起始地址
    char *begin() { return &(*(_buffer.begin())); }

    const char *begin() const { return &(*(_buffer.begin())); }

    // 获取当前写入起始地址
    char *write_addr() { return begin() + _write_pos; }

    const char *write_addr() const { return begin() + _write_pos; }

    // 获取当前读取起始地址
    const char *read_addr() const { return begin() + _read_pos; }

    // 获取缓冲区有效数据后空闲空间  after write pos
    uint64_t tail_vacancy() const { return _buffer.size() - _write_pos; }
    // 获取缓冲区有效数据前空闲空间  before read pos
    uint64_t head_vacancy() const { return _read_pos; }

    // 获取可读数据大小
    uint64_t valid_data_size() const { return _write_pos - _read_pos; }

    // 将read pos偏移向后移动
    void move_read_pos_back(const uint64_t &offset)
    {
        assert(offset <= valid_data_size());
        _read_pos += offset;
    }
    // 将write pos偏移向后移动
    void move_write_pos_back(const uint64_t &offset)
    {
        assert(offset <= tail_vacancy());
        _write_pos += offset;
    }

    // 扩充可写空间
    void expand(const uint64_t &size)
    {
        // 末尾空闲空间足够
        if (tail_vacancy() >= size)
            return;

        // 末尾空闲空间不够，加上头部空闲空间足够
        if (tail_vacancy() + head_vacancy() > size)
        {
            uint64_t vsz = valid_data_size();
            std::copy(read_addr(), read_addr() + vsz, begin());
            _read_pos = 0;
            _write_pos = vsz;
        }
        // 末尾空闲空间加上头部空闲空间不够
        else
        {
            _buffer.resize(_write_pos + size);
        }
    }

    // 写入数据
    void write(const char *data_ptr, const uint64_t &size)
    {
        expand(size);
        std::copy(data_ptr, data_ptr + size, write_addr());
        _write_pos += size;
    }

    void write(const std::string &data_str) { write(data_str.c_str(), data_str.size()); }

    void write(const buffer_t &data_buf) { write(data_buf.read_addr(), data_buf.valid_data_size()); }

    // 读取数据
    void read(char *out, const uint64_t &size)
    {
        assert(valid_data_size() >= size);
        std::copy(read_addr(), read_addr() + size, out);
        _read_pos += size;
    }

    void read(std::string *out) { read(&((*out)[0]), out->size()); }

    // 清空缓冲区
    void clear() { _read_pos = _write_pos = DEFAULTPOS; }

    /////////////           协议支持           /////////////////

    // 返回找到匹配字符串末尾的下一个位置  没找到返回空
    const char *find(const std::string &index) const
    {
        for (const char *cur = read_addr(); cur != write_addr(); ++cur)
        {
            auto it = index.begin();
            while (it != index.end() && *it == *cur)
            {
                ++it;
                ++cur;
            }
            if (it == index.end())
                return cur;
        }

        return nullptr;
    }

    // 上层怎么知道要预留多少空间？
    std::string read_until(const std::string &end)
    {
        const char *endpos = find(end);
        if (endpos == nullptr)
            return "";

        size_t n = endpos - read_addr();
        std::string out(n, 0);
        std::copy(read_addr(), endpos, &(out.front()));
        _read_pos += n; // !!!!!!!!! danger 纯指针操作
        return out;
    }

    // http
    const char *findCRLF() { return std::find<const char *, char>(read_addr(), read_addr() + valid_data_size(), '\n'); }

    std::string getline()
    {
        const char *line_end = findCRLF();
        if (line_end == nullptr)
            return "";

        std::string line;
        line.resize(line_end - read_addr() + 1);
        read(&line);
        return line;
    }

private:
    std::vector<char> _buffer;
    // 这里使用deque或者写出环形缓冲区真的有更好吗？

    uint64_t _read_pos;  // 读开始位置
    uint64_t _write_pos; // 写开始位置
};

enum conn_status
{
    DISCONNECTED,  // 待清理资源状态，已关闭连接，已处理完连接事件，清理连接对应资源
    DISCONNECTING, // 待清理事件状态，不再监视新事件，处理已发生的事件，处理完毕后更换为待清理资源状态
    CONNECTING,    // 待连接状态，正在初始化连接各项属性，以及进行连接各种初始化操作
    CONNECTED      // 正在连接状态
};

// class channel : public std::enable_shared_from_this<channel>
class channel
{
private:
    int _fd;
    loop_ptr _loop;

    // 这两个事件一定要初始化为零，如果是随机值的话，设置事件监控就会变得不可控
    uint32_t _events;  // 当前已设置监控的事件
    uint32_t _revents; // 已发生的事件

    evcb_t _read_event_callbcak;  // 读事件回调函数
    evcb_t _write_event_callbcak; // 写事件回调函数
    evcb_t _error_event_callbcak; // 异常事件回调函数
    evcb_t _close_event_callbcak; // 连接断开事件回调函数
    evcb_t _any_event_callbcak;   // 任意事件回调函数
public:
    channel(int fd, loop_ptr loop) : _fd(fd), _loop(loop), _events(0), _revents(0) {}
    // ~channel();

    int get_fd() const { return _fd; }

    uint32_t get_events() const { return _events; }

    uint32_t get_revents() const { return _revents; }

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
    bool monitor_read_event()
    {
        // _events |= EPOLLIN;
        _events |= (EPOLLIN | EPOLLRDHUP);
        // _events |= (EPOLLIN | EPOLLHUP);
        return update_events();
    }
    // 监控读事件
    bool monitor_write_event()
    {
        _events |= EPOLLOUT;
        return update_events();
    }

    // 取消监控写事件
    bool cancel_monitor_read_event()
    {
        _events &= ~EPOLLIN;
        return update_events();
    }
    // 取消监控读事件
    bool cancel_monitor_write_event()
    {
        _events &= ~EPOLLOUT;
        return update_events();
    }
    // 取消监控所有事件
    bool cancel_monitor_all_event()
    {
        _events = 0;
        return remove_events();
    }

    // 移除监控
    bool remove_events();

    bool update_events();

    // 事件处理,一旦触发了事件，就调用这个函数，由这个函数派发对应任务
    void handle_event() const
    {
        // 任意事件触发调用该回调
        // if (_any_event_callbcak)
        //     _any_event_callbcak();

        // if ((_revents & EPOLLIN) || (_revents & EPOLLRDHUP) || (_revents & EPOLLPRI))
        if ((_revents & EPOLLIN) || (_revents & EPOLLPRI))
        {
            // LOG(DEBUG, "[fd:%d][(_revents & EPOLLIN) || (_revents & EPOLLRDHUP) || (_revents & EPOLLPRI)][_revents:%d]", _fd, _revents);
            // LOG(DEBUG, "[fd:%d][(_revents & EPOLLIN) || (_revents & EPOLLPRI)][_revents:%d]", _fd, _revents);

            // // 任意事件被触发都调用该函数（如果已经被设置过的话）???这里需要调用吗？
            // if (_any_event_callbcak)
            //     _any_event_callbcak();

            // 读取加数据处理的时间会相对比较长，需要在此之前调用一次任意事件回调吗？
            // 这个函数调用之后，这个对象或者说connection会不会已经被清理了呢！？？
            if (_read_event_callbcak)
                _read_event_callbcak();
        }

        if (_revents & EPOLLOUT) // ??？ 这里也会触发段错误吗！！！？？？
        // else if (_revents & EPOLLOUT)
        {
            // LOG(DEBUG, "[fd:%d][_revents & EPOLLOUT][_revents:%d]", _fd, _revents);

            // 这个函数调用之后，这个对象或者说connection会不会已经被清理了呢！？？
            if (_write_event_callbcak)
                _write_event_callbcak();
        }
        else if (_revents & EPOLLERR)
        {
            // LOG(DEBUG, "[fd:%d][_revents & EPOLLERR][_revents:%d]", _fd, _revents);

            // 任意事件被触发都调用该函数（如果已经被设置过的话）???这里需要调用吗？
            // if (_any_event_callbcak)
            //     _any_event_callbcak();

            if (_error_event_callbcak)
                _error_event_callbcak();
        }
        else if ((_revents & EPOLLHUP) || (_revents & EPOLLRDHUP))
        {
            // LOG(DEBUG, "[fd:%d][_revents & EPOLLHUP  ||  _revents & EPOLLRDHUP][_revents:%d]", _fd, _revents);

            // 任意事件被触发都调用该函数（如果已经被设置过的话）???这里需要调用吗？
            // if (_any_event_callbcak)
            //     _any_event_callbcak();

            if (_close_event_callbcak)
                _close_event_callbcak();
        }

        // 写完后再刷新活跃度  ???
        // 任意事件被触发都调用该函数（如果已经被设置过的话）

        if (_any_event_callbcak) // 这里可能会有bug  压力测试的时候这里会触发段错误，即上面的调用中，这里已经资源释放了，这里还在访问
            _any_event_callbcak();
    }
};

class epoller
{
#define EVEBTSCAP 1024

private:
    int _epfd;
    const static int _cap = EVEBTSCAP;
    epoll_event _evs[_cap];
    // std::unordered_map<int, channel *> _channels;
    std::unordered_map<int, chan_ptr> _channels;

public:
    epoller() : _epfd(-1) { create(); }
    ~epoller()
    {
        if (_epfd != -1)
            close(_epfd);
    }

private:
    bool has_channel(const chan_ptr &chan) const { return _channels.find(chan->get_fd()) != _channels.end(); }

public:
    bool update(chan_ptr chan)
    {
        // 检查是否在连接表中
        if (has_channel(chan))
            return mod(chan);

        // 添加事件监控成功后再添加到连接表中
        if (add(chan))
            _channels[chan->get_fd()] = chan;

        return has_channel(chan);
    }
    bool remove(const chan_ptr &chan)
    {
        if (has_channel(chan))
        {
            auto it = _channels.find(chan->get_fd());
            assert(it != _channels.end());
            _channels.erase(it);
            return del(chan);
        }

        return true;
    }
    void wait(std::vector<chan_ptr> &active_links)
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
            LOG(FATAL, "[epoll create failed][%d:%s]", errno, strerror(errno));
            exit(EPOLL_CREATE_ERR);
        }

        LOG(DEBUG, "[epoll create successed][epfd:%d]", _epfd);
    }

    /////////////////////////////   add
    bool add(const chan_ptr &chan) const
    {
        epoll_event ev;
        ev.events = chan->get_events();
        ev.data.fd = chan->get_fd();
        if (-1 == epoll_ctl(_epfd, EPOLL_CTL_ADD, chan->get_fd(), &ev))
        {
            LOG(ERROR, "[epoll add fd failed][fd:%d][%d:%s]", chan->get_fd(), errno, strerror(errno));
            return false;
        }

        // LOG(DEBUG, "[epoll add fd:%d successed]", chan->get_fd());
        return true;
    }

    /////////////////////////////   del
    bool del(const chan_ptr &chan) const
    {
        // epoll模型中的红黑树是以fd为key值的，当删除红黑树节点时，需要关心节点内容是什么吗？
        // 显然是不需要的
        // 所以这里的epoll_event* 是被忽略的，可以直接传空指针
        if (-1 == epoll_ctl(_epfd, EPOLL_CTL_DEL, chan->get_fd(), nullptr))
        {
            LOG(ERROR, "[epoll delete fd failed][%d:%s]", errno, strerror(errno));
            return false;
        }

        // LOG(DEBUG, "[epoll delete fd successed][fd:%d]", chan->get_fd());
        return true;
    }

    /////////////////////////////   mod
    bool mod(const chan_ptr &chan) const
    {
        epoll_event ev;
        ev.data.fd = chan->get_fd();
        ev.events = chan->get_events();
        if (-1 == epoll_ctl(_epfd, EPOLL_CTL_MOD, chan->get_fd(), &ev))
        {
            LOG(ERROR, "[epoll modify event failed][%d:%s]", errno, strerror(errno));
            return false;
        }

        // LOG(DEBUG, "epoll modify event successed");
        return true;
    }

    /////////////////////////////   block_wait  阻塞式等待
    int block_wait()
    {
        int n = epoll_wait(_epfd, _evs, _cap, -1);
        if (-1 == n)
        {
            LOG(ERROR, "[epoll wait error][%d:%s]", errno, strerror(errno));
            return -1; // exit ?
        }

        // LOG(DEBUG, "epoll block wait block successed");
        return n;
    }
};

class time_task_t
{
private:
    uint64_t _id;        // 自身id
    uint32_t _delaytime; // 延时时间
    bool _is_cancle;     // 取消执行该任务

    timefunc_t _callback; // 任务回调函数，对象析构时自动执行

    rmfunc_t _release; // 提供管理此对象的容器，此对象析构时，自动清理容器的资源

public:
    time_task_t(const uint64_t &id, const uint32_t &time, const timefunc_t func) : _id(id), _delaytime(time), _is_cancle(true), _callback(func) {}
    ~time_task_t()
    {
        // 没有被取消并且被设置过回调函数则执行
        if (_is_cancle && _callback)
            _callback();

        _release();
    }

    // 获取任务id
    uint64_t get_id() const { return _id; }

    // 获取延迟时间
    uint32_t get_delaytime() const { return _delaytime; }

    // 设置管理执行完此任务对应释放资源的回调
    void set_release(const rmfunc_t &rm) { _release = rm; }

    void cancle() { _is_cancle = false; }
};

class timewheel
{
#define SECWHEELCAP 60
#define DEFAULTTICK 0

    typedef std::shared_ptr<time_task_t> ttsp_t;
    typedef std::weak_ptr<time_task_t> ttwp_t;

private:
    size_t _tick;     // 时间跳针，每个一个时间单位一跳
    size_t _capacity; // 跳针走一圈的跳数
    int _timer;       // timerfd返回的file descriptor
    loop_ptr _loop;
    std::unique_ptr<channel> _timer_chan;
    // chan_ptr _timer_chan;
    std::vector<std::vector<ttsp_t>> _wheel; // 循环队列
    std::unordered_map<uint64_t, ttwp_t> _ttmap;

public:
    timewheel(loop_ptr loop) : _tick(DEFAULTTICK), _capacity(SECWHEELCAP), _timer(create_timerfd()), _loop(loop), _timer_chan(new channel(_timer, _loop)), _wheel(SECWHEELCAP)
    {
        _timer_chan->set_read_event_callbcak(std::bind(&timewheel::timeout, this));
        if (!_timer_chan->monitor_read_event())
        {
            LOG(FATAL, "[read timerfd set read event monitor failed][timerfd:%d]", _timer);
            exit(TIMERFD_READ_MINITOR_ERR);
        }
    }
    // ~timewheel() {}

private:
    // 定时任务执行时，回调此函数，将该任务从_ttmap中移除
    void remove_ttwp(const uint64_t &id)
    {
        auto it = _ttmap.find(id);
        if (it != _ttmap.end())
            _ttmap.erase(it);
    }

    // 在系统中创建一个timerfd定时器
    static int create_timerfd()
    {
        int fd = timerfd_create(CLOCK_MONOTONIC, 0);
        if (-1 == fd)
        {
            LOG(FATAL, "[timerfd create failed][%d:%s]", errno, strerror(errno));
            exit(TIMERFD_CREATE_ERR);
        }

        struct itimerspec it;
        // 第一次超时的时间间隔
        it.it_interval.tv_sec = 1;
        it.it_interval.tv_nsec = 0;
        // 之后的每次超时的时间间隔
        it.it_value.tv_sec = 1;
        it.it_value.tv_nsec = 0;

        timerfd_settime(fd, 0, &it, nullptr);

        LOG(DEBUG, "[timerfd:%d is created successfully]", fd);
        return fd;
    }

    // 对定时器读取
    uint64_t read_timer() const
    {
        uint64_t times;
        int ret = read(_timer, &times, 8);
        if (-1 == ret)
        {
            if (errno == EINTR)
                return 0;

            LOG(FATAL, "read timer failed");
            exit(TIMERFD_READ_ERR);
        }
        return times;
    }

    // 向定时任务池中加入需定时执行的任务
    void add(const uint64_t &taskid, const uint32_t &delaytime, const timefunc_t task)
    {
        size_t pos = (_tick + delaytime) % _capacity; // 循环队列的访问  但是sec超过cap呢？bug

        // 如果已存在该任务，则重复添加
        if (is_task_exist(taskid))
            return _wheel[pos].push_back(_ttmap[taskid].lock());

        ttsp_t tsp(new time_task_t(taskid, delaytime, task));
        tsp->set_release(std::bind(&timewheel::remove_ttwp, this, taskid)); // 设置清理map资源的函数
        _wheel[pos].push_back(tsp);
        _ttmap[taskid] = ttwp_t(tsp);
    }

    // 刷新根据id指定的任务的过期时间，使其执行倒计时重新计时
    void refresh(const uint64_t &taskid)
    {
        if (!is_task_exist(taskid))
            return;

        ttsp_t tsp = _ttmap[taskid].lock();
        size_t pos = (_tick + tsp->get_delaytime()) % _capacity; // 循环队列的访问  但是sec超过cap呢？bug
        _wheel[pos].push_back(tsp);
    }

    // 取消指定任务的执行
    void cancel(const uint64_t &taskid)
    {
        if (!is_task_exist(taskid))
            return;

        ttsp_t tsp = _ttmap[taskid].lock();
        if (tsp)
            tsp->cancle();
    }

    void tick_tock()
    {
        // 经过一个时间间隔 执行在此时刻到时的任务
        _tick = (_tick + 1) % _capacity;
        _wheel[_tick].clear(); // 调用此时间片指向任务的shared_ptr的析构
    }

    // epoller监控到定时器事件触发，执行此任务，读取定时器数据，执行时间轮对应任务
    void timeout();

public:
    // 定时器属于公共资源，可能存在多个线程对定时器添加定时任务，所以存在线程安全问题
    // 为了尽量少地使用锁，这里直接将添加定时任务的执行交给自己绑定的eventloop，让其判断是否当前是自己对应的线程
    void add_task(const uint64_t &taskid, const uint32_t &delaytime, const timefunc_t task);

    void refresh_task_delaytime(const uint64_t &taskid);

    void cancel_task(const uint64_t &taskid);

    // 判断该任务是否存在 这个接口存在线程安全问题！！！  只能在绑定eventloop模块以及在该模块对应的线程中使用
    bool is_task_exist(const uint64_t &taskid) { return _ttmap.end() != _ttmap.find(taskid); }
};

class eventloop
{
private:
    std::thread::id _thread_id;
    epoller _epo;                        // 进行所有描述符的事件监控
    int _evfd;                           // eventfd唤醒IO事件监控有可能导致的阻塞
    std::unique_ptr<channel> _evfd_chan; //_evfd对应的事件
    // chan_ptr _evfd_chan;         //_evfd对应的事件
    timewheel _wheel;            // 延时任务池
    std::vector<taskf_t> _tasks; // 线程安全任务池
    std::mutex _mutex_task;

public:
    eventloop(/* args */) : _thread_id(std::this_thread::get_id()), _evfd(create_eventfd()), _evfd_chan(new channel(_evfd, this)), _wheel(this)
    {
        // 设置读事件处理函数
        _evfd_chan->set_read_event_callbcak(std::bind(&eventloop::read_eventfd, this));
        // 设置监控读事件
        if (!_evfd_chan->monitor_read_event())
        {
            LOG(FATAL, "[eventfd set read event monitor failed][eventfd:%d]", _evfd);
            exit(EVENTFD_READ_MINITOR_ERR);
        }
    }
    // ~eventloop();

private:
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
        if (-1 == evfd)
        {
            LOG(FATAL, "[eventfd create failed][%d]: %s", errno, strerror(errno));
            exit(EVENTFD_CREATE_ERR);
        }

        LOG(DEBUG, "[eventfd:%d is created successfully]", evfd);
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
    // 判断当前线程是否是eventloop对应的线程
    bool is_in_loop() { return _thread_id == std::this_thread::get_id(); }

    // 将操作压入任务池
    void push_in_loop(const taskf_t &cb)
    {
        {
            std::unique_lock<std::mutex> lock(_mutex_task);
            _tasks.push_back(cb);
        }
        write_eventfd(); // 向eventfd上写入数据，防止epoll事件监控时阻塞
    }

    // 判断将要执行的任务是否处于当前线程，如果是则执行，否则就压入对应任务池
    void run_in_loop(const taskf_t &cb)
    {
        if (is_in_loop())
            cb();
        else
            push_in_loop(cb);
    }

    // 添加或修改描述符的事件监控
    bool update_events(chan_ptr chan) { return _epo.update(chan); }

    // 移除描述符的监控
    bool remove_events(const chan_ptr &chan) { return _epo.remove(chan); }

    // 添加定时任务
    void add_delayed_task(const uint64_t &taskid, const uint32_t &delaytime, const timefunc_t task) { _wheel.add_task(taskid, delaytime, task); }
    // 取消定时任务
    void cancel_task(const uint64_t &taskid) { _wheel.cancel_task(taskid); }
    // 刷新定时任务
    void refresh_task_delaytime(const uint64_t &taskid) { _wheel.refresh_task_delaytime(taskid); }
    // 是否存在对应定时任务
    bool has_dalayed_task(const uint64_t &taskid) { return _wheel.is_task_exist(taskid); }

    // 三步走： 事件监控--就绪事件处理--执行任务
    void start()
    {
        while (true)
        {
            // 1. 事件监控
            std::vector<chan_ptr> active_links;
            _epo.wait(active_links);

            // 2. 就绪事件处理
            for (const auto &e : active_links)
                e->handle_event();

            // 3. 执行任务
            run_all_task();
        }
    }
};

class acceptor
{
    using accept_cb_t = std::function<void(const int)>;

private:
    tcp_sock _sock;                 // 网络套接字
    loop_ptr _loop;                 // 监控套接字获取新连接事件
    std::unique_ptr<channel> _chan; // 设置监控事件，对监控事件管理 这里可以不new出来一个对象交给智能指针管理？
    // chan_ptr _chan; // 设置监控事件，对监控事件管理 这里可以不new出来一个对象交给智能指针管理？

    accept_cb_t acceptor_cb;

public:
    acceptor(loop_ptr loop, const uint16_t &port, const std::string &ip = "0.0.0.0") : _sock(port, ip), _loop(loop), _chan(new channel(_sock.get_fd(), loop))
    {
        _chan->set_read_event_callbcak(std::bind(&acceptor::handle_accept, this));
    }
    // ~acceptor();

private:
    void handle_accept()
    {
        int fd = _sock.accept_();
        if (-1 == fd)
        {
            LOG(ERROR, "[accept failed!!!]");
            // exit(ACCEPT_ERR); // ?
            return;
        }

        if (acceptor_cb)
            acceptor_cb(fd);
    }

public:
    void setaccept_callback(const accept_cb_t &cb) { acceptor_cb = cb; }

    void listen()
    {
        if (!_chan->monitor_read_event())
            exit(SOCK_READ_MINITOR_ERR);
    }
};

class connection : public std::enable_shared_from_this<connection>
{
    using gainconn_cb_t = std::function<void(const conn_ptr &)>;
    using message_cb_t = std::function<void(const conn_ptr &, buf_ptr)>;
    using close_cb_t = std::function<void(const conn_ptr &)>;
    using anyevent_cb_t = std::function<void(const conn_ptr &)>;

private:
    // uint64_t _timer_id;        //连接对应的唯一定时器ID,由于连接ID也是唯一的，这里为了简化操作，直接使用连接ID作为定时器ID
    uint64_t _conn_id;         // 连接对应的唯一ID       真的有必要吗？？？
    int _sockfd;               // 连接关联的文件描述符
    bool _is_inactive_release; // 非活跃连接销毁的标志位，默认为false，即非活跃不销毁
    conn_status _status;       // 连接状态
    loop_ptr _loop;
    tcp_sock _socket; // 套接字管理模块
    channel _chan;    // 连接的事件管理
    // chan_ptr _chan;      // 连接的事件管理
    buffer_t _inbuffer;  // 接收缓冲区---存放读取到的数据
    buffer_t _outbuffer; // 发送缓冲区---存放待发送给对端的数据
    any_t _context;      // 存放上层根据对应协议处理接收缓冲区时读到不完整报文，根据协议保存处理该段数据时的上下文

    // 这四个回调对象是由组件使用者设置，由模块调用的
    gainconn_cb_t _conn_cb;
    message_cb_t _msg_cb;
    close_cb_t _close_cb;
    anyevent_cb_t _anyev_cb;

    // 组件内关闭连接的回调，用于清理组件内连接对应的资源
    close_cb_t _conn_manager_close_cb;

public:
    connection(const uint64_t &conn_id, const int &fd, loop_ptr loop)
        : _conn_id(conn_id), _sockfd(fd), _is_inactive_release(false), _status(CONNECTING), _loop(loop), _socket(fd), _chan(fd, loop)
    {
        _chan.set_read_event_callbcak(std::bind(&connection::handle_read, this));
        _chan.set_write_event_callbcak(std::bind(&connection::handle_write, this));
        _chan.set_close_event_callbcak(std::bind(&connection::handle_close, this));
        _chan.set_error_event_callbcak(std::bind(&connection::handle_error, this));
        _chan.set_any_event_callbcak(std::bind(&connection::handle_anyevnet, this));
    }
    ~connection() { LOG(DEBUG, "[connection is released successfully][fd:%d][%p]", _sockfd, this); }
    // ~connection() {}

private:
    //  切换协议---重置上下文以及阶段性处理函数
    void upgrade_in_loop(const any_t &context, const gainconn_cb_t &conncb, const message_cb_t &msgcb,
                         const close_cb_t &closecb, const anyevent_cb_t &anycb)
    {
        _context = context;
        _conn_cb = conncb;
        _msg_cb = msgcb;
        _close_cb = closecb;
        _anyev_cb = anycb;
    }

    // 描述符可读事件触发后调用的函数，接收socket数据放到接收缓冲区中，然后调用_msg_cb
    void handle_read()
    {
#define BUFFSIZE 65536
        // 创建局部缓冲区，将数据从tcp缓冲区中读上来
        char buf[BUFFSIZE] = {0};
        ssize_t n = _socket.recv_nonblcok(buf, BUFFSIZE);
        if (-1 == n)
            return shutdown_in_loop(); // 交给这个接口去关闭连接

        // 将数据写入接收缓冲区
        _inbuffer.write(buf, n);
        if (_inbuffer.valid_data_size() > 0)         // 接收缓冲区内有有效数据时
            _msg_cb(shared_from_this(), &_inbuffer); // shared_from_this() 获取指向自身的conn_ptr对象
    }
    // 描述符可写事件触发后调用的函数，将发送缓冲区中的数据发送出去
    void handle_write()
    {
        ssize_t n = _socket.send_nonblock(_outbuffer.read_addr(), _outbuffer.valid_data_size());
        if (-1 == n)
        {
            // 关闭连接
            if (_inbuffer.valid_data_size() > 0) // 关闭连接前将inbuffer缓冲区中的待处理数据处理掉  有必要吗？？？
                _msg_cb(shared_from_this(), &_inbuffer);

            // return release_in_loop();
            return release();
        }
        _outbuffer.move_read_pos_back(n);
        if (0 == _outbuffer.valid_data_size()) // 发送缓冲区没数据了
        {
            _chan.cancel_monitor_write_event(); // 关闭写事件监控
            if (_status == DISCONNECTING)       // 如果连接状态为待关闭，则调用release_in_loop关闭连接
                return release();
            // return release_in_loop();
        }
    }
    // 描述符触发挂断事件
    void handle_close()
    {
        // 关闭连接
        if (_inbuffer.valid_data_size() > 0) // 关闭连接前将inbuffer缓冲区中的待处理数据处理掉  有必要吗？？？
            _msg_cb(shared_from_this(), &_inbuffer);

        // release_in_loop();
        release();
    }
    // 描述符触发异常事件
    void handle_error() { handle_close(); }
    // 描述符触发任意事件
    void handle_anyevnet()
    {
        if (_is_inactive_release) // 如果有设置连接非活跃销毁，刷新连接活跃度
            _loop->refresh_task_delaytime(_conn_id);

        if (_anyev_cb) // 调用组件使用者的设置的任意事件回调函数
            _anyev_cb(shared_from_this());
    }

    // 连接获取之后。所处的状态下要进行各种设置（给channel设置回调，启动监控事件）
    void establish_connn_in_loop()
    {
        // 修改连接状态
        assert(_status == CONNECTING);
        _status = CONNECTED;

        // 连接到这里，各项参数，回调已经被组件调用设置过，更新完连接状态，启动完读事件监控才算是一个完整的开始工作的连接
        // 如果在构造函数内，或者在设置各项回调之前启动事件监控，有可能事件已经发生了，但是因为回调还没有被设置，从而错过处理机会
        // 如果非活跃销毁被启用，还会导致有事件发生而活跃度没被刷新
        // 启动读事件监控
        _chan.monitor_read_event(); // 失败？

        // 调用回调函数
        if (_conn_cb)
            _conn_cb(shared_from_this());
    }
    // 真正的释放接口
    void release_in_loop()
    {
        // LOG(DEBUG, "[release_in_loop is called][fd:%d]", _sockfd);

        if (_status == DISCONNECTED)
        {
            LOG(ERROR, "[资源释放被调用了多次！]");
            return;
        }

        // 改变连接状态
        _status = DISCONNECTED;
        // 如果启动了非活跃销毁，则取消该延时任务
        if (_is_inactive_release && _loop->has_dalayed_task(_conn_id))
            _loop->cancel_task(_conn_id);
        // 取消事件监控/将文件描述符对应的节点从epoll模型中移除
        _chan.cancel_monitor_all_event(); // 失败？
        // 关闭文件描述符
        _socket.close_();
        // 调用用户设置的关闭事件回调 这里调用？
        if (_close_cb)
            _close_cb(shared_from_this());
        // 调用组件设置的关闭事件回调
        if (_conn_manager_close_cb)
            _conn_manager_close_cb(shared_from_this());
    }

    // 为了防止上层某个连接处理时间太长导致后续连接超时被立即释放，访问后续连接时出现段错误，或者连接被立即释放导致事件派发里后续事件的访问出出现段错误
    void release() { _loop->push_in_loop(std::bind(&connection::release_in_loop, this)); }

    // 发送数据，将数据放到发送缓冲区，启动写事件监控
    void send_peer_in_loop(const std::string data) // 此接口要保证传的是右值引用
    {
        // 将数据放入发送缓冲区
        _outbuffer.write(data.c_str(), data.size());
        // 如果读事件监控没有开启就启动读事件监控
        if (!_chan.is_write_monitored())
            _chan.monitor_write_event(); // 失败？
    }
    // void send_peer_in_loop(const std::string &data) // ?
    // {
    //     // 将数据放入发送缓冲区
    //     _outbuffer.write(data.c_str(), data.size());
    //     // 如果读事件监控没有开启就启动读事件监控
    //     if (!_chan.is_write_monitored())
    //         _chan.monitor_write_event(); // 失败？
    // }
    // void send_peer_in_loop(const std::string &&data)
    // {
    //     // 将数据放入发送缓冲区
    //     _outbuffer.write(data.c_str(), data.size());
    //     // 如果读事件监控没有开启就启动读事件监控
    //     if (!_chan.is_write_monitored())
    //         _chan.monitor_write_event(); // 失败？
    // }

    //  提供给组件使用者的关闭接口，实际并不一定关闭，需判断数据待处理
    void shutdown_in_loop()
    {
        // LOG(DEBUG, "[shutdown_in_loop is called][fd:%d]", _sockfd);

        // 改变连接状态
        _status = DISCONNECTING;
        // 处理接收缓冲区待处理数据
        if (_inbuffer.valid_data_size() > 0)
            _msg_cb(shared_from_this(), &_inbuffer); // 要么在这往发送缓冲区写入数据时出错，直接关闭

        // 发出发送缓冲区待发送数据
        if (_outbuffer.valid_data_size() > 0) // 要么在这清空发送缓冲区之后关闭
            if (!_chan.is_write_monitored())
                _chan.monitor_write_event();

        // 确认能处理并发送的数据已处理完毕，就释放资源
        if (_outbuffer.valid_data_size() == 0)
            release();
        // release_in_loop();
    }
    //  启动非活跃销毁，需传入超时时间，添加定时任务
    void start_inactive_release_in_loop(const uint32_t &sec)
    {
        // 将非活跃销毁标志位置为真
        _is_inactive_release = true;

        if (_loop->has_dalayed_task(_conn_id))
            _loop->refresh_task_delaytime(_conn_id); // 如果定时任务已存在，则刷新延时时间
        else
            _loop->add_delayed_task(_conn_id, sec, std::bind(&connection::release, this)); // 如果定时任务不存在，则添加定时任务
    }
    //  取消非活跃销毁
    void stop_inactive_release_in_loop()
    {
        // 将非活跃销毁标志位置为假
        _is_inactive_release = false;

        if (_loop->has_dalayed_task(_conn_id))
            _loop->cancel_task(_conn_id);
    }

public:
    // 获取文件描述符
    int get_fd() const { return _sockfd; }
    // 获取id
    uint64_t get_id() const { return _conn_id; }
    // 判断连接是否就绪
    bool is_connected() const { return _status == CONNECTED; }

    // 设置获取连接时回调对象
    void set_gainconn_callback(const gainconn_cb_t &cb) { _conn_cb = cb; }
    // 设置请求处理回调对象
    void set_message_callback(const message_cb_t &cb) { _msg_cb = cb; }
    // 设置任意事件回调对象
    void set_close_callback(const anyevent_cb_t &cb) { _close_cb = cb; }
    // 设置管理连接者移除连接回调对象
    void set_conn_manager_close_callback(const close_cb_t &cb) { _conn_manager_close_cb = cb; }
    // 设置关闭连接回调对象
    void set_anyevent_callback(const close_cb_t &cb) { _anyev_cb = cb; }

    // 设置上下文---连接建立完成时进行回调
    void set_context(const any_t &context) { _context = context; }

    // 获取上下文
    any_ptr get_context() { return &_context; }

    // 连接获取之后, 进行channel回调设置，启动读监控，调用_conn_cb
    void establish_connn() { _loop->run_in_loop(std::bind(&connection::establish_connn_in_loop, this)); }

    // 发送数据，将数据放到发送缓冲区，启动写事件监控   注意！！！ 调用send_peer_in_loop一定要用右值引用传参
    void send_peer(const char *data, const size_t &len) { _loop->run_in_loop(std::bind(&connection::send_peer_in_loop, this, std::move(std::string(data, len)))); }
    void send_peer(const std::string &data) { _loop->run_in_loop(std::bind(&connection::send_peer_in_loop, this, std::move(std::string(data)))); }
    // void send_peer(std::string &&data) { _loop->run_in_loop(std::bind(&connection::send_peer_in_loop, this, data)); }       // 尽量调用这个接口
    void send_peer(const std::string &&data) { _loop->run_in_loop(std::bind(&connection::send_peer_in_loop, this, data)); } // 尽量调用这个接口

    //  提供给组件使用者的关闭接口，实际并不一定关闭，需判断数据待处理
    void shutdown() { _loop->run_in_loop(std::bind(&connection::shutdown_in_loop, this)); }
    // 启动非活跃销毁，需传入超时时间，添加定时任务      主动刷新？
    void start_inactive_release(const uint32_t &sec) { _loop->run_in_loop(std::bind(&connection::start_inactive_release_in_loop, this, sec)); }
    // 取消非活跃销毁
    void stop_inactive_release() { _loop->run_in_loop(std::bind(&connection::stop_inactive_release_in_loop, this)); }
    // 切换协议---重置上下文以及阶段性回调处理函数  -- 非线程安全
    // 但是同时，这个函数应该立即在对应的线程中被执行（协议切换掉后应该立即生效,或者说我们希望回调函数立即被更换，以防数据的处理出现问题）
    // 所以这个函数应该必须在对应线程内被执行
    void upgrade(const any_t &context, const gainconn_cb_t &conncb, const message_cb_t &msgcb, const close_cb_t &closecb, const anyevent_cb_t &anycb)
    {
        assert(_loop->is_in_loop()); // 必须在对应线程内
        _loop->run_in_loop(std::bind(&connection::upgrade_in_loop, this, context, conncb, msgcb, closecb, anycb));
    }
};

class connection_manager // 这里可以考虑扩展做一个内存池
{
private:
    // std::unordered_map<int, conn_ptr> _conns; // fd conn_ptr
    std::unordered_map<uint64_t, conn_ptr> _conns; // conn_id connptr
    // uint64_t _id_to_distribute;

public:
    // ~connection_manager() {}

    // connection_manager() : _id_to_distribute(0) {}
    connection_manager() {}

    connection_manager(connection_manager &&manager) : _conns(manager._conns) {}

    connection_manager(const connection_manager &) = delete;
    connection_manager &operator=(const connection_manager &) = delete;

private:
    // uint64_t id_distributor() { return _id_to_distribute++; }

public:
    // typedef std::unordered_map<int, conn_ptr>::iterator conn_iterator;
    // typedef std::unordered_map<int, conn_ptr>::const_iterator const_conn_iterator;
    typedef std::unordered_map<uint64_t, conn_ptr>::iterator conn_iterator;
    typedef std::unordered_map<uint64_t, conn_ptr>::const_iterator const_conn_iterator;

    // 创建新连接并添加到管理器中
    conn_ptr new_conn(int fd, loop_ptr loop, uint64_t conn_id)
    {
        conn_ptr pc(new connection(conn_id, fd, loop));

        _conns.insert(std::make_pair(conn_id, pc));
        return pc;
    }

    // 检测套接字对应连接是否存在
    // bool is_alive(const int &fd) const { return _conns.find(fd) != _conns.end(); }
    bool is_alive(const uint64_t &conn_id) const { return _conns.find(conn_id) != _conns.end(); }

    bool is_alive(const conn_ptr &pc) const { return _conns.find(pc->get_id()) != _conns.end(); }

    // 删除连接
    // void delconn_by_fd(const int &fd) { _conns.erase(fd); }
    // void delconn_by_fd(const int &conn_id) { _conns.erase(conn_id); }

    // void delconn_by_ptr(const conn_ptr &pc) { _conns.erase(pc->get_id()); }

    void dele_conn(const uint64_t &conn_id)
    {
        if (is_alive(conn_id))
            _conns.erase(conn_id);
    }

    // 获取连接
    conn_ptr getconn_ptr(const int &conn_id) { return _conns[conn_id]; }

    conn_ptr operator[](const int &conn_id) { return _conns[conn_id]; }

    // 已管理连接的数量
    size_t size() { return _conns.size(); }
};

class loop_thread
{
private:
    // 用于实现获取_loop同步关系，避免线程创建，但是_loop还没有实例化就调用接口获取_loop
    std::mutex _mutex;             // 互斥锁
    std::condition_variable _cond; // 条件变量

    loop_ptr _loop;      // 这个必须在线程内实例化
    std::thread _thread; //_loop对应线程

public:
    loop_thread() : _loop(nullptr), _thread(&loop_thread::thread_entry, this) {}
    ~loop_thread() { _thread.join(); }

private:
    void thread_entry()
    {
        eventloop loop;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _loop = &loop;
            _cond.notify_all();
        }
        loop.start();
    }

public:
    loop_ptr get_loop()
    {
        loop_ptr loop;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _cond.wait(lock, [&]()
                       { return _loop != nullptr; });
            loop = _loop;
        }
        return loop;
    }
};

class loop_thread_pool
{
    typedef std::vector<loop_ptr>::iterator loop_iterator;
    typedef std::vector<loop_ptr>::const_iterator const_loop_iterator;

private:
    size_t _thread_num;                    // 从属线程的数量
    size_t _loop_index;                    // 分配到新连接的eventloop索引
    loop_ptr _main_loop;                   // 主eventloop
    std::vector<loop_thread_ptr> _threads; // 所有从属线程
    std::vector<loop_ptr> _loops;          // 所有从属线程对应eventloop

    // std::vector<connection_manager> _conn_managers;

public:
    loop_thread_pool(loop_ptr mainloop) : _thread_num(0), _loop_index(0), _main_loop(mainloop) {}
    // ~loop_thread_pool();

    // 设置线程数量
    void set_thread_num(const int &num) { _thread_num = num; }

    // 创建新线程和eventloop并交给_threads和_loops
    void init()
    {
        if (_thread_num > 0 && _threads.empty() && _loops.empty())
        {
            _threads.resize(_thread_num, nullptr);
            _loops.resize(_thread_num, nullptr);
            for (int i = 0; i < _thread_num; ++i)
            {
                _threads[i] = new loop_thread;
                _loops[i] = _threads[i]->get_loop();
            }
        }
    }

    // 获取所有eventloop

    // 迭代器
    loop_iterator begin() { return _loops.begin(); }
    const_loop_iterator cbegin() const { return _loops.cbegin(); }

    loop_iterator end() { return _loops.end(); }
    const_loop_iterator cend() const { return _loops.cend(); }

    // 重载方括号
    loop_ptr operator[](const size_t &pos) { return _loops[pos]; }
};

class TcpServer
{
    using build_conn_cb_t = std::function<void(const conn_ptr &)>;
    using handle_message_cb_t = std::function<void(const conn_ptr &, buf_ptr)>;
    using destroy_conn_cb_t = std::function<void(const conn_ptr &)>;
    using anyevent_occur_cb_t = std::function<void(const conn_ptr &)>;

private:
    uint16_t _port;             // 端口
    std::string _ip;            // ip地址
    uint64_t _id_to_distribute; // 连接id分配
    uint32_t _timeout;          // 非活跃超时时长
    bool _is_inactive_release;  // 启动非活跃连接销毁的标志位。默认为false，即不关闭
    eventloop _main_loop;       // 主线程绑定的eventloop，负责将底层的连接获取上来，初始化连接，并将连接推送给其他线程负责
    acceptor _acceptor;         // 获取连接的模块
    loop_thread_pool _pool;     // 线程池，每一个线程都有一个eventloop对象与之绑定

    std::vector<std::pair<connection_manager, loop_ptr>> _conn_balance_in_loop; // 负载均衡模块

    build_conn_cb_t _build_conn;         // 获取连接，设置完各项参数之后调用
    handle_message_cb_t _handle_message; // 处理数据回调
    anyevent_occur_cb_t _anyevent_occur; // 任意事件发生时回调
    destroy_conn_cb_t _destroy_conn;     // 销毁连接前的回调

public:
    TcpServer(const uint16_t &port, const std::string &ip = "0.0.0.0")
        : _port(port), _ip(ip), _id_to_distribute(0), _timeout(0), _is_inactive_release(false), _acceptor(&_main_loop, port, ip), _pool(&_main_loop)
    {
        _acceptor.setaccept_callback(std::bind(&TcpServer::accept_connection, this, std::placeholders::_1));
        _acceptor.listen();
    }
    // ~TcpServer();

private:
    // 获取新连接
    void accept_connection(const int fd)
    {
        // LOG(DEBUG, "[accept new connection][fd:%d]", fd);

        auto &conn_and_loop = _conn_balance_in_loop[which_loop()];
        conn_ptr pc = conn_and_loop.first.new_conn(fd, conn_and_loop.second, id_distributor());

        if (_handle_message)
            pc->set_message_callback(std::bind(_handle_message, std::placeholders::_1, std::placeholders::_2));
        if (_build_conn)
            pc->set_gainconn_callback(std::bind(_build_conn, std::placeholders::_1));
        if (_destroy_conn)
            pc->set_close_callback(std::bind(_destroy_conn, std::placeholders::_1));
        if (_anyevent_occur)
            pc->set_anyevent_callback(std::bind(_anyevent_occur, std::placeholders::_1));

        pc->set_conn_manager_close_callback(std::bind(&TcpServer::remove_connection, this, &(conn_and_loop.first), std::placeholders::_1));

        pc->establish_connn();
        if (_is_inactive_release)
            pc->start_inactive_release(_timeout);
    }

    // 移除连接 这里不同的loop操作的都是属于自己的那一个connection_manager
    // void remove_connection(connection_manager *manager, const conn_ptr &pc) { manager->dele_conn(pc->get_id()); }

    // 这里如果不是在主线程中移除而是在自己的线程中直接直接移除 channel中的handle_event可能会触发段错误！！！ 那这样的负载均衡设计还有意义吗？
    void remove_connection(connection_manager *manager, const conn_ptr &pc) { _main_loop.run_in_loop(std::bind(&TcpServer::remove_connection_in_loop, this, manager, pc)); }

    void remove_connection_in_loop(connection_manager *manager, const conn_ptr &pc) { manager->dele_conn(pc->get_id()); }

    void set_delayed_task_in_loop(const uint32_t sec, const timefunc_t &task) { _main_loop.add_delayed_task(id_distributor(), sec, task); }

    size_t which_loop()
    {
        size_t idle_loop_pos = 0;
        size_t min_conn_num = _conn_balance_in_loop[0].first.size();
        for (size_t i = 0; i < _conn_balance_in_loop.size(); ++i)
            if (min_conn_num < _conn_balance_in_loop[i].first.size())
                idle_loop_pos = i;
        return idle_loop_pos;
    }

    uint64_t id_distributor() { return _id_to_distribute++; }

public:
    // 设置从属线程数量
    void set_thread_num(const int &thread_num)
    {
        if (thread_num == 0)
        {
            _conn_balance_in_loop.push_back(std::pair<connection_manager, loop_ptr>(connection_manager(), &_main_loop));
        }
        else
        {
            // 设置线程数量并初始化pool
            _pool.set_thread_num(thread_num);
            _pool.init();

            _conn_balance_in_loop.resize(thread_num);
            for (size_t i = 0; i < thread_num; ++i)
                _conn_balance_in_loop[i].second = _pool[i];
        }
    }

    // 设置连接建立完成回调
    void set_build_conn_callback(const build_conn_cb_t &cb) { _build_conn = cb; }
    // 设置处理数据回调
    void set_handle_message_callback(const handle_message_cb_t &cb) { _handle_message = cb; }
    // 设置任意事件回调
    void set_anyevent_occur_callback(const anyevent_occur_cb_t &cb) { _anyevent_occur = cb; }
    // 设置关闭连接回调
    void set_destroy_conn_callback(const destroy_conn_cb_t &cb) { _destroy_conn = cb; }

    // 设置非活跃连接销毁
    void set_inactive_release(const uint32_t &sec)
    {
        _is_inactive_release = true;
        _timeout = sec;
    }

    // 设置定时任务
    void set_delayed_task(const uint32_t sec, const timefunc_t &task) { _main_loop.run_in_loop(std::bind(&TcpServer::set_delayed_task_in_loop, this, sec, task)); }

    // 启动服务器
    void start() { _main_loop.start(); }
};

class network_set
{
public:
    network_set()
    {
        LOG(DEBUG, "[SIGPIPE has been ingnored]");
        signal(SIGPIPE, SIG_IGN);
    }
    // ~network_set();
};

const static network_set nw;

// bool channel::update_events() { return _loop->update_events(shared_from_this()); }
bool channel::update_events() { return _loop->update_events(this); }

// bool channel::remove_events() { return _loop->remove_events(shared_from_this()); }
bool channel::remove_events() { return _loop->remove_events(this); }

void timewheel::timeout()
{
    uint64_t times = read_timer();
    for (uint64_t i = 0; i < times; ++i)
        tick_tock();
    // _loop->run_in_loop(std::bind(&timewheel::tick_tock, this));
}

void timewheel::add_task(const uint64_t &taskid, const uint32_t &delaytime, const timefunc_t task) { _loop->run_in_loop(std::bind(&timewheel::add, this, taskid, delaytime, task)); }

void timewheel::refresh_task_delaytime(const uint64_t &taskid) { _loop->run_in_loop(std::bind(&timewheel::refresh, this, taskid)); }

void timewheel::cancel_task(const uint64_t &taskid) { _loop->run_in_loop(std::bind(&timewheel::cancel, this, taskid)); }
