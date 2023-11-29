#pragma once

#include "../module_test/servertest.hpp"

class echo_server
{
private:
    TcpServer _svr;

private:
    void handle_message(const conn_ptr &pc, buf_ptr pb)
    {
        std::string msg;
        msg.resize(pb->valid_data_size());
        pb->read(&msg);
        // LOG(DEBUG, "[buffer data size:%d]", pb->valid_data_size());
        // LOG(DEBUG, "[echo msg: %s]", msg.c_str());
        pc->send_peer(msg.c_str(), msg.size());
        pc->shutdown();
    }

    void close_callback(const conn_ptr &pc) { LOG(DEBUG, "[close callback is called][fd:%d]", pc->get_fd()); }

    void get_link_callback(const conn_ptr &pc) { LOG(DEBUG, "[get_link_callback is called][fd:%d]", pc->get_fd()); }

    void any_event(const conn_ptr &pc) { LOG(DEBUG, "[any_event is called][fd:%d]", pc->get_fd()); }

public:
    echo_server(const uint16_t &port) : _svr(port)
    {
        _svr.set_thread_num(2);
        _svr.set_handle_message_callback(std::bind(&echo_server::handle_message, this, std::placeholders::_1, std::placeholders::_2));
        // _svr.set_destroy_conn_callback(std::bind(&echo_server::close_callback, this, std::placeholders::_1));
        // _svr.set_build_conn_callback(std::bind(&echo_server::get_link_callback, this, std::placeholders::_1));
        // _svr.set_anyevent_occur_callback(std::bind(&echo_server::any_event, this, std::placeholders::_1));
    }
    // ~echo_server();

    void start() { _svr.start(); }
};