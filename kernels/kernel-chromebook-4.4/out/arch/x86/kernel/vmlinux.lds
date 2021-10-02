/*
 *
 * Automatically generated file; DO NOT EDIT.
 * Linux/x86 4.4.280 Kernel Configuration
 *
 */
/*
 * Helper macros to use CONFIG_ options in C/CPP expressions. Note that
 * these only work with boolean and tristate options.
 */
/*
 * Getting something that works in C and CPP for an arg that may or may
 * not be defined is tricky.  Here, if we have "#define CONFIG_BOOGER 1"
 * we match on the placeholder define, insert the "0," for arg1 and generate
 * the triplet (0, 1, 0).  Then the last step cherry picks the 2nd arg (a one).
 * When CONFIG_BOOGER is not defined, we generate a (... 1, 0) pair, and when
 * the last step cherry picks the 2nd arg, we get a zero.
 */
/*
 * IS_BUILTIN(CONFIG_FOO) evaluates to 1 if CONFIG_FOO is set to 'y', 0
 * otherwise. For boolean options, this is equivalent to
 * IS_ENABLED(CONFIG_FOO).
 */
/*
 * IS_MODULE(CONFIG_FOO) evaluates to 1 if CONFIG_FOO is set to 'm', 0
 * otherwise.
 */
/*
 * IS_REACHABLE(CONFIG_FOO) evaluates to 1 if the currently compiled
 * code can call a function defined in code compiled based on CONFIG_FOO.
 * This is similar to IS_ENABLED(), but returns false when invoked from
 * built-in code when CONFIG_FOO is set to 'm'.
 */
/*
 * IS_ENABLED(CONFIG_FOO) evaluates to 1 if CONFIG_FOO is set to 'y' or 'm',
 * 0 otherwise.
 */
/*
 * ld script for the x86 kernel
 *
 * Historic 32-bit version written by Martin Mares <mj@atrey.karlin.mff.cuni.cz>
 *
 * Modernisation, unification and other changes and fixes:
 *   Copyright (C) 2007-2009  Sam Ravnborg <sam@ravnborg.org>
 *
 *
 * Don't define absolute symbols until and unless you know that symbol
 * value is should remain constant even if kernel image is relocated
 * at run time. Absolute symbols are not relocated. If symbol value should
 * change if kernel is relocated, make the symbol section relative and
 * put it inside the section definition.
 */
/*
 * Helper macros to support writing architecture specific
 * linker scripts.
 *
 * A minimal linker scripts has following content:
 * [This is a sample, architectures may have special requiriements]
 *
 * OUTPUT_FORMAT(...)
 * OUTPUT_ARCH(...)
 * ENTRY(...)
 * SECTIONS
 * {
 *	. = START;
 *	__init_begin = .;
 *	HEAD_TEXT_SECTION
 *	INIT_TEXT_SECTION(PAGE_SIZE)
 *	INIT_DATA_SECTION(...)
 *	PERCPU_SECTION(CACHELINE_SIZE)
 *	__init_end = .;
 *
 *	_stext = .;
 *	TEXT_SECTION = 0
 *	_etext = .;
 *
 *      _sdata = .;
 *	RO_DATA_SECTION(PAGE_SIZE)
 *	RW_DATA_SECTION(...)
 *	_edata = .;
 *
 *	EXCEPTION_TABLE(...)
 *	NOTES
 *
 *	BSS_SECTION(0, 0, 0)
 *	_end = .;
 *
 *	STABS_DEBUG
 *	DWARF_DEBUG
 *
 *	DISCARDS		// must be the last
 * }
 *
 * [__init_begin, __init_end] is the init section that may be freed after init
 * 	// __init_begin and __init_end should be page aligned, so that we can
 *	// free the whole .init memory
 * [_stext, _etext] is the text section
 * [_sdata, _edata] is the data section
 *
 * Some of the included output section have their own set of constants.
 * Examples are: [__initramfs_start, __initramfs_end] for initramfs and
 *               [__nosave_begin, __nosave_end] for the nosave data
 */
/*
 * Export symbols from the kernel to modules.  Forked from module.h
 * to reduce the amount of pointless cruft we feed to gcc when only
 * exporting a simple symbol or two.
 *
 * Try not to add #includes here.  It slows compilation and makes kernel
 * hackers place grumpy comments in header files.
 */
/* Some toolchains use a `_' prefix for all user symbols. */
/* Indirect, so macros are expanded before pasting. */
/* Align . to a 8 byte boundary equals to maximum function alignment. */
/*
 * Align to a 32 byte boundary equal to the
 * alignment gcc 4.5 uses for a struct
 */
/* The actual configuration determine if the init/exit sections
 * are handled as text/data or they can be discarded (which
 * often happens at runtime)
 */
/* .data section */
/*
 * Data section helpers
 */
/*
 * Allow architectures to handle ro_after_init data on their
 * own by defining an empty RO_AFTER_INIT_DATA.
 */
/*
 * Read only Data
 */
/* RODATA & RO_DATA provided for backward compatibility.
 * All archs are supposed to use RO_DATA() */
/* .text section. Map to function alignment to avoid address changes
 * during second ld run in second ld pass when generating System.map */
/* sched.text is aling to function alignment to secure we have same
 * address even at second ld pass when generating System.map */
/* spinlock.text is aling to function alignment to secure we have same
 * address even at second ld pass when generating System.map */
/* Section used for early init (in .S files) */
/*
 * Exception table
 */
/*
 * Init task
 */
/* init and exit section handling */
/*
 * bss (Block Started by Symbol) - uninitialized data
 * zeroed during startup
 */
/*
 * Allow archectures to redefine BSS_FIRST_SECTIONS to add extra
 * sections to the front of bss.
 */
/*
 * DWARF debug sections.
 * Symbols in the DWARF debugging sections are relative to
 * the beginning of the section so we begin them at 0.
 */
  /* Stabs debugging sections.  */
/*
 * Default discarded sections.
 *
 * Some archs want to discard exit text/data at runtime rather than
 * link time due to cross-section references such as alt instructions,
 * bug table, eh_frame, etc.  DISCARDS must be the last of output
 * section definitions so that such archs put those in earlier section
 * definitions.
 */
/**
 * PERCPU_INPUT - the percpu input sections
 * @cacheline: cacheline size
 *
 * The core percpu section names and core symbols which do not rely
 * directly upon load addresses.
 *
 * @cacheline is used to align subsections to avoid false cacheline
 * sharing between subsections for different purposes.
 */
/**
 * PERCPU_VADDR - define output section for percpu area
 * @cacheline: cacheline size
 * @vaddr: explicit base address (optional)
 * @phdr: destination PHDR (optional)
 *
 * Macro which expands to output section for percpu area.
 *
 * @cacheline is used to align subsections to avoid false cacheline
 * sharing between subsections for different purposes.
 *
 * If @vaddr is not blank, it specifies explicit base address and all
 * percpu symbols will be offset from the given address.  If blank,
 * @vaddr always equals @laddr + LOAD_OFFSET.
 *
 * @phdr defines the output PHDR to use if not blank.  Be warned that
 * output PHDR is sticky.  If @phdr is specified, the next output
 * section in the linker script will go there too.  @phdr should have
 * a leading colon.
 *
 * Note that this macros defines __per_cpu_load as an absolute symbol.
 * If there is no need to put the percpu section at a predetermined
 * address, use PERCPU_SECTION.
 */
/**
 * PERCPU_SECTION - define output section for percpu area, simple version
 * @cacheline: cacheline size
 *
 * Align to PAGE_SIZE and outputs output section for percpu area.  This
 * macro doesn't manipulate @vaddr or @phdr and __per_cpu_load and
 * __per_cpu_start will be identical.
 *
 * This macro is equivalent to ALIGN(PAGE_SIZE); PERCPU_VADDR(@cacheline,,)
 * except that __per_cpu_load is defined as a relative symbol against
 * .data..percpu which is required for relocatable x86_32 configuration.
 */
/*
 * Definition of the high level *_SECTION macros
 * They will fit only a subset of the architectures
 */
/*
 * Writeable data.
 * All sections are combined in a single .data section.
 * The sections following CONSTRUCTORS are arranged so their
 * typical alignment matches.
 * A cacheline is typical/always less than a PAGE_SIZE so
 * the sections that has this restriction (or similar)
 * is located before the ones requiring PAGE_SIZE alignment.
 * NOSAVE_DATA starts and ends with a PAGE_SIZE alignment which
 * matches the requirement of PAGE_ALIGNED_DATA.
 *
 * use 0 as page_align if page_aligned data is not used */
/*
 * DO NOT MODIFY.
 *
 * This file was generated by Kbuild
 */
/* thread_info.h: low-level thread information
 *
 * Copyright (C) 2002  David Howells (dhowells@redhat.com)
 * - Incorporating suggestions made by Linus Torvalds and Dave Miller
 */
/*
 * Allow us to mark functions as 'deprecated' and have gcc emit a nice
 * warning for each use, in hopes of speeding the functions removal.
 * Usage is:
 * 		int __deprecated foo(void)
 */
/*
 * Allow us to avoid 'defined but not used' warnings on functions and data,
 * as well as force them to be emitted to the assembly file.
 *
 * As of gcc 3.4, static functions that are not marked with attribute((used))
 * may be elided from the assembly file.  As of gcc 3.4, static data not so
 * marked will not be elided, but this may change in a future gcc version.
 *
 * NOTE: Because distributions shipped with a backported unit-at-a-time
 * compiler in gcc 3.3, we must define __used to be __attribute__((used))
 * for gcc >=3.3 instead of 3.4.
 *
 * In prior versions of gcc, such functions and data would be emitted, but
 * would be warned about except with attribute((unused)).
 *
 * Mark functions that are referenced only in inline assembly as __used so
 * the code is emitted even though it appears to be unreferenced.
 */
/*
 * Rather then using noinline to prevent stack consumption, use
 * noinline_for_stack instead.  For documentation reasons.
 */
/*
 * From the GCC manual:
 *
 * Many functions do not examine any values except their arguments,
 * and have no effects except the return value.  Basically this is
 * just slightly more strict class than the `pure' attribute above,
 * since function is not allowed to read global memory.
 *
 * Note that a function that has pointer arguments and examines the
 * data pointed to must _not_ be declared `const'.  Likewise, a
 * function that calls a non-`const' function usually must not be
 * `const'.  It does not make sense for a `const' function to return
 * `void'.
 */
/*
 * Tell gcc if a function is cold. The compiler will assume any path
 * directly leading to the call is unlikely.
 */
/* Simple shorthand for a section definition */
/*
 * Assume alignment of return value.
 */
/* Are two types/vars the same type (ignoring qualifiers)? */
/* Is this type a native word size -- useful for atomic operations */
/* Compile time object size, -1 for unknown */
/*
 * Sparse complains of variable sized arrays due to the temporary variable in
 * __compiletime_assert. Unfortunately we can't just expand it out to make
 * sparse see a constant array size without breaking compiletime_assert on old
 * versions of GCC (e.g. 4.2.4), so hide the array from sparse altogether.
 */
/**
 * compiletime_assert - break build and emit msg if condition is false
 * @condition: a compile-time constant condition to check
 * @msg:       a message to emit if condition is false
 *
 * In tradition of POSIX assert, this macro will break the build if the
 * supplied condition is *false*, emitting the supplied error message if the
 * compiler has support to do so.
 */
/*
 * Prevent the compiler from merging or refetching accesses.  The compiler
 * is also forbidden from reordering successive instances of ACCESS_ONCE(),
 * but only when the compiler is aware of some particular ordering.  One way
 * to make the compiler aware of ordering is to put the two invocations of
 * ACCESS_ONCE() in different C statements.
 *
 * ACCESS_ONCE will only work on scalar types. For union types, ACCESS_ONCE
 * on a union member will work as long as the size of the member matches the
 * size of the union and the size is smaller than word size.
 *
 * The major use cases of ACCESS_ONCE used to be (1) Mediating communication
 * between process-level code and irq/NMI handlers, all running on the same CPU,
 * and (2) Ensuring that the compiler does not  fold, spindle, or otherwise
 * mutilate accesses that either do not require ordering or that interact
 * with an explicit memory barrier or atomic instruction that provides the
 * required ordering.
 *
 * If possible use READ_ONCE()/WRITE_ONCE() instead.
 */
/**
 * lockless_dereference() - safely load a pointer for later dereference
 * @p: The pointer to load
 *
 * Similar to rcu_dereference(), but for situations where the pointed-to
 * object's lifetime is managed by something other than RCU.  That
 * "something other" might be reference counting or simple immortality.
 */
/* Ignore/forbid kprobes attach on very low level functions marked by this attribute: */
/*
 * This is needed in functions which generate the stack canary, see
 * arch/x86/kernel/smpboot.c::start_secondary() for an example.
 */
/*
 * int-ll64 is used everywhere now.
 */
/*
 * asm-generic/int-ll64.h
 *
 * Integer declarations for architectures which use "long long"
 * for 64-bit types.
 */
/*
 * asm-generic/int-ll64.h
 *
 * Integer declarations for architectures which use "long long"
 * for 64-bit types.
 */
/*
 * There seems to be no way of detecting this automatically from user
 * space, so 64 bit architectures should override this in their
 * bitsperlong.h. In particular, an architecture that supports
 * both 32 and 64 bit user space must not rely on CONFIG_64BIT
 * to decide it, but rather check a compiler provided macro.
 */
/*
 * FIXME: The check currently breaks x86-64 build, so it's
 * temporarily disabled. Please fix x86-64 and reenable
 */
/* const.h: Macros for dealing with constants.  */
/* Some constant macros are used in both assembler and
 * C code.  Therefore we cannot annotate them always with
 * 'UL' and other type specifiers unilaterally.  We
 * use the following macros to deal with this.
 *
 * Similarly, _AT() will cast an expression with a type in C, but
 * leave it unchanged in asm.
 */
/* PAGE_SHIFT determines the page size */
/* Cast *PAGE_MASK to a signed type so that it is sign-extended if
   virtual addresses are 32-bits but physical addresses are larger
   (ie, 32-bit PAE). */
/*
 * Set __PAGE_OFFSET to the most negative possible address +
 * PGDIR_SIZE*16 (pgd slot 272).  The gap is to allow a space for a
 * hypervisor to fit.  Choosing 16 slots here is arbitrary, but it's
 * what Xen requires.
 */
/* See Documentation/x86/x86_64/mm.txt for a description of the memory map. */
/*
 * Kernel image size is limited to 1GiB due to the fixmap living in the
 * next 1GiB (see level2_kernel_pgt in arch/x86/kernel/head_64.S). Use
 * 512MiB by default, leaving 1.5GiB for modules once the page tables
 * are fully set up. If kernel ASLR is configured, it can extend the
 * kernel page table mapping, reducing the size of the modules area.
 */
/*
 * PER_CPU finds an address of a per-cpu variable.
 *
 * Args:
 *    var - variable name
 *    reg - 32bit register
 *
 * The resulting address is stored in the "reg" argument.
 *
 * Example:
 *    PER_CPU(cpu_gdt_descr, %ebx)
 */
/*
 * Define the "EARLY_PER_CPU" macros.  These are used for some per_cpu
 * variables that are initialized and accessed before there are per_cpu
 * areas allocated.
 */
/*
 * TOP_OF_KERNEL_STACK_PADDING is a number of unused bytes that we
 * reserve at the top of the kernel stack.  We do it because of a nasty
 * 32-bit corner case.  On x86_32, the hardware stack frame is
 * variable-length.  Except for vm86 mode, struct pt_regs assumes a
 * maximum-length frame.  If we enter from CPL 0, the top 8 bytes of
 * pt_regs don't actually exist.  Ordinarily this doesn't matter, but it
 * does in at least one case:
 *
 * If we take an NMI early enough in SYSENTER, then we can end up with
 * pt_regs that extends above sp0.  On the way out, in the espfix code,
 * we can read the saved SS value, but that value will be above sp0.
 * Without this offset, that can result in a page fault.  (We are
 * careful that, in this case, the value we read doesn't matter.)
 *
 * In vm86 mode, the hardware frame is much longer still, so add 16
 * bytes to make room for the real-mode segments.
 *
 * x86_64 has a fixed-length stack frame.
 */
/*
 * low level task data that entry.S needs immediate access to
 * - this struct should fit entirely inside of one cache line
 * - this struct shares the supervisor stack pages
 */
/*
 * thread information flags
 * - these are process state flags that various assembly files
 *   may need to access
 * - pending work-to-be-done flags are in LSW
 * - other flags in MSW
 * Warning: layout of LSW is hardcoded in entry.S
 */
/* work to do in syscall_trace_enter() */
/* work to do on any return to user space */
/* flags to check in __switch_to() */
/*
 * Avoid calls to __switch_to_xtra() on UP as STIBP is not evaluated.
 */
/*
 * macros/functions for gaining access to the thread information structure
 *
 * preempt_count needs to be 1 initially, until the scheduler is functional.
 */
/* Indirect stringification.  Doing two levels allows the parameter to be a
 * macro itself.  For example, compile with -DFOO=bar, __stringify(FOO)
 * converts to "bar".
 */
/* Some toolchains use other characters (e.g. '`') to mark new line in macro */
/*
 * For assembly routines.
 *
 * Note when using these that you must specify the appropriate
 * alignment directives yourself
 */
/*
 * This is used by architectures to keep arguments on the stack
 * untouched by the compiler by keeping them live until the end.
 * The argument stack may be owned by the assembly-language
 * caller, not the callee, and gcc doesn't always understand
 * that.
 *
 * We have the return value, and a maximum of six arguments.
 *
 * This should always be followed by a "return ret" for the
 * protection to work (ie no more work that the compiler might
 * end up needing stack temporaries for).
 */
/* Assembly files may be compiled with -traditional .. */
/* If symbol 'name' is treated as a subroutine (gets called, and returns)
 * then please use ENDPROC to mark 'name' as STT_FUNC for the benefit of
 * static analysis tools such as stack depth analyzer.
 */
/* L1 cache line size */
/* const.h: Macros for dealing with constants.  */
/* If _PAGE_BIT_PRESENT is clear, we use these: */
/* - if the user mapped it with PROT_NONE; pte_present gives true */
/*
 * The same hidden bit is used by kmemcheck, but since kmemcheck
 * works on kernel pages while soft-dirty engine on user space,
 * they do not conflict with each other.
 */
/*
 * Tracking soft dirty bit when a page goes to a swap is tricky.
 * We need a bit which can be stored in pte _and_ not conflict
 * with swap entry format. On x86 bits 1-4 are *not* involved
 * into swap entry computation, but bit 7 is used for thp migration,
 * so we borrow bit 1 for soft dirty tracking.
 *
 * Please note that this bit must be treated as swap dirty page
 * mark if and only if the PTE/PMD has present bit clear!
 */
/* Set of bits not changed in pte_modify */
/* The ASID is the lower 12 bits of CR3 */
/* Mask for all the PCID-related bits in CR3: */
/* Let X86_CR3_PCID_ASID_USER be usable for the X86_CR3_PCID_NOFLUSH bit */
/*
 * The cache modes defined here are used to translate between pure SW usage
 * and the HW defined cache mode bits and/or PAT entries.
 *
 * The resulting bits for PWT, PCD and PAT should be chosen in a way
 * to have the WB mode at index 0 (all bits clear). This is the default
 * right now and likely would break too much if changed.
 */
/*         xwr */
/*
 * early identity mapping  pte attrib macros.
 */
/*
 * generic non-linear memory support:
 *
 * 1) we will not split memory into more chunks than will fit into the flags
 *    field of the struct page
 *
 * SECTION_SIZE_BITS		2^n: size of each section
 * MAX_PHYSADDR_BITS		2^n: max size of physical address space
 * MAX_PHYSMEM_BITS		2^n: how much memory we can have in that space
 *
 */
/*
 * PGDIR_SHIFT determines what a top-level page table entry can map
 */
/*
 * 3rd level page
 */
/*
 * PMD_SHIFT determines the size of the area a middle-level
 * page table can map
 */
/*
 * entries per page directory level
 */
/* See Documentation/x86/x86_64/mm.txt for a description of the memory map. */
/* Internal svga startup constants */
/* Physical address where kernel should be loaded. */
/* Minimum kernel alignment, as a power of two */
OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64", "elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(phys_startup_64)
jiffies = jiffies_64;
/*
 * On 64-bit, align RODATA to 2MB so we retain large page mappings for
 * boundaries spanning kernel text, rodata and data sections.
 *
 * However, kernel identity mappings will have different RWX permissions
 * to the pages mapping to text and to the pages padding (which are freed) the
 * text section. Hence kernel identity mappings will be broken to smaller
 * pages. For 64-bit, kernel text and kernel identity mappings are different,
 * so we can enable protection checks as well as retain 2MB large page
 * mappings for kernel text.
 */
PHDRS {
 text PT_LOAD FLAGS(5); /* R_E */
 data PT_LOAD FLAGS(6); /* RW_ */
 percpu PT_LOAD FLAGS(6); /* RW_ */
 init PT_LOAD FLAGS(7); /* RWE */
 note PT_NOTE FLAGS(0); /* ___ */
}
SECTIONS
{
        . = (0xffffffff80000000 + ALIGN(0x1000000, 0x200000));
        phys_startup_64 = startup_64 - 0xffffffff80000000;
 /* Text and read-only data */
 .text : AT(ADDR(.text) - 0xffffffff80000000) {
  _text = .;
  /* bootstrapping code */
  *(.head.text)
  . = ALIGN(8);
  _stext = .;
  . = ALIGN(8); *(.text.hot .text.hot.*) *(.text .text.fixup) *(.text.unlikely .text.unlikely.*) *(.text.unknown .text.unknown.*) *(.ref.text)
  . = ALIGN(8); __sched_text_start = .; *(.sched.text) __sched_text_end = .;
  . = ALIGN(8); __lock_text_start = .; *(.spinlock.text) __lock_text_end = .;
  . = ALIGN(8); __kprobes_text_start = .; *(.kprobes.text) __kprobes_text_end = .;
  . = ALIGN(8); __entry_text_start = .; *(.entry.text) __entry_text_end = .;
  . = ALIGN(8); __irqentry_text_start = .; *(.irqentry.text) __irqentry_text_end = .;
  . = ALIGN(8); __softirqentry_text_start = .; *(.softirqentry.text) __softirqentry_text_end = .;
  *(.fixup)
  *(.gnu.warning)
  __indirect_thunk_start = .;
  *(.text.__x86.indirect_thunk)
  __indirect_thunk_end = .;
  /* End of text section */
  _etext = .;
 } :text = 0x9090
 .notes : AT(ADDR(.notes) - 0xffffffff80000000) { __start_notes = .; *(.note.*) __stop_notes = .; } :text :note
 . = ALIGN(16); __ex_table : AT(ADDR(__ex_table) - 0xffffffff80000000) { __start___ex_table = .; *(__ex_table) __stop___ex_table = .; } :text = 0x9090
 /* .text should occupy whole number of pages */
 . = ALIGN((1 << 12));
 . = ALIGN((1 << 21));
 . = ALIGN(((1 << 12))); .rodata : AT(ADDR(.rodata) - 0xffffffff80000000) { __start_rodata = .; *(.rodata) *(.rodata.*) *(.data..ro_after_init) *(__vermagic) . = ALIGN(8); __start___tracepoints_ptrs = .; *(__tracepoints_ptrs) __stop___tracepoints_ptrs = .; *(__tracepoints_strings) } .rodata1 : AT(ADDR(.rodata1) - 0xffffffff80000000) { *(.rodata1) } . = ALIGN(8); __bug_table : AT(ADDR(__bug_table) - 0xffffffff80000000) { __start___bug_table = .; *(__bug_table) __stop___bug_table = .; } .pci_fixup : AT(ADDR(.pci_fixup) - 0xffffffff80000000) { __start_pci_fixups_early = .; *(.pci_fixup_early) __end_pci_fixups_early = .; __start_pci_fixups_header = .; *(.pci_fixup_header) __end_pci_fixups_header = .; __start_pci_fixups_final = .; *(.pci_fixup_final) __end_pci_fixups_final = .; __start_pci_fixups_enable = .; *(.pci_fixup_enable) __end_pci_fixups_enable = .; __start_pci_fixups_resume = .; *(.pci_fixup_resume) __end_pci_fixups_resume = .; __start_pci_fixups_resume_early = .; *(.pci_fixup_resume_early) __end_pci_fixups_resume_early = .; __start_pci_fixups_suspend = .; *(.pci_fixup_suspend) __end_pci_fixups_suspend = .; __start_pci_fixups_suspend_late = .; *(.pci_fixup_suspend_late) __end_pci_fixups_suspend_late = .; } .builtin_fw : AT(ADDR(.builtin_fw) - 0xffffffff80000000) { __start_builtin_fw = .; *(.builtin_fw) __end_builtin_fw = .; } . = ALIGN(4); .tracedata : AT(ADDR(.tracedata) - 0xffffffff80000000) { __tracedata_start = .; *(.tracedata) __tracedata_end = .; } __ksymtab : AT(ADDR(__ksymtab) - 0xffffffff80000000) { __start___ksymtab = .; *(SORT(___ksymtab+*)) __stop___ksymtab = .; } __ksymtab_gpl : AT(ADDR(__ksymtab_gpl) - 0xffffffff80000000) { __start___ksymtab_gpl = .; *(SORT(___ksymtab_gpl+*)) __stop___ksymtab_gpl = .; } __ksymtab_unused : AT(ADDR(__ksymtab_unused) - 0xffffffff80000000) { __start___ksymtab_unused = .; *(SORT(___ksymtab_unused+*)) __stop___ksymtab_unused = .; } __ksymtab_unused_gpl : AT(ADDR(__ksymtab_unused_gpl) - 0xffffffff80000000) { __start___ksymtab_unused_gpl = .; *(SORT(___ksymtab_unused_gpl+*)) __stop___ksymtab_unused_gpl = .; } __ksymtab_gpl_future : AT(ADDR(__ksymtab_gpl_future) - 0xffffffff80000000) { __start___ksymtab_gpl_future = .; *(SORT(___ksymtab_gpl_future+*)) __stop___ksymtab_gpl_future = .; } __kcrctab : AT(ADDR(__kcrctab) - 0xffffffff80000000) { __start___kcrctab = .; *(SORT(___kcrctab+*)) __stop___kcrctab = .; } __kcrctab_gpl : AT(ADDR(__kcrctab_gpl) - 0xffffffff80000000) { __start___kcrctab_gpl = .; *(SORT(___kcrctab_gpl+*)) __stop___kcrctab_gpl = .; } __kcrctab_unused : AT(ADDR(__kcrctab_unused) - 0xffffffff80000000) { __start___kcrctab_unused = .; *(SORT(___kcrctab_unused+*)) __stop___kcrctab_unused = .; } __kcrctab_unused_gpl : AT(ADDR(__kcrctab_unused_gpl) - 0xffffffff80000000) { __start___kcrctab_unused_gpl = .; *(SORT(___kcrctab_unused_gpl+*)) __stop___kcrctab_unused_gpl = .; } __kcrctab_gpl_future : AT(ADDR(__kcrctab_gpl_future) - 0xffffffff80000000) { __start___kcrctab_gpl_future = .; *(SORT(___kcrctab_gpl_future+*)) __stop___kcrctab_gpl_future = .; } __ksymtab_strings : AT(ADDR(__ksymtab_strings) - 0xffffffff80000000) { *(__ksymtab_strings) } __init_rodata : AT(ADDR(__init_rodata) - 0xffffffff80000000) { *(.ref.rodata) } __param : AT(ADDR(__param) - 0xffffffff80000000) { __start___param = .; *(__param) __stop___param = .; } __modver : AT(ADDR(__modver) - 0xffffffff80000000) { __start___modver = .; *(__modver) __stop___modver = .; . = ALIGN(((1 << 12))); __end_rodata = .; } . = ALIGN(((1 << 12)));
 . = ALIGN((1 << 21)); __end_rodata_hpage_align = .;
 /* Data */
 .data : AT(ADDR(.data) - 0xffffffff80000000) {
  /* Start of data section */
  _sdata = .;
  /* init_task */
  . = ALIGN(((1 << 12) << (2 + 0))); *(.data..init_task)
  . = ALIGN((1 << 12)); *(.data..page_aligned)
  . = ALIGN((1 << (6))); *(.data..cacheline_aligned)
  *(.data) *(.ref.data) *(.data..shared_aligned) *(.data.unlikely) . = ALIGN(32); *(__tracepoints) . = ALIGN(8); __start___jump_table = .; *(__jump_table) __stop___jump_table = .; . = ALIGN(8); __start___verbose = .; *(__verbose) __stop___verbose = .; __start___trace_bprintk_fmt = .; *(__trace_printk_fmt) __stop___trace_bprintk_fmt = .; __start___tracepoint_str = .; *(__tracepoint_str) __stop___tracepoint_str = .;
  CONSTRUCTORS
  /* rarely changed data like cpu maps */
  . = ALIGN((1 << 6)); *(.data..read_mostly) . = ALIGN((1 << 6));
  /* End of data section */
  _edata = .;
 } :data
 . = ALIGN((1 << 12));
 __vvar_page = .;
 .vvar : AT(ADDR(.vvar) - 0xffffffff80000000) {
  /* work around gold bug 13023 */
  __vvar_beginning_hack = .;
  /* Place all vvars at the offsets in asm/vvar.h. */
/*
 * vvar.h: Shared vDSO/kernel variable declarations
 * Copyright (c) 2011 Andy Lutomirski
 * Subject to the GNU General Public License, version 2
 *
 * A handful of variables are accessible (read-only) from userspace
 * code in the vsyscall page and the vdso.  They are declared here.
 * Some other file must define them with DEFINE_VVAR.
 *
 * In normal kernel code, they are used like any other variable.
 * In user code, they are accessed through the VVAR macro.
 *
 * These variables live in a page of kernel data that has an extra RO
 * mapping for userspace.  Each variable needs a unique offset within
 * that page; specify that offset with the DECLARE_VVAR macro.  (If
 * you mess up, the linker will catch it.)
 */
/* The kernel linker script defines its own magic to put vvars in the
 * right place.
 */
/* DECLARE_VVAR(offset, type, name) */
. = __vvar_beginning_hack + 128; *(.vvar_vsyscall_gtod_data)
  /*
		 * Pad the rest of the page with zeros.  Otherwise the loader
		 * can leave garbage here.
		 */
  . = __vvar_beginning_hack + (1 << 12);
 } :data
       . = ALIGN(__vvar_page + (1 << 12), (1 << 12));
 /* Init code and data - will be freed after init */
 . = ALIGN((1 << 12));
 .init.begin : AT(ADDR(.init.begin) - 0xffffffff80000000) {
  __init_begin = .; /* paired with __init_end */
 }
 /*
	 * percpu offsets are zero-based on SMP.  PERCPU_VADDR() changes the
	 * output PHDR, so the next output section - .init.text - should
	 * start another segment - init.
	 */
 __per_cpu_load = .; .data..percpu 0 : AT(__per_cpu_load - 0xffffffff80000000) { __per_cpu_start = .; __per_cpu_user_mapped_start = .; *(.data..percpu..first) . = ALIGN((1 << 6)); *(.data..percpu..user_mapped) *(.data..percpu..user_mapped..shared_aligned) . = ALIGN((1 << 12)); *(.data..percpu..user_mapped..page_aligned) __per_cpu_user_mapped_end = .; . = ALIGN((1 << 12)); *(.data..percpu..page_aligned) . = ALIGN((1 << 6)); *(.data..percpu..read_mostly) . = ALIGN((1 << 6)); *(.data..percpu) *(.data..percpu..shared_aligned) __per_cpu_end = .; } :percpu . = __per_cpu_load + SIZEOF(.data..percpu);
 ASSERT(SIZEOF(.data..percpu) < 0x1000000,
        "per-CPU data too large - increase CONFIG_PHYSICAL_START")
 . = ALIGN((1 << 12)); .init.text : AT(ADDR(.init.text) - 0xffffffff80000000) { _sinittext = .; *(.init.text) *(.text.startup) *(.meminit.text) _einittext = .; }
 :init
 /*
	 * Section for code used exclusively before alternatives are run. All
	 * references to such code must be patched out by alternatives, normally
	 * by using X86_FEATURE_ALWAYS CPU feature bit.
	 *
	 * See static_cpu_has() for an example.
	 */
 .altinstr_aux : AT(ADDR(.altinstr_aux) - 0xffffffff80000000) {
  *(.altinstr_aux)
 }
 .init.data : AT(ADDR(.init.data) - 0xffffffff80000000) { *(.init.data) *(.meminit.data) . = ALIGN(8); __start_mcount_loc = .; *(__mcount_loc) __stop_mcount_loc = .; *(.init.rodata) . = ALIGN(8); __start_ftrace_events = .; *(_ftrace_events) __stop_ftrace_events = .; __start_ftrace_enum_maps = .; *(_ftrace_enum_map) __stop_ftrace_enum_maps = .; . = ALIGN(8); __start_syscalls_metadata = .; *(__syscalls_metadata) __stop_syscalls_metadata = .; *(.meminit.rodata) . = ALIGN(8); __clk_of_table = .; *(__clk_of_table) *(__clk_of_table_end) . = ALIGN(8); __cpu_method_of_table = .; *(__cpu_method_of_table) *(__cpu_method_of_table_end) . = ALIGN(8); __cpuidle_method_of_table = .; *(__cpuidle_method_of_table) *(__cpuidle_method_of_table_end) . = ALIGN(32); __dtb_start = .; *(.dtb.init.rodata) __dtb_end = .; . = ALIGN(8); __irqchip_of_table = .; *(__irqchip_of_table) *(__irqchip_of_table_end) . = ALIGN(8); __irqchip_acpi_probe_table = .; *(__irqchip_acpi_probe_table) __irqchip_acpi_probe_table_end = .; . = ALIGN(8); __clksrc_acpi_probe_table = .; *(__clksrc_acpi_probe_table) __clksrc_acpi_probe_table_end = .; . = ALIGN(32); __earlycon_table = .; *(__earlycon_table) __earlycon_table_end = .; . = ALIGN(16); __setup_start = .; *(.init.setup) __setup_end = .; __initcall_start = .; *(.initcallearly.init) __initcall0_start = .; *(.initcall0.init) *(.initcall0s.init) __initcall1_start = .; *(.initcall1.init) *(.initcall1s.init) __initcall2_start = .; *(.initcall2.init) *(.initcall2s.init) __initcall3_start = .; *(.initcall3.init) *(.initcall3s.init) __initcall4_start = .; *(.initcall4.init) *(.initcall4s.init) __initcall5_start = .; *(.initcall5.init) *(.initcall5s.init) __initcallrootfs_start = .; *(.initcallrootfs.init) *(.initcallrootfss.init) __initcall6_start = .; *(.initcall6.init) *(.initcall6s.init) __initcall7_start = .; *(.initcall7.init) *(.initcall7s.init) __initcall_end = .; __con_initcall_start = .; *(.con_initcall.init) __con_initcall_end = .; __security_initcall_start = .; *(.security_initcall.init) __security_initcall_end = .; . = ALIGN(4); __initramfs_start = .; *(.init.ramfs) . = ALIGN(8); *(.init.ramfs.info) }
 .x86_cpu_dev.init : AT(ADDR(.x86_cpu_dev.init) - 0xffffffff80000000) {
  __x86_cpu_dev_start = .;
  *(.x86_cpu_dev.init)
  __x86_cpu_dev_end = .;
 }
 /*
	 * start address and size of operations which during runtime
	 * can be patched with virtualization friendly instructions or
	 * baremetal native ones. Think page table operations.
	 * Details in paravirt_types.h
	 */
 . = ALIGN(8);
 .parainstructions : AT(ADDR(.parainstructions) - 0xffffffff80000000) {
  __parainstructions = .;
  *(.parainstructions)
  __parainstructions_end = .;
 }
 /*
	 * struct alt_inst entries. From the header (alternative.h):
	 * "Alternative instructions for different CPU types or capabilities"
	 * Think locking instructions on spinlocks.
	 */
 . = ALIGN(8);
 .altinstructions : AT(ADDR(.altinstructions) - 0xffffffff80000000) {
  __alt_instructions = .;
  *(.altinstructions)
  __alt_instructions_end = .;
 }
 /*
	 * And here are the replacement instructions. The linker sticks
	 * them as binary blobs. The .altinstructions has enough data to
	 * get the address and the length of them to patch the kernel safely.
	 */
 .altinstr_replacement : AT(ADDR(.altinstr_replacement) - 0xffffffff80000000) {
  *(.altinstr_replacement)
 }
 /*
	 * struct iommu_table_entry entries are injected in this section.
	 * It is an array of IOMMUs which during run time gets sorted depending
	 * on its dependency order. After rootfs_initcall is complete
	 * this section can be safely removed.
	 */
 .iommu_table : AT(ADDR(.iommu_table) - 0xffffffff80000000) {
  __iommu_table = .;
  *(.iommu_table)
  __iommu_table_end = .;
 }
 . = ALIGN(8);
 .apicdrivers : AT(ADDR(.apicdrivers) - 0xffffffff80000000) {
  __apicdrivers = .;
  *(.apicdrivers);
  __apicdrivers_end = .;
 }
 . = ALIGN(8);
 /*
	 * .exit.text is discard at runtime, not link time, to deal with
	 *  references from .altinstructions and .eh_frame
	 */
 .exit.text : AT(ADDR(.exit.text) - 0xffffffff80000000) {
  *(.exit.text) *(.text.exit) *(.memexit.text)
 }
 .exit.data : AT(ADDR(.exit.data) - 0xffffffff80000000) {
  *(.exit.data) *(.fini_array) *(.dtors) *(.memexit.data) *(.memexit.rodata)
 }
 . = ALIGN((1 << 12));
 /* freed after init ends here */
 .init.end : AT(ADDR(.init.end) - 0xffffffff80000000) {
  __init_end = .;
 }
 /*
	 * smp_locks might be freed after init
	 * start/end must be page aligned
	 */
 . = ALIGN((1 << 12));
 .smp_locks : AT(ADDR(.smp_locks) - 0xffffffff80000000) {
  __smp_locks = .;
  *(.smp_locks)
  . = ALIGN((1 << 12));
  __smp_locks_end = .;
 }
 .data_nosave : AT(ADDR(.data_nosave) - 0xffffffff80000000) {
  . = ALIGN((1 << 12)); __nosave_begin = .; *(.data..nosave) . = ALIGN((1 << 12)); __nosave_end = .;
 }
 /* BSS */
 . = ALIGN((1 << 12));
 .bss : AT(ADDR(.bss) - 0xffffffff80000000) {
  __bss_start = .;
  *(.bss..page_aligned)
  *(.bss)
  . = ALIGN((1 << 12));
  __bss_stop = .;
 }
 . = ALIGN((1 << 12));
 .brk : AT(ADDR(.brk) - 0xffffffff80000000) {
  __brk_base = .;
  . += 64 * 1024; /* 64k alignment slop space */
  *(.brk_reservation) /* areas brk users have reserved */
  __brk_limit = .;
 }
 _end = .;
        .stab 0 : { *(.stab) } .stabstr 0 : { *(.stabstr) } .stab.excl 0 : { *(.stab.excl) } .stab.exclstr 0 : { *(.stab.exclstr) } .stab.index 0 : { *(.stab.index) } .stab.indexstr 0 : { *(.stab.indexstr) } .comment 0 : { *(.comment) }
        .debug 0 : { *(.debug) } .line 0 : { *(.line) } .debug_srcinfo 0 : { *(.debug_srcinfo) } .debug_sfnames 0 : { *(.debug_sfnames) } .debug_aranges 0 : { *(.debug_aranges) } .debug_pubnames 0 : { *(.debug_pubnames) } .debug_info 0 : { *(.debug_info .gnu.linkonce.wi.*) } .debug_abbrev 0 : { *(.debug_abbrev) } .debug_line 0 : { *(.debug_line) } .debug_frame 0 : { *(.debug_frame) } .debug_str 0 : { *(.debug_str) } .debug_loc 0 : { *(.debug_loc) } .debug_macinfo 0 : { *(.debug_macinfo) } .debug_weaknames 0 : { *(.debug_weaknames) } .debug_funcnames 0 : { *(.debug_funcnames) } .debug_typenames 0 : { *(.debug_typenames) } .debug_varnames 0 : { *(.debug_varnames) }
 /* Sections to be discarded */
 /DISCARD/ : { *(.exit.text) *(.text.exit) *(.memexit.text) *(.exit.data) *(.fini_array) *(.dtors) *(.memexit.data) *(.memexit.rodata) *(.exitcall.exit) *(.discard) *(.discard.*) }
 /DISCARD/ : { *(.eh_frame) }
}
/*
 * Per-cpu symbols which need to be offset from __per_cpu_load
 * for the boot processor.
 */
init_per_cpu__gdt_page = ABSOLUTE(gdt_page) + __per_cpu_load;
init_per_cpu__irq_stack_union = ABSOLUTE(irq_stack_union) + __per_cpu_load;
/*
 * Build-time check on the image size:
 */
. = ASSERT((_end - _text <= 0x40000000),
    "kernel image bigger than KERNEL_IMAGE_SIZE");
. = ASSERT((irq_stack_union == 0),
           "irq_stack_union is not at start of per-cpu area");
