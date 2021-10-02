OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64", "elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(startup_64)
SECTIONS
{
 . = 0;
 .head.text : {
  _head = . ;
  KEEP(*(.head.text))
  _ehead = . ;
 }
 .rodata..compressed : {
  *(.rodata..compressed)
 }
 .text : {
  _text = .;
  *(.text)
  *(.text.*)
  _etext = . ;
 }
 .rodata : {
  _rodata = . ;
  *(.rodata)
  *(.rodata.*)
  _erodata = . ;
 }
 .got : {
  _got = .;
  KEEP(*(.got.plt))
  KEEP(*(.got))
  _egot = .;
 }
 .data : {
  _data = . ;
  *(.data)
  *(.data.*)
  _edata = . ;
 }
 . = ALIGN((1 << (6)));
 .bss : {
  _bss = . ;
  *(.bss)
  *(.bss.*)
  *(COMMON)
  . = ALIGN(8);
  _ebss = .;
 }
       . = ALIGN((1 << 12));
       .pgtable : {
  _pgtable = . ;
  *(.pgtable)
  _epgtable = . ;
 }
 . = ALIGN((1 << 12));
 _end = .;
}
