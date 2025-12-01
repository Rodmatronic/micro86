#ifndef TYPES_H
#define TYPES_H

typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;
typedef unsigned long  ulong;

// BSD compatibility
typedef uint	u_int;
typedef ushort	u_short;
typedef uchar	u_char;
typedef ulong	u_long;

typedef uint pde_t;
typedef unsigned long  size_t;
typedef int dev_t;
typedef int ino_t;
typedef int FILE;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef unsigned long time_t;
typedef int mode_t;
typedef int uid_t;
typedef int gid_t;
typedef int idx_t;
typedef size_t ssize_t;
typedef int pid_t;
typedef long long intmax_t;
typedef long int64_t;
typedef int wchar_t;
typedef unsigned int	u_int32_t;
typedef unsigned short	u_int16_t;
typedef size_t off_t;

#define	UINTPTR_MAX		0xffffffffUL

#ifndef	SIZE_MAX
#define	SIZE_MAX		UINTPTR_MAX
#endif


#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#endif
