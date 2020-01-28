MEMORY
{
	pmu_sram_code : ORIGIN = 0, LENGTH = 0x3000
}

OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)

SECTIONS
{
	/* Don't need unwind tables */
	/DISCARD/ : {
		*(.ARM.exidx*)
		*(.ARM.extab*)
	}

	/* Kernel code finds init section because it knows they are first */
	.init : { *(.init*) } > pmu_sram_code
	. = ALIGN(4);

	.text : { *(.text*) } > pmu_sram_code
	. = ALIGN(4);

	.rodata : { *(SORT_BY_ALIGNMENT(.rodata*)) } > pmu_sram_code
	. = ALIGN(4);

	.data : {
		*(SORT_BY_ALIGNMENT(.data*))
		. = ALIGN(4);

		/* We purposely put bss as part of data to avoid initting */
		*(SORT_BY_ALIGNMENT(.bss*))
		. = ALIGN(4);
	} > pmu_sram_code
}
