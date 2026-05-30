/*
 * MIT License
 * Copyright (c) 2023 _VIFEXTech
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <errno.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <unistd.h>

#include "at32f435_437.h"

__attribute__((weak)) int _isatty(int fd)
{
    if (fd >= STDIN_FILENO && fd <= STDERR_FILENO) {
        return 1;
    }

    errno = EBADF;
    return 0;
}

__attribute__((weak)) int _close(int fd)
{
    if (fd >= STDIN_FILENO && fd <= STDERR_FILENO) {
        return 0;
    }

    errno = EBADF;
    return -1;
}

__attribute__((weak)) int _lseek(int fd, int ptr, int dir)
{
    errno = EBADF;
    return -1;
}

__attribute__((weak)) int _fstat(int fd, struct stat* st)
{
    if (fd >= STDIN_FILENO && fd <= STDERR_FILENO) {
        st->st_mode = S_IFCHR;
        return 0;
    }

    errno = EBADF;
    return 0;
}

__attribute__((weak)) int _read(int file, char* ptr, int len)
{
    return -1;
}

__attribute__((weak)) int _write(int file, char* ptr, int len)
{
    (void)file;
    for (int i = 0; i < len; i++) {
        while (!(USART1->sts & USART_TDBE_FLAG))
            ;
        USART1->dt = (uint8_t)ptr[i];
    }
    return len;
}

__attribute__((weak)) int _getpid(void)
{
    return -1;
}

__attribute__((weak)) int _kill(pid_t pid, int sig)
{
    return -1;
}

__attribute__((weak)) clock_t _times(struct tms* buf)
{
    return -1;
}

/* FatFS timestamp stub (no RTC) */
#include "ff.h"
DWORD get_fattime(void)
{
    /* Return a fixed timestamp: 2025-01-01 00:00:00 */
    return ((DWORD)(2025 - 1980) << 25) | ((DWORD)1 << 21) | ((DWORD)1 << 16);
}
