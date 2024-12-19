#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/limits.h>
#include <kern/stat.h>
#include <kern/seek.h>
#include <lib.h>
#include <uio.h>
#include <proc.h>
#include <current.h>
#include <synch.h>
#include <vfs.h>
#include <vnode.h>
#include <file.h>
#include <syscall.h>
#include <copyinout.h>

/*
 * Add your file-related functions here ...
 */


int sys_open(userptr_t filename, int flags, mode_t mode, int *retval) {
    int ret_val;
    struct vnode *vn;
    char fname[PATH_MAX];

    if (filename == NULL) return EFAULT;
    ret_val = copyinstr((const_userptr_t)filename, fname, PATH_MAX, NULL);
    if (ret_val) return EFAULT;
    
    for (int i = 0; i < OPEN_MAX; i++) {
        if (curproc->fdt->fd[i] == -1) {
            ret_val = vfs_open(fname, flags, mode, &vn);
            if (ret_val) return ret_val;
        
            
            for (int j = 0; j < OPEN_MAX; j++) {
                if (oft->fd_entries[j] == NULL) {
                    struct fd_entry *fd_entry = kmalloc(sizeof(*fd_entry));
                    fd_entry->vn = vn;
                    fd_entry->fp = 0;
                    fd_entry->mode = O_ACCMODE & flags; 
                    fd_entry->ref = 1;
                    oft->fd_entries[j] = fd_entry;
                    curproc->fdt->fd[i] = j;
                    *retval = i;
                    return ret_val;
                }
            }
        }
    }
    return 0;
}

int sys_close(int fd) {

    if(fd<0 || fd>=OPEN_MAX) return EBADF;
    if(curproc->fdt->fd[fd] == -1) return EBADF;

    struct fd_entry *f = oft->fd_entries[curproc->fdt->fd[fd]];
    f->ref--;
    if (f->ref == 0) {
        vfs_close(f->vn);
        kfree(oft->fd_entries[curproc->fdt->fd[fd]]);
    }
    oft->fd_entries[curproc->fdt->fd[fd]] = NULL;
    curproc->fdt->fd[fd] = -1;
    return 0;
}

ssize_t sys_read(int fd, void *buf, size_t buflen, int *retval) {
    struct iovec iov;
    struct uio u;
    int result; 
    
    if(fd<0 || fd>=OPEN_MAX) return EBADF;
    if (buf == NULL) return EFAULT;
    if (buf == ((void *)0x80000000)) return EFAULT;
    if (buf == ((void *)0x40000000)) return EFAULT;
    if (buflen <= 0) return EINVAL;
    if(curproc->fdt->fd[fd] == -1) return EBADF;

    struct fd_entry *f= oft->fd_entries[curproc->fdt->fd[fd]];
    if (f == NULL) return EBADF;
    
    
    if (f->mode == O_WRONLY){
        //kprintf("\n~~* open() got fd %d with flags %d but flags should be (O_WRONLY|O_CREAT) %d ~~\n", fd, f->mode, (O_WRONLY|O_CREAT));
        return EBADF;
    } 
    //defo triggers return val fix 
    //
           
    uio_uinit(&iov, &u, buf, buflen, f->fp, UIO_READ);
    result = VOP_READ(f->vn, &u);
    if (result) return EINVAL; 
    f->fp = u.uio_offset;
    *retval = buflen - u.uio_resid;
    //return success, all other codes should trigger error
    return 0;
}

ssize_t sys_write(int fd, void *buf, size_t nbytes, int *retval) {
    struct iovec iov;
    struct uio u;
    int result; 

    if (fd<0 || fd>=OPEN_MAX) return EBADF;
    if (buf == NULL) return EFAULT;
    if (buf == ((void *)0x80000000)) return EFAULT;
    if (buf == ((void *)0x40000000)) return EFAULT;
    if (nbytes<1) return EFAULT;
    if(curproc->fdt->fd[fd] == -1) return EBADF;

    struct fd_entry *f = oft->fd_entries[curproc->fdt->fd[fd]];
    if (f == NULL) return EBADF;
    if (f->mode == O_RDONLY) return EBADF;


    uio_uinit(&iov, &u, buf, nbytes, f->fp, UIO_WRITE);
    result = VOP_WRITE(f->vn, &u);
    if (result) return result;
    f->fp = u.uio_offset;
    *retval = nbytes - u.uio_resid;
    return 0;
}

int sys_dup2(int oldfd, int newfd, int *retval) {
    //if (oldfd == newfd) return newfd;
    if (newfd>=OPEN_MAX || oldfd>=OPEN_MAX) return EBADF;
    if (newfd<0 || oldfd<0 ) return EBADF;
    if (curproc->fdt->fd[oldfd] == -1) return EBADF;
    
    if (curproc->fdt->fd[newfd] != -1) {
    
        int result = sys_close(curproc->fdt->fd[newfd]);
        if (result) return result; 
        curproc->fdt->fd[newfd] = curproc->fdt->fd[oldfd];
        oft->fd_entries[curproc->fdt->fd[oldfd]]->ref++;
        *retval = newfd;
    }
    return 0;
}

int sys_lseek(int fd, off_t pos, int whence, off_t *retval) {

    if (fd<0 || fd>=OPEN_MAX) return EBADF;
    if(curproc->fdt->fd[fd] == -1) return EBADF;
    

    struct stat s; 
    struct fd_entry *f = oft->fd_entries[curproc->fdt->fd[fd]];
    if(!VOP_ISSEEKABLE(f->vn)) return ESPIPE;
    int result;

    result = VOP_STAT(f->vn, &s);
    if (result) return result;

    if (whence == SEEK_SET) {
        if (pos<0) return EINVAL;
        f->fp = pos;
    } else if (whence == SEEK_CUR) {
        f->fp += pos;
        if (f->fp<0) return EINVAL;
    } else if (whence == SEEK_END) {
        f->fp = s.st_size + pos;
    } else return EINVAL;


    *retval = f->fp;
    return 0;
}


