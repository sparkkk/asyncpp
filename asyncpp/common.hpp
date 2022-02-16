#pragma once

namespace asyncpp
{
    enum result_code {
        SUCCEED = 0,
        INCORRECT_STATE,
        TRY_FAILED,
        TIME_OUT,
    };
}