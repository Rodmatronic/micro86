#include "../include/errno.h"
#include "../include/stdio.h"

void
verrc(int eval, int code, const char *fmt, va_list ap)
{
	(void)fprintf(stderr, "%s: ", program_name);
	if (fmt != NULL) {
		(void)vprintf(stderr, fmt, ap);
		(void)fprintf(stderr, ": ");
	}
	(void)fprintf(stderr, "%s\n", strerror(code));
	exit(eval);
}

void
errc(int eval, int code, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	verrc(eval, code, fmt, ap);
	va_end(ap);
}

void clearerr(FILE *stream) {
    (void)stream;
}

void warn(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    fprintf(stderr, "%s: ", program_name);
    vprintf(stderr, fmt, args);
    fprintf(stderr, ": %s\n", strerror(errno));

    va_end(args);
}

int ferror(FILE *stream) {
    return 0;
}

void err(int eval, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    fprintf(stderr, "error: ");
    vprintf(stderr, fmt, args);

    if (errno)
        fprintf(stderr, ": %s", strerror(errno));

    fprintf(stderr, "\n");
    va_end(args);
    exit(eval);
}

char*
strerror(int errno)
{
	switch (errno) {
	case 0:
		return "No such file or directory";
	case 1:
		return "Operation not permitted";
	case 2:
		return "No such file or directory";
	case 3:
		return "No such process";
	case 4:
		return "Interrupted system call";
	case 5:
		return "Input/output error";
	case 6:
		return "Device not configured";
	case 7:
		return "Argument list too long";
	case 8:
		return "Exec format error";
	case 9:
		return "Bad file descriptor";
	case 10:
		return "No child processes";
	case 11:
		return "Resource deadlock avoided";
	case 12:
		return "Cannot allocate memory";
	case 13:
		return "Permission denied";
	case 14:
		return "Bad address";
	case 15:
		return "Block device required";
	case 16:
		return "Device busy";
	case 17:
		return "File exists";
	case 18:
		return "Cross-device link";
	case 19:
		return "Operation not supported by device";
	case 20:
		return "Not a directory";
	case 21:
		return "Is a directory";
	case 22:
		return "Invalid argument";
	case 23:
		return "Too many open files in system";
	case 24:
		return "Too many open files";
	case 25:
		return "Inappropriate ioctl for device";
	case 26:
		return "Text file busy";
	case 27:
		return "File too large";
	case 28:
		return "No space left on device";
	case 29:
		return "Illegal seek";
	case 30:
		return "Read-only file system";
	case 31:
		return "Too many links";
	case 32:
		return "Broken pipe";
	case 33:
		return "Numerical argument out of domain";
	case 34:
		return "Result too large";
	case 35:
		return "Resource temporarily unavailable";
	case 36:
		return "Operation now in progress";
	case 37:
		return "Operation already in progress";
	case 38:
		return "Socket operation on non-socket";
	case 39:
		return "Destination address required";
	case 40:
		return "Message too long";
	case 41:
		return "Protocol wrong type for socket";
	case 42:
		return "Protocol not available";
	case 43:
		return "Protocol not supported";
	case 44:
		return "Socket type not supported";
	case 45:
		return "Operation not supported";
	case 46:
		return "Protocol family not supported";
	case 47:
		return "Address family not supported by protocol family";
	case 48:
		return "Address already in use";
	case 49:
		return "Can't assign requested address";
	case 50:
		return "Network is down";
	case 51:
		return "Network is unreachable";
	case 52:
		return "Network dropped connection on reset";
	case 53:
		return "Software caused connection abort";
	case 54:
		return "Connection reset by peer";
	case 55:
		return "No buffer space available";
	case 56:
		return "Socket is already connected";
	case 57:
		return "Socket is not connected";
	case 58:
		return "Can't send after socket shutdown";
	case 59:
		return "Too many references: can't splice";
	case 60:
		return "Operation timed out";
	case 61:
		return "Connection refused";
	case 62:
		return "Too many levels of symbolic links";
	case 63:
		return "File name too long";
	case 64:
		return "Host is down";
	case 65:
		return "No route to host";
	case 66:
		return "Directory not empty";
	case 67:
		return "Too many processes";
	case 68:
		return "Too many users";
	case 69:
		return "Disc quota exceeded";
	case 70:
		return "Stale NFS file handle";
	case 71:
		return "Too many levels of remote in path";
	case 72:
		return "RPC struct is bad";
	case 73:
		return "RPC version wrong";
	case 74:
		return "RPC prog. not avail";
	case 75:
		return " Program version wrong";
	case 76:
		return "Bad procedure for program";
	case 77:
		return "No locks available";
	case 78:
		return "Function not implemented";
	case 79:
		return "Inappropriate file type or format";
	case 80:
		return "Authentication error";
	case 81:
		return "Need authenticator";
	case 82:
		return "Identifier removed";
	case 83:
		return "No message of desired type";
	default:
		return "Bad ERRNO";
	}
}
