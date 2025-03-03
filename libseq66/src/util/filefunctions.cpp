/*
 *  This file is part of seq66.
 *
 *  seq66 is free software; you can redistribute it and/or modify it under the
 *  terms of the GNU General Public License as published by the Free Software
 *  Foundation; either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  seq66 is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with seq66; if not, write to the Free Software Foundation, Inc., 59 Temple
 *  Place, Suite 330, Boston, MA  02111-1307  USA
 */

/**
 * \file          filefunctions.cpp
 *
 *    Provides the implementations for safe replacements for the various C
 *    file functions.
 *
 * \library       seq66 application
 * \author        Chris Ahlstrom
 * \date          2015-11-20
 * \updates       2021-07-18
 * \version       $Revision$
 *
 *    We basically include only the functions we need for Seq66, not
 *    much more than that.  These functions are adapted from our xpc_basic
 *    project.
 *
 *    In addition, internally we want to handle all file-specifications in
 *    UNIX format, even if the name includes Window's constructs such as "C:".
 *    So the normalize_path() function (which converts both ways) is used
 *    heavily.  For OS calls, we want to use the native format.
 */

#include <algorithm>                    /* std::replace() function          */
#include <cctype>                       /* std::toupper() function          */
#include <cstdlib>                      /* realpath(3) or _fullpath()       */
#include <string.h>                     /* strlen(), strerror_r() etc.      */
#include <sys/stat.h>

#include "util/basic_macros.hpp"        /* support and platform macros      */
#include "util/calculations.hpp"        /* seq66::current_date_time()       */
#include "util/filefunctions.hpp"       /* free functions in seq66 n'space  */
#include "util/strfunctions.hpp"        /* free functions in seq66 n'space  */

/**
 *  All file-specifications internal to Seq66 and in its configuration files
 *  use the UNIX path separator ("/"), no matter what the operating system.
 *  Also, select the HOME or LOCALAPPDATA environment variables depending on
 *  whether building for Windows or not.  LOCALAPPDATA points to the root of
 *  the Windows user's configuration directory, AppData/Local.
 */

#if defined SEQ66_PLATFORM_WINDOWS
#define SEQ66_PATH_SLASH                "\\"
#define SEQ66_PATH_SLASH_CHAR           '\\'
#define SEQ66_ENV_HOME                  "LOCALAPPDATA"
#else
#define SEQ66_PATH_SLASH                "/"
#define SEQ66_PATH_SLASH_CHAR           '/'
#define SEQ66_ENV_HOME                  "HOME"
#endif

/*
 *  More legacy configuration macros.
 */

#if SEQ66_HAVE_LIMITS_H
#include <limits.h>                     /* PATH_MAX                         */
#endif

#if defined SEQ66_PLATFORM_GLIBC        /* TO DO!!!!                        */
#include <sys/auxv.h>                   /* getauxvalue() glibc function     */
#endif

#if defined SEQ66_PLATFORM_WINDOWS      /* Microsoft compiler               */

#include <dir.h>                        /* file-name info and getcwd()      */
#include <io.h>                         /* _access_s()                      */
#include <share.h>                      /* _SH_DENYNO                       */

#if defined SEQ66_PLATFORM_MSVC         /* versus Windows with Qt+MingW     */
#define F_OK        0x00                /* existence                        */
#define X_OK        0x01                /* executable, not useful w/Windows */
#define W_OK        0x02                /* writability                      */
#define R_OK        0x04                /* readability                      */

#define S_ACCESS    _access_s           /* Microsoft's safe access()        */
#define S_CHDIR     _chdir              /* Microsoft's chdir()              */
#define S_CLOSE     _close              /* Microsoft's close()              */
#define S_FOPEN     fopen_s             /* Microsoft's safe fopen()         */
#define S_GETCWD    _getcwd             /* Microsoft's getcwd()             */
#define S_LSEEK     _lseek              /* Microsoft's lseek()              */
#define S_MKDIR     _mkdir              /* Microsoft's mkdir()              */
#define S_OPEN      _sopen_s            /* Microsoft's safe open()          */
#define S_RMDIR     _rmdir              /* Microsoft's rmdir()              */
#define S_STAT      _stat               /* Microsoft's stat function        */
#define S_MAX_FNAME _MAX_FNAME          /* Microsoft's filename size (260!) */
#define S_MAX_PATH  _MAX_PATH           /* Microsoft's path size (260!)     */

using stat_t = struct _stat;

#else                                   /* not SEQ66_PLATFORM_MSVC: Mingw   */

#define S_ACCESS    _access_s           /* Microsoft's safe access()        */
#define S_CHDIR     chdir
#define S_CLOSE     _close              /* Microsoft's close()              */
#define S_FOPEN     fopen_s             /* Microsoft's safe fopen()         */
#define S_GETCWD    _getcwd             /* Microsoft's getcwd()             */
#define S_LSEEK     _lseek              /* Microsoft's lseek()              */
#define S_MKDIR     mkdir
#define S_OPEN      _sopen_s            /* Microsoft's safe open()          */
#define S_RMDIR     rmdir
#define S_STAT      stat
#define S_MAX_FNAME _MAX_FNAME          /* Microsoft's filename size (260!) */
#define S_MAX_PATH  _MAX_PATH           /* Microsoft's path size (260!)     */

using stat_t = struct stat;

#endif                                  /* SEQ66_PLATFORM_MSVC      */

#else                                   /* non-Microsoft stuff follows      */

#include <unistd.h>

#define S_ACCESS    access              /* ISO/POSIX/BSD unsafe access()    */
#define S_CHDIR     chdir               /* ISO/POSIX/BSD chdir()            */
#define S_CLOSE     close               /* ISO/POSIX/BSD close()            */
#define S_FOPEN     fopen               /* ISO/POSIX/BSD safe sprintf()     */
#define S_GETCWD    getcwd              /* ISO/POSIX/BSD getcwd()           */
#define S_LSEEK     lseek               /* ISO/POSIX/BSD lseek()            */
#define S_MKDIR     mkdir               /* ISO/POSIX/BSD mkdir()            */
#define S_OPEN      open                /* ISO/POSIX/BSD safe open()        */
#define S_RMDIR     rmdir               /* ISO/POSIX/BSD rmdir()            */
#define S_STAT      stat                /* ISO/POSIX/BSD stat function      */
#define S_MAX_FNAME NAME_MAX            /* ISO/POSIX/BSD stat function      */
#define S_MAX_PATH  PATH_MAX            /* POSIX path size                  */

using stat_t = struct stat;

#endif                                  /* SEQ66_PLATFORM_WINDOWS           */

/**
 *  A macro for the error code when a file does not exist.
 */

#define ERRNO_FILE_DOESNT_EXIST     2   /* true in Windows and Linux        */

/**
 *  A more comprehensible name for the type of errno.
 */

using errno_t = int;

/*
 *  Do not document a namespace; it breaks Doxygen.
 */

namespace seq66
{

/*
 *  Almost equivalent to SEQ66_PLATFORM_WINDOWS.
 */

#if defined SEQ66_PLATFORM_MING_OR_WINDOWS

/**
 *  An internal function to assist other copying functions.  All parameters
 *  are assumed to have been guaranteed valid by the caller.
 *
 * \param destination
 *      The pointer to the area to receive the copy.
 *
 * \param destsize
 *      The maximum number of characters to be copied, plus one.  The space
 *      needed for the terminating null character must be included in this
 *      count.
 *
 * \param source
 *      The null-terminated string to be copied.
 *
 * \param sourcelimit
 *      An optional number of characters to which to limit the source string.
 *      It defaults to 0.  The caller does not need to guarantee that it is
 *      not longer than the actual length of the source; the copying stops at
 *      the null character.
 *
 *      -   If 0, the limit is set to the length of the source.
 *      -   If greater than the actual length of the source, the limit is set
 *          to the length of the source.
 *
 * \return
 *      Returns 'true' if the parameters were valid, so that the whole source
 *      string could be copied (without truncation).  Even if the parameters
 *      were invalid, the destination still can be used safely -- it might be
 *      empty or truncated, though.
 */

static bool
s_stringcopy
(
   char * destination,
   size_t destsize,
   const char * source,
   size_t sourcelimit = 0
)
{
    bool result = false;
    size_t length = strlen(source);             /* inefficient              */
    *destination = 0;                           /* empty out destination    */
    if (sourcelimit > length || sourcelimit == 0)
        sourcelimit = length;

    if (sourcelimit <= INT_MAX)                 /* just a sanity check      */
    {
        /*
         * This code checks the length of the source string, and decides
         * if it can copy the whole string, or has to limit the string and
         * enforce the null terminator.
         */

#if defined SEQ66_PLATFORM_MSVC

        int rcode;
        size_t count = _TRUNCATE;                /* overloaded parameter!!  */
        if (sourcelimit < destsize)              /* truncation impossible   */
        {
            destsize = sourcelimit + 1;
            count = sourcelimit;
        }
        rcode = strncpy_s                        /* guaranteed null byte    */
        (
            destination, destsize, source, count
        );
        if (rcode == 0)
            result = true;
        else
            warn_message(T_("stringcopy truncation"));
#else
        if (sourcelimit < destsize)              /* truncation impossible   */
        {
            destsize = sourcelimit + 1;
            result = true;
        }
        else                                     /* truncation definite     */
            warn_message(T_("stringcopy truncation"));

        (void) strncpy(destination, source, destsize);
        destination[destsize-1] = 0;
#endif
    }
    return result;
}

#endif  // defined SEQ66_PLATFORM_MING_OR_WINDOWS

/**
 *  A static function to make safer copies of a system error message than does
 *  strerror().  Replaces strerror() [Borland], strerror_s() [Microsoft], and
 *  strerror_r() [GNU].  The differences in the three functions are accounted
 *  for in this implementation.
 *
 * \win32
 *    The Windows version returns a 0 (success) when the errnum value is such
 *    that it yields an "Unknown error"!  In Microsoft's crtdefs.h, errno_t is
 *    an integer.  The values in Microsoft's errno.h go up to only a couple
 *    hundred.  Nobody defines an ELAST value, so we will here, for now.
 *
 * \param errnum
 *      The errno variable to be translated to a message.
 *
 * \return
 *      Returns an empty string if no error occurred in the function.
 */

static std::string
string_errno (errno_t errnum)
{
    std::string result;
    char dest[1024];                    /* static allocation for now only   */
    bool ok = true;                     /* start optimistically             */
    dest[0] = 0;                        /* make it an empty string to start */

#if defined SEQ66_PLATFORM_MING_OR_WINDOWS
#define ELAST (EWOULDBLOCK + 1)         /* Can change at vendor whim!       */

    if (errnum != 0)
    {
        int rcode = strerror_s(dest, sizeof dest, errnum);
        ok = (errnum >= 0) && (errnum < ELAST) ? (rcode == 0) : false ;
    }
    else
        ok = s_stringcopy(dest, sizeof dest, T_("Possible truncation"));

#endif

#if defined SEQ66_PLATFORM_XSI

    int rcode = strerror_r(int(errnum), dest, sizeof dest);
    ok = rcode == 0;

#elif defined SEQ66_PLATFORM_GNU

    /*
     * This code gets compiled in Qt/Mingw on Windows.
     */

    char * msg = strerror(int(errnum));
    (void) strncpy(dest, msg, sizeof dest - 1);

#else

    const char * msg = strerror(errnum);
    (void) strncpy(dest, msg, sizeof dest - 1);

#endif

    if (ok)
        result = std::string(dest);
    else
        result = std::string("string_errno() internal error");

    return result;
}

/**
 *  Checks for a file error, reporting it to the error-log if there is one.
 *
 * \private
 *      This function is visible and used only in this module, and assumes
 *      that the pointer parameters have been checked by the caller.
 *
 * \param filename
 *      Provides the name of the file to be handled.
 *
 * \param mode
 *      Provides the mode of usage for the file, which can be one of the
 *      file-opening mode, or the number of the function in which the failure
 *      occurred.
 *
 * \param errnum
 *      Provides the error code.  If 0, there is no error to report.
 *
 * \return
 *      Returns the status of the \a errnum variable.
 */

static bool
s_file_error
(
    const std::string & filename,
    const std::string & mode,
    int errnum
)
{
    bool result = errnum == 0;
    if (! result)
    {
        std::string temp = string_errno(errnum);
        temp += " (mode/function " + mode + ")";
        file_error(temp, filename);
    }
    return result;
}

/**
 *  Checks a file for the desired access modes.  The following modes are
 *  defined, and can be OR'd:
 *
\verbatim
      POSIX Value Windows  Meaning
      F_OK    0   0x00     Existence
      X_OK    1   N/A      Executable
      W_OK    2   0x04     Writable
      R_OK    4   0x02     Readable
\endverbatim
 *
 * \win32
 *      Windows does not provide a mode to check for executability.
 *
 * \param filename
 *      Provides the name of the file to be checked.
 *
 * \param mode
 *      Provides the mode of the file to check for.  This value should be in
 *      the cross-platform set of file-mode's for the various versions of the
 *      fopen() function.
 *
 * \return
 *      Returns 'true' if the requested modes are all supported for the file.
 */

bool
file_access (const std::string & filename, int mode)
{
    bool result = file_name_good(filename);
    if (result)
    {
#if defined SEQ66_PLATFORM_MSVC
       /**
        * Passing in X_OK here on Windows 7 yields a debug assertion!  For
        * now, we just have to return false if that value is part of the mode
        * mask.
        */

        if (mode & X_OK)
        {
            errprint("cannot test X_OK (executable bit) on Windows");
            result = false;
        }
        else
        {
            int errnum = S_ACCESS(filename.c_str(), mode);
            result = errnum == 0;
        }
#else
        int errnum = S_ACCESS(filename.c_str(), mode);
        result = errnum == 0;
#endif
    }
    return result;
}

/**
 *    Checks a file for existence.
 *
 * \param filename
 *    Provides the name of the file to be checked.
 *
 * \return
 *    Returns 'true' if the file exists.
 *
 */

bool
file_exists (const std::string & filename)
{
    return file_access(filename, F_OK);
}

/**
 *    Checks a file for readability.
 *
 * \param filename
 *    Provides the name of the file to be checked.
 *
 * \return
 *    Returns 'true' if the file is readable.
 *
 */

bool
file_readable (const std::string & filename)
{
    return file_access(filename, R_OK);
}

/**
 *    Checks a file for writability.
 *
 * \param filename
 *    Provides the name of the file to be checked.
 *
 * \return
 *    Returns 'true' if the file is writable.
 *
 */

bool
file_writable (const std::string & filename)
{
    return file_access(filename, W_OK);
}

/**
 *    Checks a file for readability and writability.  An even stronger test
 *    than file_exists.  At present, we see no need to distinguish read and
 *    write permissions.  We assume the file is fully accessible only if the
 *    file has both permissions.
 *
 *    This can be surprising if one wants only to read a file, and the file is
 *    read-only.
 *
 * \param filename
 *    Provides the name of the file to be checked.
 *
 * \return
 *    Returns 'true' if the file is readable and writable.
 */

bool
file_read_writable (const std::string & filename)
{
    return file_access(filename, R_OK|W_OK);
}

/**
 *    Checks a file for the ability to be executed.
 *
 * \param filename
 *    Provides the name of the file to be checked.
 *
 * \return
 *    Returns 'true' if the file exists.
 */

bool
file_executable (const std::string & filename)
{
    bool result = file_name_good(filename);
    if (result)
    {
        stat_t statusbuf;
        int statresult = S_STAT(filename.c_str(), &statusbuf);
        if (statresult == 0)                          /* a good file handle? */
        {
#if defined SEQ66_PLATFORM_MSVC
            result = (statusbuf.st_mode & _S_IEXEC) != 0;
#else
            result =
            (
                ((statusbuf.st_mode & S_IXUSR) != 0) ||
                ((statusbuf.st_mode & S_IXGRP) != 0) ||
                ((statusbuf.st_mode & S_IXOTH) != 0)
            );
#endif
        }
        else
            result = false;
    }
    return result;
}

/**
 *    Checks a file to see if it is a directory.  This function is also used
 *    in the function of the same name in fileutilities.cpp.
 *
 * \param filename
 *    Provides the name of the directory to be checked.
 *
 * \return
 *    Returns 'true' if the file is a directory.
 */

bool
file_is_directory (const std::string & filename)
{
    bool result = file_name_good(filename);
    if (result)
    {
        stat_t statusbuf;
        int statresult = S_STAT(filename.c_str(), &statusbuf);
        if (statresult == 0)                           // a good file handle?
        {
#if defined SEQ66_PLATFORM_MSVC
            result = (statusbuf.st_mode & _S_IFDIR) != 0;
#else
            result = (statusbuf.st_mode & S_IFDIR) != 0;
#endif
        }
        else
            result = false;
    }
    return result;
}

/**
 *  Verifies that a file-name pointer is legal.  The following checks are
 *  made:
 *
 *          -  The file-name is not empty.
 *          -  The file-name is not one of the following:
 *              -  "stdout"
 *              -  "stdin"
 *              -  "stderr"
 *
 *  Does not replace any function, but is a helper function that is worthwhile
 *  to expose publicly.
 *
 * \param fname
 *    Provides the file name to be verified.
 *
 * \return
 *    Returns 'true' if the file-name is valid.
 */

bool
file_name_good (const std::string & fname)
{
    bool result = ! fname.empty();
    if (result)
    {
        result = fname != "stdout" && fname != "stdin" && fname != "stderr";
        if (! result)
            file_message(T_("file-name invalid"), fname);
    }
    return result;
}

/**
 *  Verifies that a file-mode pointer is legal.  The following checks of the
 *  file-mode string are made:
 *
 *       -  The mode is not empty.
 *       -  The mode is one of the following:
 *          -  r, w, a
 *          -  r+, w+, a+
 *          -  rb, wb, ab
 *          -  rb+, wb+, ab+
 *          -  r+b, w+b, a+b
 *          -  rt, wt, at
 *          -  rt+, wt+, at+
 *          -  r+t, w+t, a+t
 *          In other words, non-standard extensions are not allowed.
 *
 *  Does not replace any function, but is a helper function that is worthwhile
 *  to expose publicly.
 *
 * \param mode
 *      Provides the file-opening mode string to be verified.
 *
 * \return
 *      Returns 'true' if the file-name is valid.
 *
 */

bool
file_mode_good (const std::string & mode)
{
    bool result = ! mode.empty();
    if (result)
    {
        /*
         * Accept only valid characters here.  Note that this test will
         * reject empty strings as well.
         */

        result = mode[0] == 'r' || mode[0] == 'w' || mode[0] == 'a';
        if (result)
        {
            if (mode[1] != 0)
            {
                result = mode[1] == '+' || mode[1] == 'b' || mode[1] == 't';
                if (result)
                {
                    if (mode[2] != 0)
                    {
                        result = mode[2] == '+' || mode[2] == 'b' ||
                            mode[2] == 't';

                        if (result)
                            result = mode[3] == 0;
                    }
                }
            }
        }
        if (! result)
            file_message(T_("file-mode invalid"), mode);
    }
    return result;
}

/**
 *  Opens a file in the desired operating mode.  It replaces fopen() or
 *  fopen_s() [Microsoft].
 *
 * \note
 *      Both the Microsoft and GNU "fopen" functions provide an errno-style
 *      error code when the function fails.  The Microsoft function fopen_s()
 *      returns the error-code directly.  The fopen(3) function returns a null
 *      pointer, and sets the value of 'errno' to indicate the error.
 *
 * \param filename
 *      Provides the name of the file to be opened.
 *
 * \param mode
 *      Provides the mode of the file.  See the file_mode_good() function.
 *      This value should be in the cross-platform set of file-mode's for the
 *      various versions of the fopen() function.
 *
 * \return
 *      Returns the file pointer if the function succeeded.  Otherwise, a null
 *      pointer is returned.  The caller must check this value before
 *      proceeding to use the file-handle.
 */

FILE *
file_open (const std::string & filename, const std::string & mode)
{
    FILE * filehandle = nullptr;
    if (file_name_good(filename) && ! mode.empty())
    {
#if defined SEQ66_PLATFORM_WINDOWS      /* MSVC undefined in Qt on Windows  */
        int errnum = (int) S_FOPEN(&filehandle, filename.c_str(), mode.c_str());
        if (errnum != 0)
            filehandle = nullptr;
#else
        int errnum = 0;
        filehandle = S_FOPEN(filename.c_str(), mode.c_str());
        if (is_nullptr(filehandle))
            errnum = errno;
#endif

        (void) s_file_error(filename, mode, errnum);
    }
    return filehandle;
}

/**
 *    Opens a file for binary reading.  This function calls
 *    file_open(filename, "rb").  It replaces fopen().
 *
 * \param filename
 *      Provides the name of the file to be opened.
 *
 * \return
 *      Returns the file pointer if the function succeeded.  Otherwise, a null
 *      pointer is returned.  The caller must check this value before
 *      proceeding to use the file-handle.
 */

FILE *
file_open_for_read (const std::string & filename)
{
    FILE * filehandle = nullptr;
    if (file_readable(filename))
        filehandle = file_open(filename, "rb");  /* open for reading only    */

    return filehandle;
}

/**
 *  Recreates a file for binary writing.  This function calls
 *  file_open(filename, "wb").  Replaces fopen().
 *
 * \note
 *      One might expect that this function should fail if the file already
 *      exists.  Indeed, that would be a nice function to have.  However, this
 *      function supports legacy code, and we can't modify what it does at
 *      this time.
 *
 * \param filename
 *      Provides the name of the file to be opened.
 *
 * \return
 *      Returns the file pointer if the function succeeded.  Otherwise, a null
 *      pointer is returned.  The caller must check this value before
 *      proceeding to use the file-handle.
 */

FILE *
file_create_for_write (const std::string & filename)
{
    return file_open(filename, "wb");
}

bool
file_write_string (const std::string & filename, const std::string & text)
{
    FILE * fptr = file_open(filename, "w");
    bool result = not_nullptr(fptr);
    if (result)
    {
        size_t len = text.length();
        size_t rc = fwrite(text.c_str(), sizeof(char), len, fptr);
        if (rc < len)
        {
            errprintf("could not write to '%s'", filename.c_str());
            result = false;
        }
        (void) file_close(fptr, filename);
    }
    return result;
}

/**
 *  Closes a file opened by the file_open() functions. Replaces fclose().
 *
 * \param filehandle
 *      Provides the file-handle to be closed.
 *
 * \param filename
 *      Provides the name of the file to be closed.  Used only for error
 *      reporting.  If you don't care about it, pass in an empty string,
 *      which is the default value.
 *
 * \return
 *      Returns 'true' if the close operation succeeded.
 */

bool
file_close (FILE * filehandle, const std::string & filename)
{
    bool result = not_nullptr_assert(filehandle, T_("file_close() null handle"));
    if (result)
    {
        int rcode = fclose(filehandle);
        result = s_file_error(filename, __func__, rcode);
    }
    else
    {
        /*
         * if (! filename.empty())
         *     errprintex(filename, T_("File"));
         */
    }
    return result;
}

/**
 *  Copies a file to a file with a different name.  The file names can be
 *  absolute, or they can be relative.  This function will refuse to copy a
 *  file onto itself.
 *
 *  (This check is simply a check for string-equivalence of the names; the
 *  check does not try to figure out if the paths resolve to the same
 *  file-name and path, for now.  The get_full_path() function will work only
 *  if the file already exists.)
 *
 * \param oldfile
 *      The full path to the source file.
 *
 * \param newfile
 *      The full path to the destination file.
 *
 * \todo
 *      Consider fflush() to flush the user-space buffers provided by the C
 *      library, and then fsync() to have the kernel flush the file to disk,
 *      followed by fsync() on the directory itself to finish the process.
 *      See the Linux man pages [fflush(3), sync(2), and fsync(2)].
 *
 * \return
 *      Returns 'true' if all of the parameters were valid, and all of the
 *      operations succeeded.  Returns 'false' otherwise.
 */

bool
file_copy
(
    const std::string & oldfile,
    const std::string & newfile
)
{
    bool result = file_name_good(oldfile) && file_name_good(newfile);
    if (result)
    {
        result = get_full_path(newfile) != get_full_path(oldfile);
        if (result)
        {
            FILE * input = file_open_for_read(oldfile);
            if (not_nullptr(input))
            {
                bool okinput = false;
                bool okoutput = false;
                FILE * output = file_create_for_write(newfile);
                if (not_nullptr(output))
                {
                    int ci;
                    while ((ci = fgetc(input)) != EOF)
                    {
                        int co = fputc(ci, output);
                        if (co == EOF)
                            break;
                    }
                    okoutput = file_close(output, newfile);
                }
                okinput = file_close(input, oldfile);
                result = okinput && okoutput;
            }
        }
        else
            warn_message(T_("filenames are equivalent"));
    }
    return result;
}

/**
 *  Appends a character buffer to a file in the configuration directory.
 *  Useful for dumping error information, such as under PortMidi in Windows.
 *
 * \param filename
 *      Provides the full path to the file plus the file-name and
 *      file-extension.  One way to create such a path is using the
 *      rcsettings::config_filespec() function.
 *
 * \param data
 *      Provides the string data to write, already formatted and ready to go.
 *
 * \return
 *      Returns true if all steps in the process succeeded.
 */

bool
file_append_log
(
    const std::string & filename,
    const std::string & data
)
{
    FILE * fp = file_open(filename, "a");           /* open for appending   */
    bool result = not_nullptr(fp);
    if (result)
    {
        std::string log = "\n";
        log += current_date_time();
        log += "\n";
        log += data.data();
        log += "\n\n";

        size_t rc = fwrite(log.data(), sizeof(char), log.size(), fp);
        if (rc < log.size())
        {
            errprintf("could not write to '%s'", filename.c_str());
            result = false;
        }
        (void) file_close(fp, filename);
    }
    return result;
}

/**
 *  Check if the file-name has a "/", or, in Windows, "\" or ":".  The latter
 *  case covers names like "C:filename.ext".
 *
 *  Obviously, this function otherwise assumes a well-formed file
 *  specification.
 *
 * \param filename
 *      The name of the file, to be tested.
 *
 * \return
 *      Returns true if any directory characters are found.
 */

bool
name_has_path (const std::string & filename)
{
    auto pos = filename.find_first_of("/");
    bool result = pos != std::string::npos;
#if defined SEQ66_PLATFORM_WINDOWS
    if (! result)
    {
        pos = filename.find_first_of("\\");
        result = pos != std::string::npos;
        if (! result)
        {
            pos = filename.find_first_of(":");
            result = pos != std::string::npos;
        }
    }
#endif
    return result;
}

/**
 *  Detects if the filename is likely to contain a root path:
 *
\verbatim
        /dir/dir2 ...
        \dir\dir2 ...
        x:
\endverbatim
 *
 */

bool
name_has_root_path (const std::string & filename)
{
    auto pos = filename.find_first_of("/");
    bool result = pos != std::string::npos;
#if defined SEQ66_PLATFORM_WINDOWS
    if (! result)
    {
        pos = filename.find_first_of("\\");
        result = pos != std::string::npos;
    }
#endif
    if (result)
        result = pos == 0;              /* path starts with "/" or "\"  */

#if defined SEQ66_PLATFORM_WINDOWS
    if (! result)
    {
        pos = filename.find_first_of(":");
        result = pos != std::string::npos;
        if (result)
            result = std::isalpha(filename[0]) && pos == 1;
    }
#endif
    return result;
}

/**
 *  A function to ensure that the ~/.config/seq66 directory exists.  This
 *  function is actually a little more general than that, but it is not
 *  sufficiently general, in general, General.  Consider using
 *  make_directory_path(), defined elsewhere in this module.  This one is now
 *  static.
 *
 * \param pathname
 *      Provides the name of the path to create.  The parent directory of the
 *      final directory must already exist.
 *
 * \return
 *      Returns true if the path-name exists.
 */

#if defined SEQ66_PLATFORM_GNU
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

static bool
make_directory (const std::string & pathname)
{
    bool result = file_name_good(pathname);
    if (result)
    {
        static struct stat st =
        {
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 /* and more for Linux! */
        };
        if (S_STAT(pathname.c_str(), &st) == -1)
        {
#if defined SEQ66_PLATFORM_WINDOWS
            int rcode = S_MKDIR(pathname.c_str());
#else
            int rcode = S_MKDIR(pathname.c_str(), 0755);    /* rwxr-xr-x    */
#endif
            result = rcode == 0;
            if (! result)
                file_error("mkdir() failed", pathname);
        }
    }
    return result;
}

/**
 *  Creates a directory based on its full directory path.  This function is a
 *  turbo version of the mkdir() and _mkdir() functions.  It ensures that the
 *  whole sequences of directories in a path-name are created, if they don't
 *  exist already.  In addition, the path-name parameter is massaged in
 *  whatever way is necessary, such as removing any terminating backslash.
 *  For now, since at least _mkdir() can handle either the '/' or the '\', we
 *  don't worry about converting to the '\' for DOS-like stuff.  In fact, for
 *  the Seq66 project, all paths are UNIX-style internally, and calling the
 *  normalize_path() function guarantees that.  The input pathname is in the
 *  form of:
 *
\verbatim
           /dir0/dir1/dir2/.../dirn/
           C:/dir0/dir1/dir2/.../dirn/
\endverbatim
 *
 *  The code works by temporarily converting each '/' character in sequence to
 *  a null, and seeing if the resulting entity (drive or directory) exists.
 *  If not, the attempt is made to create it.  If it succeeds, the null is
 *  converted back to a '/', and the next subdirectory is worked on, until all
 *  are done.  For absolute UNIX or Windows paths, we do not remove the first
 *  slash.
 *
 * \bug
 *      This uses C calls.
 *
 *  Replaces:
 *
 *    -  _mkdir() [Microsoft]
 *    -  mkdir() [GNU].
 *
 * \param directory_name
 *      Provides the name of the directory to create.
 *
 * \return
 *      Returns true if the create operation succeeded.  It also returns true
 *      if the directory already exists.
 */

bool
make_directory_path (const std::string & directory_name)
{
    bool result = file_name_good(directory_name);
    std::string dirname = normalize_path(directory_name);
    if (result)
    {
        if (file_exists(dirname))               /* directory already exists */
            return true;
    }
    if (result)
    {
        result = dirname.length() < S_MAX_PATH;
        if (! result)
            file_error("Path too long", dirname);
    }
    if (result)
    {
        char currdir[S_MAX_PATH];
        bool more = true;
        int slash = '/';
        char * nextptr;                         /* just what it says!       */
        (void) strncpy(currdir, dirname.c_str(), sizeof currdir - 1);

        char * endptr = &currdir[0];            /* start at the beginning   */
        char * ending = strchr(endptr, '\0');
        do
        {
            nextptr = strchr(endptr, slash);    /* find next slash          */
            if (is_nullptr(nextptr))            /* if still not found ...   */
            {
                more = false;                   /* ...this is last subdir   */
                if (endptr < ending)            /* ...if not yet at end     */
                    nextptr = ending;           /* ...this will be the end  */
            }
            else
            {
                if (nextptr == endptr)
                {
                    ++endptr;
                    continue;
                }
            }
            if (not_nullptr(nextptr))
            {
                *nextptr = 0;                   /* make null terminator     */
                if (! file_exists(currdir))     /* subdirectory exists?     */
                {
                    if (! make_directory(std::string(currdir)))
                    {
                        more = result = false;
                        break;
                    }
                }
                if (more)
                {
                    *nextptr = char(slash);     /* restore the slash        */
                    endptr = nextptr + 1;       /* point just past slash    */
                }
            }
        } while (more);
    }
    return result;
}

/**
 *  Deletes a directory based on its directory name.  This function first makes
 *  sure that the name is valid, using the file_name_good() function.
 *
 * Replaces:
 *
 *    -  _rmdir() [Microsoft]
 *    -  rmdir() [GNU].  Note, however, that file_delete() can be called and
 *      will work with both files and directories.
 *
 * \param filename
 *      Provides the name of the file to be deleted.
 *
 * \return
 *      Returns true if the delete operation succeeded.  It also returns true
 *      if the directory did not exist in the first place.
 */

bool
delete_directory (const std::string & filename)
{
    bool result = file_name_good(filename) && file_exists(filename);
    if (result)
    {
        int rcode = S_RMDIR(filename.c_str());
        if (rcode == (-1))
            result = s_file_error(filename, __func__, errno);
    }
    return result;
}

/**
 *  Provides the path name of the current working directory.  This function is
 *  a wrapper for getcwd() and other such functions.  It obtains the current
 *  working directory in the application.
 *
 * \return
 *      The pointer to the string containg the name of the current directory.
 *      This name is the full path name for the directory.  If an error
 *      occurs, then an empty string is returned.
 */

std::string
get_current_directory ()
{
    std::string result;
    char temp[PATH_MAX];
    char * cwd = S_GETCWD(temp, PATH_MAX);      /* get current directory      */
    if (not_nullptr(cwd))
    {
        size_t len = strlen(cwd);
        if (len > 0)
        {
            result = cwd;
        }
        else
        {
            errprint("empty directory name returned");
        }
    }
    else
    {
        errprint("could not get current directory");
    }
    return result;
}

/**
 *  Given a path, relative or not, this function returns the full path.
 *  It uses the Linux function realpath(3), which returns the canonicalized
 *  absolute path-name.  For Windows, the function _fullpath() is used.
 *
 * \param path
 *      Provides the path, which may be relative.
 *
 * \return
 *      Returns the full path.  If a problem occurs, the result is empty.
 */

std::string
get_full_path (const std::string & path)
{
    std::string result;                         /* default empty result     */
    if (file_name_good(path))
    {
#if defined SEQ66_PLATFORM_WINDOWS              /* _MSVC not defined in Qt  */
        char * resolved_path = NULL;            /* what a relic!            */
        char temp[256];
        resolved_path = _fullpath(temp, path.c_str(), 256);
        if (not_NULL(resolved_path))
            result = resolved_path;
#else
        char * resolved_path = NULL;            /* what a relic!            */
        resolved_path = realpath(path.c_str(), NULL);
        if (not_NULL(resolved_path))
        {
            result = resolved_path;
            free(resolved_path);
        }
#endif
    }
    return result;
}

/**
 *  Returns the default path separator. Based on the operating system:  a
 *  backslash for Windows and (laughing) CPM and DOS, and a forward slash
 *  for UNIXen.
 */

char
path_slash ()
{
    return SEQ66_PATH_SLASH_CHAR;
}

/**
 *  Makes sure that the path-name is a UNIX path, separated by forward slashes
 *  (the solidus), or a Windows path, separated by back slashes.  It also
 *  converts "~" to the user's HOME or LOCALAPPDATA directory.
 *
 * \param path
 *      Provides the path, which should be a full path-name.
 *
 * \param to_unix
 *      Defaults to true, which converts "\" to "/".  False converts in the
 *      opposite direction.
 *
 * \param terminate
 *      If true, tack on a final separator, if necessary. Defaults to false.
 *      Do not set to true if you know the path ends with a file-name.
 *
 * \return
 *      The possibly modified path is returned.  If path is not valid (e.g.
 *      the name of a console file handle), then an empty string is returned.
 */

std::string
normalize_path (const std::string & path, bool to_unix, bool terminate)
{
    std::string result;
    if (file_name_good(path))
    {
        result = path;

        auto circumpos = result.find_first_of("~");
        if (circumpos != std::string::npos)
        {
            result.replace(circumpos, 1, user_home());
        }
        if (to_unix)
        {
            auto pos = path.find_first_of("\\");
            if (pos != std::string::npos)
                std::replace(result.begin(), result.end(), '\\', '/');

            if (terminate && result[result.length()-1] != '/')
                result += "/";
        }
        else
        {
            auto pos = path.find_first_of("/");
            if (pos != std::string::npos)
                std::replace(result.begin(), result.end(), '/', '\\');

            if (terminate && result[result.length()-1] != '\\')
                result += "\\";
        }
    }
    return result;
}

/**
 *  Shortens a file-specification to make sure it is no longer than the
 *  provided length value.  This is done by removing character in the middle,
 *  if necessary, and replacing them with an ellipse.
 *
 *  This function operates by first trying to find the <code> /home </code>
 *  directory.  If found, it strips off <code> /home/username </code> and
 *  replace it with the Linux <code> ~ </code> replacement for the <code>
 *  $HOME </code> environment variable.  This function assumes that the
 *  "username" portion <i> must </i> exist, and that there's no goofy stuff
 *  like double-slashes in the path.
 *
 * \param fpath
 *      The file specification, including the full path to the file, and the
 *      name of the file.
 *
 * \param leng
 *      Provides the length to which to limit the string.
 *
 * \return
 *      Returns the fpath parameter, possibly shortened to fit within the
 *      desired length.
 */

std::string
shorten_file_spec (const std::string & fpath, int leng)
{
    std::string home = user_home();
    std::string newhome = "~";
    std::string newpath = fpath;
    if (contains(fpath, home))
        newpath = newpath.replace(0, home.length(), newhome);

    std::size_t pathsize = newpath.size();
    if (pathsize <= std::size_t(leng))
    {
        return newpath;
    }
    else
    {

        std::string ellipse("...");
        std::size_t halflength = (std::size_t(leng) - ellipse.size()) / 2 - 1;
        std::string result = newpath.substr(0, halflength);
        std::string lastpart = newpath.substr(pathsize-halflength-1);
        result = result + ellipse + lastpart;
        return result;
    }
}

/**
 *  Normalize as per the OS for which this module was built.
 *
 * \param path
 *      Provides the path, which should be a full path-name.
 *
 * \param terminate
 *      If true, tack on a final separator, if necessary. Defaults to false.
 *      Do not set to true if you know the path ends with a file-name.
 *
 * \return
 *      The possibly modified path is returned.
 */

std::string
os_normalize_path (const std::string & path, bool terminate)
{
#if defined SEQ66_PLATFORM_UNIX
    bool to_unix = true;
#else
    bool to_unix = false;
#endif

    return normalize_path(path, to_unix, terminate);
}

/**
 *  Makes sure the path is using the proper separators, and that a separator
 *  appears at the end.  The path is trimmed and normalized, but not
 *  terminated.  Compare to the clean_path() function.
 *
 * \param file
 *      The file-name to fix.  Assumed to be a file name.
 *
 * \param tounix
 *      Use UNIX conventions for the path separator/terminator.  Defaults to
 *      true.
 *
 * \return
 *      Returns the possibly-modified file-name.
 */

std::string
clean_file (const std::string & file, bool to_unix)
{
    std::string result = file;
    (void) trim(result, SEQ66_TRIM_CHARS_QUOTES);
    return normalize_path(result, to_unix, false);      /* no added slash   */
}

/**
 *  Makes sure the path is using the proper separators, and that a separator
 *  appears at the end.  The path is trimmed, normalized, and then properly
 *  terminated.  Compare to the clean_file() function.
 *
 * \param path
 *      The path-name to fix.  Assumed to be a directory name.
 *
 * \param tounix
 *      Use UNIX conventions for the path separator/terminator.  Defaults to
 *      true.
 *
 * \return
 *      Returns the possibly-modified path.
 */

std::string
clean_path (const std::string & path, bool to_unix)
{
    std::string result = path;
    (void) trim(result, SEQ66_TRIM_CHARS_QUOTES);
    return normalize_path(result, to_unix, true);       /* an added slash   */
}

std::string
append_file
(
    const std::string & path,
    const std::string & filename,
    bool to_unix
)
{
    std::string result = path;
    if (! result.empty())
    {
        (void) rtrim(result, SEQ66_TRIM_CHARS_PATHS);
        result += path_slash();
    }
    result += filename;
    return normalize_path(result, to_unix, false);
}

std::string
append_path
(
    const std::string & path,
    const std::string & pathname,
    bool to_unix
)
{
    std::string result = path;
    std::string pn = pathname;
    if (! result.empty())
    {
        (void) rtrim(result, SEQ66_TRIM_CHARS_PATHS);
        result += path_slash();
    }
    if (! pn.empty())
    {
        (void) trim(pn, SEQ66_TRIM_CHARS_PATHS);
        pn += path_slash();
    }
    result += pn;
    return normalize_path(result, to_unix, true);
}


/**
 *  Cleans up the path to make sure it is either valid or empty, and then
 *  appends the base file-name (hopefully in the format "base.extension") to
 *  it.
 *
 *  This function works solely using UNIX conventions, it is for internal use.
 *  If desired, it can be converted to Windows conventions using
 *  os_normalize_path().
 */

std::string
filename_concatenate (const std::string & path, const std::string & filebase)
{
    std::string result = clean_path(path);
    result += filebase;
    return result;
}

/**
 *  This function concatenates two paths robustly, if not efficiently.
 *
 * \param path0
 *      The main path, which can be a root path or a relative path. It is
 *      modified only to add a terminating slash, if necessary.
 *
 * \param path1
 *      The seconds path, which can only be a relative path (or a base
 *      filename such as "x.ext").  If it starts with a slash, that is
 *      removed.
 *
 * \return
 *      Returns the two paths:  path0 + "/" + path1 + "/", where the slashes
 *      are added only if not already present.
 */

std::string
pathname_concatenate (const std::string & path0, const std::string & path1)
{
    std::string result = clean_path(path0);
    std::string cleanpath1 = clean_path(path1);
    if (cleanpath1[0] == '/')
        cleanpath1 = cleanpath1.erase(0, 1);

    result += cleanpath1;
    return result;
}


/**
 *  This function is a kind of inverse of filename_concatenate().  Note that
 *  it does not split out the file-extension.  Also note that the results are
 *  in UNIX convention.  And there isn't any error checking for empty input or
 *  results.
 *
 * \param fullpath
 *      The putative full file-specification, including path and base-name.
 *
 * \param [out] path
 *      The destination for the path portion of the full path.
 *
 * \param [out] filebase
 *      The destination for the base-name (xxxx.yyy) portion of the full path.
 *
 * \return
 *      Returns true if there was a path in the filename, as indicated by the
 *      presence of a "/".  False is returned otherwise, and the \a fullpath
 *      argument is copied to the \a filebase destination parameter and the \a
 *      path argument is cleared.
 */

bool
filename_split
(
    const std::string & fullpath,
    std::string & path,
    std::string & filebase
)
{
    std::string temp = normalize_path(fullpath);
    auto spos = temp.find_last_of("/");
    bool result = spos != std::string::npos;
    if (result)
    {
        auto pos = spos + 1;
        path = temp.substr(0, pos);                     /* include slash    */
        filebase = temp.substr(pos, temp.length() - pos);
    }
    else
    {
        path.clear();
        filebase = fullpath;
    }
    return result;
}

std::string
file_path_set (const std::string & fullpath, const std::string & newpath)
{
    std::string path;
    std::string filebase;
    (void) filename_split(fullpath, path, filebase);
    return filename_concatenate(newpath, filebase);
}

/**
 *  Uses filename_split to extract only the base part of the file
 *  specification ("xxxx.yyy").
 *
 * \param fullpath
 *      The path name from which to extract.
 *
 * \param noext
 *      If set to true (the default is false), then the extenstion (".yyy") is
 *      also stripped.
 */

std::string
filename_base (const std::string & fullpath, bool noext)
{
    std::string result;
    std::string path;
    (void) filename_split(fullpath, path, result);
    if (noext)
    {
        auto dpos = result.find_last_of(".");
        bool ok = dpos != std::string::npos;
        if (ok)
            result = result.substr(0, dpos);
    }
    return result;
}

/**
 *  Gets a file extension, defined simply as the text after the last period
 *  in the path.
 *
 *  We could use libmagic to determine the actual file type, but that is
 *  probably overkill for our purposes.
 *
 * \param path
 *      Provides the file name or the full path to the file.
 *
 * \return
 *      Returns the file extension.  If there is no period, then an empty
 *      string is returned.
 */

std::string
file_extension (const std::string & path)
{
    std::string result;
    auto ppos = path.find_last_of(".");
    if (ppos != std::string::npos)
    {
        auto end_index = path.length() - 1;
        result = path.substr(ppos + 1, end_index - 1);
    }
    return result;
}

/**
 *  This function makes sure that the file-extension of the given path is set
 *  to the given extension parameter.
 *
 * \param path
 *      Provides the path-name, which can have an extension, or not. It can
 *      also be a simple base filename, with no path.
 *
 * \param ext
 *      Provides the desired extension.  It must include the period, as in
 *      ".ctrl". If this parameter is empty, and an extension exists, it is
 *      stripped off.  This is the default value.
 *
 * \return
 *      Returns a copy of the augmented (or extension-stripped) string.
 */

std::string
file_extension_set (const std::string & path, const std::string & ext)
{
    std::string result;
    auto ppos = path.find_last_of(".");
    if (ppos != std::string::npos)
    {
        result = path.substr(0, ppos);
        if (! ext.empty())
            result += ext;
    }
    else
    {
        result = path;
        result += ext;
    }
    return result;
}

/**
 *  Sees if file-extensions match, case-insensitively.
 *
 * \param path
 *      Provides the file name or the full path to the file.
 *
 * \param target
 *      Provides the file extension to match against, without the period.
 *
 * \return
 *      Returns true if the file extensions match.
 */

bool
file_extension_match (const std::string & path, const std::string & target)
{
    std::string ext = file_extension(path);
    return strcasecompare(ext, target);
}

/**
 *  Sets the current directory for the application.  A wrapper replacement for
 *  chdir() or _chdir().  It sets the current working directory in the
 *  application.  This function is necessary in order to make sure the current
 *  working directory is a safe place to work.
 *
 * \return
 *      Returns true if the path name is good, and the chdir() call succeeded.
 *      Otherwise, false is returned.
 *
 * \param path
 *      The full or relative name of the directory.
 */

bool
set_current_directory (const std::string & path)
{
    bool result = false;
    if (! path.empty())
    {
        int rcode = S_CHDIR(path.c_str());
        result = is_posix_success(rcode);
        if (! result)
        {
            errprintf("could not set current directory '%s'", path.c_str());
        }
    }
    return result;
}


/**
 *  An alternative on Linux to using either /proc/self/exe or argv[0] is using
 *  the information passed by the ELF interpreter, made available by glibc.
 *  The getauxval() function is a glibc extension; check so that it doesn't
 *  return NULL (indicating that the ELF interpreter hasn't provided the
 *  AT_EXECFN parameter). This is never actually a problem on Linux.
 */

std::string
executable_full_path ()
{
    std::string result;

#if defined SEQ66_PLATFORM_GLIBC        /* TO DO!!!!                        */
    const char * p = (const char *) getauxval(AT_EXECFN);
    if (not_nullptr(p))
    {
        result = p;
    }
#endif

    return result;
}

/**
 *  Gets the user's $HOME (Linux) or $LOCALAPPDAT (Windows) directory from the
 *  current environment.
 *
 * \return
 *      Returns the value of $HOME, such as "/home/ahlstromcj" or
 *      "C:\Users\ahlstromcj\AppData\local".  Notice the lack of a terminating
 *      path-slash.  If getenv() fails, an empty string is returned.
 */

std::string
user_home ()
{
    std::string result;
    char * env = std::getenv(SEQ66_ENV_HOME);
    if (not_nullptr(env))
    {
        result = std::string(env);
    }
    else
    {
        file_error("std::getenv() failed", SEQ66_ENV_HOME);
    }
    return result;
}

}           // namespace seq66

/*
 * filefunctions.cpp
 *
 * vim: sw=4 ts=4 wm=4 et ft=cpp
 */

