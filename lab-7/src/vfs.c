#include "vfs.h"
#include "thread.h"
#include "vm.h"
#include "mm.h"
#include "string.h"
#include "cpio.h"
#include "framebuffer.h"
#include "uart.h"

struct entry { char *name; struct vnode *node; };
struct ramnode { struct entry entries[16]; unsigned count; unsigned char *data; size_t size; int readonly; };
static struct vnode *root;
static struct filesystem *filesystems[8];
static unsigned fs_count;
static int equal(const char *a, const char *b) { while (*a && *a == *b) {a++; b++;} return *a == *b; }
static void copy(void *d, const void *s, size_t n) { unsigned char *a=d; const unsigned char *b=s; while(n--) *a++=*b++; }
static void *zero(size_t n) { void *p=kmalloc(n); if(p) strset(p,0,n); return p; }
static struct vnode *newnode(int dir);
static int ram_lookup(struct vnode *d,const char *name,struct vnode **out) {
	struct ramnode *r=d->internal;
	for(unsigned i=0;i<r->count;i++) if(equal(name,r->entries[i].name)) { *out=r->entries[i].node; return 0; }
	return -1;
}
static int ram_create(struct vnode *d,const char *name,int dir,struct vnode **out) {
	struct ramnode *r=d->internal; struct vnode *exists;
	size_t len=utils_strlen(name)-1;
	if(r->readonly || !len || len>255 || r->count==16 || !ram_lookup(d,name,&exists)) return -1;
	char *saved=kmalloc(len+1); if(!saved) return -1;
	struct vnode *v=newnode(dir); if(!v) {kfree(saved);return -1;}
	v->parent=d; struct entry *e=&r->entries[r->count++]; e->name=saved; copy(e->name,name,len+1); e->node=v; *out=v; return 0;
}
static long ram_read(struct file *f,void *buf,size_t n) {
	struct ramnode *r=f->node->internal;
	if(f->pos>=r->size) return 0;
	if(n>r->size-f->pos) n=r->size-f->pos;
	copy(buf,r->data+f->pos,n); f->pos+=n; return n;
}
static long ram_write(struct file *f,const void *buf,size_t n) {
	struct ramnode *r=f->node->internal;
	if(r->readonly || f->pos>4096) return -1;
	if(n>4096-f->pos) n=4096-f->pos;
	if(n && !r->data) { r->data=zero(4096); if(!r->data) return -1; }
	if(n) copy(r->data+f->pos,buf,n);
	f->pos+=n; if(r->size<f->pos) r->size=f->pos; return n;
}
static long ram_seek(struct file *f,long offset,int whence) {
	struct ramnode *r=f->node->internal;
	if(whence || offset<0 || (size_t)offset>r->size) return -1;
	return f->pos=offset;
}
static int ram_image(struct vnode *v, const void **data, size_t *size) {
	struct ramnode *r=v->internal;
	/* Lazy executable mappings need immutable backing, retained by the archive. */
	if(!r->readonly)return -1;
	*data=r->data; *size=r->size; return 0;
}
static const struct vnode_operations ram_ops={ram_lookup,ram_create};
static const struct file_operations ram_fops={ram_read,ram_write,ram_seek,0,ram_image};
static struct vnode *newnode(int dir) {
	struct vnode *v=zero(sizeof(*v)); if(!v) return 0;
	v->internal=zero(sizeof(struct ramnode)); if(!v->internal) {kfree(v);return 0;}
	v->directory=dir; v->ops=&ram_ops; v->fops=&ram_fops; return v;
}
static struct vnode *tmp_setup(void) { return newnode(1); }
int register_filesystem(struct filesystem *fs) {
	if(!fs || !fs->name || !fs->setup || fs_count==8) return -1;
	for(unsigned i=0;i<fs_count;i++) if(equal(fs->name,filesystems[i]->name)) return -1;
	filesystems[fs_count++]=fs; return 0;
}
/* Resolve components without lexical normalization: file/.. must fail. Mounted
	* roots inherit the covered directory's parent so .. crosses mount boundaries. */
static int walk(const char *path,struct vnode **out,int parent,char leaf[256]) {
	if(!path || utils_strlen(path)>256) return -1;
	struct thread *t=get_current();
	struct vnode *v=path[0]=='/' ? (t->root?t->root:root) : (t->cwd?t->cwd:root);
	const char *p=path;
	while(*p) {
		while(*p=='/') p++;
		if(!*p) { if(!v->directory) return -1; break; }
		char name[256]; unsigned n=0;
		while(*p && *p!='/') {if(n==255) return -1;name[n++]=*p++;} name[n]=0;
		const char *next=p; while(*next=='/') next++;
		if(!v->directory) return -1;
		if(parent && !*next) { if(equal(name,".")||equal(name,"..")) return -1; copy(leaf,name,n+1); *out=v; return 0; }
		if(equal(name,".")) continue;
		if(equal(name,"..")) { if(v!=(t->root?t->root:root) && v->parent) v=v->parent; continue; }
		if(v->ops->lookup(v,name,&v)<0) return -1;
		while(v->mounted) v=v->mounted;
	}
	if(parent) return -1;
	*out=v; return 0;
}
int vfs_lookup(const char *path,struct vnode **out) {return walk(path,out,0,0);}
int vfs_open(const char *path,int flags,struct file **out) {
	struct vnode *v; char name[256];
	preempt_disable();
	int rc=vfs_lookup(path,&v);
	if(rc<0 && (flags&O_CREAT)) {
		struct vnode *d; size_t n=utils_strlen(path)-1;
		if(n && path[n-1]!='/' && !walk(path,&d,1,name)) rc=d->ops->create(d,name,0,&v);
	}
	if(rc<0 || v->directory) {preempt_enable();return -1;}
	struct file *f=zero(sizeof(*f));
	if(f) {f->node=v; f->flags=flags; f->refs=1;*out=f;}
	preempt_enable(); return f?0:-1;
}
int vfs_close(struct file *f) {if(!f)return -1;preempt_disable();if(!--f->refs)kfree(f);preempt_enable();return 0;}
long vfs_read(struct file *f,void *b,size_t n) {if(!f || !f->node->fops->read)return -1;return f->node->fops->read(f,b,n);}
long vfs_write(struct file *f,const void *b,size_t n) {if(!f || !f->node->fops->write)return -1;return f->node->fops->write(f,b,n);}
long vfs_seek(struct file *f,long o,int w) {if(!f || !f->node->fops->seek)return -1;return f->node->fops->seek(f,o,w);}
int vfs_mkdir(const char *path) {
	struct vnode *d,*v;char name[256];preempt_disable();
	int r=walk(path,&d,1,name);if(!r)r=d->ops->create(d,name,1,&v);preempt_enable();return r;
}
int vfs_mount(const char *path,const char *fs) {
	preempt_disable(); struct vnode *v;int rc=-1;
	if(!vfs_lookup(path,&v) && v->directory) for(unsigned i=0;i<fs_count;i++) if(equal(fs,filesystems[i]->name)) {
		struct vnode *r=filesystems[i]->setup();if(r){r->parent=v->parent;v->mounted=r;rc=0;}break;
	}
	preempt_enable();return rc;
}
int vfs_chdir(const char *path) {struct vnode *v;if(vfs_lookup(path,&v)<0 || !v->directory)return -1;get_current()->cwd=v;return 0;}
static long console_read(struct file *f,void *b,size_t n) {
	(void)f;char *p=b;size_t i=0;while(i<n){if(uart_async_read(p+i))i++;else {if(get_current()->killed)thread_exit();schedule();}}return i;
}
static long console_write(struct file *f,const void *b,size_t n) {(void)f;const char *p=b;for(size_t i=0;i<n;i++)uart_write_char(p[i]);return n;}
static long fb_write(struct file *f,const void *b,size_t n) {
	FRAME_BUFFER *fb=framebuffer_get();size_t size=(size_t)fb->pitch*fb->height;
	if(!fb->fb_addr || f->pos>size)return -1;
	if(n>size-f->pos)n=size-f->pos;
	copy(fb->fb_addr+f->pos,b,n);f->pos+=n;return n;
}
static long fb_seek(struct file *f,long o,int w) {FRAME_BUFFER *fb=framebuffer_get();if(w || o<0 || (size_t)o>(size_t)fb->pitch*fb->height)return -1;return f->pos=o;}
static int fb_ioctl(struct file *f,unsigned long req,void *b) {(void)f;FRAME_BUFFER *fb=framebuffer_get();if(req || !fb->fb_addr)return -1;copy(b,fb,16);return 0;}
static const struct file_operations uart_ops={console_read,console_write,0,0,0};
static const struct file_operations fb_ops={0,fb_write,fb_seek,fb_ioctl,0};
static struct vnode *dev_setup(void) {
	struct vnode *r=newnode(1),*v;if(!r)return 0;
	if(ram_create(r,"uart",0,&v)<0)return 0;
	v->fops=&uart_ops;
	if(ram_create(r,"framebuffer",0,&v)<0)return 0;
	v->fops=&fb_ops;
	((struct ramnode *)r->internal)->readonly=1;return r;
}
static void readonly_tree(struct vnode *v) {struct ramnode *r=v->internal;r->readonly=1;for(unsigned i=0;i<r->count;i++)readonly_tree(r->entries[i].node);}
static struct vnode *init_setup(void) {
	struct vnode *r=newnode(1);if(!r)return 0;
	const unsigned char *p=(void *)cpio_addr,*end=(void *)cpio_end;
	while(p && end && p<=end && (size_t)(end-p)>=sizeof(struct cpio_header)) {
		const struct cpio_header *h=(void *)p;
		if(h->c_magic[0]!='0' || h->c_magic[1]!='7' || h->c_magic[2]!='0' || h->c_magic[3]!='7' || h->c_magic[4]!='0' || (h->c_magic[5]!='1' && h->c_magic[5]!='2'))return 0;
		size_t ns=utils_atoi(h->c_namesize,8),sz=utils_atoi(h->c_filesize,8);
		size_t off=(sizeof(*h)+ns+3)&~3UL;
		if(!ns || ns>256 || off>(size_t)(end-p) || sz>(size_t)(end-p)-off)return 0;
		const char *name=(void *)(p+sizeof(*h));if(name[ns-1])return 0;
		if(equal(name,"TRAILER!!!"))break;
		struct vnode *d=r,*v=r; const char *s=name;
		while(*s) {
			while(*s=='/')s++;
			if(!*s)break;
			char part[256];unsigned n=0;while(*s && *s!='/'){if(n==255)return 0;part[n++]=*s++;}part[n]=0;
			if(equal(part,"."))continue;
			if(equal(part,".."))return 0;
			const char *q=s;while(*q=='/')q++;
			int dir=*q || ((utils_atoi(h->c_mode,8)&0170000)==0040000);
			if(ram_lookup(d,part,&v)<0 && ram_create(d,part,dir,&v)<0)return 0;
			if(*q && !v->directory)return 0;
			d=v;
		}
		if(!v->directory){struct ramnode *rn=v->internal;rn->data=(void *)(p+off);rn->size=sz;}
		size_t step=(off+sz+3)&~3UL;if(step>(size_t)(end-p))return 0;p+=step;
	}
	readonly_tree(r);return r;
}
static struct filesystem tmpfs={"tmpfs",tmp_setup},initfs={"initramfs",init_setup},devfs={"devfs",dev_setup};
int vfs_init(void) {
	register_filesystem(&tmpfs);register_filesystem(&initfs);register_filesystem(&devfs);
	root=tmp_setup();if(!root)return -1;
	get_current()->root=get_current()->cwd=root;
	if(vfs_mkdir("/initramfs") || vfs_mount("/initramfs","initramfs") || vfs_mkdir("/dev") || vfs_mount("/dev","devfs"))return -1;
	return 0;
}
int vfs_task_init(struct thread *t) {
	t->root=root;t->cwd=root;
	for(int i=0;i<3;i++)if(vfs_open("/dev/uart",i?1:0,&t->fds[i])<0){vfs_task_close(t);return -1;}
	return 0;
}
void vfs_task_clone(struct thread *d,struct thread *s) {preempt_disable();d->root=s->root;d->cwd=s->cwd;for(int i=0;i<16;i++){d->fds[i]=s->fds[i];if(d->fds[i])d->fds[i]->refs++;}preempt_enable();}
void vfs_task_close(struct thread *t) {for(int i=0;i<16;i++)if(t->fds[i]){vfs_close(t->fds[i]);t->fds[i]=0;}}
int vfs_image(const char *path,const void **data,size_t *size) {
	struct vnode *v;
	if(vfs_lookup(path,&v)<0 || v->directory || !v->fops->image)return -1;
	return v->fops->image(v,data,size);
}
long vfs_syscall(unsigned nr,unsigned long *a) {
	thread_t *t=get_current();char path[256],fs[256];long result=-1;
	if(nr==11 || nr==15 || nr==17) {if(vm_copystr(t->vm,path,a[0],256)<0)return -1;}
	if(nr==11) {for(int i=0;i<16;i++)if(!t->fds[i])return vfs_open(path,a[1],&t->fds[i])<0?-1:i;return -1;}
	if(nr==15)return vfs_mkdir(path);
	if(nr==17)return vfs_chdir(path);
	if(nr==16) {if(vm_copystr(t->vm,path,a[1],256)<0 || vm_copystr(t->vm,fs,a[2],256)<0)return -1;return vfs_mount(path,fs);}
	if(a[0]>=16 || !t->fds[a[0]])return -1;
	struct file *f=t->fds[a[0]];
	if(nr==12){t->fds[a[0]]=0;return vfs_close(f);}
	if(nr==18)return vfs_seek(f,(long)a[1],a[2]);
	if(nr==19){unsigned info[4];if(!f->node->fops->ioctl || vm_user_check(t->vm,a[2],16,PROT_WRITE)<0)return -1;result=f->node->fops->ioctl(f,a[1],info);if(!result)result=vm_copyout(t->vm,a[2],info,16);return result;}
	if(nr!=13 && nr!=14)return -1;
	if(vm_user_check(t->vm,a[1],a[2],nr==13?PROT_READ:PROT_WRITE)<0)return -1;
	size_t done=0;unsigned char buf[256];
	while(done<a[2]) {
		size_t n=a[2]-done;if(n>sizeof(buf))n=sizeof(buf);
		/* Serialize shared offsets and tmpfs metadata; UART reads must yield. */
		if(nr==13 && vm_copyin(t->vm,buf,a[1]+done,n)<0)return done?(long)done:-1;
		int blocking=f->node->fops==&uart_ops && nr==14;
		if(!blocking)preempt_disable();
		long r=nr==13?vfs_write(f,buf,n):vfs_read(f,buf,n);
		if(!blocking)preempt_enable();
		if(r<0)return done?(long)done:-1;
		if(nr==14 && vm_copyout(t->vm,a[1]+done,buf,r)<0)return done?(long)done:-1;
		done+=r;if((size_t)r<n)break;
	}
	return done;
}
