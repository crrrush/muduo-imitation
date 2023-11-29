
#include <iostream>
#include <string>
#include <functional>
#include <memory>

#include "any.hpp"
#include "servertest.hpp"

using namespace std;

void close_link(const conn_ptr &pc)
{
    LOG(DEBUG, "[close link is called][fd:%d]", pc->get_fd());
}

void anything_callback(const conn_ptr &pc)
{
    LOG(DEBUG, "[any event callback is called][fd:%d]", pc->get_fd());
    // sleep(1);
}

void handle_request(const conn_ptr &pc, buf_ptr pbuf)
{
    LOG(DEBUG, "[handle_request is called][fd:%d]", pc->get_fd());
    string msg;
    msg.resize(pbuf->valid_data_size());
    pbuf->read(&msg);
    pbuf->clear();

    string resp("recv sucess!");
    pc->send_peer(resp.c_str(), resp.length());
}

void gain_link(const conn_ptr &pc)
{
    LOG(DEBUG, "[get a connection][pc:%p][fd:%d]", pc.get(), pc->get_fd());
}

int main()
{
    unique_ptr<TcpServer> ps(new TcpServer(8888));
    
    // 设置从属线程数量
    ps->set_thread_num(2);

    // 设置回调函数
    ps->set_build_conn_callback(std::bind(gain_link, std::placeholders::_1));
    ps->set_handle_message_callback(std::bind(handle_request, std::placeholders::_1, std::placeholders::_2));
    ps->set_anyevent_occur_callback(std::bind(anything_callback, std::placeholders::_1));
    ps->set_destroy_conn_callback(std::bind(close_link, std::placeholders::_1));

    // 设置非活跃超时销毁
    ps->set_inactive_release(5);

    // 启动服务器
    ps->start();


    return 0;
}

// void close_link(const conn_ptr &pc)
// {
//     LOG(DEBUG, "[close link is called][fd:%d]", pc->get_fd());
// }

// void link_err(const conn_ptr &pc)
// {
//     LOG(ERROR, "[link error][fd:%d]", pc->get_fd());
// }

// // void anything_callback(channel *pc)
// void anything_callback(const conn_ptr &pc)
// {
//     LOG(DEBUG, "[any event callback is called][fd:%d]", pc->get_fd());
//     sleep(1);
// }

// void handle_request(const conn_ptr &pc, buf_ptr pbuf)
// {
//     LOG(DEBUG, "[handle_request is called][fd:%d]", pc->get_fd());
//     string msg;
//     msg.resize(pbuf->valid_data_size());
//     pbuf->read(&msg);
//     pbuf->clear();

//     string resp("recv sucess!");
//     pc->send_peer(resp.c_str(), resp.length());
// }

// void gain_link(const conn_ptr &pc)
// {
//     LOG(DEBUG, "[get a connection][pc:%p][fd:%d]", pc.get(), pc->get_fd());
// }

// void Accept(loop_thread_pool *pool, const int fd)
// {
//     LOG(DEBUG, "[Acceptor in][accept fd: %d]", fd);

//     // 这两个的调用顺序不能搞错!!!
//     loop_ptr loop = pool->which_loop();
//     connection_manager &conns = pool->which_connection_manager();

//     conn_ptr pc = conns.newconn(fd, loop);
//     LOG(DEBUG, "[new connection success][connection ptr:%d]", pc.get());

//     pc->set_message_callback(std::bind(handle_request, std::placeholders::_1, std::placeholders::_2));
//     pc->set_gainconn_callback(std::bind(gain_link, std::placeholders::_1));
//     pc->set_close_callback(std::bind(close_link, std::placeholders::_1));
//     pc->set_anyevent_callback(std::bind(anything_callback, std::placeholders::_1));
//     pc->set_conn_manager_close_callback(std::bind(connection_manager::delconn, &conns, std::placeholders::_1));

//     pc->establish_connn();
//     pc->start_inactive_release(5);
// }

// int main()
// {
//     unique_ptr<eventloop> mainloop(new eventloop); // 主线程eventloop，只负责获取新连接
//     unique_ptr<acceptor> pa(new acceptor(mainloop.get(), 8888));

//     unique_ptr<loop_thread_pool> pool(new loop_thread_pool(mainloop.get()));
//     pool->set_thread_num(2);
//     pool->init();

//     pa->setaccept_callback(std::bind(Accept, pool.get(), std::placeholders::_1));
//     pa->listen();

//     mainloop->start();

//     return 0;
// }

// int main()
// {
//     unique_ptr<eventloop> mainloop(new eventloop); // 主线程eventloop，只负责获取新连接
//     unique_ptr<acceptor> pa(new acceptor(mainloop.get(), 8888));

//     unique_ptr<loop_thread> pt(new loop_thread);// 新线程eventloop，负责获取到的连接的事件监控以及业务处理

//     pa->setaccept_callback(std::bind(Accept, pt->get_loop(), std::placeholders::_1)); // 多个新线程怎么办？
//     pa->listen();

//     mainloop->start();

//     return 0;
// }

// int main()
// {
//     unique_ptr<acceptor> pa(new acceptor(loop.get(), 8888));
//     pa->setaccept_callback(std::bind(Accept, std::placeholders::_1));
//     pa->listen();

//     loop->start();

//     return 0;
// }

// int main()
// {
//     unique_ptr<eventloop> loop(new eventloop);
//     unique_ptr<tcp_sock> ps(new tcp_sock);
//     // eventloop *loop = new eventloop;
//     // tcp_sock *ps = new tcp_sock;
//     ps->create_server(8888);
//     ps->set_nonblock(ps->get_fd());

//     // channel* pc = new channel(pc->get_fd(), loop);
//     // unique_ptr<channel> pc(new channel(ps->get_fd(), loop));
//     unique_ptr<channel> pc(new channel(ps->get_fd(), loop.get()));

//     // pc->set_read_event_callbcak(std::bind(Accept, ps, loop));
//     pc->set_read_event_callbcak(std::bind(Accept, ps.get(), loop.get()));
//     if (!pc->monitor_read_event())
//         exit(SOCK_READ_MINITOR_ERR);

//     while (true)
//         loop->start();

//     return 0;
// }

// // void close_link(channel *pc)
// void close_link(shared_ptr<channel> pc)
// {
//     LOG(DEBUG, "[close link is called][fd:%d]", pc->get_fd());
//     pc->remove_events();
//     close(pc->get_fd());
//     // delete pc;
// }

// // void link_err(channel *pc)
// void link_err(shared_ptr<channel> pc)
// {
//     LOG(ERROR, "link error");
//     close_link(pc);
// }

// // void anything_callback(channel *pc)
// void anything_callback(shared_ptr<channel> pc, eventloop *loop, const uint64_t &id)
// {
//     LOG(DEBUG, "any event callback is called");
//     loop->refresh_task_delaytime(id);
// }

// // void handle_request(channel *pc)
// void handle_request(shared_ptr<channel> pc)
// {
//     LOG(DEBUG, "handle_request is called");

//     char buf[1024] = {0};
//     ssize_t n = read(pc->get_fd(), buf, 1024);
//     if (-1 == n)
//     {
//         LOG(DEBUG, "[client error][%d : %s]", errno, strerror(errno));
//         close_link(pc);
//         return;
//     }
//     else if (n == 0)
//     {
//         LOG(DEBUG, "[client quit][%d : %s]", errno, strerror(errno));
//         // close_link(pc);
//         return;
//     }

//     buf[n] = 0;
//     cout << buf << endl;
//     // 处理

//     string resp("recv sucess!");
//     if (-1 == write(pc->get_fd(), resp.c_str(), resp.size()))
//     {
//         close_link(pc);
//         return;
//     }
// }

// void send_response(shared_ptr<channel> pc)
// {
//     string resp("recv sucess!");
//     write(pc->get_fd(), resp.c_str(), resp.size());
// }

// void Accept(tcp_sock *ps, eventloop *loop)
// {
//     int fd = ps->accept_();
//     static uint64_t taskid = 1;
//     // channel *pc = new channel(fd, loop);
//     shared_ptr<channel> pc(new channel(fd, loop));
//     pc->set_read_event_callbcak(std::bind(handle_request, pc));
//     // pc->set_write_event_callbcak(std::bind(send_response, pc));
//     // pc->set_close_event_callbcak(std::bind(close_link, pc));
//     pc->set_error_event_callbcak(std::bind(link_err, pc));
//     pc->set_any_event_callbcak(std::bind(anything_callback, pc, loop, taskid));

//     // pc->monitor_write_event();
//     loop->add_delayed_task(taskid++, 3, std::bind(close_link, pc));
//     // loop->add_delayed_task(taskid++, 3, nullptr);
//     pc->monitor_read_event();

//     // loop->update_events(pc);
// }

// int main()
// {
//     unique_ptr<eventloop> loop(new eventloop);
//     unique_ptr<tcp_sock> ps(new tcp_sock);
//     ps->create_server(8888);
//     ps->set_nonblock(ps->get_fd());
//     conn_ptr pc = conn_manager->newconn(ps->get_fd(), loop.get());

//     while (true)
//         loop->start();

//     return 0;
// }

// class Test
// {
// public:
//     Test() { cout << "构造\n"; }
//     Test(const Test &t) { cout << "拷贝构造\n"; }
//     ~Test() { cout << "析构\n"; }
// };

// int main()
// {
//     string s = "123123abc";
//     int i = 123123;
//     any as(s);
//     any ai(i);
//     cout << *as.get<string>() << endl;
//     cout << *ai.get<int>() << endl;

//     as = to_string(*ai.get<int>());
//     cout << *as.get<string>() << endl;

//     Test t;
//     {
//         any at(t);
//     }

//     return 0;
// }