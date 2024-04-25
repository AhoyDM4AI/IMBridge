/**
 * Copyright (c) 2021 OceanBase
 * OceanBase CE is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan PubL v2.
 * You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 */

#include "os_errno.h"
#include <string.h>
#include <cstdio>
using namespace oceanbase::common;

static const char* OS_ERRNO_NAME[OS_MAX_ERROR_CODE];
static const char* OS_ERRNO_MSG[OS_MAX_ERROR_CODE];
static int OS_ERRNO[OS_MAX_ERROR_CODE];

static struct OSStrErrorInit {
  OSStrErrorInit()
  {
    memset(OS_ERRNO_NAME, 0, sizeof(OS_ERRNO_NAME));
    memset(OS_ERRNO_MSG, 0, sizeof(OS_ERRNO_MSG));
    memset(OS_ERRNO, 0, sizeof(OS_ERRNO));

    OS_ERRNO_NAME[-OS_ENOENT] = "OS_ENOENT";
    OS_ERRNO_MSG[-OS_ENOENT] = "No such file or directory";
    OS_ERRNO[-OS_ENOENT] = 2;
    OS_ERRNO_NAME[-OS_ESRCH] = "OS_ESRCH";
    OS_ERRNO_MSG[-OS_ESRCH] = "No such process";
    OS_ERRNO[-OS_ESRCH] = 3;
    OS_ERRNO_NAME[-OS_EINTR] = "OS_EINTR";
    OS_ERRNO_MSG[-OS_EINTR] = "Interrupted system call";
    OS_ERRNO[-OS_EINTR] = 4;
    OS_ERRNO_NAME[-OS_EIO] = "OS_EIO";
    OS_ERRNO_MSG[-OS_EIO] = "I/O error";
    OS_ERRNO[-OS_EIO] = 5;
    OS_ERRNO_NAME[-OS_ENXIO] = "OS_ENXIO";
    OS_ERRNO_MSG[-OS_ENXIO] = "No such device or address";
    OS_ERRNO[-OS_ENXIO] = 6;
    OS_ERRNO_NAME[-OS_E2BIG] = "OS_E2BIG";
    OS_ERRNO_MSG[-OS_E2BIG] = "Argument list too long";
    OS_ERRNO[-OS_E2BIG] = 7;
    OS_ERRNO_NAME[-OS_ENOEXEC] = "OS_ENOEXEC";
    OS_ERRNO_MSG[-OS_ENOEXEC] = "Exec format error";
    OS_ERRNO[-OS_ENOEXEC] = 8;
    OS_ERRNO_NAME[-OS_EBADF] = "OS_EBADF";
    OS_ERRNO_MSG[-OS_EBADF] = "Bad file number";
    OS_ERRNO[-OS_EBADF] = 9;
    OS_ERRNO_NAME[-OS_ECHILD] = "OS_ECHILD";
    OS_ERRNO_MSG[-OS_ECHILD] = "No child processes";
    OS_ERRNO[-OS_ECHILD] = 10;
    OS_ERRNO_NAME[-OS_EAGAIN] = "OS_EAGAIN";
    OS_ERRNO_MSG[-OS_EAGAIN] = "Try again";
    OS_ERRNO[-OS_EAGAIN] = 11;
    OS_ERRNO_NAME[-OS_ENOMEM] = "OS_ENOMEM";
    OS_ERRNO_MSG[-OS_ENOMEM] = "Out of memory";
    OS_ERRNO[-OS_ENOMEM] = 12;
    OS_ERRNO_NAME[-OS_EACCES] = "OS_EACCES";
    OS_ERRNO_MSG[-OS_EACCES] = "Permission denied";
    OS_ERRNO[-OS_EACCES] = 13;
    OS_ERRNO_NAME[-OS_EFAULT] = "OS_EFAULT";
    OS_ERRNO_MSG[-OS_EFAULT] = "Bad address";
    OS_ERRNO[-OS_EFAULT] = 14;
    OS_ERRNO_NAME[-OS_ENOTBLK] = "OS_ENOTBLK";
    OS_ERRNO_MSG[-OS_ENOTBLK] = "Block device required";
    OS_ERRNO[-OS_ENOTBLK] = 15;
    OS_ERRNO_NAME[-OS_EBUSY] = "OS_EBUSY";
    OS_ERRNO_MSG[-OS_EBUSY] = "Device or resource busy";
    OS_ERRNO[-OS_EBUSY] = 16;
    OS_ERRNO_NAME[-OS_EEXIST] = "OS_EEXIST";
    OS_ERRNO_MSG[-OS_EEXIST] = "File exists";
    OS_ERRNO[-OS_EEXIST] = 17;
    OS_ERRNO_NAME[-OS_EXDEV] = "OS_EXDEV";
    OS_ERRNO_MSG[-OS_EXDEV] = "Cross-device link";
    OS_ERRNO[-OS_EXDEV] = 18;
    OS_ERRNO_NAME[-OS_ENODEV] = "OS_ENODEV";
    OS_ERRNO_MSG[-OS_ENODEV] = "No such device";
    OS_ERRNO[-OS_ENODEV] = 19;
    OS_ERRNO_NAME[-OS_ENOTDIR] = "OS_ENOTDIR";
    OS_ERRNO_MSG[-OS_ENOTDIR] = "Not a directory";
    OS_ERRNO[-OS_ENOTDIR] = 20;
    OS_ERRNO_NAME[-OS_EISDIR] = "OS_EISDIR";
    OS_ERRNO_MSG[-OS_EISDIR] = "Is a directory";
    OS_ERRNO[-OS_EISDIR] = 21;
    OS_ERRNO_NAME[-OS_EINVAL] = "OS_EINVAL";
    OS_ERRNO_MSG[-OS_EINVAL] = "Invalid argument";
    OS_ERRNO[-OS_EINVAL] = 22;
    OS_ERRNO_NAME[-OS_ENFILE] = "OS_ENFILE";
    OS_ERRNO_MSG[-OS_ENFILE] = "File table overflow";
    OS_ERRNO[-OS_ENFILE] = 23;
    OS_ERRNO_NAME[-OS_EMFILE] = "OS_EMFILE";
    OS_ERRNO_MSG[-OS_EMFILE] = "Too many open files";
    OS_ERRNO[-OS_EMFILE] = 24;
    OS_ERRNO_NAME[-OS_ENOTTY] = "OS_ENOTTY";
    OS_ERRNO_MSG[-OS_ENOTTY] = "Not a typewriter";
    OS_ERRNO[-OS_ENOTTY] = 25;
    OS_ERRNO_NAME[-OS_ETXTBSY] = "OS_ETXTBSY";
    OS_ERRNO_MSG[-OS_ETXTBSY] = "Text file busy";
    OS_ERRNO[-OS_ETXTBSY] = 26;
    OS_ERRNO_NAME[-OS_EFBIG] = "OS_EFBIG";
    OS_ERRNO_MSG[-OS_EFBIG] = "File too large";
    OS_ERRNO[-OS_EFBIG] = 27;
    OS_ERRNO_NAME[-OS_ENOSPC] = "OS_ENOSPC";
    OS_ERRNO_MSG[-OS_ENOSPC] = "No space left on device";
    OS_ERRNO[-OS_ENOSPC] = 28;
    OS_ERRNO_NAME[-OS_ESPIPE] = "OS_ESPIPE";
    OS_ERRNO_MSG[-OS_ESPIPE] = "Illegal seek";
    OS_ERRNO[-OS_ESPIPE] = 29;
    OS_ERRNO_NAME[-OS_EROFS] = "OS_EROFS";
    OS_ERRNO_MSG[-OS_EROFS] = "Read-only file system";
    OS_ERRNO[-OS_EROFS] = 30;
    OS_ERRNO_NAME[-OS_EMLINK] = "OS_EMLINK";
    OS_ERRNO_MSG[-OS_EMLINK] = "Too many links";
    OS_ERRNO[-OS_EMLINK] = 31;
    OS_ERRNO_NAME[-OS_EPIPE] = "OS_EPIPE";
    OS_ERRNO_MSG[-OS_EPIPE] = "Broken pipe";
    OS_ERRNO[-OS_EPIPE] = 32;
    OS_ERRNO_NAME[-OS_EDOM] = "OS_EDOM";
    OS_ERRNO_MSG[-OS_EDOM] = "Math argument out of domain of func";
    OS_ERRNO[-OS_EDOM] = 33;
    OS_ERRNO_NAME[-OS_ERANGE] = "OS_ERANGE";
    OS_ERRNO_MSG[-OS_ERANGE] = "Math result not representable";
    OS_ERRNO[-OS_ERANGE] = 34;
    OS_ERRNO_NAME[-OS_EDEADLK] = "OS_EDEADLK";
    OS_ERRNO_MSG[-OS_EDEADLK] = "Resource deadlock would occur";
    OS_ERRNO[-OS_EDEADLK] = 35;
    OS_ERRNO_NAME[-OS_ENAMETOOLONG] = "OS_ENAMETOOLONG";
    OS_ERRNO_MSG[-OS_ENAMETOOLONG] = "File name too long";
    OS_ERRNO[-OS_ENAMETOOLONG] = 36;
    OS_ERRNO_NAME[-OS_ENOLCK] = "OS_ENOLCK";
    OS_ERRNO_MSG[-OS_ENOLCK] = "No record locks available";
    OS_ERRNO[-OS_ENOLCK] = 37;
    OS_ERRNO_NAME[-OS_ENOSYS] = "OS_ENOSYS";
    OS_ERRNO_MSG[-OS_ENOSYS] = "Function not implemented";
    OS_ERRNO[-OS_ENOSYS] = 38;
    OS_ERRNO_NAME[-OS_ENOTEMPTY] = "OS_ENOTEMPTY";
    OS_ERRNO_MSG[-OS_ENOTEMPTY] = "Directory not empty";
    OS_ERRNO[-OS_ENOTEMPTY] = 39;
    OS_ERRNO_NAME[-OS_ELOOP] = "OS_ELOOP";
    OS_ERRNO_MSG[-OS_ELOOP] = "Too many symbolic links encountered";
    OS_ERRNO[-OS_ELOOP] = 40;
    OS_ERRNO_NAME[-OS_EWOULDBLOCK] = "OS_EWOULDBLOCK";
    OS_ERRNO_MSG[-OS_EWOULDBLOCK] = "Operation would block";
    OS_ERRNO[-OS_EWOULDBLOCK] = 11;
    OS_ERRNO_NAME[-OS_ENOMSG] = "OS_ENOMSG";
    OS_ERRNO_MSG[-OS_ENOMSG] = "No message of desired type";
    OS_ERRNO[-OS_ENOMSG] = 42;
    OS_ERRNO_NAME[-OS_EIDRM] = "OS_EIDRM";
    OS_ERRNO_MSG[-OS_EIDRM] = "Identifier removed";
    OS_ERRNO[-OS_EIDRM] = 43;
    OS_ERRNO_NAME[-OS_ECHRNG] = "OS_ECHRNG";
    OS_ERRNO_MSG[-OS_ECHRNG] = "Channel number out of range";
    OS_ERRNO[-OS_ECHRNG] = 44;
    OS_ERRNO_NAME[-OS_EL2NSYNC] = "OS_EL2NSYNC";
    OS_ERRNO_MSG[-OS_EL2NSYNC] = "Level 2 not synchronized";
    OS_ERRNO[-OS_EL2NSYNC] = 45;
    OS_ERRNO_NAME[-OS_EL3HLT] = "OS_EL3HLT";
    OS_ERRNO_MSG[-OS_EL3HLT] = "Level 3 halted";
    OS_ERRNO[-OS_EL3HLT] = 46;
    OS_ERRNO_NAME[-OS_EL3RST] = "OS_EL3RST";
    OS_ERRNO_MSG[-OS_EL3RST] = "Level 3 reset";
    OS_ERRNO[-OS_EL3RST] = 47;
    OS_ERRNO_NAME[-OS_ELNRNG] = "OS_ELNRNG";
    OS_ERRNO_MSG[-OS_ELNRNG] = "Link number out of range";
    OS_ERRNO[-OS_ELNRNG] = 48;
    OS_ERRNO_NAME[-OS_EUNATCH] = "OS_EUNATCH";
    OS_ERRNO_MSG[-OS_EUNATCH] = "Protocol driver not attached";
    OS_ERRNO[-OS_EUNATCH] = 49;
    OS_ERRNO_NAME[-OS_ENOCSI] = "OS_ENOCSI";
    OS_ERRNO_MSG[-OS_ENOCSI] = "No CSI structure available";
    OS_ERRNO[-OS_ENOCSI] = 50;
    OS_ERRNO_NAME[-OS_EL2HLT] = "OS_EL2HLT";
    OS_ERRNO_MSG[-OS_EL2HLT] = "Level 2 halted";
    OS_ERRNO[-OS_EL2HLT] = 51;
    OS_ERRNO_NAME[-OS_EBADE] = "OS_EBADE";
    OS_ERRNO_MSG[-OS_EBADE] = "Invalid exchange";
    OS_ERRNO[-OS_EBADE] = 52;
    OS_ERRNO_NAME[-OS_EBADR] = "OS_EBADR";
    OS_ERRNO_MSG[-OS_EBADR] = "Invalid request descriptor";
    OS_ERRNO[-OS_EBADR] = 53;
    OS_ERRNO_NAME[-OS_EXFULL] = "OS_EXFULL";
    OS_ERRNO_MSG[-OS_EXFULL] = "Exchange full";
    OS_ERRNO[-OS_EXFULL] = 54;
    OS_ERRNO_NAME[-OS_ENOANO] = "OS_ENOANO";
    OS_ERRNO_MSG[-OS_ENOANO] = "No anode";
    OS_ERRNO[-OS_ENOANO] = 55;
    OS_ERRNO_NAME[-OS_EBADRQC] = "OS_EBADRQC";
    OS_ERRNO_MSG[-OS_EBADRQC] = "Invalid request code";
    OS_ERRNO[-OS_EBADRQC] = 56;
    OS_ERRNO_NAME[-OS_EBADSLT] = "OS_EBADSLT";
    OS_ERRNO_MSG[-OS_EBADSLT] = "Invalid slot";
    OS_ERRNO[-OS_EBADSLT] = 57;
    OS_ERRNO_NAME[-OS_EBFONT] = "OS_EBFONT";
    OS_ERRNO_MSG[-OS_EBFONT] = "Bad font file format";
    OS_ERRNO[-OS_EBFONT] = 59;
    OS_ERRNO_NAME[-OS_ENOSTR] = "OS_ENOSTR";
    OS_ERRNO_MSG[-OS_ENOSTR] = "Device not a stream";
    OS_ERRNO[-OS_ENOSTR] = 60;
    OS_ERRNO_NAME[-OS_ENODATA] = "OS_ENODATA";
    OS_ERRNO_MSG[-OS_ENODATA] = "No data available";
    OS_ERRNO[-OS_ENODATA] = 61;
    OS_ERRNO_NAME[-OS_ETIME] = "OS_ETIME";
    OS_ERRNO_MSG[-OS_ETIME] = "Timer expired";
    OS_ERRNO[-OS_ETIME] = 62;
    OS_ERRNO_NAME[-OS_ENOSR] = "OS_ENOSR";
    OS_ERRNO_MSG[-OS_ENOSR] = "OOut of streams resources";
    OS_ERRNO[-OS_ENOSR] = 63;
    OS_ERRNO_NAME[-OS_ENONET] = "OS_ENONET";
    OS_ERRNO_MSG[-OS_ENONET] = "Machine is not on the network";
    OS_ERRNO[-OS_ENONET] = 64;
    OS_ERRNO_NAME[-OS_ENOPKG] = "OS_ENOPKG";
    OS_ERRNO_MSG[-OS_ENOPKG] = "Package not installed";
    OS_ERRNO[-OS_ENOPKG] = 65;
    OS_ERRNO_NAME[-OS_EREMOTE] = "OS_EREMOTE";
    OS_ERRNO_MSG[-OS_EREMOTE] = "Object is remote";
    OS_ERRNO[-OS_EREMOTE] = 66;
    OS_ERRNO_NAME[-OS_ENOLINK] = "OS_ENOLINK";
    OS_ERRNO_MSG[-OS_ENOLINK] = "Link has been severed";
    OS_ERRNO[-OS_ENOLINK] = 67;
    OS_ERRNO_NAME[-OS_EADV] = "OS_EADV";
    OS_ERRNO_MSG[-OS_EADV] = "Advertise error";
    OS_ERRNO[-OS_EADV] = 68;
    OS_ERRNO_NAME[-OS_ESRMNT] = "OS_ESRMNT";
    OS_ERRNO_MSG[-OS_ESRMNT] = "Srmount error";
    OS_ERRNO[-OS_ESRMNT] = 69;
    OS_ERRNO_NAME[-OS_ECOMM] = "OS_ECOMM";
    OS_ERRNO_MSG[-OS_ECOMM] = "Communication error on send";
    OS_ERRNO[-OS_ECOMM] = 70;
    OS_ERRNO_NAME[-OS_EPROTO] = "OS_EPROTO";
    OS_ERRNO_MSG[-OS_EPROTO] = "Protocol error";
    OS_ERRNO[-OS_EPROTO] = 71;
    OS_ERRNO_NAME[-OS_EMULTIHOP] = "OS_EMULTIHOP";
    OS_ERRNO_MSG[-OS_EMULTIHOP] = "Multihop attempted";
    OS_ERRNO[-OS_EMULTIHOP] = 72;
    OS_ERRNO_NAME[-OS_EDOTDOT] = "OS_EDOTDOT";
    OS_ERRNO_MSG[-OS_EDOTDOT] = "RFS specific error";
    OS_ERRNO[-OS_EDOTDOT] = 73;
    OS_ERRNO_NAME[-OS_EBADMSG] = "OS_EBADMSG";
    OS_ERRNO_MSG[-OS_EBADMSG] = "Not a data message";
    OS_ERRNO[-OS_EBADMSG] = 74;
    OS_ERRNO_NAME[-OS_EOVERFLOW] = "OS_EOVERFLOW";
    OS_ERRNO_MSG[-OS_EOVERFLOW] = "Value too large for defined data type";
    OS_ERRNO[-OS_EOVERFLOW] = 75;
    OS_ERRNO_NAME[-OS_ENOTUNIQ] = "OS_ENOTUNIQ";
    OS_ERRNO_MSG[-OS_ENOTUNIQ] = "Name not unique on network";
    OS_ERRNO[-OS_ENOTUNIQ] = 76;
    OS_ERRNO_NAME[-OS_EBADFD] = "OS_EBADFD";
    OS_ERRNO_MSG[-OS_EBADFD] = "File descriptor in bad state";
    OS_ERRNO[-OS_EBADFD] = 77;
    OS_ERRNO_NAME[-OS_EREMCHG] = "OS_EREMCHG";
    OS_ERRNO_MSG[-OS_EREMCHG] = "Remote address changed";
    OS_ERRNO[-OS_EREMCHG] = 78;
    OS_ERRNO_NAME[-OS_ELIBACC] = "OS_ELIBACC";
    OS_ERRNO_MSG[-OS_ELIBACC] = "Can not access a needed shared library";
    OS_ERRNO[-OS_ELIBACC] = 79;
    OS_ERRNO_NAME[-OS_ELIBBAD] = "OS_ELIBBAD";
    OS_ERRNO_MSG[-OS_ELIBBAD] = "Accessing a corrupted shared library";
    OS_ERRNO[-OS_ELIBBAD] = 80;
    OS_ERRNO_NAME[-OS_ELIBSCN] = "OS_ELIBSCN";
    OS_ERRNO_MSG[-OS_ELIBSCN] = ".lib section in a.out corrupted";
    OS_ERRNO[-OS_ELIBSCN] = 81;
    OS_ERRNO_NAME[-OS_ELIBMAX] = "OS_ELIBMAX";
    OS_ERRNO_MSG[-OS_ELIBMAX] = "Attempting to link in too many shared libraries";
    OS_ERRNO[-OS_ELIBMAX] = 82;
    OS_ERRNO_NAME[-OS_ELIBEXEC] = "OS_ELIBEXEC";
    OS_ERRNO_MSG[-OS_ELIBEXEC] = "Cannot exec a shared library directly";
    OS_ERRNO[-OS_ELIBEXEC] = 83;
    OS_ERRNO_NAME[-OS_EILSEQ] = "OS_EILSEQ";
    OS_ERRNO_MSG[-OS_EILSEQ] = "Illegal byte sequence";
    OS_ERRNO[-OS_EILSEQ] = 84;
    OS_ERRNO_NAME[-OS_ERESTART] = "OS_ERESTART";
    OS_ERRNO_MSG[-OS_ERESTART] = "Interrupted system call should be restarted";
    OS_ERRNO[-OS_ERESTART] = 85;
    OS_ERRNO_NAME[-OS_ESTRPIPE] = "OS_ESTRPIPE";
    OS_ERRNO_MSG[-OS_ESTRPIPE] = "Streams pipe error";
    OS_ERRNO[-OS_ESTRPIPE] = 86;
    OS_ERRNO_NAME[-OS_EUSERS] = "OS_EUSERS";
    OS_ERRNO_MSG[-OS_EUSERS] = "Too many users";
    OS_ERRNO[-OS_EUSERS] = 87;
    OS_ERRNO_NAME[-OS_ENOTSOCK] = "OS_ENOTSOCK";
    OS_ERRNO_MSG[-OS_ENOTSOCK] = "Socket operation on non-socket";
    OS_ERRNO[-OS_ENOTSOCK] = 88;
    OS_ERRNO_NAME[-OS_EDESTADDRREQ] = "OS_EDESTADDRREQ";
    OS_ERRNO_MSG[-OS_EDESTADDRREQ] = "Destination address required";
    OS_ERRNO[-OS_EDESTADDRREQ] = 89;
    OS_ERRNO_NAME[-OS_EMSGSIZE] = "OS_EMSGSIZE";
    OS_ERRNO_MSG[-OS_EMSGSIZE] = "Message too long";
    OS_ERRNO[-OS_EMSGSIZE] = 90;
    OS_ERRNO_NAME[-OS_EPROTOTYPE] = "OS_EPROTOTYPE";
    OS_ERRNO_MSG[-OS_EPROTOTYPE] = "Protocol wrong type for socket";
    OS_ERRNO[-OS_EPROTOTYPE] = 91;
    OS_ERRNO_NAME[-OS_ENOPROTOOPT] = "OS_ENOPROTOOPT";
    OS_ERRNO_MSG[-OS_ENOPROTOOPT] = "Protocol not available";
    OS_ERRNO[-OS_ENOPROTOOPT] = 92;
    OS_ERRNO_NAME[-OS_EPROTONOSUPPORT] = "OS_EPROTONOSUPPORT";
    OS_ERRNO_MSG[-OS_EPROTONOSUPPORT] = "Protocol not supported";
    OS_ERRNO[-OS_EPROTONOSUPPORT] = 93;
    OS_ERRNO_NAME[-OS_ESOCKTNOSUPPORT] = "OS_ESOCKTNOSUPPORT";
    OS_ERRNO_MSG[-OS_ESOCKTNOSUPPORT] = "Socket type not supported";
    OS_ERRNO[-OS_ESOCKTNOSUPPORT] = 94;
    OS_ERRNO_NAME[-OS_EOPNOTSUPP] = "OS_EOPNOTSUPP";
    OS_ERRNO_MSG[-OS_EOPNOTSUPP] = "Operation not supported on transport endpoint";
    OS_ERRNO[-OS_EOPNOTSUPP] = 95;
    OS_ERRNO_NAME[-OS_EPFNOSUPPORT] = "OS_EPFNOSUPPORT";
    OS_ERRNO_MSG[-OS_EPFNOSUPPORT] = "Protocol family not supported";
    OS_ERRNO[-OS_EPFNOSUPPORT] = 96;
    OS_ERRNO_NAME[-OS_EAFNOSUPPORT] = "OS_EAFNOSUPPORT";
    OS_ERRNO_MSG[-OS_EAFNOSUPPORT] = "Address family not supported by protocol";
    OS_ERRNO[-OS_EAFNOSUPPORT] = 97;
    OS_ERRNO_NAME[-OS_EADDRINUSE] = "OS_EADDRINUSE";
    OS_ERRNO_MSG[-OS_EADDRINUSE] = "Address already in use";
    OS_ERRNO[-OS_EADDRINUSE] = 98;
    OS_ERRNO_NAME[-OS_EADDRNOTAVAIL] = "OS_EADDRNOTAVAIL";
    OS_ERRNO_MSG[-OS_EADDRNOTAVAIL] = "Cannot assign requested address";
    OS_ERRNO[-OS_EADDRNOTAVAIL] = 99;
    OS_ERRNO_NAME[-OS_ENETDOWN] = "OS_ENETDOWN";
    OS_ERRNO_MSG[-OS_ENETDOWN] = "Network is down";
    OS_ERRNO[-OS_ENETDOWN] = 100;
    OS_ERRNO_NAME[-OS_ENETUNREACH] = "OS_ENETUNREACH";
    OS_ERRNO_MSG[-OS_ENETUNREACH] = "Network is unreachable";
    OS_ERRNO[-OS_ENETUNREACH] = 101;
    OS_ERRNO_NAME[-OS_ENETRESET] = "OS_ENETRESET";
    OS_ERRNO_MSG[-OS_ENETRESET] = "Network dropped connection because of reset";
    OS_ERRNO[-OS_ENETRESET] = 102;
    OS_ERRNO_NAME[-OS_ECONNABORTED] = "OS_ECONNABORTED";
    OS_ERRNO_MSG[-OS_ECONNABORTED] = "Software caused connection abort";
    OS_ERRNO[-OS_ECONNABORTED] = 103;
    OS_ERRNO_NAME[-OS_ECONNRESET] = "OS_ECONNRESET";
    OS_ERRNO_MSG[-OS_ECONNRESET] = "Connection reset by peer";
    OS_ERRNO[-OS_ECONNRESET] = 104;
    OS_ERRNO_NAME[-OS_ENOBUFS] = "OS_ENOBUFS";
    OS_ERRNO_MSG[-OS_ENOBUFS] = "No buffer space available";
    OS_ERRNO[-OS_ENOBUFS] = 105;
    OS_ERRNO_NAME[-OS_EISCONN] = "OS_EISCONN";
    OS_ERRNO_MSG[-OS_EISCONN] = "Transport endpoint is already connected";
    OS_ERRNO[-OS_EISCONN] = 106;
    OS_ERRNO_NAME[-OS_ENOTCONN] = "OS_ENOTCONN";
    OS_ERRNO_MSG[-OS_ENOTCONN] = "Transport endpoint is not connected";
    OS_ERRNO[-OS_ENOTCONN] = 107;
    OS_ERRNO_NAME[-OS_ESHUTDOWN] = "OS_ESHUTDOWN";
    OS_ERRNO_MSG[-OS_ESHUTDOWN] = "Cannot send after transport endpoint shutdown";
    OS_ERRNO[-OS_ESHUTDOWN] = 108;
    OS_ERRNO_NAME[-OS_ETOOMANYREFS] = "OS_ETOOMANYREFS";
    OS_ERRNO_MSG[-OS_ETOOMANYREFS] = "Too many references: cannot splice";
    OS_ERRNO[-OS_ETOOMANYREFS] = 109;
    OS_ERRNO_NAME[-OS_ETIMEDOUT] = "OS_ETIMEDOUT";
    OS_ERRNO_MSG[-OS_ETIMEDOUT] = "Connection timed out";
    OS_ERRNO[-OS_ETIMEDOUT] = 110;
    OS_ERRNO_NAME[-OS_ECONNREFUSED] = "OS_ECONNREFUSED";
    OS_ERRNO_MSG[-OS_ECONNREFUSED] = "Connection refused";
    OS_ERRNO[-OS_ECONNREFUSED] = 111;
    OS_ERRNO_NAME[-OS_EHOSTDOWN] = "OS_EHOSTDOWN";
    OS_ERRNO_MSG[-OS_EHOSTDOWN] = "Host is down";
    OS_ERRNO[-OS_EHOSTDOWN] = 112;
    OS_ERRNO_NAME[-OS_EHOSTUNREACH] = "OS_EHOSTUNREACH";
    OS_ERRNO_MSG[-OS_EHOSTUNREACH] = "No route to host";
    OS_ERRNO[-OS_EHOSTUNREACH] = 113;
    OS_ERRNO_NAME[-OS_EALREADY] = "OS_EALREADY";
    OS_ERRNO_MSG[-OS_EALREADY] = "Operation already in progress";
    OS_ERRNO[-OS_EALREADY] = 114;
    OS_ERRNO_NAME[-OS_EINPROGRESS] = "OS_EINPROGRESS";
    OS_ERRNO_MSG[-OS_EINPROGRESS] = "Operation now in progress";
    OS_ERRNO[-OS_EINPROGRESS] = 115;
    OS_ERRNO_NAME[-OS_ESTALE] = "OS_ESTALE";
    OS_ERRNO_MSG[-OS_ESTALE] = "Stale file handle";
    OS_ERRNO[-OS_ESTALE] = 116;
    OS_ERRNO_NAME[-OS_EUCLEAN] = "OS_EUCLEAN";
    OS_ERRNO_MSG[-OS_EUCLEAN] = "Structure needs cleaning";
    OS_ERRNO[-OS_EUCLEAN] = 117;
    OS_ERRNO_NAME[-OS_ENOTNAM] = "OS_ENOTNAM";
    OS_ERRNO_MSG[-OS_ENOTNAM] = "Not a XENIX named type file";
    OS_ERRNO[-OS_ENOTNAM] = 118;
    OS_ERRNO_NAME[-OS_ENAVAIL] = "OS_ENAVAIL";
    OS_ERRNO_MSG[-OS_ENAVAIL] = "No XENIX semaphores available";
    OS_ERRNO[-OS_ENAVAIL] = 119;
    OS_ERRNO_NAME[-OS_EISNAM] = "OS_EISNAM";
    OS_ERRNO_MSG[-OS_EISNAM] = "Is a named type file";
    OS_ERRNO[-OS_EISNAM] = 120;
    OS_ERRNO_NAME[-OS_EREMOTEIO] = "OS_EREMOTEIO";
    OS_ERRNO_MSG[-OS_EREMOTEIO] = "Remote I/O error";
    OS_ERRNO[-OS_EREMOTEIO] = 121;
    OS_ERRNO_NAME[-OS_EDQUOT] = "OS_EDQUOT";
    OS_ERRNO_MSG[-OS_EDQUOT] = "Quota exceeded";
    OS_ERRNO[-OS_EDQUOT] = 122;
    OS_ERRNO_NAME[-OS_ENOMEDIUM] = "OS_ENOMEDIUM";
    OS_ERRNO_MSG[-OS_ENOMEDIUM] = "No medium found";
    OS_ERRNO[-OS_ENOMEDIUM] = 123;
    OS_ERRNO_NAME[-OS_EMEDIUMTYPE] = "OS_EMEDIUMTYPE";
    OS_ERRNO_MSG[-OS_EMEDIUMTYPE] = "Wrong medium type";
    OS_ERRNO[-OS_EMEDIUMTYPE] = 124;
    OS_ERRNO_NAME[-OS_ECANCELED] = "OS_ECANCELED";
    OS_ERRNO_MSG[-OS_ECANCELED] = "Operation Canceled";
    OS_ERRNO[-OS_ECANCELED] = 125;
    OS_ERRNO_NAME[-OS_ENOKEY] = "OS_ENOKEY";
    OS_ERRNO_MSG[-OS_ENOKEY] = "Required key not available";
    OS_ERRNO[-OS_ENOKEY] = 126;
    OS_ERRNO_NAME[-OS_EKEYEXPIRED] = "OS_EKEYEXPIRED";
    OS_ERRNO_MSG[-OS_EKEYEXPIRED] = "Key has expired";
    OS_ERRNO[-OS_EKEYEXPIRED] = 127;
    OS_ERRNO_NAME[-OS_EKEYREVOKED] = "OS_EKEYREVOKED";
    OS_ERRNO_MSG[-OS_EKEYREVOKED] = "Key has been revoked";
    OS_ERRNO[-OS_EKEYREVOKED] = 128;
    OS_ERRNO_NAME[-OS_EKEYREJECTED] = "OS_EKEYREJECTED";
    OS_ERRNO_MSG[-OS_EKEYREJECTED] = "Key was rejected by service";
    OS_ERRNO[-OS_EKEYREJECTED] = 129;
    OS_ERRNO_NAME[-OS_EOWNERDEAD] = "OS_EOWNERDEAD";
    OS_ERRNO_MSG[-OS_EOWNERDEAD] = "Owner died";
    OS_ERRNO[-OS_EOWNERDEAD] = 130;
    OS_ERRNO_NAME[-OS_ENOTRECOVERABLE] = "OS_ENOTRECOVERABLE";
    OS_ERRNO_MSG[-OS_ENOTRECOVERABLE] = "State not recoverable";
    OS_ERRNO[-OS_ENOTRECOVERABLE] = 131;
    OS_ERRNO_NAME[-OS_ERFKILL] = "OS_ERFKILL";
    OS_ERRNO_MSG[-OS_ERFKILL] = "Operation not possible due to RF-kill";
    OS_ERRNO[-OS_ERFKILL] = 132;
    OS_ERRNO_NAME[-OS_EHWPOISON] = "OS_EHWPOISON";
    OS_ERRNO_MSG[-OS_EHWPOISON] = "Memory page has hardware error";
    OS_ERRNO[-OS_EHWPOISON] = 133;
  }
} local_init;

namespace oceanbase {
namespace common {
const char* str_os_error_name(const int err)
{
  const char* ret = "Unknown error";
  if (0 == err) {
    ret = "OB_SUCCESS";
  } else if (0 > err && err > -OS_MAX_ERROR_CODE) {
    ret = OS_ERRNO_NAME[-err];
    if (NULL == ret || '\0' == ret[0]) {
      ret = "Unknown Error";
    }
  }
  return ret;
}
const char* str_os_error_msg(const int err)
{
  const char* ret = NULL;
  if (0 == err) {
    ret = NULL;
  } else if (0 > err && err > -OS_MAX_ERROR_CODE) {
    ret = OS_ERRNO_MSG[-err];
    if (NULL == ret || '\0' == ret[0]) {
      ret = NULL;
    }
  }
  return ret;
}
int os_errno(const int err)
{
  int ret = -1;
  if (0 >= err && err > -OS_MAX_ERROR_CODE) {
    ret = OS_ERRNO[-err];
  }
  return ret;
}
}  // end namespace common
}  // end namespace oceanbase
