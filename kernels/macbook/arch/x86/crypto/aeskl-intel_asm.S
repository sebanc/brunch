/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Implement AES algorithm using Intel AES Key Locker instructions.
 *
 * Most code is based from the AES-NI implementation, aesni-intel_asm.S
 *
 */

#include <linux/linkage.h>
#include <asm/inst.h>
#include <asm/frame.h>
#include "aes-intel_asm.S"

.text

#define STATE1	%xmm0
#define STATE2	%xmm1
#define STATE3	%xmm2
#define STATE4	%xmm3
#define STATE5	%xmm4
#define STATE6	%xmm5
#define STATE7	%xmm6
#define STATE8	%xmm7
#define STATE	STATE1

#define IV	%xmm9
#define KEY	%xmm10
#define BSWAP_MASK %xmm11
#define CTR	%xmm12
#define INC	%xmm13

#ifdef __x86_64__
#define IN1	%xmm8
#define IN2	%xmm9
#define IN3	%xmm10
#define IN4	%xmm11
#define IN5	%xmm12
#define IN6	%xmm13
#define IN7	%xmm14
#define IN8	%xmm15
#define IN	IN1
#define TCTR_LOW %r11
#else
#define IN	%xmm1
#endif

#ifdef __x86_64__
#define AREG	%rax
#define HANDLEP	%rdi
#define OUTP	%rsi
#define KLEN	%r9d
#define INP	%rdx
#define T1	%r10
#define LEN	%rcx
#define IVP	%r8
#else
#define AREG	%eax
#define HANDLEP	%edi
#define OUTP	AREG
#define KLEN	%ebx
#define INP	%edx
#define T1	%ecx
#define LEN	%esi
#define IVP	%ebp
#endif

#define UKEYP	OUTP
#define GF128MUL_MASK %xmm11

/*
 * int aeskl_setkey(struct crypto_aes_ctx *ctx, const u8 *in_key, unsigned int key_len)
 */
SYM_FUNC_START(aeskl_setkey)
	FRAME_BEGIN
#ifndef __x86_64__
	push HANDLEP
	movl (FRAME_OFFSET+8)(%esp),  HANDLEP	# ctx
	movl (FRAME_OFFSET+12)(%esp), UKEYP	# in_key
	movl (FRAME_OFFSET+16)(%esp), %edx	# key_len
#endif
	movl %edx, 480(HANDLEP)
	movdqu (UKEYP), STATE1
	mov $1, %eax
	cmp $16, %dl
	je .Lsetkey_128

	movdqu 0x10(UKEYP), STATE2
	encodekey256 %eax, %eax
	movdqu STATE4, 0x30(HANDLEP)
	jmp .Lsetkey_end
.Lsetkey_128:
	encodekey128 %eax, %eax

.Lsetkey_end:
	movdqu STATE1, (HANDLEP)
	movdqu STATE2, 0x10(HANDLEP)
	movdqu STATE3, 0x20(HANDLEP)

	xor AREG, AREG
#ifndef __x86_64__
	popl HANDLEP
#endif
	FRAME_END
	ret
SYM_FUNC_END(aeskl_setkey)

/*
 * int _aeskl_enc(const void *ctx, u8 *dst, const u8 *src)
 */
SYM_FUNC_START(_aeskl_enc)
	FRAME_BEGIN
#ifndef __x86_64__
	pushl HANDLEP
	pushl KLEN
	movl (FRAME_OFFSET+12)(%esp), HANDLEP	# ctx
	movl (FRAME_OFFSET+16)(%esp), OUTP	# dst
	movl (FRAME_OFFSET+20)(%esp), INP	# src
#endif
	movdqu (INP), STATE
	movl 480(HANDLEP), KLEN

	cmp $16, KLEN
	je .Lenc_128
	aesenc256kl (HANDLEP), STATE
	jz .Lenc_err
	jmp .Lenc_noerr
.Lenc_128:
	aesenc128kl (HANDLEP), STATE
	jz .Lenc_err

.Lenc_noerr:
	xor AREG, AREG
	jmp .Lenc_end
.Lenc_err:
	mov $1, AREG
.Lenc_end:
	movdqu STATE, (OUTP)
#ifndef __x86_64__
	popl KLEN
	popl HANDLEP
#endif
	FRAME_END
	ret
SYM_FUNC_END(_aeskl_enc)

/*
 * int _aeskl_dec(const void *ctx, u8 *dst, const u8 *src)
 */
SYM_FUNC_START(_aeskl_dec)
	FRAME_BEGIN
#ifndef __x86_64__
	pushl HANDLEP
	pushl KLEN
	movl (FRAME_OFFSET+12)(%esp), HANDLEP	# ctx
	movl (FRAME_OFFSET+16)(%esp), OUTP	# dst
	movl (FRAME_OFFSET+20)(%esp), INP	# src
#endif
	movdqu (INP), STATE
	mov 480(HANDLEP), KLEN

	cmp $16, KLEN
	je .Ldec_128
	aesdec256kl (HANDLEP), STATE
	jz .Ldec_err
	jmp .Ldec_noerr
.Ldec_128:
	aesdec128kl (HANDLEP), STATE
	jz .Ldec_err

.Ldec_noerr:
	xor AREG, AREG
	jmp .Ldec_end
.Ldec_err:
	mov $1, AREG
.Ldec_end:
	movdqu STATE, (OUTP)
#ifndef __x86_64__
	popl KLEN
	popl HANDLEP
#endif
	FRAME_END
	ret
SYM_FUNC_END(_aeskl_dec)

#ifdef __x86_64__

/*
 * XTS implementation
 */

/*
 * _aeskl_gf128mul_x_ble: 	internal ABI
 *	Multiply in GF(2^128) for XTS IVs
 * input:
 *	IV:	current IV
 *	GF128MUL_MASK == mask with 0x87 and 0x01
 * output:
 *	IV:	next IV
 * changed:
 *	CTR:	== temporary value
 */
#define _aeskl_gf128mul_x_ble() \
	pshufd $0x13, IV, KEY; \
	paddq IV, IV; \
	psrad $31, KEY; \
	pand GF128MUL_MASK, KEY; \
	pxor KEY, IV;

/*
 * int _aeskl_xts_encrypt(const struct crypto_aes_ctx *ctx, u8 *dst,
 *			  const u8 *src, unsigned int len, le128 *iv)
 */
SYM_FUNC_START(_aeskl_xts_encrypt)
	FRAME_BEGIN
	movdqa .Lgf128mul_x_ble_mask(%rip), GF128MUL_MASK
	movups (IVP), IV

	mov 480(HANDLEP), KLEN

.Lxts_enc8:
	sub $128, LEN
	jl .Lxts_enc1_pre

	movdqa IV, STATE1
	movdqu (INP), INC
	pxor INC, STATE1
	movdqu IV, (OUTP)

	_aeskl_gf128mul_x_ble()
	movdqa IV, STATE2
	movdqu 0x10(INP), INC
	pxor INC, STATE2
	movdqu IV, 0x10(OUTP)

	_aeskl_gf128mul_x_ble()
	movdqa IV, STATE3
	movdqu 0x20(INP), INC
	pxor INC, STATE3
	movdqu IV, 0x20(OUTP)

	_aeskl_gf128mul_x_ble()
	movdqa IV, STATE4
	movdqu 0x30(INP), INC
	pxor INC, STATE4
	movdqu IV, 0x30(OUTP)

	_aeskl_gf128mul_x_ble()
	movdqa IV, STATE5
	movdqu 0x40(INP), INC
	pxor INC, STATE5
	movdqu IV, 0x40(OUTP)

	_aeskl_gf128mul_x_ble()
	movdqa IV, STATE6
	movdqu 0x50(INP), INC
	pxor INC, STATE6
	movdqu IV, 0x50(OUTP)

	_aeskl_gf128mul_x_ble()
	movdqa IV, STATE7
	movdqu 0x60(INP), INC
	pxor INC, STATE7
	movdqu IV, 0x60(OUTP)

	_aeskl_gf128mul_x_ble()
	movdqa IV, STATE8
	movdqu 0x70(INP), INC
	pxor INC, STATE8
	movdqu IV, 0x70(OUTP)

	cmp $16, KLEN
	je .Lxts_enc8_128
	aesencwide256kl (%rdi)
	jz .Lxts_enc_ret_err
	jmp .Lxts_enc8_end
.Lxts_enc8_128:
	aesencwide128kl (%rdi)
	jz .Lxts_enc_ret_err

.Lxts_enc8_end:
	movdqu 0x00(OUTP), INC
	pxor INC, STATE1
	movdqu STATE1, 0x00(OUTP)

	movdqu 0x10(OUTP), INC
	pxor INC, STATE2
	movdqu STATE2, 0x10(OUTP)

	movdqu 0x20(OUTP), INC
	pxor INC, STATE3
	movdqu STATE3, 0x20(OUTP)

	movdqu 0x30(OUTP), INC
	pxor INC, STATE4
	movdqu STATE4, 0x30(OUTP)

	movdqu 0x40(OUTP), INC
	pxor INC, STATE5
	movdqu STATE5, 0x40(OUTP)

	movdqu 0x50(OUTP), INC
	pxor INC, STATE6
	movdqu STATE6, 0x50(OUTP)

	movdqu 0x60(OUTP), INC
	pxor INC, STATE7
	movdqu STATE7, 0x60(OUTP)

	movdqu 0x70(OUTP), INC
	pxor INC, STATE8
	movdqu STATE8, 0x70(OUTP)

	_aeskl_gf128mul_x_ble()

	add $128, INP
	add $128, OUTP
	test LEN, LEN
	jnz .Lxts_enc8

.Lxts_enc_ret_iv:
	movups IV, (IVP)
.Lxts_enc_ret_noerr:
	xor AREG, AREG
	jmp .Lxts_enc_ret
.Lxts_enc_ret_err:
	mov $1, AREG
.Lxts_enc_ret:
	FRAME_END
	ret

.Lxts_enc1_pre:
	add $128, LEN
	jz .Lxts_enc_ret_iv
	sub $16, LEN
	jl .Lxts_enc_cts4

.Lxts_enc1:
	movdqu (INP), STATE1
	pxor IV, STATE1

	cmp $16, KLEN
	je .Lxts_enc1_128
	aesenc256kl (HANDLEP), STATE1
	jz .Lxts_enc_ret_err
	jmp .Lxts_enc1_end
.Lxts_enc1_128:
	aesenc128kl (HANDLEP), STATE1
	jz .Lxts_enc_ret_err

.Lxts_enc1_end:
	pxor IV, STATE1
	_aeskl_gf128mul_x_ble()

	test LEN, LEN
	jz .Lxts_enc1_out

	add $16, INP
	sub $16, LEN
	jl .Lxts_enc_cts1

	movdqu STATE1, (OUTP)
	add $16, OUTP
	jmp .Lxts_enc1

.Lxts_enc1_out:
	movdqu STATE1, (OUTP)
	jmp .Lxts_enc_ret_iv

.Lxts_enc_cts4:
	movdqu STATE8, STATE1
	sub $16, OUTP

.Lxts_enc_cts1:
	lea .Lcts_permute_table(%rip), T1
	add LEN, INP		/* rewind input pointer */
	add $16, LEN		/* # bytes in final block */
	movups (INP), IN1

	mov T1, IVP
	add $32, IVP
	add LEN, T1
	sub LEN, IVP
	add OUTP, LEN

	movups (T1), STATE2
	movaps STATE1, STATE3
	pshufb STATE2, STATE1
	movups STATE1, (LEN)

	movups (IVP), STATE1
	pshufb STATE1, IN1
	pblendvb STATE3, IN1
	movaps IN1, STATE1

	pxor IV, STATE1

	cmp $16, KLEN
	je .Lxts_enc1_cts_128
	aesenc256kl (HANDLEP), STATE1
	jz .Lxts_enc_ret_err
	jmp .Lxts_enc1_cts_end
.Lxts_enc1_cts_128:
	aesenc128kl (HANDLEP), STATE1
	jz .Lxts_enc_ret_err

.Lxts_enc1_cts_end:
	pxor IV, STATE1
	movups STATE1, (OUTP)
	jmp .Lxts_enc_ret_noerr
SYM_FUNC_END(_aeskl_xts_encrypt)

/*
 * int _aeskl_xts_decrypt(const struct crypto_aes_ctx *ctx, u8 *dst,
 *			  const u8 *src, unsigned int len, le128 *iv)
 */
SYM_FUNC_START(_aeskl_xts_decrypt)
	FRAME_BEGIN
	movdqa .Lgf128mul_x_ble_mask(%rip), GF128MUL_MASK
	movups (IVP), IV

	mov 480(HANDLEP), KLEN

	test $15, LEN
	jz .Lxts_dec8
	sub $16, LEN

.Lxts_dec8:
	sub $128, LEN
	jl .Lxts_dec1_pre

	movdqa IV, STATE1
	movdqu (INP), INC
	pxor INC, STATE1
	movdqu IV, (OUTP)

	_aeskl_gf128mul_x_ble()
	movdqa IV, STATE2
	movdqu 0x10(INP), INC
	pxor INC, STATE2
	movdqu IV, 0x10(OUTP)

	_aeskl_gf128mul_x_ble()
	movdqa IV, STATE3
	movdqu 0x20(INP), INC
	pxor INC, STATE3
	movdqu IV, 0x20(OUTP)

	_aeskl_gf128mul_x_ble()
	movdqa IV, STATE4
	movdqu 0x30(INP), INC
	pxor INC, STATE4
	movdqu IV, 0x30(OUTP)

	_aeskl_gf128mul_x_ble()
	movdqa IV, STATE5
	movdqu 0x40(INP), INC
	pxor INC, STATE5
	movdqu IV, 0x40(OUTP)

	_aeskl_gf128mul_x_ble()
	movdqa IV, STATE6
	movdqu 0x50(INP), INC
	pxor INC, STATE6
	movdqu IV, 0x50(OUTP)

	_aeskl_gf128mul_x_ble()
	movdqa IV, STATE7
	movdqu 0x60(INP), INC
	pxor INC, STATE7
	movdqu IV, 0x60(OUTP)

	_aeskl_gf128mul_x_ble()
	movdqa IV, STATE8
	movdqu 0x70(INP), INC
	pxor INC, STATE8
	movdqu IV, 0x70(OUTP)

	cmp $16, KLEN
	je .Lxts_dec8_128
	aesdecwide256kl (%rdi)
	jz .Lxts_dec_ret_err
	jmp .Lxts_dec8_end
.Lxts_dec8_128:
	aesdecwide128kl (%rdi)
	jz .Lxts_dec_ret_err

.Lxts_dec8_end:
	movdqu 0x00(OUTP), INC
	pxor INC, STATE1
	movdqu STATE1, 0x00(OUTP)

	movdqu 0x10(OUTP), INC
	pxor INC, STATE2
	movdqu STATE2, 0x10(OUTP)

	movdqu 0x20(OUTP), INC
	pxor INC, STATE3
	movdqu STATE3, 0x20(OUTP)

	movdqu 0x30(OUTP), INC
	pxor INC, STATE4
	movdqu STATE4, 0x30(OUTP)

	movdqu 0x40(OUTP), INC
	pxor INC, STATE5
	movdqu STATE5, 0x40(OUTP)

	movdqu 0x50(OUTP), INC
	pxor INC, STATE6
	movdqu STATE6, 0x50(OUTP)

	movdqu 0x60(OUTP), INC
	pxor INC, STATE7
	movdqu STATE7, 0x60(OUTP)

	movdqu 0x70(OUTP), INC
	pxor INC, STATE8
	movdqu STATE8, 0x70(OUTP)

	_aeskl_gf128mul_x_ble()

	add $128, INP
	add $128, OUTP
	test LEN, LEN
	jnz .Lxts_dec8

.Lxts_dec_ret_iv:
	movups IV, (IVP)
.Lxts_dec_ret_noerr:
	xor AREG, AREG
	jmp .Lxts_dec_ret
.Lxts_dec_ret_err:
	mov $1, AREG
.Lxts_dec_ret:
	FRAME_END
	ret

.Lxts_dec1_pre:
	add $128, LEN
	jz .Lxts_dec_ret_iv

.Lxts_dec1:
	movdqu (INP), STATE1

	add $16, INP
	sub $16, LEN
	jl .Lxts_dec_cts1

	pxor IV, STATE1

	cmp $16, KLEN
	je .Lxts_dec1_128
	aesdec256kl (HANDLEP), STATE1
	jz .Lxts_dec_ret_err
	jmp .Lxts_dec1_end
.Lxts_dec1_128:
	aesdec128kl (HANDLEP), STATE1
	jz .Lxts_dec_ret_err

.Lxts_dec1_end:
	pxor IV, STATE1
	_aeskl_gf128mul_x_ble()

	test LEN, LEN
	jz .Lxts_dec1_out

	movdqu STATE1, (OUTP)
	add $16, OUTP
	jmp .Lxts_dec1

.Lxts_dec1_out:
	movdqu STATE1, (OUTP)
	jmp .Lxts_dec_ret_iv

.Lxts_dec_cts1:
	movdqa IV, STATE5
	_aeskl_gf128mul_x_ble()

	pxor IV, STATE1

	cmp $16, KLEN
	je .Lxts_dec1_cts_pre_128
	aesdec256kl (HANDLEP), STATE1
	jz .Lxts_dec_ret_err
	jmp .Lxts_dec1_cts_pre_end
.Lxts_dec1_cts_pre_128:
	aesdec128kl (HANDLEP), STATE1
	jz .Lxts_dec_ret_err

.Lxts_dec1_cts_pre_end:
	pxor IV, STATE1

	lea .Lcts_permute_table(%rip), T1
	add LEN, INP		/* rewind input pointer */
	add $16, LEN		/* # bytes in final block */
	movups (INP), IN1

	mov T1, IVP
	add $32, IVP
	add LEN, T1
	sub LEN, IVP
	add OUTP, LEN

	movups (T1), STATE2
	movaps STATE1, STATE3
	pshufb STATE2, STATE1
	movups STATE1, (LEN)

	movups (IVP), STATE1
	pshufb STATE1, IN1
	pblendvb STATE3, IN1
	movaps IN1, STATE1

	pxor STATE5, STATE1

	cmp $16, KLEN
	je .Lxts_dec1_cts_128
	aesdec256kl (HANDLEP), STATE1
	jz .Lxts_dec_ret_err
	jmp .Lxts_dec1_cts_end
.Lxts_dec1_cts_128:
	aesdec128kl (HANDLEP), STATE1
	jz .Lxts_dec_ret_err

.Lxts_dec1_cts_end:
	pxor STATE5, STATE1

	movups STATE1, (OUTP)
	jmp .Lxts_dec_ret_noerr

SYM_FUNC_END(_aeskl_xts_decrypt)

#endif
