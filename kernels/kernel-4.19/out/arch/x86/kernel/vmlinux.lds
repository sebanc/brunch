OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64", "elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(phys_startup_64)
jiffies = jiffies_64;
PHDRS {
 text PT_LOAD FLAGS(5);
 data PT_LOAD FLAGS(6);
 percpu PT_LOAD FLAGS(6);
 init PT_LOAD FLAGS(7);
 note PT_NOTE FLAGS(0);
}
SECTIONS
{
 . = (0xffffffff80000000 + ALIGN(0x1000000, 0x200000));
 phys_startup_64 = ABSOLUTE(startup_64 - 0xffffffff80000000);
 .text : AT(ADDR(.text) - 0xffffffff80000000) {
  _text = .;
  _stext = .;
  KEEP(*(.head.text))
  . = ALIGN(8); *(.text.hot .text.hot.*) *(.text .text.fixup) *(.text.unlikely .text.unlikely.*) *(.text.unknown .text.unknown.*) . = ALIGN(8); __noinstr_text_start = .; *(.noinstr.text) __noinstr_text_end = .; *(.text..refcount) *(.ref.text)
  . = ALIGN(8); __sched_text_start = .; *(.sched.text) __sched_text_end = .;
  . = ALIGN(8); __cpuidle_text_start = .; *(.cpuidle.text) __cpuidle_text_end = .;
  . = ALIGN(8); __lock_text_start = .; *(.spinlock.text) __lock_text_end = .;
  . = ALIGN(8); __kprobes_text_start = .; *(.kprobes.text) __kprobes_text_end = .;
  . = ALIGN((1 << 21));
  . = ALIGN(8); __entry_text_start = .; *(.entry.text) __entry_text_end = .;
  . = ALIGN(8); __irqentry_text_start = .; *(.irqentry.text) __irqentry_text_end = .;
  . = ALIGN((1 << 21));
  . = ALIGN(8); __softirqentry_text_start = .; *(.softirqentry.text) __softirqentry_text_end = .;
  *(.fixup)
  *(.gnu.warning)
  . = ALIGN((1 << 12));
  __entry_trampoline_start = .;
  _entry_trampoline = .;
  *(.entry_trampoline)
  . = ALIGN((1 << 12));
  __entry_trampoline_end = .;
  ASSERT(. - _entry_trampoline == (1 << 12), "entry trampoline is too big");
  __indirect_thunk_start = .;
  *(.text.__x86.indirect_thunk)
  __indirect_thunk_end = .;
  _etext = .;
 } :text = 0x9090
 .notes : AT(ADDR(.notes) - 0xffffffff80000000) { __start_notes = .; KEEP(*(.note.*)) __stop_notes = .; } :text :note
 . = ALIGN(16); __ex_table : AT(ADDR(__ex_table) - 0xffffffff80000000) { __start___ex_table = .; KEEP(*(__ex_table)) __stop___ex_table = .; } :text = 0x9090
 . = ALIGN((1 << 12));
 . = ALIGN((1 << 21));
 . = ALIGN(((1 << 12))); .rodata : AT(ADDR(.rodata) - 0xffffffff80000000) { __start_rodata = .; *(.rodata) *(.rodata.*) . = ALIGN(8); __start_ro_after_init = .; *(.data..ro_after_init) __end_ro_after_init = .; KEEP(*(__vermagic)) . = ALIGN(8); __start___tracepoints_ptrs = .; KEEP(*(__tracepoints_ptrs)) __stop___tracepoints_ptrs = .; *(__tracepoints_strings) } .rodata1 : AT(ADDR(.rodata1) - 0xffffffff80000000) { *(.rodata1) } .pci_fixup : AT(ADDR(.pci_fixup) - 0xffffffff80000000) { __start_pci_fixups_early = .; KEEP(*(.pci_fixup_early)) __end_pci_fixups_early = .; __start_pci_fixups_header = .; KEEP(*(.pci_fixup_header)) __end_pci_fixups_header = .; __start_pci_fixups_final = .; KEEP(*(.pci_fixup_final)) __end_pci_fixups_final = .; __start_pci_fixups_enable = .; KEEP(*(.pci_fixup_enable)) __end_pci_fixups_enable = .; __start_pci_fixups_resume = .; KEEP(*(.pci_fixup_resume)) __end_pci_fixups_resume = .; __start_pci_fixups_resume_early = .; KEEP(*(.pci_fixup_resume_early)) __end_pci_fixups_resume_early = .; __start_pci_fixups_suspend = .; KEEP(*(.pci_fixup_suspend)) __end_pci_fixups_suspend = .; __start_pci_fixups_suspend_late = .; KEEP(*(.pci_fixup_suspend_late)) __end_pci_fixups_suspend_late = .; } .builtin_fw : AT(ADDR(.builtin_fw) - 0xffffffff80000000) ALIGN(8) { __start_builtin_fw = .; KEEP(*(.builtin_fw)) __end_builtin_fw = .; } __ksymtab : AT(ADDR(__ksymtab) - 0xffffffff80000000) { __start___ksymtab = .; KEEP(*(SORT(___ksymtab+*))) __stop___ksymtab = .; } __ksymtab_gpl : AT(ADDR(__ksymtab_gpl) - 0xffffffff80000000) { __start___ksymtab_gpl = .; KEEP(*(SORT(___ksymtab_gpl+*))) __stop___ksymtab_gpl = .; } __ksymtab_unused : AT(ADDR(__ksymtab_unused) - 0xffffffff80000000) { __start___ksymtab_unused = .; KEEP(*(SORT(___ksymtab_unused+*))) __stop___ksymtab_unused = .; } __ksymtab_unused_gpl : AT(ADDR(__ksymtab_unused_gpl) - 0xffffffff80000000) { __start___ksymtab_unused_gpl = .; KEEP(*(SORT(___ksymtab_unused_gpl+*))) __stop___ksymtab_unused_gpl = .; } __ksymtab_gpl_future : AT(ADDR(__ksymtab_gpl_future) - 0xffffffff80000000) { __start___ksymtab_gpl_future = .; KEEP(*(SORT(___ksymtab_gpl_future+*))) __stop___ksymtab_gpl_future = .; } __kcrctab : AT(ADDR(__kcrctab) - 0xffffffff80000000) { __start___kcrctab = .; KEEP(*(SORT(___kcrctab+*))) __stop___kcrctab = .; } __kcrctab_gpl : AT(ADDR(__kcrctab_gpl) - 0xffffffff80000000) { __start___kcrctab_gpl = .; KEEP(*(SORT(___kcrctab_gpl+*))) __stop___kcrctab_gpl = .; } __kcrctab_unused : AT(ADDR(__kcrctab_unused) - 0xffffffff80000000) { __start___kcrctab_unused = .; KEEP(*(SORT(___kcrctab_unused+*))) __stop___kcrctab_unused = .; } __kcrctab_unused_gpl : AT(ADDR(__kcrctab_unused_gpl) - 0xffffffff80000000) { __start___kcrctab_unused_gpl = .; KEEP(*(SORT(___kcrctab_unused_gpl+*))) __stop___kcrctab_unused_gpl = .; } __kcrctab_gpl_future : AT(ADDR(__kcrctab_gpl_future) - 0xffffffff80000000) { __start___kcrctab_gpl_future = .; KEEP(*(SORT(___kcrctab_gpl_future+*))) __stop___kcrctab_gpl_future = .; } __ksymtab_strings : AT(ADDR(__ksymtab_strings) - 0xffffffff80000000) { *(__ksymtab_strings) } __init_rodata : AT(ADDR(__init_rodata) - 0xffffffff80000000) { *(.ref.rodata) } __param : AT(ADDR(__param) - 0xffffffff80000000) { __start___param = .; KEEP(*(__param)) __stop___param = .; } __modver : AT(ADDR(__modver) - 0xffffffff80000000) { __start___modver = .; KEEP(*(__modver)) __stop___modver = .; . = ALIGN(((1 << 12))); __end_rodata = .; } . = ALIGN(((1 << 12)));
 . = ALIGN((1 << 21)); __end_rodata_hpage_align = .; __end_rodata_aligned = .;
 .data : AT(ADDR(.data) - 0xffffffff80000000) {
  _sdata = .;
  . = ALIGN(((1 << 12) << (2 + 0))); __start_init_task = .; init_thread_union = .; init_stack = .; KEEP(*(.data..init_task)) KEEP(*(.data..init_thread_info)) . = __start_init_task + ((1 << 12) << (2 + 0)); __end_init_task = .;
  . = ALIGN((1 << 12)); *(.data..page_aligned) . = ALIGN((1 << 12));
  . = ALIGN((1 << (6))); *(.data..cacheline_aligned)
  *(.xiptext) *(.data) *(.ref.data) *(.data..shared_aligned) *(.data.unlikely) __start_once = .; *(.data.once) __end_once = .; . = ALIGN(32); *(__tracepoints) . = ALIGN(8); __start___jump_table = .; KEEP(*(__jump_table)) __stop___jump_table = .; . = ALIGN(8); __start___verbose = .; KEEP(*(__verbose)) __stop___verbose = .; __start___trace_bprintk_fmt = .; KEEP(*(__trace_printk_fmt)) __stop___trace_bprintk_fmt = .; . = ALIGN(32); __start__bpf_raw_tp = .; KEEP(*(__bpf_raw_tp_map)) __stop__bpf_raw_tp = .; __start___tracepoint_str = .; KEEP(*(__tracepoint_str)) __stop___tracepoint_str = .;
  CONSTRUCTORS
  . = ALIGN((1 << 6)); *(.data..read_mostly) . = ALIGN((1 << 6));
  _edata = .;
 } :data
 . = ALIGN(8); __bug_table : AT(ADDR(__bug_table) - 0xffffffff80000000) { __start___bug_table = .; KEEP(*(__bug_table)) __stop___bug_table = .; }
 . = ALIGN(4); .orc_unwind_ip : AT(ADDR(.orc_unwind_ip) - 0xffffffff80000000) { __start_orc_unwind_ip = .; KEEP(*(.orc_unwind_ip)) __stop_orc_unwind_ip = .; } . = ALIGN(2); .orc_unwind : AT(ADDR(.orc_unwind) - 0xffffffff80000000) { __start_orc_unwind = .; KEEP(*(.orc_unwind)) __stop_orc_unwind = .; } . = ALIGN(4); .orc_lookup : AT(ADDR(.orc_lookup) - 0xffffffff80000000) { orc_lookup = .; . += (((SIZEOF(.text) + (1 << 8) - 1) / (1 << 8)) + 1) * 4; orc_lookup_end = .; }
 . = ALIGN((1 << 12));
 __vvar_page = .;
 .vvar : AT(ADDR(.vvar) - 0xffffffff80000000) {
  __vvar_beginning_hack = .;
. = __vvar_beginning_hack + 128; *(.vvar_vsyscall_gtod_data)
  . = __vvar_beginning_hack + (1 << 12);
 } :data
 . = ALIGN(__vvar_page + (1 << 12), (1 << 12));
 . = ALIGN((1 << 12));
 .init.begin : AT(ADDR(.init.begin) - 0xffffffff80000000) {
  __init_begin = .;
 }
 __per_cpu_load = .; .data..percpu 0 : AT(__per_cpu_load - 0xffffffff80000000) { __per_cpu_start = .; *(.data..percpu..first) . = ALIGN((1 << 12)); *(.data..percpu..page_aligned) . = ALIGN((1 << 6)); *(.data..percpu..read_mostly) . = ALIGN((1 << 6)); *(.data..percpu) *(.data..percpu..shared_aligned) . = ALIGN((1 << 12)); *(.data..decrypted) *(.data..percpu..decrypted) . = ALIGN((1 << 12)); __per_cpu_end = .; } :percpu . = __per_cpu_load + SIZEOF(.data..percpu);
 ASSERT(SIZEOF(.data..percpu) < 0x1000000,
        "per-CPU data too large - increase CONFIG_PHYSICAL_START")
 . = ALIGN((1 << 12)); .init.text : AT(ADDR(.init.text) - 0xffffffff80000000) { _sinittext = .; *(.init.text .init.text.*) *(.text.startup) *(.meminit.text*) _einittext = .; }
 :init
 .altinstr_aux : AT(ADDR(.altinstr_aux) - 0xffffffff80000000) {
  *(.altinstr_aux)
 }
 .init.data : AT(ADDR(.init.data) - 0xffffffff80000000) { KEEP(*(SORT(___kentry+*))) *(.init.data init.data.*) *(.meminit.data*) . = ALIGN(8); __start_mcount_loc = .; KEEP(*(__mcount_loc)) __stop_mcount_loc = .; *(.init.rodata .init.rodata.*) . = ALIGN(8); __start_ftrace_events = .; KEEP(*(_ftrace_events)) __stop_ftrace_events = .; __start_ftrace_eval_maps = .; KEEP(*(_ftrace_eval_map)) __stop_ftrace_eval_maps = .; . = ALIGN(8); __start_syscalls_metadata = .; KEEP(*(__syscalls_metadata)) __stop_syscalls_metadata = .; *(.meminit.rodata) . = ALIGN(8); __clk_of_table = .; KEEP(*(__clk_of_table)) KEEP(*(__clk_of_table_end)) . = ALIGN(8); __cpu_method_of_table = .; KEEP(*(__cpu_method_of_table)) KEEP(*(__cpu_method_of_table_end)) . = ALIGN(8); __cpuidle_method_of_table = .; KEEP(*(__cpuidle_method_of_table)) KEEP(*(__cpuidle_method_of_table_end)) . = ALIGN(32); __dtb_start = .; KEEP(*(.dtb.init.rodata)) __dtb_end = .; . = ALIGN(8); __irqchip_acpi_probe_table = .; KEEP(*(__irqchip_acpi_probe_table)) __irqchip_acpi_probe_table_end = .; . = ALIGN(8); __timer_acpi_probe_table = .; KEEP(*(__timer_acpi_probe_table)) __timer_acpi_probe_table_end = .; . = ALIGN(8); __earlycon_table = .; KEEP(*(__earlycon_table)) __earlycon_table_end = .; . = ALIGN(16); __setup_start = .; KEEP(*(.init.setup)) __setup_end = .; __initcall_start = .; KEEP(*(.initcallearly.init)) __initcall0_start = .; KEEP(*(.initcall0.init)) KEEP(*(.initcall0s.init)) __initcall1_start = .; KEEP(*(.initcall1.init)) KEEP(*(.initcall1s.init)) __initcall2_start = .; KEEP(*(.initcall2.init)) KEEP(*(.initcall2s.init)) __initcall3_start = .; KEEP(*(.initcall3.init)) KEEP(*(.initcall3s.init)) __initcall4_start = .; KEEP(*(.initcall4.init)) KEEP(*(.initcall4s.init)) __initcall5_start = .; KEEP(*(.initcall5.init)) KEEP(*(.initcall5s.init)) __initcallrootfs_start = .; KEEP(*(.initcallrootfs.init)) KEEP(*(.initcallrootfss.init)) __initcall6_start = .; KEEP(*(.initcall6.init)) KEEP(*(.initcall6s.init)) __initcall7_start = .; KEEP(*(.initcall7.init)) KEEP(*(.initcall7s.init)) __initcall_end = .; __con_initcall_start = .; KEEP(*(.con_initcall.init)) __con_initcall_end = .; __security_initcall_start = .; KEEP(*(.security_initcall.init)) __security_initcall_end = .; . = ALIGN(4); __initramfs_start = .; KEEP(*(.init.ramfs)) . = ALIGN(8); KEEP(*(.init.ramfs.info)) }
 .x86_cpu_dev.init : AT(ADDR(.x86_cpu_dev.init) - 0xffffffff80000000) {
  __x86_cpu_dev_start = .;
  *(.x86_cpu_dev.init)
  __x86_cpu_dev_end = .;
 }
 . = ALIGN(8);
 .parainstructions : AT(ADDR(.parainstructions) - 0xffffffff80000000) {
  __parainstructions = .;
  *(.parainstructions)
  __parainstructions_end = .;
 }
 . = ALIGN(8);
 .altinstructions : AT(ADDR(.altinstructions) - 0xffffffff80000000) {
  __alt_instructions = .;
  *(.altinstructions)
  __alt_instructions_end = .;
 }
 .altinstr_replacement : AT(ADDR(.altinstr_replacement) - 0xffffffff80000000) {
  *(.altinstr_replacement)
 }
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
 .exit.text : AT(ADDR(.exit.text) - 0xffffffff80000000) {
  *(.exit.text) *(.text.exit) *(.memexit.text)
 }
 .exit.data : AT(ADDR(.exit.data) - 0xffffffff80000000) {
  *(.exit.data .exit.data.*) *(.fini_array .fini_array.*) *(.dtors .dtors.*) *(.memexit.data*) *(.memexit.rodata*)
 }
 . = ALIGN((1 << 12));
 .init.end : AT(ADDR(.init.end) - 0xffffffff80000000) {
  __init_end = .;
 }
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
 . = ALIGN((1 << 12));
 .bss : AT(ADDR(.bss) - 0xffffffff80000000) {
  __bss_start = .;
  *(.bss..page_aligned)
  . = ALIGN((1 << 12));
  *(.bss)
  . = ALIGN((1 << 21)); __start_bss_decrypted = .; *(.bss..decrypted); . = ALIGN((1 << 12)); __start_bss_decrypted_unused = .; . = ALIGN((1 << 21)); __end_bss_decrypted = .;
  . = ALIGN((1 << 12));
  __bss_stop = .;
 }
 . = ALIGN((1 << 12));
 .brk : AT(ADDR(.brk) - 0xffffffff80000000) {
  __brk_base = .;
  . += 64 * 1024;
  *(.brk_reservation)
  __brk_limit = .;
 }
 . = ALIGN((1 << 12));
 _end = .;
 .stab 0 : { *(.stab) } .stabstr 0 : { *(.stabstr) } .stab.excl 0 : { *(.stab.excl) } .stab.exclstr 0 : { *(.stab.exclstr) } .stab.index 0 : { *(.stab.index) } .stab.indexstr 0 : { *(.stab.indexstr) } .comment 0 : { *(.comment) }
 .debug 0 : { *(.debug) } .line 0 : { *(.line) } .debug_srcinfo 0 : { *(.debug_srcinfo) } .debug_sfnames 0 : { *(.debug_sfnames) } .debug_aranges 0 : { *(.debug_aranges) } .debug_pubnames 0 : { *(.debug_pubnames) } .debug_info 0 : { *(.debug_info .gnu.linkonce.wi.*) } .debug_abbrev 0 : { *(.debug_abbrev) } .debug_line 0 : { *(.debug_line) } .debug_frame 0 : { *(.debug_frame) } .debug_str 0 : { *(.debug_str) } .debug_loc 0 : { *(.debug_loc) } .debug_macinfo 0 : { *(.debug_macinfo) } .debug_pubtypes 0 : { *(.debug_pubtypes) } .debug_ranges 0 : { *(.debug_ranges) } .debug_weaknames 0 : { *(.debug_weaknames) } .debug_funcnames 0 : { *(.debug_funcnames) } .debug_typenames 0 : { *(.debug_typenames) } .debug_varnames 0 : { *(.debug_varnames) } .debug_gnu_pubnames 0 : { *(.debug_gnu_pubnames) } .debug_gnu_pubtypes 0 : { *(.debug_gnu_pubtypes) } .debug_types 0 : { *(.debug_types) } .debug_addr 0 : { *(.debug_addr) } .debug_line_str 0 : { *(.debug_line_str) } .debug_loclists 0 : { *(.debug_loclists) } .debug_macro 0 : { *(.debug_macro) } .debug_names 0 : { *(.debug_names) } .debug_rnglists 0 : { *(.debug_rnglists) } .debug_str_offsets 0 : { *(.debug_str_offsets) }
 /DISCARD/ : { *(.exit.text) *(.text.exit) *(.memexit.text) *(.exit.data .exit.data.*) *(.fini_array .fini_array.*) *(.dtors .dtors.*) *(.memexit.data*) *(.memexit.rodata*) *(.exitcall.exit) *(.discard) *(.discard.*) }
 /DISCARD/ : {
  *(.eh_frame)
 }
}
init_per_cpu__gdt_page = ABSOLUTE(gdt_page) + __per_cpu_load;
init_per_cpu__irq_stack_union = ABSOLUTE(irq_stack_union) + __per_cpu_load;
. = ASSERT((_end - _text <= (1024 * 1024 * 1024)),
    "kernel image bigger than KERNEL_IMAGE_SIZE");
. = ASSERT((irq_stack_union == 0),
           "irq_stack_union is not at start of per-cpu area");
