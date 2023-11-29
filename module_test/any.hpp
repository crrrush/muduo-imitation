#pragma once

#include <iostream>
#include <cassert>
#include <typeinfo>

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

    ~any_t() {delete _content;}

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
