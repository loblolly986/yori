/**
 * @file printfa.c
 *
 * Ansi versions of printf functions.
 *
 * Copyright (c) 2014 Malcolm J. Smith
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#undef UNICODE
#undef _UNICODE

#include "yoripch.h"
#include "yorilib.h"

#include "printf.inc"

/**
 Process a printf format string and output the result into a NULL terminated
 ANSI buffer of specified size.

 @param szDest The buffer to populate with the result.

 @param len The number of characters in the buffer.

 @param szFmt The format string to process.

 @return The number of characters successfully populated into the buffer, or
         -1 on error.
 */
int
YoriLibSPrintfSA(
    __out_ecount(len) LPSTR szDest,
    __in  DWORD len,
    __in  LPCSTR szFmt,
    ...
    )
{
    va_list marker;
    int out_len;

    va_start( marker, szFmt );
    out_len = YoriLibVSPrintfA(szDest, len, szFmt, marker);
    va_end( marker );
    return out_len;
}

/**
 Process a printf format string and output the result into a NULL terminated
 ANSI buffer which is assumed to be large enough to contain the result.

 @param szDest The buffer to populate with the result.

 @param szFmt The format string to process.

 @return The number of characters successfully populated into the buffer, or
         -1 on error.
 */
int
YoriLibSPrintfA(
    __out LPSTR szDest,
    __in LPCSTR szFmt,
    ...
    )
{
    va_list marker;
    int out_len;

    va_start( marker, szFmt );
#if defined(_MSC_VER) && (_MSC_VER >= 1700)
#pragma warning(suppress: 6386) // Obviously this is the buffer unsafe version
#endif
    out_len = YoriLibVSPrintfA(szDest, (DWORD)-1, szFmt, marker);
    va_end( marker );
    return out_len;
}

// vim:sw=4:ts=4:et:
