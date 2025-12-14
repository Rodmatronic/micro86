// File type and permission masks
#define S_IFMT  0170000  // Mask for file type
#define S_IFREG 0100000  // Regular file
#define S_IFDIR 0040000  // Directory
#define S_IFCHR 0020000  // Character device
#define S_IFBLK	0060000
#define S_IFIFO	0010000

// Permission bits
#define S_IRUSR 0400  // User read
#define S_IWUSR 0200  // User write
#define S_IXUSR 0100  // User execute
#define S_IRGRP 0040  // Group read
#define S_IWGRP 0020  // Group write
#define S_IXGRP 0010  // Group execute
#define S_IROTH 0004  // Other read
#define S_IWOTH 0002  // Other write
#define S_IXOTH 0001  // Other execute
#define	S_IRWXU	0000700
#define	S_IRWXG	000007
#define	S_IRWXO	0000007
#define	S_ISTXT	0001000	

#define	S_ISUID	04000		/* set user id on execution */
#define	S_ISGID	02000		/* set group id on execution */
#define	S_ISVTX	01000		/* save swapped text even after use */
#define	S_IREAD	00400		/* read permission, owner */
#define	S_IWRITE	00200		/* write permission, owner */
#define	S_IEXEC	00100		/* execute/search permission, owner */

#define S_ISREG(mode)	((mode&0xF000) == 0x8000)
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)

#define C_ISFIFO	0010000
#define C_ISLNK		0120000
#define C_ISSOCK	0140000

#define S_ISFIFO(mode)	((mode&0xF000) == 0x1000)
#define S_IFLNK  0120000  // Symbolic link
#define S_IFSOCK 0140000  // Socket

#define S_ISLNK(m)   (((m) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(m)  (((m) & S_IFMT) == S_IFSOCK)

#define	DEFFILEMODE	(S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)
#define	ACCESSPERMS	(S_IRWXU|S_IRWXG|S_IRWXO)

struct stat {
	unsigned int st_mode;     // File type and permissions
	unsigned int st_ctime;    // creation time
	unsigned int st_lmtime;	 // modification time
	int st_dev;       // File system's disk device
	unsigned int st_ino;      // Inode number
	short st_nlink;   // Number of links to file
	unsigned int st_size;     // Size of file in bytes
	int st_gid;
	int st_uid;
};

struct statx_timestamp {
	int64_t tv_sec;
	uint32_t tv_nsec, __pad;
};

struct statx {
	uint32_t stx_mask;
	uint32_t stx_blksize;
	uint64_t stx_attributes;
	uint32_t stx_nlink;
	uint32_t stx_uid;
	uint32_t stx_gid;
	uint16_t stx_mode;
	uint16_t __pad0[1];
	uint64_t stx_ino;
	uint64_t stx_size;
	uint64_t stx_blocks;
	uint64_t stx_attributes_mask;
	struct statx_timestamp stx_atime;
	struct statx_timestamp stx_btime;
	struct statx_timestamp stx_ctime;
	struct statx_timestamp stx_mtime;
	uint32_t stx_rdev_major;
	uint32_t stx_rdev_minor;
	uint32_t stx_dev_major;
	uint32_t stx_dev_minor;
	uint64_t __pad1[14];
};

