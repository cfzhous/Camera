/**
 * @file ret.hpp
 * @author zhoutong (zhoutotong@qq.com)
 * @brief 
 * @version 0.1
 * @date 2022-04-25
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#pragma once
#include <string>
#include <iostream>
#include <cxxabi.h>

namespace awe
{
/**
 * @brief 基类型
 * 
 */
class BaseRet
{
public:
    BaseRet(const std::string &reason);
    std::string reason() const;
    inline void setReason(const std::string &str) { __m_reason = str; }

private:
    std::string __m_reason;
};

/**
 * @brief 返回值基本模板
 * 
 * @tparam T_ 
 */
template<typename T_>
class Ret : public BaseRet
{
public:
    Ret(const bool &ok, const T_ &val, const std::string &reason = "") : BaseRet(reason), __m_ok(ok), __m_value(val){}
    Ret(const bool &ok, const std::string &reason = "") : BaseRet(reason), __m_ok(ok) {}
    Ret() = delete;

    operator bool()
    {
        return __m_ok;
    }

    T_* operator &()
    {
        return &__m_value;
    }

    T_* operator ->()
    {
        return &__m_value;
    }

    T_* ptr()
    {
        return &__m_value;
    }

    T_& val()
    {
        return __m_value;
    }

    inline void setRet(const bool ret) { __m_ok = ret; }
    inline void setObject(T_ obj) { __m_value = obj; }

private:
    bool __m_ok;
    T_ __m_value;
};


template<>
class Ret<std::string> : public BaseRet
{
public:
    Ret(const bool &ok, const std::string &val, const std::string &reason = "") : BaseRet(reason), __m_ok(ok), __m_value(val){}
    Ret() = delete;

    operator bool()
    {
        return __m_ok;
    }

    std::string* operator &()
    {
        return &__m_value;
    }

    std::string* operator ->()
    {
        return &__m_value;
    }

    std::string* ptr()
    {
        return &__m_value;
    }

    std::string& val()
    {
        return __m_value;
    }

    inline void setRet(const bool ret) { __m_ok = ret; }
    inline void setObject(std::string obj) { __m_value = obj; }

private:
    bool __m_ok;
    std::string __m_value;
};


/**
 * @brief bool 类型返回
 * 
 * @tparam  
 */
template<>
class Ret<bool> : public BaseRet
{
public:
    Ret(const bool &ok, const std::string &reason = "") : BaseRet(reason), __m_ok(ok){}
    Ret(const Ret<bool> &ok) : BaseRet(ok.reason()), __m_ok(ok.__m_ok){}
    Ret() = delete;

    operator bool() const
    {
        return __m_ok;
    }

    Ret<bool> operator = (const bool &ok)
    {
        __m_ok = ok;
        return __m_ok;
    }

    Ret<bool> operator = (const Ret<bool> &ok)
    {
        __m_ok = ok.__m_ok;
        return __m_ok;
    }

    std::string val()
    {
        return (__m_ok ? "True" : "False");
    }

private:
    bool __m_ok;
};


enum Exception {
    ECEPTION_NULL = 0,
};

using RetBool = Ret<bool>;
using RetInt = Ret<int>;
using RetStatus = Ret<Exception>;

template<typename T>
std::ostream& operator << (std::ostream& fout, const Ret<T> &value)
{
    fout << "[" << (value ? "True" : "False") << "]<" << value.val() << ">{" << value.reason() << "}";
    return fout;
}

template<typename T>
std::ostream& operator << (std::ostream& fout, Ret<T> &value)
{
    fout << "[" << (value ? "True" : "False") << "]<" << value.val() << ">{" << value.reason() << "}";
    return fout;
}


template<typename T>
const std::string getTypeName()
{
    // See: https://gcc.gnu.org/onlinedocs/libstdc++/libstdc++-html-USERS-4.3/a01696.html
    char *buf = abi::__cxa_demangle(typeid( typename std::decay<T>::type ).name(), nullptr, nullptr, nullptr);
    std::string name = buf;
    if(buf)
    {
        free(buf);
        buf = nullptr;
    }
    return name;
}

} // namespace awe
