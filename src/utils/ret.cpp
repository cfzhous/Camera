/**
 * @file ret.cpp
 * @author zhoutong (zhoutotong@qq.com)
 * @brief 
 * @version 0.1
 * @date 2021-12-09
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include "ret.hpp"

namespace awe
{
BaseRet::BaseRet(const std::string &reason) : __m_reason(reason)
{

}

std::string BaseRet::reason() const
{
    return __m_reason;
}


} // namespace awe
