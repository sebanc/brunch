/*
 * arch/arm/mach-rochchip/embedded/call_with_mmu_disabled.S
 *
 * Copyright (c) 2014 Google, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This is based off of the call_with_stack procedure in
 * arch/arm/lib/call_with_stack.S
 */

#include <asm/assembler.h>
#include <linux/linkage.h>

/*
 * void call_with_mmu_disabled(void (*fn)(void *), void *arg, void *sp)
 *
 * Call the physical address fn, with the physical address, arg, as an argument,
 * on the physical address for the stack, sp.
 */
	.align	5
	.pushsection	.idmap.text, "ax"
ENTRY(call_with_mmu_disabled)
	instr_sync
	mrc	p15, 0, r3, c1, c0, 0
	bic	r3, r3, #0x1		@ disable mmu
	bic	r3, r3, #0x800		@ disable BP
	bic	r3, r3, #0x1000		@ disable I-cache
	mcr	p15, 0, r3, c1, c0, 0
	instr_sync
	str	sp, [r2, #-4]!
	str	lr, [r2, #-4]!

	mov	sp, r2
	mov	r2, r0
	mov	r0, r1

	badr	lr, 1f
	ret	r2
1:
	ldr lr, [sp]
	ldr	sp, [sp, #4]
	instr_sync
	mrc	p15, 0, r3, c1, c0, 0
	orr	r3, r3, #0x1		@ enable mmu
	orr	r3, r3, #0x800		@ enable BP
	orr r3, r3, #0x1000		@ enable I-cache
	mcr	p15, 0, r3, c1, c0, 0
	instr_sync
	ret	lr
ENDPROC(call_with_mmu_disabled)
	.popsection
