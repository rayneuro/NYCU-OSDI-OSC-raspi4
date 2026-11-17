#include "syscall.h"
#define C(n,a,b,c) syscall3(n,(unsigned long)(a),(unsigned long)(b),(unsigned long)(c))
#define CHECK(x) do { if(!(x)){uart_write("VFS FAIL line ",14);char b[4]={'0'+__LINE__/100,'0'+(__LINE__/10)%10,'0'+__LINE__%10,'\n'};uart_write(b,4);exit(1);} } while(0)
__attribute__((section(".text.entry"))) void _start(void) {
	char b[32]={0};
	CHECK(C(13,1,"VFS stdout\n",11)==11);
	CHECK(C(15,"/work",0,0)==0);
	CHECK(C(15,"/work/sub",0,0)==0);
	CHECK(C(17,"/work/sub",0,0)==0);
	int fd=C(11,"../hello",0102,0);CHECK(fd==3);
	CHECK(C(13,fd,"abc",3)==3);
	CHECK(C(18,fd,0,0)==0);
	CHECK(C(14,fd,b,10)==3 && b[0]=='a' && b[2]=='c');
	CHECK(C(14,fd,b,1)==0);
	CHECK(C(11,"../hello/..",0,0)==-1);
	CHECK(C(15,"/work/sub",0,0)==-1);
	CHECK(C(16,0,"/work/sub","tmpfs")==0);
	CHECK(C(17,"/work/sub",0,0)==0);
	CHECK(C(11,"../hello",0,0)==4);
	CHECK(C(17,"/initramfs",0,0)==0);
	CHECK(C(11,"nope",0102,0)==-1);
	CHECK(C(15,"bad",0,0)==-1);
	int ro=C(11,"file1.txt",0,0);CHECK(ro==5);
	CHECK(C(14,ro,b,1)==1);
	CHECK(C(13,ro,"x",1)==-1);
	CHECK(C(14,ro,(void *)0x40000000,1)==-1);
	int fb=C(11,"/dev/framebuffer",1,0);CHECK(fb==6);
	unsigned info[4]={0};CHECK(C(19,fb,0,info)==0 && info[0]==1024 && info[2]>=4096);
	unsigned color=0x00ff0000;CHECK(C(13,fb,&color,4)==4);CHECK(C(18,fb,0,0)==0);
	CHECK(C(14,fb,b,1)==-1);CHECK(C(18,fb,-1,0)==-1);
	CHECK(fork()>=0);
	CHECK(C(12,fd,0,0)==0);CHECK(C(12,fd,0,0)==-1);
	int fds[16],n=0;while(n<16){int f=C(11,"/dev/uart",1,0);if(f<0)break;fds[n++]=f;}
	CHECK(n==10);for(int i=0;i<n;i++)CHECK(C(12,fds[i],0,0)==0);
	CHECK(C(13,1,"VFS PASS\n",9)==9);exit(0);
}
