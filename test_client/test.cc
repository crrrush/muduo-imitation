
#include <iostream>
#include <string>
#include <memory>

#include "../module_test/tcp_sock.hpp"

using namespace std;

int main()
{
    unique_ptr<tcp_sock> pc(new tcp_sock);
    pc->create_client(8888, "0.0.0.0");

    int i = 0;
    while (true)
    {
        string mag = "hello world!!!";
        if (-1 == write(pc->get_fd(), mag.c_str(), mag.size()))
        {
            cout << "error!!! client quit\n";
            break;
        }

        char buf[1024] = {0};
        ssize_t n = read(pc->get_fd(), buf, 1024);
        if (-1 == n)
        {
            cout << "error!!! client quit\n";
            break;
        }
        buf[n] = 0;
        string resp;
        resp += buf;

        cout << resp <<" : "<< i++ << endl;
        // sleep(1);
        // if(i == 2) sleep(4);
        // if(i == 5) break;
    }


    // pc->close_();

    return 0;
}

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
