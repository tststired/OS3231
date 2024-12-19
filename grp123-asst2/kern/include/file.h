/*
 * Declarations for file handle and file table management.
 */

#ifndef _FILE_H_
#define _FILE_H_

/*
 * Contains some file-related maximum length constants
 */
#include <limits.h>


/*
 * Put your function declarations and data types here ...
 */

// per-process file descriptor table
// contains pointers to of_table entries
struct fd_table {
    int fd[OPEN_MAX];
};

// open files table
// keeps entries containing a vnode and file pointer
struct of_table {
    struct fd_entry *fd_entries[OPEN_MAX];
};

// contains vnode and file pointer offset
struct fd_entry {
    struct vnode *vn;
    off_t fp;
    int ref;
    int mode; 
};

struct of_table *oft;
int sys_open(userptr_t filename, int flags, mode_t  mode, int *retval);
int sys_close(int fd);
ssize_t sys_read(int fd, void *buf, size_t buflen, int *retval);
ssize_t sys_write(int fd, void *buf, size_t nbytes, int *retval);
int sys_dup2(int oldfd, int newfd, int *retval);
int sys_lseek(int fd, off_t pos, int whence, off_t *retval);

#endif /* _FILE_H_ */
