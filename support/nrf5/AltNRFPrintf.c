/*
 *
 *    Copyright (c) 2020 Jay Logue
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

/**
 *    @file
 *          An alternate implementation of the nRF5 SDK nrf_fprintf() functions
 *          designed to work with the tiny printf implementation published by
 *          Marco Paland (https://github.com/mpaland/printf).
 *
 *          Note that this code requires a version of mpaland printf that provides
 *          the fctvprintf() function.
 *
 *          To enable this code, set NRF_FPRINTF_ENABLED to 1 and include this
 *          file instead of nrf_fprintf.c and nrf_fprintf_format.c in your project.
 *
 */

#include <sdk_common.h>

#if NRF_MODULE_ENABLED(NRF_FPRINTF)

#include <nrf_assert.h>
#include <nrf_fprintf.h>

// Include header file from mpaland printf.
#include "printf.h"

void nrf_fprintf_buffer_out(char ch, void* arg)
{
    nrf_fprintf_ctx_t *ctx = (nrf_fprintf_ctx_t *)arg;

#if NRF_MODULE_ENABLED(NRF_FPRINTF_FLAG_AUTOMATIC_CR_ON_LF)
    if (ch == '\n')
    {
        nrf_fprintf_buffer_out('\r', arg);
    }
#endif

    ctx->p_io_buffer[ctx->io_buffer_cnt] = ch;
    ctx->io_buffer_cnt += 1;

    if (ctx->io_buffer_cnt >= ctx->io_buffer_size)
    {
        nrf_fprintf_buffer_flush(ctx);
    }
}

void nrf_fprintf_buffer_flush(nrf_fprintf_ctx_t *ctx)
{
    ASSERT(ctx != NULL);

    if (ctx->io_buffer_cnt > 0)
    {
        ctx->fwrite(ctx->p_user_ctx, ctx->p_io_buffer, ctx->io_buffer_cnt);
        ctx->io_buffer_cnt = 0;
    }
}

void nrf_fprintf_fmt(nrf_fprintf_ctx_t *ctx, char const *format, va_list *va)
{
    ASSERT(ctx != NULL);
    ASSERT(ctx->fwrite != NULL);
    ASSERT(ctx->p_io_buffer != NULL);
    ASSERT(ctx->io_buffer_size > 0);

    if (format != NULL)
    {
        fctvprintf(nrf_fprintf_buffer_out, ctx, format, *va);
    }
}

void nrf_fprintf(nrf_fprintf_ctx_t *ctx, char const *format, ...)
{
    va_list va;
    va_start(va, format);

    nrf_fprintf_fmt(ctx, format, &va);

    va_end(va);
}

#endif // NRF_MODULE_ENABLED(NRF_FPRINTF)
