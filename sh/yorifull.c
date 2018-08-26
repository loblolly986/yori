/**
 * @file sh/yorifull.c
 *
 * Yori table of supported builtins for the full/regular build of Yori
 *
 * Copyright (c) 2017 Malcolm J. Smith
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

#include "yori.h"

/**
 Declaration for the builtin command.
 */
YORI_CMD_BUILTIN YoriCmd_ALIAS;

/**
 Declaration for the builtin command.
 */
YORI_CMD_BUILTIN YoriCmd_BUILTIN;

/**
 Declaration for the builtin command.
 */
YORI_CMD_BUILTIN YoriCmd_CHDIR;

/**
 Declaration for the builtin command.
 */
YORI_CMD_BUILTIN YoriCmd_COLOR;

/**
 Declaration for the builtin command.
 */
YORI_CMD_BUILTIN YoriCmd_EXIT;

/**
 Declaration for the builtin command.
 */
YORI_CMD_BUILTIN YoriCmd_FALSE;

/**
 Declaration for the builtin command.
 */
YORI_CMD_BUILTIN YoriCmd_FG;

/**
 Declaration for the builtin command.
 */
YORI_CMD_BUILTIN YoriCmd_FOR;

/**
 Declaration for the builtin command.
 */
YORI_CMD_BUILTIN YoriCmd_HISTORY;

/**
 Declaration for the builtin command.
 */
YORI_CMD_BUILTIN YoriCmd_IF;

/**
 Declaration for the builtin command.
 */
YORI_CMD_BUILTIN YoriCmd_JOB;

/**
 Declaration for the builtin command.
 */
YORI_CMD_BUILTIN YoriCmd_PUSHD;

/**
 Declaration for the builtin command.
 */
YORI_CMD_BUILTIN YoriCmd_REM;

/**
 Declaration for the builtin command.
 */
YORI_CMD_BUILTIN YoriCmd_SET;

/**
 Declaration for the builtin command.
 */
YORI_CMD_BUILTIN YoriCmd_SETLOCAL;

/**
 Declaration for the builtin command.
 */
YORI_CMD_BUILTIN YoriCmd_TRUE;

/**
 Declaration for the builtin command.
 */
YORI_CMD_BUILTIN YoriCmd_VER;

/**
 Declaration for the builtin command.
 */
YORI_CMD_BUILTIN YoriCmd_WAIT;

/**
 Declaration for the builtin command.
 */
YORI_CMD_BUILTIN YoriCmd_YS;

/**
 Declaration for the builtin command.
 */
YORI_CMD_BUILTIN YoriCmd_Z;

/**
 The list of builtin commands supported by this build of Yori.
 */
CONST YORI_BUILTIN_NAME_MAPPING
YoriShBuiltins[] = {
                    {_T("ALIAS"),     YoriCmd_ALIAS},
                    {_T("BUILTIN"),   YoriCmd_BUILTIN},
                    {_T("CHDIR"),     YoriCmd_CHDIR},
                    {_T("COLOR"),     YoriCmd_COLOR},
                    {_T("EXIT"),      YoriCmd_EXIT},
                    {_T("FALSE"),     YoriCmd_FALSE},
                    {_T("FG"),        YoriCmd_FG},
                    {_T("FOR"),       YoriCmd_FOR},
                    {_T("HISTORY"),   YoriCmd_HISTORY},
                    {_T("IF"),        YoriCmd_IF},
                    {_T("JOB"),       YoriCmd_JOB},
                    {_T("PUSHD"),     YoriCmd_PUSHD},
                    {_T("REM"),       YoriCmd_REM},
                    {_T("SET"),       YoriCmd_SET},
                    {_T("SETLOCAL"),  YoriCmd_SETLOCAL},
                    {_T("TRUE"),      YoriCmd_TRUE},
                    {_T("VER"),       YoriCmd_VER},
                    {_T("WAIT"),      YoriCmd_WAIT},
                    {_T("YS"),        YoriCmd_YS},
                    {_T("Z"),         YoriCmd_Z},
                    {NULL,            NULL}
                   };

// vim:sw=4:ts=4:et:
