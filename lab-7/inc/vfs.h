#ifndef VFS_H
#define VFS_H
#include <stddef.h>
#define O_CREAT 0100
#define VFS_FDS 16
struct vnode;
struct file;
struct file_operations {
	long (*read)(struct file *, void *, size_t);
	long (*write)(struct file *, const void *, size_t);
	long (*seek)(struct file *, long, int);
	int (*ioctl)(struct file *, unsigned long, void *);
	int (*image)(struct vnode *, const void **, size_t *);
};
struct vnode_operations {
	int (*lookup)(struct vnode *, const char *, struct vnode **);
	int (*create)(struct vnode *, const char *, int, struct vnode **);
};
struct vnode {
	struct vnode *parent, *mounted;
	const struct vnode_operations *ops;
	const struct file_operations *fops;
	void *internal;
	int directory;
};
struct file { struct vnode *node; size_t pos; int flags; unsigned refs; };
struct filesystem { const char *name; struct vnode *(*setup)(void); };
struct thread;
int register_filesystem(struct filesystem *fs);
int vfs_init(void);
int vfs_lookup(const char *path, struct vnode **out);
int vfs_open(const char *path, int flags, struct file **out);
int vfs_close(struct file *f);
long vfs_read(struct file *f, void *buf, size_t n);
long vfs_write(struct file *f, const void *buf, size_t n);
long vfs_seek(struct file *f, long offset, int whence);
int vfs_mkdir(const char *path);
int vfs_mount(const char *path, const char *fs);
int vfs_chdir(const char *path);
int vfs_task_init(struct thread *t);
void vfs_task_clone(struct thread *dst, struct thread *src);
void vfs_task_close(struct thread *t);
long vfs_syscall(unsigned nr, unsigned long *args);
int vfs_image(const char *path, const void **data, size_t *size);
#endif
