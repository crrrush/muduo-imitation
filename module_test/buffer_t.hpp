#pragma once

#include <iostream>
#include <vector>
#include <algorithm>

#include <cassert>

class buffer_t;

using buf_ptr = buffer_t *;

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
