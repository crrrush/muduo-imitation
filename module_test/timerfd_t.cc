
#include <iostream>
#include <unistd.h>

#include <sys/timerfd.h>

using namespace std;

class test
{
public:
    test(/* args */) { cout << "test()" << endl; }
    ~test() { cout << "~test()" << endl; }
};

void deltest(test *t) { delete t; };

int main()
{
    int fd = timerfd_create(CLOCK_MONOTONIC, 0);

    struct itimerspec it;
    // 第一次超时的时间间隔
    it.it_interval.tv_sec = 3;
    it.it_interval.tv_nsec = 0;
    // 之后的每次超时的时间间隔
    it.it_value.tv_sec = 1;
    it.it_value.tv_nsec = 0;

    timerfd_settime(fd, 0, &it, nullptr);

    while(true)
    {
        uint64_t n;
        int ret = read(fd, &n, 8);

        printf("已超时%ld次\n", n);
        sleep(5);
        read(fd, &n, 8);
        printf("已超时%ld次\n", n);
    }

    close(fd);

    return 0;
}