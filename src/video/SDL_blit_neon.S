/*
 * (C) Gražvydas "notaz" Ignotas, 2011,2012
 *
 * This work is licensed under the terms of any of these licenses
 * (at your option):
 *  - GNU GPL, version 2 or later.
 *  - GNU LGPL, version 2.1 or later.
 * See the COPYING file in the top-level directory.
 */

.text
.align 2

@ void *dst, const void *src, int count, uint abits
.macro do_argb bgr2rgb
    vdup.i8    d0, r3
0:
    vld4.8     {d4-d7}, [r1]!
.if \bgr2rgb
    vswp       d4, d6		@ BGR->RGB
.endif
    vmov.i8    d7, d0
    subs       r2, r2, #8
    blt        do_argb_finish
    vst4.8     {d4-d7}, [r0]!
    bxeq       lr
    nop
    b          0b
.endm

@ void *dst, const void *src, int count, uint global_alpha
.macro do_argb_alpha bgr2rgb global_alpha
    mov        r12, #0xff
.if \global_alpha
    vdup.16    q11, r3
.endif
    vdup.i16   q12, r12
0:
    pld        [r1, #64*2]
    pld        [r0, #64*2]
    vld4.8     {d4-d7}, [r1]!
    vld4.8     {d0-d3}, [r0]
.if \bgr2rgb
    vswp       d4, d6		@ BGR->RGB
.endif
.if !\global_alpha
    vmovl.u8   q11, d7
.endif
    @ d = (((s-d)*a+255)>>8)+d
    vsubl.u8   q8, d4, d0
    vsubl.u8   q9, d5, d1
    vsubl.u8   q10,d6, d2
    vmul.s16   q8, q8, q11
    vmul.s16   q9, q9, q11
    vmul.s16   q10,q10,q11
    vaddhn.i16 d4, q8, q12
    vaddhn.i16 d5, q9, q12
    vaddhn.i16 d6, q10,q12
    vadd.i8    q2, q0
    vadd.i8    d6, d2
    vmov.i8    d7, d3
    subs       r2, r2, #8
    blt        do_argb_finish
    vst4.8     {d4-d7}, [r0]!
    bxeq       lr
    nop
    b          0b
.endm


do_argb_finish:
    add        r2, r2, #8
    vzip.8     d4, d5		@ RRR..|GGG.. -> RGRG..
    vzip.8     d6, d7		@ BBB..|000.. -> B0B0..
    vzip.16    q2, q3
              
    vst1.32    d4[0], [r0]!
    cmp        r2, #1
    bxle       lr
    vst1.32    d4[1], [r0]!
    cmp        r2, #2
    bxle       lr
    vst1.32    d5[0], [r0]!
    cmp        r2, #3
    bxle       lr
    vst1.32    d5[1], [r0]!
    cmp        r2, #4
    bxle       lr
    vst1.32    d6[0], [r0]!
    cmp        r2, #5
    bxle       lr
    vst1.32    d6[1], [r0]!
    cmp        r2, #6
    bxle       lr
    vst1.32    d7[0], [r0]!
    bx         lr


.global neon_ARGBtoXRGB
neon_ARGBtoXRGB:
    do_argb 0

.global neon_ABGRtoXRGB
neon_ABGRtoXRGB:
    do_argb 1

.global neon_ARGBtoXRGBalpha
neon_ARGBtoXRGBalpha:
    do_argb_alpha 0, 0

.global neon_ABGRtoXRGBalpha
neon_ABGRtoXRGBalpha:
    do_argb_alpha 1, 0

.global neon_ARGBtoXRGBalphaS
neon_ARGBtoXRGBalphaS:
    do_argb_alpha 0, 1

.global neon_ABGRtoXRGBalphaS
neon_ABGRtoXRGBalphaS:
    do_argb_alpha 1, 1

@ vim:filetype=armasm
