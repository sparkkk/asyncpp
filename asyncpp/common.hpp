#pragma once

#include <type_traits>

namespace asyncpp
{
    enum result_code {
        SUCCEED = 0,
        INVALID_ARGUMENTS,
        INCORRECT_STATE,
        UNAVAILABLE_OR_TIMEOUT,
        DISABLED,
        BLOCKED,
    };


    template<
        template<size_t ...> typename _T,
        size_t _Begin,
        size_t _End, 
        bool _Final = (_Begin < _End),
        size_t ... _Values>
    struct Range : public Range<_T, _Begin, _End-1, (_Begin < _End-1), _End-1, _Values...>
    {
    };
    template<
        template<size_t ...> typename _T,
        size_t _Begin,
        size_t _End, 
        size_t ... _Values>
    struct Range<_T, _Begin, _End, false, _Values...> : public _T<_Values...>
    {
    };

    template<size_t ... _Values>
    struct Seq {};

    template<
        size_t _Begin,
        size_t _End, 
        bool _Final = (_Begin < _End),
        size_t ... _Values>
    struct Range2 : public Range2<_Begin, _End-1, (_Begin < _End-1), _End-1, _Values...>
    {
    };
    template<
        size_t _Begin,
        size_t _End, 
        size_t ... _Values>
    struct Range2<_Begin, _End, false, _Values...>
    {
        typedef Seq<_Values...> seq;
    };
}