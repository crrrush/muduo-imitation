#pragma once

#include <iostream>
#include <string>
#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <unistd.h>

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

#define LOG(lv, format, ...) do{\
    if(lv < DEFAULT_LEVEL) break;\
    time_t t = time(nullptr);\
    struct tm *lt = localtime(&t);\
    char tmbuf[32] = {0};\
    strftime(tmbuf, 31, "%H:%M:%S", lt);\
    fprintf(stdout,"[pid:0X%X][%s %s:%d][%s] " format "\n", pthread_self(), tmbuf, __FILE__, __LINE__, level_str((enum level)lv), ##__VA_ARGS__);\
}while(0)

