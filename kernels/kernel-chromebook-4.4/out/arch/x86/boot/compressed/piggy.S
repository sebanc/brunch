.section ".rodata..compressed","a",@progbits
.globl z_input_len
z_input_len = 6477396
.globl z_output_len
z_output_len = 20578244
.globl z_extract_offset
z_extract_offset = 0xd84000
.globl z_extract_offset_negative
z_extract_offset_negative = -0xd84000
.globl z_run_size
z_run_size = 20733952
.globl input_data, input_data_end
input_data:
.incbin "arch/x86/boot/compressed/vmlinux.bin.gz"
input_data_end:
