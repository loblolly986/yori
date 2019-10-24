/**
 * @file lib/fileenum.c
 *
 * Yori file enumeration routines
 *
 * Copyright (c) 2017-2018 Malcolm J. Smith
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

#include "yoripch.h"
#include "yorilib.h"


/**
 A dynamically allocated structure so as to avoid putting excessive load
 on the stack.  This can be overwritten for each match.
 */
typedef struct _YORILIB_FOREACHFILE_CONTEXT {

    /**
     The user provided file specification after trimming file:///, if
     necessary.
     */
    YORI_STRING EffectiveFileSpec;

    /**
     A fully qualified path to the directory being enumerated.  This is
     calculated once to ensure any objects found within the directory can
     have a full path generated by simple appends, without recalculation.
     */
    YORI_STRING ParentFullPath;

    /**
     A buffer to hold the path of any object found in the directory,
     generated via ParentFullPath above and the name of any object found
     via enumerate.
     */
    YORI_STRING FullPath;

    /**
     The number of phases in the enumerate.  Enumerations within a single
     directory only require a single phase, but recursive enumerates require
     a phase to operate on the current directory and a phase to recurse into
     any subdirectories.
     */
    DWORD NumberPhases;

    /**
     Indicates the current phase number being used.  Note that for recursive
     operations, recursion may occur before or after the directory being
     processed, so this number does not by itself indicate the operation
     being performed.
     */
    DWORD CurrentPhase;

    /**
     The number of characters in EffectiveFileSpec to the final slash. A
     seperator may not be specified in EffectiveFileSpec, so this is only
     meaningful if the local FinalSlashFound is set.
     */
    DWORD CharsToFinalSlash;

    /**
     Specifies an enumeration criteria to use if recursively invoking one of
     the enumeration functions to operate on a subdirectory.
     */
    YORI_STRING RecurseCriteria;

    /**
     The result of the Win32 FindFirstFile operation for the current
     file.
     */
    WIN32_FIND_DATA FileInfo;

} YORILIB_FOREACHFILE_CONTEXT, *PYORILIB_FOREACHFILE_CONTEXT;

/**
 Call a callback for every file matching a specified file pattern.

 @param FileSpec The pattern to match against.

 @param MatchFlags Specifies the behavior of the match, including whether
        it should be applied recursively and the recursing behavior.

 @param Depth Indicates the current recursion depth.  If this function is
        reentered, this value is incremented.

 @param Callback The callback to invoke on each match.

 @param ErrorCallback Optionally points to a function to invoke if a
        directory cannot be enumerated.  If NULL, the caller does not care
        about failures and wants to silently continue.

 @param Context Caller provided context to pass to the callback.
 */
__success(return)
BOOL
YoriLibForEachFileEnum(
    __in PYORI_STRING FileSpec,
    __in DWORD MatchFlags,
    __in DWORD Depth,
    __in PYORILIB_FILE_ENUM_FN Callback,
    __in_opt PYORILIB_FILE_ENUM_ERROR_FN ErrorCallback,
    __in_opt PVOID Context
    )
{
    HANDLE hFind;
    BOOLEAN FinalSlashFound;
    BOOLEAN ReportObject;
    BOOLEAN DotFile;
    BOOLEAN Result;
    BOOLEAN RecursePhase;
    BOOLEAN IsLink;
    PYORILIB_FOREACHFILE_CONTEXT ForEachContext = NULL;

    Result = TRUE;

    //
    //  Allocate heap for state that seems too large to have on the stack
    //  as part of a recursive algorithm
    //

    ForEachContext = YoriLibMalloc(sizeof(YORILIB_FOREACHFILE_CONTEXT));
    if (ForEachContext == NULL) {
        return FALSE;
    }
    YoriLibInitEmptyString(&ForEachContext->RecurseCriteria);

    //
    //  This is currently only needed for the GetFileAttributes call.  It may
    //  be possible to relax this, possibly allocating within this routine if
    //  it's really necessary.
    //

    ASSERT(YoriLibIsStringNullTerminated(FileSpec));

    //
    //  Check if there are home paths to expand
    //

    YoriLibInitEmptyString(&ForEachContext->EffectiveFileSpec);
    ForEachContext->EffectiveFileSpec.StartOfString = FileSpec->StartOfString;
    ForEachContext->EffectiveFileSpec.LengthInChars = FileSpec->LengthInChars;

    //
    //  Check if it's a file:/// prefixed path.  Because Win32 will handle
    //  path seperators in either direction, we can handle these by just
    //  skipping the prefix.
    //

    if (ForEachContext->EffectiveFileSpec.LengthInChars >= sizeof("file:///")) {
        if (YoriLibCompareStringWithLiteralInsensitiveCount(&ForEachContext->EffectiveFileSpec, _T("file:///"), sizeof("file:///") - 1) == 0) {
            ForEachContext->EffectiveFileSpec.StartOfString = &ForEachContext->EffectiveFileSpec.StartOfString[sizeof("file:///") - 1];
            ForEachContext->EffectiveFileSpec.LengthInChars -= sizeof("file:///") - 1;
        }
    }

    //
    //  If this is the first level enumerate and the caller wanted directory
    //  contents as opposed to directories themselves replace the caller
    //  provided expression with one ending in \* .
    //
    //  If the caller wanted recursive directory enumeration and specified
    //  an actual directory, ensure it's a full path so we can find the
    //  parent and apply the correct string to search within the parent.
    //  This differs from the above case because in this case the caller
    //  wants to observe the directory itself (and contents) rather than
    //  just contents.
    //

    if (Depth == 0) {
        YORI_STRING NewFileSpec;
        DWORD FileAttributes;
        if ((MatchFlags & YORILIB_FILEENUM_DIRECTORY_CONTENTS) != 0) {

            FileAttributes = GetFileAttributes(FileSpec->StartOfString);
            if (FileAttributes != (DWORD)-1 &&
                (FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
    
                if (!YoriLibAllocateString(&NewFileSpec, ForEachContext->EffectiveFileSpec.LengthInChars + 3)) {
                    YoriLibFree(ForEachContext);
                    return FALSE;
                }
    
                NewFileSpec.LengthInChars = YoriLibSPrintf(NewFileSpec.StartOfString, _T("%y\\*"), &ForEachContext->EffectiveFileSpec);
                memcpy(&ForEachContext->EffectiveFileSpec, &NewFileSpec, sizeof(YORI_STRING));
            }
        } else if ((MatchFlags & (YORILIB_FILEENUM_RECURSE_AFTER_RETURN | YORILIB_FILEENUM_RECURSE_BEFORE_RETURN)) != 0) {
            FileAttributes = GetFileAttributes(FileSpec->StartOfString);
            if (FileAttributes != (DWORD)-1 &&
                (FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {

                YoriLibInitEmptyString(&NewFileSpec);
                if (!YoriLibGetFullPathNameReturnAllocation(&ForEachContext->EffectiveFileSpec, TRUE, &NewFileSpec, NULL)) {
                    YoriLibFree(ForEachContext);
                    return FALSE;
                }

                memcpy(&ForEachContext->EffectiveFileSpec, &NewFileSpec, sizeof(YORI_STRING));
            }
        }
    }

    //
    //  See if the search criteria contains a path as well as a search
    //  specification.  If so, remember this point, since we'll need to
    //  reassemble combined paths in response to each match.
    //

    ForEachContext->CharsToFinalSlash = ForEachContext->EffectiveFileSpec.LengthInChars;
    FinalSlashFound = FALSE;
    while (ForEachContext->CharsToFinalSlash > 0) {
        ForEachContext->CharsToFinalSlash--;
        if (YoriLibIsSep(ForEachContext->EffectiveFileSpec.StartOfString[ForEachContext->CharsToFinalSlash])) {
            ForEachContext->CharsToFinalSlash++;
            FinalSlashFound = TRUE;
            break;
        }

        //
        //  If it's x:foobar treat the ':' as the final slash, so any future
        //  criteria is applied after it.  Note this is ambiguous as it could
        //  be a stream, so this is scoped specifically to the single letter
        //  case.
        //

        if (ForEachContext->CharsToFinalSlash == 1 &&
            YoriLibIsDriveLetterWithColon(&ForEachContext->EffectiveFileSpec)) {

            ForEachContext->CharsToFinalSlash++;
            FinalSlashFound = TRUE;
            break;
        }
    }

    ForEachContext->NumberPhases = 1;
    if ((MatchFlags & (YORILIB_FILEENUM_RECURSE_AFTER_RETURN | YORILIB_FILEENUM_RECURSE_BEFORE_RETURN)) != 0) {
        ForEachContext->NumberPhases++;
    }

    YoriLibInitEmptyString(&ForEachContext->ParentFullPath);

    if (FinalSlashFound) {
        YORI_STRING DirectoryPart;

        YoriLibInitEmptyString(&DirectoryPart);
        DirectoryPart.StartOfString = ForEachContext->EffectiveFileSpec.StartOfString;
        DirectoryPart.LengthInChars = ForEachContext->CharsToFinalSlash;

        //
        //  Trim trailing slashes, except if the string is just a slash, in
        //  which case it's meaningful.
        //
        //  MSFIX This really wants to apply all the EffectiveRoot logic.
        //

        if ((DirectoryPart.LengthInChars > 3 ||
             !YoriLibIsDriveLetterWithColonAndSlash(&DirectoryPart)) &&
            DirectoryPart.LengthInChars > 1 &&
            YoriLibIsSep(DirectoryPart.StartOfString[DirectoryPart.LengthInChars - 1])) {

            DirectoryPart.LengthInChars--;
        }

        if (!YoriLibGetFullPathNameReturnAllocation(&DirectoryPart, TRUE, &ForEachContext->ParentFullPath, NULL)) {
            YoriLibFreeStringContents(&ForEachContext->EffectiveFileSpec);
            YoriLibFree(ForEachContext);
            return FALSE;
        }

    } else {
        YORI_STRING ThisDir;
        YoriLibConstantString(&ThisDir, _T("."));
        if (!YoriLibGetFullPathNameReturnAllocation(&ThisDir, TRUE, &ForEachContext->ParentFullPath, NULL)) {
            YoriLibFreeStringContents(&ForEachContext->EffectiveFileSpec);
            YoriLibFree(ForEachContext);
            return FALSE;
        }
    }

    //
    //  If the result ends with a \, truncate it since all children we
    //  report will unconditionally have a \ inserted between their name
    //  and the parent.  This result will happen with X:\ type paths.
    //

    if (ForEachContext->ParentFullPath.LengthInChars > 0 &&
        YoriLibIsSep(ForEachContext->ParentFullPath.StartOfString[ForEachContext->ParentFullPath.LengthInChars - 1])) {

        ForEachContext->ParentFullPath.LengthInChars--;
        ForEachContext->ParentFullPath.StartOfString[ForEachContext->ParentFullPath.LengthInChars] = '\0';
    }

    if (!YoriLibAllocateString(&ForEachContext->FullPath, ForEachContext->ParentFullPath.LengthInChars + 1 + sizeof(ForEachContext->FileInfo.cFileName) / sizeof(TCHAR) + 1)) {
        YoriLibFreeStringContents(&ForEachContext->EffectiveFileSpec);
        YoriLibFree(ForEachContext);
        return FALSE;
    }

    for (ForEachContext->CurrentPhase = 0; ForEachContext->CurrentPhase < ForEachContext->NumberPhases; ForEachContext->CurrentPhase++) {

        RecursePhase = FALSE;
        if ((MatchFlags & (YORILIB_FILEENUM_RECURSE_AFTER_RETURN | YORILIB_FILEENUM_RECURSE_BEFORE_RETURN)) == (YORILIB_FILEENUM_RECURSE_AFTER_RETURN | YORILIB_FILEENUM_RECURSE_BEFORE_RETURN)) {
            if (ForEachContext->CurrentPhase == 0) {
                RecursePhase = TRUE;
            }
        } else if ((MatchFlags & YORILIB_FILEENUM_RECURSE_AFTER_RETURN) != 0) {
            if (ForEachContext->CurrentPhase == 1) {
                RecursePhase = TRUE;
            }
        } else if ((MatchFlags & YORILIB_FILEENUM_RECURSE_BEFORE_RETURN) != 0) {
            if (ForEachContext->CurrentPhase == 0) {
                RecursePhase = TRUE;
            }
        }

        //
        //  If we're recursing but should apply the file match pattern on
        //  every subdirectory, brew up a new search criteria now for "*"
        //  so we can find every subdirectory.
        //

        if (RecursePhase &&
            (MatchFlags & YORILIB_FILEENUM_RECURSE_PRESERVE_WILD) != 0) {

            ForEachContext->FullPath.LengthInChars = YoriLibSPrintfS(ForEachContext->FullPath.StartOfString, ForEachContext->FullPath.LengthAllocated, _T("%y\\*"), &ForEachContext->ParentFullPath);
            hFind = FindFirstFile(ForEachContext->FullPath.StartOfString, &ForEachContext->FileInfo);
        } else {
            if (FinalSlashFound) {
                ForEachContext->FullPath.LengthInChars = YoriLibSPrintfS(ForEachContext->FullPath.StartOfString, ForEachContext->FullPath.LengthAllocated, _T("%y\\%s"), &ForEachContext->ParentFullPath, &ForEachContext->EffectiveFileSpec.StartOfString[ForEachContext->CharsToFinalSlash]);
            } else {
                ForEachContext->FullPath.LengthInChars = YoriLibSPrintfS(ForEachContext->FullPath.StartOfString, ForEachContext->FullPath.LengthAllocated, _T("%y\\%y"), &ForEachContext->ParentFullPath, &ForEachContext->EffectiveFileSpec);
            }
            hFind = FindFirstFile(ForEachContext->FullPath.StartOfString, &ForEachContext->FileInfo);

            //
            //  If we can't enumerate it because it's a volume root, cook up
            //  the data by hand and set hFind to NULL to indicate that the
            //  enumeration sort of worked.
            //

            if (hFind == INVALID_HANDLE_VALUE) {
                if ((ForEachContext->FullPath.LengthInChars == 3 && YoriLibIsDriveLetterWithColonAndSlash(&ForEachContext->FullPath)) ||
                    (ForEachContext->FullPath.LengthInChars == 7 && YoriLibIsPrefixedDriveLetterWithColonAndSlash(&ForEachContext->FullPath))) {

                    if (YoriLibUpdateFindDataFromFileInformation(&ForEachContext->FileInfo, ForEachContext->FullPath.StartOfString, FALSE)) {
                        ForEachContext->FileInfo.cFileName[0] = '\0';
                        ForEachContext->FileInfo.cAlternateFileName[0] = '\0';
                        hFind = NULL;
                    }
                }
            }
        }

        if (hFind == INVALID_HANDLE_VALUE) {
            if (ErrorCallback != NULL) {
                if (!ErrorCallback(&ForEachContext->FullPath, GetLastError(), Depth, Context)) {
                    Result = FALSE;
                }
                break;
            }
        } else {
            do {

                ReportObject = TRUE;
                DotFile = FALSE;

                //
                //  If the result is . or .., it's never interesting.  The caller
                //  might have wanted this from a match in the parent if we were
                //  recursing.
                //

                if (_tcscmp(ForEachContext->FileInfo.cFileName, _T(".")) == 0 ||
                    _tcscmp(ForEachContext->FileInfo.cFileName, _T("..")) == 0) {

                    if ((MatchFlags & YORILIB_FILEENUM_INCLUDE_DOTFILES) == 0) {
                        ReportObject = FALSE;
                    }
                    DotFile = TRUE;
                }

                //
                //  Check if this object should be reported given its directory
                //  status.
                //

                if ((ForEachContext->FileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                    if ((MatchFlags & YORILIB_FILEENUM_RETURN_DIRECTORIES) == 0) {
                        ReportObject = FALSE;
                    }
                } else {
                    if ((MatchFlags & YORILIB_FILEENUM_RETURN_FILES) == 0) {
                        ReportObject = FALSE;
                    }
                }

                //
                //  If we're recursing and have been told to not traverse
                //  links, check if this is a link.
                //

                IsLink = FALSE;
                if ((MatchFlags & YORILIB_FILEENUM_NO_LINK_TRAVERSE) != 0 &&
                    (ForEachContext->FileInfo.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 &&
                    (ForEachContext->FileInfo.dwReserved0 == IO_REPARSE_TAG_MOUNT_POINT ||
                     ForEachContext->FileInfo.dwReserved0 == IO_REPARSE_TAG_SYMLINK)) {

                    IsLink = TRUE;
                }

                //
                //  Check if this object should be recursed into.
                //

                if (!DotFile &&
                    (ForEachContext->FileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
                    RecursePhase &&
                    !IsLink) {

                    DWORD FileNameLen = _tcslen(ForEachContext->FileInfo.cFileName);
                    DWORD WildLength = 2;

                    if ((MatchFlags & YORILIB_FILEENUM_RECURSE_PRESERVE_WILD) != 0) {

                        WildLength = ForEachContext->EffectiveFileSpec.LengthInChars - ForEachContext->CharsToFinalSlash;
                    }

                    if (!YoriLibAllocateString(&ForEachContext->RecurseCriteria,
                                               ForEachContext->CharsToFinalSlash + FileNameLen + 1 + WildLength + 1)) {
                        Result = FALSE;
                        break;
                    }

                    if (FinalSlashFound) {
                        memcpy(ForEachContext->RecurseCriteria.StartOfString,
                               ForEachContext->EffectiveFileSpec.StartOfString,
                               ForEachContext->CharsToFinalSlash * sizeof(TCHAR));
                        ForEachContext->RecurseCriteria.LengthInChars = ForEachContext->CharsToFinalSlash;
                    }
                    memcpy(&ForEachContext->RecurseCriteria.StartOfString[ForEachContext->RecurseCriteria.LengthInChars],
                           ForEachContext->FileInfo.cFileName,
                           FileNameLen * sizeof(TCHAR));
                    ForEachContext->RecurseCriteria.LengthInChars += FileNameLen;
                    ForEachContext->RecurseCriteria.StartOfString[ForEachContext->RecurseCriteria.LengthInChars] = '\\';
                    ForEachContext->RecurseCriteria.LengthInChars++;

                    //
                    //  Try to implement support for recursively matching a given
                    //  wild.
                    //

                    if ((MatchFlags & YORILIB_FILEENUM_RECURSE_PRESERVE_WILD) != 0) {
                        if (FinalSlashFound) {
                            _tcscpy(&ForEachContext->RecurseCriteria.StartOfString[ForEachContext->RecurseCriteria.LengthInChars],
                                    &ForEachContext->EffectiveFileSpec.StartOfString[ForEachContext->CharsToFinalSlash]);
                        } else {
                            ASSERT(ForEachContext->CharsToFinalSlash == 0);
                            _tcscpy(&ForEachContext->RecurseCriteria.StartOfString[ForEachContext->RecurseCriteria.LengthInChars],
                                    ForEachContext->EffectiveFileSpec.StartOfString);
                        }
                        ForEachContext->RecurseCriteria.LengthInChars += WildLength;
                    } else {
                        ForEachContext->RecurseCriteria.StartOfString[ForEachContext->RecurseCriteria.LengthInChars] = '*';
                        ForEachContext->RecurseCriteria.LengthInChars++;
                        ForEachContext->RecurseCriteria.StartOfString[ForEachContext->RecurseCriteria.LengthInChars] = '\0';
                    }

                    if (!YoriLibForEachFileEnum(&ForEachContext->RecurseCriteria, MatchFlags, Depth + 1, Callback, ErrorCallback, Context)) {
                        Result = FALSE;
                        break;
                    }

                    YoriLibFreeStringContents(&ForEachContext->RecurseCriteria);
                }

                //
                //  Report the object to the caller if we've determined that it
                //  should be reported.
                //

                if (ReportObject && !RecursePhase) {

                    //
                    //  Convert the found path into a fully qualified path before
                    //  reporting it.
                    //

                    ForEachContext->FullPath.LengthInChars = YoriLibSPrintfS(ForEachContext->FullPath.StartOfString, ForEachContext->FullPath.LengthAllocated, _T("%y\\%s"), &ForEachContext->ParentFullPath, ForEachContext->FileInfo.cFileName);

                    if (!Callback(&ForEachContext->FullPath, &ForEachContext->FileInfo, Depth, Context)) {
                        Result = FALSE;
                        break;
                    }

                    if (YoriLibIsOperationCancelled()) {
                        Result = FALSE;
                        break;
                    }
                }

            } while (hFind != INVALID_HANDLE_VALUE && hFind != NULL && FindNextFile(hFind, &ForEachContext->FileInfo));

            YoriLibFreeStringContents(&ForEachContext->RecurseCriteria);

            if (hFind != NULL && hFind != INVALID_HANDLE_VALUE) {
                FindClose(hFind);
            }

            if (Result == FALSE) {
                break;
            }
        }
    }

    YoriLibFreeStringContents(&ForEachContext->EffectiveFileSpec);
    YoriLibFreeStringContents(&ForEachContext->ParentFullPath);
    YoriLibFreeStringContents(&ForEachContext->FullPath);
    YoriLibFree(ForEachContext);

    return Result;
}

/**
 Enumerate the set of possible files matching a user specified pattern.
 This function is responsible for expanding Yori defined sequences, including
 {}, [], and ~ operators.

 @param FileSpec The user provided file specification to enumerate matches on.

 @param MatchFlags Specifies the behavior of the match, including whether
        it should be applied recursively and the recursing behavior.

 @param Depth Indicates the current recursion depth.  If this function is
        reentered, this value is incremented.

 @param Callback The callback to invoke on each match.

 @param ErrorCallback Optionally points to a function to invoke if a
        directory cannot be enumerated.  If NULL, the caller does not care
        about failures and wants to silently continue.

 @param Context Caller provided context to pass to the callback.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
__success(return)
BOOL
YoriLibForEachFile(
    __in PYORI_STRING FileSpec,
    __in DWORD MatchFlags,
    __in DWORD Depth,
    __in PYORILIB_FILE_ENUM_FN Callback,
    __in_opt PYORILIB_FILE_ENUM_ERROR_FN ErrorCallback,
    __in_opt PVOID Context
    )
{
    YORI_STRING BeforeOperator;
    YORI_STRING AfterOperator;
    YORI_STRING SubstituteValues;
    YORI_STRING MatchValue;
    YORI_STRING NewFileSpec;
    DWORD CharsToOperator;
    BOOL SingleCharMode;

    if (MatchFlags & YORILIB_FILEENUM_BASIC_EXPANSION) {
        return YoriLibForEachFileEnum(FileSpec, MatchFlags, Depth, Callback, ErrorCallback, Context);
    }

    SingleCharMode = FALSE;
    CharsToOperator = YoriLibCountStringNotContainingChars(FileSpec, _T("{["));

    //
    //  If there are no [ or { operators, expand any ~ operators and 
    //  proceed to enumerate the OS provided * and ? operators
    //

    if (CharsToOperator == FileSpec->LengthInChars) {

        if (YoriLibExpandHomeDirectories(FileSpec, &NewFileSpec)) {
            BOOL Result;
            Result = YoriLibForEachFileEnum(&NewFileSpec, MatchFlags, Depth, Callback, ErrorCallback, Context);
            YoriLibFreeStringContents(&NewFileSpec);
            return Result;
        }

        return YoriLibForEachFileEnum(FileSpec, MatchFlags, Depth, Callback, ErrorCallback, Context);
    }

    YoriLibInitEmptyString(&BeforeOperator);
    YoriLibInitEmptyString(&AfterOperator);
    YoriLibInitEmptyString(&SubstituteValues);

    if (FileSpec->StartOfString[CharsToOperator] == '[') {
        SingleCharMode = TRUE;
    }

    BeforeOperator = *FileSpec;
    BeforeOperator.LengthInChars = CharsToOperator;

    SubstituteValues.StartOfString = &FileSpec->StartOfString[CharsToOperator + 1];
    SubstituteValues.LengthInChars = FileSpec->LengthInChars - CharsToOperator - 1;

    CharsToOperator = YoriLibCountStringNotContainingChars(&SubstituteValues, SingleCharMode?_T("]"):_T("}"));
    if (CharsToOperator == SubstituteValues.LengthInChars) {
        return YoriLibForEachFileEnum(FileSpec, MatchFlags, Depth, Callback, ErrorCallback, Context);
    }

    AfterOperator.StartOfString = &SubstituteValues.StartOfString[CharsToOperator + 1];
    AfterOperator.LengthInChars = SubstituteValues.LengthInChars - CharsToOperator - 1;

    SubstituteValues.LengthInChars = CharsToOperator;

    if (SingleCharMode) {
        MatchValue = SubstituteValues;
        MatchValue.LengthAllocated = MatchValue.LengthInChars;
        MatchValue.LengthInChars = 1;
        YoriLibInitEmptyString(&NewFileSpec);
        if (!YoriLibAllocateString(&NewFileSpec, BeforeOperator.LengthInChars + MatchValue.LengthInChars + AfterOperator.LengthInChars + 1)) {
            return FALSE;
        }
        while(TRUE) {

            YoriLibYPrintf(&NewFileSpec, _T("%y%y%y"), &BeforeOperator, &MatchValue, &AfterOperator);

            if (!YoriLibForEachFile(&NewFileSpec, MatchFlags, Depth, Callback, ErrorCallback, Context)) {
                YoriLibFreeStringContents(&NewFileSpec);
                return FALSE;
            }

            if (MatchValue.LengthAllocated <= 1) {
                break;
            }

            MatchValue.LengthAllocated--;
            MatchValue.StartOfString++;
        }
        YoriLibFreeStringContents(&NewFileSpec);
    } else {
        while(TRUE) {
            MatchValue = SubstituteValues;
            CharsToOperator = YoriLibCountStringNotContainingChars(&SubstituteValues, _T(","));

            MatchValue.LengthInChars = CharsToOperator;

            YoriLibInitEmptyString(&NewFileSpec);
            if (!YoriLibAllocateString(&NewFileSpec, BeforeOperator.LengthInChars + MatchValue.LengthInChars + AfterOperator.LengthInChars + 1)) {
                return FALSE;
            }

            YoriLibYPrintf(&NewFileSpec, _T("%y%y%y"), &BeforeOperator, &MatchValue, &AfterOperator);

            if (!YoriLibForEachFile(&NewFileSpec, MatchFlags, Depth, Callback, ErrorCallback, Context)) {
                YoriLibFreeStringContents(&NewFileSpec);
                return FALSE;
            }

            YoriLibFreeStringContents(&NewFileSpec);

            if (SubstituteValues.LengthInChars <= CharsToOperator + 1) {
                break;
            }

            SubstituteValues.StartOfString = &SubstituteValues.StartOfString[CharsToOperator + 1];
            SubstituteValues.LengthInChars -= CharsToOperator + 1;
        }
    }

    return TRUE;
}

/**
 Compare a file name against a wildcard criteria to see if it matches.

 @param FileName The file name to compare.
 
 @param Wildcard The string that may contain wildcards to compare against.

 @return TRUE to indicate a match, FALSE to indicate no match.
 */
__success(return)
BOOL
YoriLibDoesFileMatchExpression (
    __in PYORI_STRING FileName,
    __in PYORI_STRING Wildcard
    )
{
    DWORD FileIndex, WildIndex;

    TCHAR CompareFile;
    TCHAR CompareWild;

    FileIndex = 0;
    WildIndex = 0;

    while (FileIndex < FileName->LengthInChars && WildIndex < Wildcard->LengthInChars) {

        CompareFile = YoriLibUpcaseChar(FileName->StartOfString[FileIndex]);
        CompareWild = YoriLibUpcaseChar(Wildcard->StartOfString[WildIndex]);

        FileIndex++;
        WildIndex++;

        if (CompareWild == '?') {

            //
            //  '?' matches with everything.  We've already advanced to the next
            //  char, so continue.
            //

        } else if (CompareWild == '*') {

            //
            //  Skip over repeated wildcards.
            //

            while (WildIndex < Wildcard->LengthInChars) {
                CompareWild = YoriLibUpcaseChar(Wildcard->StartOfString[WildIndex]);
                if (CompareWild != '*' && CompareWild != '?') {
                    break;
                }
                WildIndex++;
            }

            //
            //  If we're at the end of the string, consisting entirely of
            //  wildcards, then any file name ending would match.
            //

            if (WildIndex == Wildcard->LengthInChars) {
                return TRUE;
            }

            //
            //  If there's a literal after the wildcard, look forward in the
            //  file name to see if it's there.
            //

            while (FileIndex < FileName->LengthInChars) {
                CompareFile = YoriLibUpcaseChar(FileName->StartOfString[FileIndex]);
                if (CompareFile == CompareWild) {
                    break;
                }
                FileIndex++;
            }

            //
            //  There is a literal after the wild but it wasn't found in the
            //  file name.  This is not a match.
            //

            if (FileIndex == FileName->LengthInChars) {
                return FALSE;
            }

        } else {
            if (CompareFile != CompareWild) {
                return FALSE;
            }
        }
    }

    //
    //  Skip over repeated wildcards.
    //

    while (WildIndex < Wildcard->LengthInChars) {
        ASSERT(FileIndex == FileName->LengthInChars);
        CompareWild = YoriLibUpcaseChar(Wildcard->StartOfString[WildIndex]);
        if (CompareWild != '*' && CompareWild != '?') {
            break;
        }
        WildIndex++;
    }

    if (FileIndex == FileName->LengthInChars && WildIndex == Wildcard->LengthInChars) {
        return TRUE;
    }

    return FALSE;
}

/**
 Generate information typically returned from a directory enumeration by
 opening the file and querying information from it.  This is used for named
 streams which do not go through a regular file enumeration.

 @param FindData On successful completion, populated with information 
        typically returned by the system when enumerating files.

 @param FullPath Pointer to a NULL terminate string referring to the full
        path to the file.

 @param CopyName TRUE if the full path's file name component should also be
        copied into the find data structure.  FALSE if the caller does not
        need this or will do it manually.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
__success(return)
BOOL
YoriLibUpdateFindDataFromFileInformation (
    __out PWIN32_FIND_DATA FindData,
    __in LPTSTR FullPath,
    __in BOOL CopyName
    )
{
    HANDLE hFile;
    BY_HANDLE_FILE_INFORMATION FileInfo;
    LPTSTR FinalSlash;

    hFile = CreateFile(FullPath,
                       FILE_READ_ATTRIBUTES,
                       FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                       NULL,
                       OPEN_EXISTING,
                       FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_OPEN_NO_RECALL,
                       NULL);

    if (hFile != INVALID_HANDLE_VALUE) {

        GetFileInformationByHandle(hFile, &FileInfo);

        FindData->dwFileAttributes = FileInfo.dwFileAttributes;
        FindData->ftCreationTime = FileInfo.ftCreationTime;
        FindData->ftLastAccessTime = FileInfo.ftLastAccessTime;
        FindData->ftLastWriteTime = FileInfo.ftLastWriteTime;
        FindData->nFileSizeHigh = FileInfo.nFileSizeHigh;
        FindData->nFileSizeLow  = FileInfo.nFileSizeLow;

        CloseHandle(hFile);

        if (CopyName) {
            FinalSlash = _tcsrchr(FullPath, '\\');
            if (FinalSlash) {
                YoriLibSPrintfS(FindData->cFileName, MAX_PATH, _T("%s"), FinalSlash + 1);
            } else {
                YoriLibSPrintfS(FindData->cFileName, MAX_PATH, _T("%s"), FullPath);
            }
        }
        return TRUE;
    }
    return FALSE;
}

// vim:sw=4:ts=4:et:
