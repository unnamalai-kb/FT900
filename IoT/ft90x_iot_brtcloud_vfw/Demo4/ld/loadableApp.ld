/* Default linker script, for normal executables */
/*OUTPUT_FORMAT("elf32-v3")*/
/*OUTPUT_ARCH(v3)*/

INCLUDE kernel-api.ld /* This file includes all the kernel symbols */
/* Allow the command line to override the memory region sizes.  */
__PMSIZE = DEFINED(__PMSIZE)  ? __PMSIZE : 256K;
__RAMSIZE = DEFINED(__RAMSIZE) ? __RAMSIZE : 64K;

/* The loadable apps are allocated in user space - the last 112K of (PM - Bootloader) and last 28K of global data ram */
MEMORY
{
  userpm        (rx)   : ORIGIN = 0x00 + 235 * 1024 , LENGTH = 17K
  userram       (rw!x) : ORIGIN = 0x800000 + 55 * 1024, LENGTH = 10K
}
SECTIONS
{

/* All the loadable app code goes inside this section. */
  usertext  :
  {
    _text_load_address = .;
    *(.text*)
    *(.strings)
    *(._pm*)
    *(.init)
    *(.fini)
    . = ALIGN(4);
     _etext = . ;
  }  > userpm 
  


/* All the loadable data goes inside this section. */
    userdata	:
  {
  	. = 0x04;
  	. = ALIGN(4);
    _user_data_ram_origin = .;
    *(.data)
	*(.data*)
	*(.rodata)
    *(.rodata*)
  . = ALIGN(4);
     _edata = . ;
  }  > userram AT>userpm
   
   /* special constants used by loader, we add them to both the ELF output and the data segment */
       appname	:
  {
    KEEP(*(appname))
  }  > userram AT>userpm
  
         appvsn	:
  {
    KEEP(*(appvsn))
  }  > userram AT>userpm
  
         kapivsn	:
  {
    KEEP(*(kapivsn))
  }  > userram AT>userpm

   _user_data_load_start = LOADADDR(userdata);
   _user_data_load_end =  _user_data_load_start + SIZEOF(userdata) + SIZEOF (appname) + SIZEOF (appvsn) + SIZEOF (kapivsn);
   

     
  /* All the loadable BSS goes inside this section. */
  userbss :
  {
       . = ALIGN(4); /* Align BSS start to 32 bit address */
     _user_bss_start = . ;
    *(.bss)
    *(COMMON)
     _end = . ;
  }  > userram
  
    _user_heap_start = 0x80E000; /* Last 8K of ram for user heap */
    
}
