#pragma once

#include <algorithm>
#include <stdexcept>
#include <stdio.h>
#include <string>

template<class T>
static inline T* outOfMemory(T* p)
{
    if(p) return p;
    throw std::runtime_error("Out of memory");
}

static inline std::string i2s(size_t i)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%i", int(i)); // � �.�.
    return buf;
}

static inline std::string ucase(const std::string& a)
{
    std::string r;
    r.resize(a.size());
    std::transform(a.begin(), a.end(), r.begin(), toupper); return r;
}

template<class T>
inline T& get(std::list<T>& a, unsigned n)
{
    typename std::list<T>::iterator i = a.begin();
    for(;n>0; n--) i++;
    return *i;
}

template<class T> inline void assure_and_fast_null(std::vector<T>& a, unsigned e)
{
    size_t s = a.size();
    if(e >= s)
    {
        size_t os = s;
        do
        {
            s += s/2 + 1; // переполнение маловероятно
        } while(e >= s);
        a.resize(s);
        memset(&a[os], -1, (s-os)*sizeof(T));
    }
}
