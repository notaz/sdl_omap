@ vim:filetype=armasm

.global do_clut @ void *dest, void *src, unsigned short *pal, int count

do_clut:
    stmfd   sp!, {r4-r7,lr}
    mov     lr, #0xff
    mov     r3, r3, lsr #2
    mov     lr, lr, lsl #1
0:
    ldr     r7, [r1], #4
    subs    r3, r3, #1
    and     r4, lr, r7, lsl #1
    and     r5, lr, r7, lsr #7
    and     r6, lr, r7, lsr #15
    and     r7, lr, r7, lsr #23
    ldrh    r4, [r2, r4]
    ldrh    r5, [r2, r5]
    ldrh    r6, [r2, r6]
    ldrh    r7, [r2, r7]
    orr     r4, r4, r5, lsl #16
    orr     r6, r6, r7, lsl #16
    stmia   r0!, {r4,r6}
    bne     0b

    ldmfd   sp!, {r4-r7,pc}

