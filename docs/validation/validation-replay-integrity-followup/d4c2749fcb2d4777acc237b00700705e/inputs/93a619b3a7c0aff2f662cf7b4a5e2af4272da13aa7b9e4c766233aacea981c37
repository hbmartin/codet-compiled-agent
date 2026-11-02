/* DOCUMENTARY REGISTER-STATE C, NOT C-CALLABLE APPLICATION FUNCTIONS.
 * PPC32 is an editorial machine state; LOAD32/STORE32 are big-endian loads/stores.
 * TRANSFER preserves register/stack arguments and transfers control without adding
 * a C call frame. CALL resumes after the actual machine callee returns.
 * These primitives are semantic notation, deliberately not implementations that
 * could execute imported library calls. The exact assembly remains primary.
 */
typedef struct { unsigned int r[32], lr, ctr, cr, pc; } PPC32;

/* start @ 0x000026dc: Initial SP supplies argc, argv and envp; stack aligns to32, then __start; trap if it returns. */
void ABI_000026dc(PPC32 *s)
{
    s->r[26] = s->r[1];
    s->r[1] = (s->r[1] - 4) & 0xffffffe0U;
    s->r[0] = 0; STORE32(s->r[1], 0);
    STORE32(s->r[1] - 64, s->r[1]); s->r[1] -= 64;
    s->r[3] = LOAD32(s->r[26]); s->r[4] = s->r[26] + 4;
    s->r[27] = (s->r[3] + 1) << 2; s->r[5] = s->r[4] + s->r[27];
    s->r[11] = 0x271c; s->ctr = s->r[11]; s->lr = 0x2718;
    CALL(s->ctr, s); TRAP();
}

/* candidate_00002890 @ 0x00002890: PIC tail transfer to _exit (0x0002cfc0); r3-r10 arguments and original LR survive. */
void ABI_00002890(PPC32 *s)
{
    s->r[0] = s->lr; s->lr = 0x00002898;
    s->r[12] = 0x0002cfc0; s->lr = s->r[0];
    s->ctr = s->r[12]; TRANSFER(s->ctr, s);
}

/* candidate_000028b0 @ 0x000028b0: PIC tail transfer to _main (0x00005498); r3-r10 arguments and original LR survive. */
void ABI_000028b0(PPC32 *s)
{
    s->r[0] = s->lr; s->lr = 0x000028b8;
    s->r[12] = 0x00005498; s->lr = s->r[0];
    s->ctr = s->r[12]; TRANSFER(s->ctr, s);
}

/* candidate_000028d0 @ 0x000028d0: PIC tail transfer to _atexit (0x0002d000); r3-r10 arguments and original LR survive. */
void ABI_000028d0(PPC32 *s)
{
    s->r[0] = s->lr; s->lr = 0x000028d8;
    s->r[12] = 0x0002d000; s->lr = s->r[0];
    s->ctr = s->r[12]; TRANSFER(s->ctr, s);
}

/* candidate_000028f0 @ 0x000028f0: PIC tail transfer to __dyld_func_lookup (0x00002ca4); r3-r10 arguments and original LR survive. */
void ABI_000028f0(PPC32 *s)
{
    s->r[0] = s->lr; s->lr = 0x000028f8;
    s->r[12] = 0x00002ca4; s->lr = s->r[0];
    s->ctr = s->r[12]; TRANSFER(s->ctr, s);
}

/* candidate_00002910 @ 0x00002910: PIC tail transfer to __call_objcInit (0x00002a90); r3-r10 arguments and original LR survive. */
void ABI_00002910(PPC32 *s)
{
    s->r[0] = s->lr; s->lr = 0x00002918;
    s->r[12] = 0x00002a90; s->lr = s->r[0];
    s->ctr = s->r[12]; TRANSFER(s->ctr, s);
}

/* candidate_00002930 @ 0x00002930: PIC tail transfer to __call_mod_init_funcs (0x00002990); r3-r10 arguments and original LR survive. */
void ABI_00002930(PPC32 *s)
{
    s->r[0] = s->lr; s->lr = 0x00002938;
    s->r[12] = 0x00002990; s->lr = s->r[0];
    s->ctr = s->r[12]; TRANSFER(s->ctr, s);
}

/* candidate_00002950 @ 0x00002950: PIC tail transfer to ___keymgr_dwarf2_register_sections (0x0002cfe0); r3-r10 arguments and original LR survive. */
void ABI_00002950(PPC32 *s)
{
    s->r[0] = s->lr; s->lr = 0x00002958;
    s->r[12] = 0x0002cfe0; s->lr = s->r[0];
    s->ctr = s->r[12]; TRANSFER(s->ctr, s);
}

/* candidate_00002970 @ 0x00002970: PIC tail transfer to __dyld_init_check (0x00002c4c); r3-r10 arguments and original LR survive. */
void ABI_00002970(PPC32 *s)
{
    s->r[0] = s->lr; s->lr = 0x00002978;
    s->r[12] = 0x00002c4c; s->lr = s->r[0];
    s->ctr = s->r[12]; TRANSFER(s->ctr, s);
}

/* candidate_000029d8 @ 0x000029d8: PIC tail transfer to __dyld_func_lookup (0x00002ca4); r3-r10 arguments and original LR survive. */
void ABI_000029d8(PPC32 *s)
{
    s->r[0] = s->lr; s->lr = 0x000029e0;
    s->r[12] = 0x00002ca4; s->lr = s->r[0];
    s->ctr = s->r[12]; TRANSFER(s->ctr, s);
}

/* candidate_00002bec @ 0x00002bec: PIC tail transfer to _crt_strbeginswith (0x00002a34); r3-r10 arguments and original LR survive. */
void ABI_00002bec(PPC32 *s)
{
    s->r[0] = s->lr; s->lr = 0x00002bf4;
    s->r[12] = 0x00002a34; s->lr = s->r[0];
    s->ctr = s->r[12]; TRANSFER(s->ctr, s);
}

/* candidate_00002c0c @ 0x00002c0c: PIC tail transfer to _crt_basename (0x000029f8); r3-r10 arguments and original LR survive. */
void ABI_00002c0c(PPC32 *s)
{
    s->r[0] = s->lr; s->lr = 0x00002c14;
    s->r[12] = 0x000029f8; s->lr = s->r[0];
    s->ctr = s->r[12]; TRANSFER(s->ctr, s);
}

/* candidate_00002c2c @ 0x00002c2c: PIC tail transfer to __dyld_func_lookup (0x00002ca4); r3-r10 arguments and original LR survive. */
void ABI_00002c2c(PPC32 *s)
{
    s->r[0] = s->lr; s->lr = 0x00002c34;
    s->r[12] = 0x00002ca4; s->lr = s->r[0];
    s->ctr = s->r[12]; TRANSFER(s->ctr, s);
}

/* dyld_stub_binding_helper @ 0x00002c8c: Dynamic loader binder pointer from __dyld+0; r12 receives Mach-O header VA0x1000. */
void ABI_00002c8c(PPC32 *s)
{
    s->r[12] = 0x40000; s->r[0] = LOAD32(0x39948);
    s->ctr = s->r[0]; s->r[12] = 0x1000; TRANSFER(s->ctr, s);
}

/* __dyld_func_lookup @ 0x00002ca4: Dynamic loader lookup pointer from __dyld+4; lookup arguments remain in r3/r4. */
void ABI_00002ca4(PPC32 *s)
{
    s->r[11] = 0x40000; s->r[11] = LOAD32(0x3994c);
    s->ctr = s->r[11]; TRANSFER(s->ctr, s);
}

/* candidate_00002d3c @ 0x00002d3c: PIC tail transfer to __keymgr_set_and_unlock_processwide_ptr (0x0002d0e0); r3-r10 arguments and original LR survive. */
void ABI_00002d3c(PPC32 *s)
{
    s->r[0] = s->lr; s->lr = 0x00002d44;
    s->r[12] = 0x0002d0e0; s->lr = s->r[0];
    s->ctr = s->r[12]; TRANSFER(s->ctr, s);
}

/* candidate_00002d5c @ 0x00002d5c: PIC tail transfer to __keymgr_get_and_lock_processwide_ptr (0x0002d080); r3-r10 arguments and original LR survive. */
void ABI_00002d5c(PPC32 *s)
{
    s->r[0] = s->lr; s->lr = 0x00002d64;
    s->r[12] = 0x0002d080; s->lr = s->r[0];
    s->ctr = s->r[12]; TRANSFER(s->ctr, s);
}

/* candidate_00002d7c @ 0x00002d7c: PIC tail transfer to _calloc (0x0002d100); r3-r10 arguments and original LR survive. */
void ABI_00002d7c(PPC32 *s)
{
    s->r[0] = s->lr; s->lr = 0x00002d84;
    s->r[12] = 0x0002d100; s->lr = s->r[0];
    s->ctr = s->r[12]; TRANSFER(s->ctr, s);
}

/* candidate_00002ebc @ 0x00002ebc: PIC tail transfer to _free (0x0002d0c0); r3-r10 arguments and original LR survive. */
void ABI_00002ebc(PPC32 *s)
{
    s->r[0] = s->lr; s->lr = 0x00002ec4;
    s->r[12] = 0x0002d0c0; s->lr = s->r[0];
    s->ctr = s->r[12]; TRANSFER(s->ctr, s);
}

/* candidate_00002edc @ 0x00002edc: PIC tail transfer to __keymgr_set_and_unlock_processwide_ptr (0x0002d0e0); r3-r10 arguments and original LR survive. */
void ABI_00002edc(PPC32 *s)
{
    s->r[0] = s->lr; s->lr = 0x00002ee4;
    s->r[12] = 0x0002d0e0; s->lr = s->r[0];
    s->ctr = s->r[12]; TRANSFER(s->ctr, s);
}

/* candidate_00002efc @ 0x00002efc: PIC tail transfer to _abort (0x0002d020); r3-r10 arguments and original LR survive. */
void ABI_00002efc(PPC32 *s)
{
    s->r[0] = s->lr; s->lr = 0x00002f04;
    s->r[12] = 0x0002d020; s->lr = s->r[0];
    s->ctr = s->r[12]; TRANSFER(s->ctr, s);
}

/* candidate_00002f1c @ 0x00002f1c: PIC tail transfer to __keymgr_get_and_lock_processwide_ptr (0x0002d080); r3-r10 arguments and original LR survive. */
void ABI_00002f1c(PPC32 *s)
{
    s->r[0] = s->lr; s->lr = 0x00002f24;
    s->r[12] = 0x0002d080; s->lr = s->r[0];
    s->ctr = s->r[12]; TRANSFER(s->ctr, s);
}

/* candidate_00002fb8 @ 0x00002fb8: PIC tail transfer to __dyld_register_func_for_remove_image (0x0002d060); r3-r10 arguments and original LR survive. */
void ABI_00002fb8(PPC32 *s)
{
    s->r[0] = s->lr; s->lr = 0x00002fc0;
    s->r[12] = 0x0002d060; s->lr = s->r[0];
    s->ctr = s->r[12]; TRANSFER(s->ctr, s);
}

/* candidate_00002fd8 @ 0x00002fd8: PIC tail transfer to __dyld_register_func_for_add_image (0x0002d040); r3-r10 arguments and original LR survive. */
void ABI_00002fd8(PPC32 *s)
{
    s->r[0] = s->lr; s->lr = 0x00002fe0;
    s->r[12] = 0x0002d040; s->lr = s->r[0];
    s->ctr = s->r[12]; TRANSFER(s->ctr, s);
}

/* candidate_00002ff8 @ 0x00002ff8: PIC tail transfer to __init_keymgr (0x0002d0a0); r3-r10 arguments and original LR survive. */
void ABI_00002ff8(PPC32 *s)
{
    s->r[0] = s->lr; s->lr = 0x00003000;
    s->r[12] = 0x0002d0a0; s->lr = s->r[0];
    s->ctr = s->r[12]; TRANSFER(s->ctr, s);
}

/* _fprintf$LDBLStub @ 0x0002ca60: Cached Darwin stdio binding; slot 0x0030c944, name string 0x00036870. Spills/reloads r3-r10; no FPR-save instructions. Resolver prefers $LDBL128 then the base libc symbol. */
void ABI_0002ca60(PPC32 *s)
{
    s->r[0] = s->lr; s->lr = 0x0002ca68; s->r[2] = s->lr; s->lr = s->r[0];
    s->r[12] = LOAD32(0x0030c944); SET_CR7_CMP_SIGNED(s->r[12], 0);
    if (s->r[12] == 0) {
        STORE32(s->r[1]-8,s->r[30]); STORE32(s->r[1]-4,s->r[31]);
        STORE32(s->r[1]+8,s->r[0]); STORE32(s->r[1]-80,s->r[1]);
        s->r[1]-=80; s->r[30]=s->r[1]; s->r[31]=s->r[2];
        for (int reg=3; reg<=10; ++reg) STORE32(s->r[30]+104+4*(reg-3),s->r[reg]);
        s->r[3]=0x00036870; s->lr=0x0002cac4; CALL(0x2cf14,s);
        s->r[12]=s->r[3]; s->r[2]=0x0030c944; STORE32(s->r[2],s->r[12]);
        for (int reg=3; reg<=10; ++reg) s->r[reg]=LOAD32(s->r[30]+104+4*(reg-3));
        s->r[1]=LOAD32(s->r[1]); s->r[0]=LOAD32(s->r[1]+8); s->lr=s->r[0];
        s->r[30]=LOAD32(s->r[1]-8); s->r[31]=LOAD32(s->r[1]-4);
    }
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _printf$LDBLStub @ 0x0002cb0c: Cached Darwin stdio binding; slot 0x0030c948, name string 0x00036878. Spills/reloads r3-r10; no FPR-save instructions. Resolver prefers $LDBL128 then the base libc symbol. */
void ABI_0002cb0c(PPC32 *s)
{
    s->r[0] = s->lr; s->lr = 0x0002cb14; s->r[2] = s->lr; s->lr = s->r[0];
    s->r[12] = LOAD32(0x0030c948); SET_CR7_CMP_SIGNED(s->r[12], 0);
    if (s->r[12] == 0) {
        STORE32(s->r[1]-8,s->r[30]); STORE32(s->r[1]-4,s->r[31]);
        STORE32(s->r[1]+8,s->r[0]); STORE32(s->r[1]-80,s->r[1]);
        s->r[1]-=80; s->r[30]=s->r[1]; s->r[31]=s->r[2];
        for (int reg=3; reg<=10; ++reg) STORE32(s->r[30]+104+4*(reg-3),s->r[reg]);
        s->r[3]=0x00036878; s->lr=0x0002cb70; CALL(0x2cf14,s);
        s->r[12]=s->r[3]; s->r[2]=0x0030c948; STORE32(s->r[2],s->r[12]);
        for (int reg=3; reg<=10; ++reg) s->r[reg]=LOAD32(s->r[30]+104+4*(reg-3));
        s->r[1]=LOAD32(s->r[1]); s->r[0]=LOAD32(s->r[1]+8); s->lr=s->r[0];
        s->r[30]=LOAD32(s->r[1]-8); s->r[31]=LOAD32(s->r[1]-4);
    }
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _snprintf$LDBLStub @ 0x0002cbb8: Cached Darwin stdio binding; slot 0x0030c94c, name string 0x00036880. Spills/reloads r3-r10; no FPR-save instructions. Resolver prefers $LDBL128 then the base libc symbol. */
void ABI_0002cbb8(PPC32 *s)
{
    s->r[0] = s->lr; s->lr = 0x0002cbc0; s->r[2] = s->lr; s->lr = s->r[0];
    s->r[12] = LOAD32(0x0030c94c); SET_CR7_CMP_SIGNED(s->r[12], 0);
    if (s->r[12] == 0) {
        STORE32(s->r[1]-8,s->r[30]); STORE32(s->r[1]-4,s->r[31]);
        STORE32(s->r[1]+8,s->r[0]); STORE32(s->r[1]-80,s->r[1]);
        s->r[1]-=80; s->r[30]=s->r[1]; s->r[31]=s->r[2];
        for (int reg=3; reg<=10; ++reg) STORE32(s->r[30]+104+4*(reg-3),s->r[reg]);
        s->r[3]=0x00036880; s->lr=0x0002cc1c; CALL(0x2cf14,s);
        s->r[12]=s->r[3]; s->r[2]=0x0030c94c; STORE32(s->r[2],s->r[12]);
        for (int reg=3; reg<=10; ++reg) s->r[reg]=LOAD32(s->r[30]+104+4*(reg-3));
        s->r[1]=LOAD32(s->r[1]); s->r[0]=LOAD32(s->r[1]+8); s->lr=s->r[0];
        s->r[30]=LOAD32(s->r[1]-8); s->r[31]=LOAD32(s->r[1]-4);
    }
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _sscanf$LDBLStub @ 0x0002cc64: Cached Darwin stdio binding; slot 0x0030c950, name string 0x0003688c. Spills/reloads r3-r10; no FPR-save instructions. Resolver prefers $LDBL128 then the base libc symbol. */
void ABI_0002cc64(PPC32 *s)
{
    s->r[0] = s->lr; s->lr = 0x0002cc6c; s->r[2] = s->lr; s->lr = s->r[0];
    s->r[12] = LOAD32(0x0030c950); SET_CR7_CMP_SIGNED(s->r[12], 0);
    if (s->r[12] == 0) {
        STORE32(s->r[1]-8,s->r[30]); STORE32(s->r[1]-4,s->r[31]);
        STORE32(s->r[1]+8,s->r[0]); STORE32(s->r[1]-80,s->r[1]);
        s->r[1]-=80; s->r[30]=s->r[1]; s->r[31]=s->r[2];
        for (int reg=3; reg<=10; ++reg) STORE32(s->r[30]+104+4*(reg-3),s->r[reg]);
        s->r[3]=0x0003688c; s->lr=0x0002ccc8; CALL(0x2cf14,s);
        s->r[12]=s->r[3]; s->r[2]=0x0030c950; STORE32(s->r[2],s->r[12]);
        for (int reg=3; reg<=10; ++reg) s->r[reg]=LOAD32(s->r[30]+104+4*(reg-3));
        s->r[1]=LOAD32(s->r[1]); s->r[0]=LOAD32(s->r[1]+8); s->lr=s->r[0];
        s->r[30]=LOAD32(s->r[1]-8); s->r[31]=LOAD32(s->r[1]-4);
    }
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _vfprintf$LDBLStub @ 0x0002cd10: Cached Darwin stdio binding; slot 0x0030c954, name string 0x00036894. Spills/reloads r3-r10; no FPR-save instructions. Resolver prefers $LDBL128 then the base libc symbol. */
void ABI_0002cd10(PPC32 *s)
{
    s->r[0] = s->lr; s->lr = 0x0002cd18; s->r[2] = s->lr; s->lr = s->r[0];
    s->r[12] = LOAD32(0x0030c954); SET_CR7_CMP_SIGNED(s->r[12], 0);
    if (s->r[12] == 0) {
        STORE32(s->r[1]-8,s->r[30]); STORE32(s->r[1]-4,s->r[31]);
        STORE32(s->r[1]+8,s->r[0]); STORE32(s->r[1]-80,s->r[1]);
        s->r[1]-=80; s->r[30]=s->r[1]; s->r[31]=s->r[2];
        for (int reg=3; reg<=10; ++reg) STORE32(s->r[30]+104+4*(reg-3),s->r[reg]);
        s->r[3]=0x00036894; s->lr=0x0002cd74; CALL(0x2cf14,s);
        s->r[12]=s->r[3]; s->r[2]=0x0030c954; STORE32(s->r[2],s->r[12]);
        for (int reg=3; reg<=10; ++reg) s->r[reg]=LOAD32(s->r[30]+104+4*(reg-3));
        s->r[1]=LOAD32(s->r[1]); s->r[0]=LOAD32(s->r[1]+8); s->lr=s->r[0];
        s->r[30]=LOAD32(s->r[1]-8); s->r[31]=LOAD32(s->r[1]-4);
    }
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _vsnprintf$LDBLStub @ 0x0002cdbc: Cached Darwin stdio binding; slot 0x0030c958, name string 0x000368a0. Spills/reloads r3-r10; no FPR-save instructions. Resolver prefers $LDBL128 then the base libc symbol. */
void ABI_0002cdbc(PPC32 *s)
{
    s->r[0] = s->lr; s->lr = 0x0002cdc4; s->r[2] = s->lr; s->lr = s->r[0];
    s->r[12] = LOAD32(0x0030c958); SET_CR7_CMP_SIGNED(s->r[12], 0);
    if (s->r[12] == 0) {
        STORE32(s->r[1]-8,s->r[30]); STORE32(s->r[1]-4,s->r[31]);
        STORE32(s->r[1]+8,s->r[0]); STORE32(s->r[1]-80,s->r[1]);
        s->r[1]-=80; s->r[30]=s->r[1]; s->r[31]=s->r[2];
        for (int reg=3; reg<=10; ++reg) STORE32(s->r[30]+104+4*(reg-3),s->r[reg]);
        s->r[3]=0x000368a0; s->lr=0x0002ce20; CALL(0x2cf14,s);
        s->r[12]=s->r[3]; s->r[2]=0x0030c958; STORE32(s->r[2],s->r[12]);
        for (int reg=3; reg<=10; ++reg) s->r[reg]=LOAD32(s->r[30]+104+4*(reg-3));
        s->r[1]=LOAD32(s->r[1]); s->r[0]=LOAD32(s->r[1]+8); s->lr=s->r[0];
        s->r[30]=LOAD32(s->r[1]-8); s->r[31]=LOAD32(s->r[1]-4);
    }
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _sprintf$LDBLStub @ 0x0002ce68: Cached Darwin stdio binding; slot 0x0030c95c, name string 0x000368ac. Spills/reloads r3-r10; no FPR-save instructions. Resolver prefers $LDBL128 then the base libc symbol. */
void ABI_0002ce68(PPC32 *s)
{
    s->r[0] = s->lr; s->lr = 0x0002ce70; s->r[2] = s->lr; s->lr = s->r[0];
    s->r[12] = LOAD32(0x0030c95c); SET_CR7_CMP_SIGNED(s->r[12], 0);
    if (s->r[12] == 0) {
        STORE32(s->r[1]-8,s->r[30]); STORE32(s->r[1]-4,s->r[31]);
        STORE32(s->r[1]+8,s->r[0]); STORE32(s->r[1]-80,s->r[1]);
        s->r[1]-=80; s->r[30]=s->r[1]; s->r[31]=s->r[2];
        for (int reg=3; reg<=10; ++reg) STORE32(s->r[30]+104+4*(reg-3),s->r[reg]);
        s->r[3]=0x000368ac; s->lr=0x0002cecc; CALL(0x2cf14,s);
        s->r[12]=s->r[3]; s->r[2]=0x0030c95c; STORE32(s->r[2],s->r[12]);
        for (int reg=3; reg<=10; ++reg) s->r[reg]=LOAD32(s->r[30]+104+4*(reg-3));
        s->r[1]=LOAD32(s->r[1]); s->r[0]=LOAD32(s->r[1]+8); s->lr=s->r[0];
        s->r[30]=LOAD32(s->r[1]-8); s->r[31]=LOAD32(s->r[1]-4);
    }
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _exit: lazy import @ 0x0002cfc0, slot 0x000396c4. */
void ABI_0002cfc0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002cfc8; s->r[11]=s->lr;
    s->r[11]=0x000396c4; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* ___keymgr_dwarf2_register_sections: lazy import @ 0x0002cfe0, slot 0x000396c8. */
void ABI_0002cfe0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002cfe8; s->r[11]=s->lr;
    s->r[11]=0x000396c8; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _atexit: lazy import @ 0x0002d000, slot 0x000396cc. */
void ABI_0002d000(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d008; s->r[11]=s->lr;
    s->r[11]=0x000396cc; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _abort: lazy import @ 0x0002d020, slot 0x000396d0. */
void ABI_0002d020(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d028; s->r[11]=s->lr;
    s->r[11]=0x000396d0; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* __dyld_register_func_for_add_image: lazy import @ 0x0002d040, slot 0x000396d4. */
void ABI_0002d040(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d048; s->r[11]=s->lr;
    s->r[11]=0x000396d4; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* __dyld_register_func_for_remove_image: lazy import @ 0x0002d060, slot 0x000396d8. */
void ABI_0002d060(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d068; s->r[11]=s->lr;
    s->r[11]=0x000396d8; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* __keymgr_get_and_lock_processwide_ptr: lazy import @ 0x0002d080, slot 0x000396dc. */
void ABI_0002d080(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d088; s->r[11]=s->lr;
    s->r[11]=0x000396dc; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* __init_keymgr: lazy import @ 0x0002d0a0, slot 0x000396e0. */
void ABI_0002d0a0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d0a8; s->r[11]=s->lr;
    s->r[11]=0x000396e0; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _free: lazy import @ 0x0002d0c0, slot 0x000396e4. */
void ABI_0002d0c0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d0c8; s->r[11]=s->lr;
    s->r[11]=0x000396e4; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* __keymgr_set_and_unlock_processwide_ptr: lazy import @ 0x0002d0e0, slot 0x000396e8. */
void ABI_0002d0e0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d0e8; s->r[11]=s->lr;
    s->r[11]=0x000396e8; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _calloc: lazy import @ 0x0002d100, slot 0x000396ec. */
void ABI_0002d100(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d108; s->r[11]=s->lr;
    s->r[11]=0x000396ec; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _strncmp: lazy import @ 0x0002d120, slot 0x000396f0. */
void ABI_0002d120(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d128; s->r[11]=s->lr;
    s->r[11]=0x000396f0; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _unlink: lazy import @ 0x0002d140, slot 0x000396f4. */
void ABI_0002d140(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d148; s->r[11]=s->lr;
    s->r[11]=0x000396f4; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _puts: lazy import @ 0x0002d160, slot 0x000396f8. */
void ABI_0002d160(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d168; s->r[11]=s->lr;
    s->r[11]=0x000396f8; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _closedir: lazy import @ 0x0002d180, slot 0x000396fc. */
void ABI_0002d180(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d188; s->r[11]=s->lr;
    s->r[11]=0x000396fc; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _malloc: lazy import @ 0x0002d1a0, slot 0x00039700. */
void ABI_0002d1a0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d1a8; s->r[11]=s->lr;
    s->r[11]=0x00039700; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _putchar: lazy import @ 0x0002d1c0, slot 0x00039704. */
void ABI_0002d1c0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d1c8; s->r[11]=s->lr;
    s->r[11]=0x00039704; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _getenv: lazy import @ 0x0002d1e0, slot 0x00039708. */
void ABI_0002d1e0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d1e8; s->r[11]=s->lr;
    s->r[11]=0x00039708; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _readdir: lazy import @ 0x0002d200, slot 0x0003970c. */
void ABI_0002d200(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d208; s->r[11]=s->lr;
    s->r[11]=0x0003970c; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _read: lazy import @ 0x0002d220, slot 0x00039710. */
void ABI_0002d220(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d228; s->r[11]=s->lr;
    s->r[11]=0x00039710; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _opendir: lazy import @ 0x0002d240, slot 0x00039714. */
void ABI_0002d240(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d248; s->r[11]=s->lr;
    s->r[11]=0x00039714; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _signal: lazy import @ 0x0002d260, slot 0x00039718. */
void ABI_0002d260(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d268; s->r[11]=s->lr;
    s->r[11]=0x00039718; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _strlen: lazy import @ 0x0002d280, slot 0x0003971c. */
void ABI_0002d280(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d288; s->r[11]=s->lr;
    s->r[11]=0x0003971c; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _close: lazy import @ 0x0002d2a0, slot 0x00039720. */
void ABI_0002d2a0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d2a8; s->r[11]=s->lr;
    s->r[11]=0x00039720; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _fgets: lazy import @ 0x0002d2c0, slot 0x00039724. */
void ABI_0002d2c0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d2c8; s->r[11]=s->lr;
    s->r[11]=0x00039724; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _strcmp: lazy import @ 0x0002d2e0, slot 0x00039728. */
void ABI_0002d2e0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d2e8; s->r[11]=s->lr;
    s->r[11]=0x00039728; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _open: lazy import @ 0x0002d300, slot 0x0003972c. */
void ABI_0002d300(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d308; s->r[11]=s->lr;
    s->r[11]=0x0003972c; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _memcpy: lazy import @ 0x0002d320, slot 0x00039730. */
void ABI_0002d320(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d328; s->r[11]=s->lr;
    s->r[11]=0x00039730; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _strchr: lazy import @ 0x0002d340, slot 0x00039734. */
void ABI_0002d340(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d348; s->r[11]=s->lr;
    s->r[11]=0x00039734; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _fflush: lazy import @ 0x0002d360, slot 0x00039738. */
void ABI_0002d360(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d368; s->r[11]=s->lr;
    s->r[11]=0x00039738; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _memset: lazy import @ 0x0002d380, slot 0x0003973c. */
void ABI_0002d380(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d388; s->r[11]=s->lr;
    s->r[11]=0x0003973c; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _strncpy: lazy import @ 0x0002d3a0, slot 0x00039740. */
void ABI_0002d3a0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d3a8; s->r[11]=s->lr;
    s->r[11]=0x00039740; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _atoi: lazy import @ 0x0002d3c0, slot 0x00039744. */
void ABI_0002d3c0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d3c8; s->r[11]=s->lr;
    s->r[11]=0x00039744; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _getpwuid: lazy import @ 0x0002d3e0, slot 0x00039748. */
void ABI_0002d3e0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d3e8; s->r[11]=s->lr;
    s->r[11]=0x00039748; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _stat: lazy import @ 0x0002d400, slot 0x0003974c. */
void ABI_0002d400(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d408; s->r[11]=s->lr;
    s->r[11]=0x0003974c; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _fopen: lazy import @ 0x0002d420, slot 0x00039750. */
void ABI_0002d420(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d428; s->r[11]=s->lr;
    s->r[11]=0x00039750; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _atof: lazy import @ 0x0002d440, slot 0x00039754. */
void ABI_0002d440(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d448; s->r[11]=s->lr;
    s->r[11]=0x00039754; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _strerror: lazy import @ 0x0002d460, slot 0x00039758. */
void ABI_0002d460(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d468; s->r[11]=s->lr;
    s->r[11]=0x00039758; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _getcwd: lazy import @ 0x0002d480, slot 0x0003975c. */
void ABI_0002d480(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d488; s->r[11]=s->lr;
    s->r[11]=0x0003975c; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _fclose: lazy import @ 0x0002d4a0, slot 0x00039760. */
void ABI_0002d4a0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d4a8; s->r[11]=s->lr;
    s->r[11]=0x00039760; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _fwrite: lazy import @ 0x0002d4c0, slot 0x00039764. */
void ABI_0002d4c0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d4c8; s->r[11]=s->lr;
    s->r[11]=0x00039764; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _strcasecmp: lazy import @ 0x0002d4e0, slot 0x00039768. */
void ABI_0002d4e0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d4e8; s->r[11]=s->lr;
    s->r[11]=0x00039768; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _getuid: lazy import @ 0x0002d500, slot 0x0003976c. */
void ABI_0002d500(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d508; s->r[11]=s->lr;
    s->r[11]=0x0003976c; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* ___error: lazy import @ 0x0002d520, slot 0x00039770. */
void ABI_0002d520(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d528; s->r[11]=s->lr;
    s->r[11]=0x00039770; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _fread: lazy import @ 0x0002d540, slot 0x00039774. */
void ABI_0002d540(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d548; s->r[11]=s->lr;
    s->r[11]=0x00039774; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _fseek: lazy import @ 0x0002d560, slot 0x00039778. */
void ABI_0002d560(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d568; s->r[11]=s->lr;
    s->r[11]=0x00039778; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _ftell: lazy import @ 0x0002d580, slot 0x0003977c. */
void ABI_0002d580(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d588; s->r[11]=s->lr;
    s->r[11]=0x0003977c; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _fputc: lazy import @ 0x0002d5a0, slot 0x00039780. */
void ABI_0002d5a0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d5a8; s->r[11]=s->lr;
    s->r[11]=0x00039780; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _mkdir: lazy import @ 0x0002d5c0, slot 0x00039784. */
void ABI_0002d5c0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d5c8; s->r[11]=s->lr;
    s->r[11]=0x00039784; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _rewind: lazy import @ 0x0002d5e0, slot 0x00039788. */
void ABI_0002d5e0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d5e8; s->r[11]=s->lr;
    s->r[11]=0x00039788; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _memcmp: lazy import @ 0x0002d600, slot 0x0003978c. */
void ABI_0002d600(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d608; s->r[11]=s->lr;
    s->r[11]=0x0003978c; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* __exit: lazy import @ 0x0002d620, slot 0x00039790. */
void ABI_0002d620(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d628; s->r[11]=s->lr;
    s->r[11]=0x00039790; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _fork: lazy import @ 0x0002d640, slot 0x00039794. */
void ABI_0002d640(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d648; s->r[11]=s->lr;
    s->r[11]=0x00039794; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _time: lazy import @ 0x0002d660, slot 0x00039798. */
void ABI_0002d660(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d668; s->r[11]=s->lr;
    s->r[11]=0x00039798; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _waitpid: lazy import @ 0x0002d680, slot 0x0003979c. */
void ABI_0002d680(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d688; s->r[11]=s->lr;
    s->r[11]=0x0003979c; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _strstr: lazy import @ 0x0002d6a0, slot 0x000397a0. */
void ABI_0002d6a0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d6a8; s->r[11]=s->lr;
    s->r[11]=0x000397a0; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* ___tolower: lazy import @ 0x0002d6c0, slot 0x000397a4. */
void ABI_0002d6c0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d6c8; s->r[11]=s->lr;
    s->r[11]=0x000397a4; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* ___maskrune: lazy import @ 0x0002d6e0, slot 0x000397a8. */
void ABI_0002d6e0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d6e8; s->r[11]=s->lr;
    s->r[11]=0x000397a8; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _getpid: lazy import @ 0x0002d700, slot 0x000397ac. */
void ABI_0002d700(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d708; s->r[11]=s->lr;
    s->r[11]=0x000397ac; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _unsetenv: lazy import @ 0x0002d720, slot 0x000397b0. */
void ABI_0002d720(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d728; s->r[11]=s->lr;
    s->r[11]=0x000397b0; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _getdtablesize: lazy import @ 0x0002d740, slot 0x000397b4. */
void ABI_0002d740(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d748; s->r[11]=s->lr;
    s->r[11]=0x000397b4; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _alarm: lazy import @ 0x0002d760, slot 0x000397b8. */
void ABI_0002d760(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d768; s->r[11]=s->lr;
    s->r[11]=0x000397b8; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _execl: lazy import @ 0x0002d780, slot 0x000397bc. */
void ABI_0002d780(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d788; s->r[11]=s->lr;
    s->r[11]=0x000397bc; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _setenv: lazy import @ 0x0002d7a0, slot 0x000397c0. */
void ABI_0002d7a0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d7a8; s->r[11]=s->lr;
    s->r[11]=0x000397c0; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _strcasestr: lazy import @ 0x0002d7c0, slot 0x000397c4. */
void ABI_0002d7c0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d7c8; s->r[11]=s->lr;
    s->r[11]=0x000397c4; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _dup2: lazy import @ 0x0002d7e0, slot 0x000397c8. */
void ABI_0002d7e0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d7e8; s->r[11]=s->lr;
    s->r[11]=0x000397c8; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _kill: lazy import @ 0x0002d800, slot 0x000397cc. */
void ABI_0002d800(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d808; s->r[11]=s->lr;
    s->r[11]=0x000397cc; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _fputs: lazy import @ 0x0002d820, slot 0x000397d0. */
void ABI_0002d820(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d828; s->r[11]=s->lr;
    s->r[11]=0x000397d0; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _strrchr: lazy import @ 0x0002d840, slot 0x000397d4. */
void ABI_0002d840(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d848; s->r[11]=s->lr;
    s->r[11]=0x000397d4; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _fnmatch: lazy import @ 0x0002d860, slot 0x000397d8. */
void ABI_0002d860(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d868; s->r[11]=s->lr;
    s->r[11]=0x000397d8; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _pclose: lazy import @ 0x0002d880, slot 0x000397dc. */
void ABI_0002d880(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d888; s->r[11]=s->lr;
    s->r[11]=0x000397dc; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _popen: lazy import @ 0x0002d8a0, slot 0x000397e0. */
void ABI_0002d8a0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d8a8; s->r[11]=s->lr;
    s->r[11]=0x000397e0; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _rename: lazy import @ 0x0002d8c0, slot 0x000397e4. */
void ABI_0002d8c0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d8c8; s->r[11]=s->lr;
    s->r[11]=0x000397e4; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _strtok_r: lazy import @ 0x0002d8e0, slot 0x000397e8. */
void ABI_0002d8e0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d8e8; s->r[11]=s->lr;
    s->r[11]=0x000397e8; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _usleep: lazy import @ 0x0002d900, slot 0x000397ec. */
void ABI_0002d900(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d908; s->r[11]=s->lr;
    s->r[11]=0x000397ec; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _realpath: lazy import @ 0x0002d920, slot 0x000397f0. */
void ABI_0002d920(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d928; s->r[11]=s->lr;
    s->r[11]=0x000397f0; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _curl_easy_init: lazy import @ 0x0002d940, slot 0x000397f4. */
void ABI_0002d940(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d948; s->r[11]=s->lr;
    s->r[11]=0x000397f4; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _realloc: lazy import @ 0x0002d960, slot 0x000397f8. */
void ABI_0002d960(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d968; s->r[11]=s->lr;
    s->r[11]=0x000397f8; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _curl_slist_append: lazy import @ 0x0002d980, slot 0x000397fc. */
void ABI_0002d980(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d988; s->r[11]=s->lr;
    s->r[11]=0x000397fc; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _curl_easy_getinfo: lazy import @ 0x0002d9a0, slot 0x00039800. */
void ABI_0002d9a0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d9a8; s->r[11]=s->lr;
    s->r[11]=0x00039800; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _curl_easy_setopt: lazy import @ 0x0002d9c0, slot 0x00039804. */
void ABI_0002d9c0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d9c8; s->r[11]=s->lr;
    s->r[11]=0x00039804; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _curl_slist_free_all: lazy import @ 0x0002d9e0, slot 0x00039808. */
void ABI_0002d9e0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002d9e8; s->r[11]=s->lr;
    s->r[11]=0x00039808; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _curl_easy_perform: lazy import @ 0x0002da00, slot 0x0003980c. */
void ABI_0002da00(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002da08; s->r[11]=s->lr;
    s->r[11]=0x0003980c; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _curl_easy_cleanup: lazy import @ 0x0002da20, slot 0x00039810. */
void ABI_0002da20(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002da28; s->r[11]=s->lr;
    s->r[11]=0x00039810; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _curl_global_init: lazy import @ 0x0002da40, slot 0x00039814. */
void ABI_0002da40(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002da48; s->r[11]=s->lr;
    s->r[11]=0x00039814; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _mkstemp: lazy import @ 0x0002da60, slot 0x00039818. */
void ABI_0002da60(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002da68; s->r[11]=s->lr;
    s->r[11]=0x00039818; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _write: lazy import @ 0x0002da80, slot 0x0003981c. */
void ABI_0002da80(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002da88; s->r[11]=s->lr;
    s->r[11]=0x0003981c; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _pipe: lazy import @ 0x0002daa0, slot 0x00039820. */
void ABI_0002daa0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002daa8; s->r[11]=s->lr;
    s->r[11]=0x00039820; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _chdir: lazy import @ 0x0002dac0, slot 0x00039824. */
void ABI_0002dac0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002dac8; s->r[11]=s->lr;
    s->r[11]=0x00039824; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _execvp: lazy import @ 0x0002dae0, slot 0x00039828. */
void ABI_0002dae0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002dae8; s->r[11]=s->lr;
    s->r[11]=0x00039828; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _init_pair: lazy import @ 0x0002db00, slot 0x0003982c. */
void ABI_0002db00(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002db08; s->r[11]=s->lr;
    s->r[11]=0x0003982c; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _has_colors: lazy import @ 0x0002db20, slot 0x00039830. */
void ABI_0002db20(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002db28; s->r[11]=s->lr;
    s->r[11]=0x00039830; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _doupdate: lazy import @ 0x0002db40, slot 0x00039834. */
void ABI_0002db40(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002db48; s->r[11]=s->lr;
    s->r[11]=0x00039834; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _wrefresh: lazy import @ 0x0002db60, slot 0x00039838. */
void ABI_0002db60(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002db68; s->r[11]=s->lr;
    s->r[11]=0x00039838; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _wborder: lazy import @ 0x0002db80, slot 0x0003983c. */
void ABI_0002db80(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002db88; s->r[11]=s->lr;
    s->r[11]=0x0003983c; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _wgetch: lazy import @ 0x0002dba0, slot 0x00039840. */
void ABI_0002dba0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002dba8; s->r[11]=s->lr;
    s->r[11]=0x00039840; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _ungetch: lazy import @ 0x0002dbc0, slot 0x00039844. */
void ABI_0002dbc0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002dbc8; s->r[11]=s->lr;
    s->r[11]=0x00039844; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _waddch: lazy import @ 0x0002dbe0, slot 0x00039848. */
void ABI_0002dbe0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002dbe8; s->r[11]=s->lr;
    s->r[11]=0x00039848; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _wbkgd: lazy import @ 0x0002dc00, slot 0x0003984c. */
void ABI_0002dc00(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002dc08; s->r[11]=s->lr;
    s->r[11]=0x0003984c; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _wnoutrefresh: lazy import @ 0x0002dc20, slot 0x00039850. */
void ABI_0002dc20(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002dc28; s->r[11]=s->lr;
    s->r[11]=0x00039850; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _flushinp: lazy import @ 0x0002dc40, slot 0x00039854. */
void ABI_0002dc40(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002dc48; s->r[11]=s->lr;
    s->r[11]=0x00039854; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _cbreak: lazy import @ 0x0002dc60, slot 0x00039858. */
void ABI_0002dc60(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002dc68; s->r[11]=s->lr;
    s->r[11]=0x00039858; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _mvwprintw: lazy import @ 0x0002dc80, slot 0x0003985c. */
void ABI_0002dc80(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002dc88; s->r[11]=s->lr;
    s->r[11]=0x0003985c; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _wattr_on: lazy import @ 0x0002dca0, slot 0x00039860. */
void ABI_0002dca0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002dca8; s->r[11]=s->lr;
    s->r[11]=0x00039860; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _pnoutrefresh: lazy import @ 0x0002dcc0, slot 0x00039864. */
void ABI_0002dcc0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002dcc8; s->r[11]=s->lr;
    s->r[11]=0x00039864; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _wmove: lazy import @ 0x0002dce0, slot 0x00039868. */
void ABI_0002dce0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002dce8; s->r[11]=s->lr;
    s->r[11]=0x00039868; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _gettimeofday: lazy import @ 0x0002dd00, slot 0x0003986c. */
void ABI_0002dd00(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002dd08; s->r[11]=s->lr;
    s->r[11]=0x0003986c; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _noecho: lazy import @ 0x0002dd20, slot 0x00039870. */
void ABI_0002dd20(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002dd28; s->r[11]=s->lr;
    s->r[11]=0x00039870; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _wtimeout: lazy import @ 0x0002dd40, slot 0x00039874. */
void ABI_0002dd40(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002dd48; s->r[11]=s->lr;
    s->r[11]=0x00039874; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _memmove: lazy import @ 0x0002dd60, slot 0x00039878. */
void ABI_0002dd60(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002dd68; s->r[11]=s->lr;
    s->r[11]=0x00039878; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _keypad: lazy import @ 0x0002dd80, slot 0x0003987c. */
void ABI_0002dd80(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002dd88; s->r[11]=s->lr;
    s->r[11]=0x0003987c; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _newwin: lazy import @ 0x0002dda0, slot 0x00039880. */
void ABI_0002dda0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002dda8; s->r[11]=s->lr;
    s->r[11]=0x00039880; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _start_color: lazy import @ 0x0002ddc0, slot 0x00039884. */
void ABI_0002ddc0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002ddc8; s->r[11]=s->lr;
    s->r[11]=0x00039884; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _setlocale: lazy import @ 0x0002dde0, slot 0x00039888. */
void ABI_0002dde0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002dde8; s->r[11]=s->lr;
    s->r[11]=0x00039888; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _wprintw: lazy import @ 0x0002de00, slot 0x0003988c. */
void ABI_0002de00(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002de08; s->r[11]=s->lr;
    s->r[11]=0x0003988c; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _delwin: lazy import @ 0x0002de20, slot 0x00039890. */
void ABI_0002de20(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002de28; s->r[11]=s->lr;
    s->r[11]=0x00039890; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _nodelay: lazy import @ 0x0002de40, slot 0x00039894. */
void ABI_0002de40(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002de48; s->r[11]=s->lr;
    s->r[11]=0x00039894; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _initscr: lazy import @ 0x0002de60, slot 0x00039898. */
void ABI_0002de60(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002de68; s->r[11]=s->lr;
    s->r[11]=0x00039898; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _strncat: lazy import @ 0x0002de80, slot 0x0003989c. */
void ABI_0002de80(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002de88; s->r[11]=s->lr;
    s->r[11]=0x0003989c; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _wresize: lazy import @ 0x0002dea0, slot 0x000398a0. */
void ABI_0002dea0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002dea8; s->r[11]=s->lr;
    s->r[11]=0x000398a0; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _ioctl: lazy import @ 0x0002dec0, slot 0x000398a4. */
void ABI_0002dec0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002dec8; s->r[11]=s->lr;
    s->r[11]=0x000398a4; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _wattr_off: lazy import @ 0x0002dee0, slot 0x000398a8. */
void ABI_0002dee0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002dee8; s->r[11]=s->lr;
    s->r[11]=0x000398a8; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _scrollok: lazy import @ 0x0002df00, slot 0x000398ac. */
void ABI_0002df00(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002df08; s->r[11]=s->lr;
    s->r[11]=0x000398ac; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _curs_set: lazy import @ 0x0002df20, slot 0x000398b0. */
void ABI_0002df20(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002df28; s->r[11]=s->lr;
    s->r[11]=0x000398b0; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _resize_term: lazy import @ 0x0002df40, slot 0x000398b4. */
void ABI_0002df40(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002df48; s->r[11]=s->lr;
    s->r[11]=0x000398b4; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _resizeterm: lazy import @ 0x0002df60, slot 0x000398b8. */
void ABI_0002df60(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002df68; s->r[11]=s->lr;
    s->r[11]=0x000398b8; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _endwin: lazy import @ 0x0002df80, slot 0x000398bc. */
void ABI_0002df80(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002df88; s->r[11]=s->lr;
    s->r[11]=0x000398bc; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _newpad: lazy import @ 0x0002dfa0, slot 0x000398c0. */
void ABI_0002dfa0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002dfa8; s->r[11]=s->lr;
    s->r[11]=0x000398c0; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _use_default_colors: lazy import @ 0x0002dfc0, slot 0x000398c4. */
void ABI_0002dfc0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002dfc8; s->r[11]=s->lr;
    s->r[11]=0x000398c4; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _werase: lazy import @ 0x0002dfe0, slot 0x000398c8. */
void ABI_0002dfe0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002dfe8; s->r[11]=s->lr;
    s->r[11]=0x000398c8; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _wclear: lazy import @ 0x0002e000, slot 0x000398cc. */
void ABI_0002e000(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002e008; s->r[11]=s->lr;
    s->r[11]=0x000398cc; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _fcntl: lazy import @ 0x0002e020, slot 0x000398d0. */
void ABI_0002e020(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002e028; s->r[11]=s->lr;
    s->r[11]=0x000398d0; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _select: lazy import @ 0x0002e040, slot 0x000398d4. */
void ABI_0002e040(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002e048; s->r[11]=s->lr;
    s->r[11]=0x000398d4; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _strncasecmp: lazy import @ 0x0002e060, slot 0x000398d8. */
void ABI_0002e060(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002e068; s->r[11]=s->lr;
    s->r[11]=0x000398d8; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _curl_easy_strerror: lazy import @ 0x0002e080, slot 0x000398dc. */
void ABI_0002e080(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002e088; s->r[11]=s->lr;
    s->r[11]=0x000398dc; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _sqlite3_step: lazy import @ 0x0002e0a0, slot 0x000398e0. */
void ABI_0002e0a0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002e0a8; s->r[11]=s->lr;
    s->r[11]=0x000398e0; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _sqlite3_exec: lazy import @ 0x0002e0c0, slot 0x000398e4. */
void ABI_0002e0c0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002e0c8; s->r[11]=s->lr;
    s->r[11]=0x000398e4; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _sqlite3_errmsg: lazy import @ 0x0002e0e0, slot 0x000398e8. */
void ABI_0002e0e0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002e0e8; s->r[11]=s->lr;
    s->r[11]=0x000398e8; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _sqlite3_column_count: lazy import @ 0x0002e100, slot 0x000398ec. */
void ABI_0002e100(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002e108; s->r[11]=s->lr;
    s->r[11]=0x000398ec; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _sqlite3_column_text: lazy import @ 0x0002e120, slot 0x000398f0. */
void ABI_0002e120(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002e128; s->r[11]=s->lr;
    s->r[11]=0x000398f0; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _sqlite3_finalize: lazy import @ 0x0002e140, slot 0x000398f4. */
void ABI_0002e140(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002e148; s->r[11]=s->lr;
    s->r[11]=0x000398f4; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _sqlite3_changes: lazy import @ 0x0002e160, slot 0x000398f8. */
void ABI_0002e160(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002e168; s->r[11]=s->lr;
    s->r[11]=0x000398f8; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _strdup: lazy import @ 0x0002e180, slot 0x000398fc. */
void ABI_0002e180(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002e188; s->r[11]=s->lr;
    s->r[11]=0x000398fc; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _sqlite3_column_name: lazy import @ 0x0002e1a0, slot 0x00039900. */
void ABI_0002e1a0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002e1a8; s->r[11]=s->lr;
    s->r[11]=0x00039900; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _sqlite3_free: lazy import @ 0x0002e1c0, slot 0x00039904. */
void ABI_0002e1c0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002e1c8; s->r[11]=s->lr;
    s->r[11]=0x00039904; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _sqlite3_prepare: lazy import @ 0x0002e1e0, slot 0x00039908. */
void ABI_0002e1e0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002e1e8; s->r[11]=s->lr;
    s->r[11]=0x00039908; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _sqlite3_close: lazy import @ 0x0002e200, slot 0x0003990c. */
void ABI_0002e200(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002e208; s->r[11]=s->lr;
    s->r[11]=0x0003990c; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _sqlite3_open: lazy import @ 0x0002e220, slot 0x00039910. */
void ABI_0002e220(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002e228; s->r[11]=s->lr;
    s->r[11]=0x00039910; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _sqlite3_column_int: lazy import @ 0x0002e240, slot 0x00039914. */
void ABI_0002e240(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002e248; s->r[11]=s->lr;
    s->r[11]=0x00039914; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _sqrt: lazy import @ 0x0002e260, slot 0x00039918. */
void ABI_0002e260(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002e268; s->r[11]=s->lr;
    s->r[11]=0x00039918; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _sqlite3_bind_int: lazy import @ 0x0002e280, slot 0x0003991c. */
void ABI_0002e280(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002e288; s->r[11]=s->lr;
    s->r[11]=0x0003991c; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _strtod: lazy import @ 0x0002e2a0, slot 0x00039920. */
void ABI_0002e2a0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002e2a8; s->r[11]=s->lr;
    s->r[11]=0x00039920; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _sqlite3_bind_text: lazy import @ 0x0002e2c0, slot 0x00039924. */
void ABI_0002e2c0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002e2c8; s->r[11]=s->lr;
    s->r[11]=0x00039924; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _strcpy: lazy import @ 0x0002e2e0, slot 0x00039928. */
void ABI_0002e2e0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002e2e8; s->r[11]=s->lr;
    s->r[11]=0x00039928; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _pow: lazy import @ 0x0002e300, slot 0x0003992c. */
void ABI_0002e300(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002e308; s->r[11]=s->lr;
    s->r[11]=0x0003992c; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _execlp: lazy import @ 0x0002e320, slot 0x00039930. */
void ABI_0002e320(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002e328; s->r[11]=s->lr;
    s->r[11]=0x00039930; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _sysctlbyname: lazy import @ 0x0002e340, slot 0x00039934. */
void ABI_0002e340(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002e348; s->r[11]=s->lr;
    s->r[11]=0x00039934; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _uname: lazy import @ 0x0002e360, slot 0x00039938. */
void ABI_0002e360(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002e368; s->r[11]=s->lr;
    s->r[11]=0x00039938; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _NSIsSymbolNameDefinedWithHint: lazy import @ 0x0002e380, slot 0x0003993c. */
void ABI_0002e380(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002e388; s->r[11]=s->lr;
    s->r[11]=0x0003993c; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _NSLookupAndBindSymbolWithHint: lazy import @ 0x0002e3a0, slot 0x00039940. */
void ABI_0002e3a0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002e3a8; s->r[11]=s->lr;
    s->r[11]=0x00039940; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}

/* _NSAddressOfSymbol: lazy import @ 0x0002e3c0, slot 0x00039944. */
void ABI_0002e3c0(PPC32 *s)
{
    s->r[0]=s->lr; s->lr=0x0002e3c8; s->r[11]=s->lr;
    s->r[11]=0x00039944; s->lr=s->r[0]; s->r[12]=LOAD32(s->r[11]);
    s->ctr=s->r[12]; TRANSFER(s->ctr,s);
}
