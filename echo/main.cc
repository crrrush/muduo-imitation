#include <memory>

#include "echo_server.hpp"

int main()
{
    std::unique_ptr<echo_server> ps(new echo_server(8888));
    ps->start();

    return 0;
}