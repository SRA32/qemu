#include "qemu/osdep.h"
#include "qemu/qemu-print.h"
#include "exec/log.h"
#include "cpu.h"
#include "instr.h"
#include "tcg/tcg-op.h"
#include "exec/helper-proto.h"
#include "exec/helper-gen.h"
#include "exec/translator.h"
#include "exec/translation-block.h"
#include "exec/target_page.h"

#define HELPER_H "helper.h"
#include "exec/helper-info.c.inc"
#undef  HELPER_H

#define DISAS_JUMP DISAS_TARGET_0
#define DISAS_UPDATE DISAS_TARGET_1
#define DISAS_EXIT DISAS_TARGET_2

static TCGv_i32 cpu_pc;
static TCGv_i32 cpu_flags;
static TCGv_i32 cpu_regs[REG_COUNT];
static TCGv_i32 cpu_crs[CR_COUNT];

static const char *reg_names[REG_COUNT] = {
    "X0", "RP", "SP", "FP", "R00",
    "R01",  "R02",  "R03",  "R04", 
    "R05",  "R06",  "R07",  "R08", 
    "R09",  "R10",  "R11",  "R12", 
    "R13",  "R14",  "R15",  "R16", 
    "R17",  "R18",  "R19",  "R20", 
    "R21",  "R22",  "R23",  "R24", 
    "R25",  "R26",  "R27"
};

static const char *cr_names[CR_COUNT] = {
    "INTB", "SVPC", "SVFL", "ESRC",
    "????", "????", "????", "PTBL",
    "????", "????", "????", "????",
    "????", "????", "????", "????"
};

typedef struct {
    DisasContextBase base;
    vaddr pc;
} DisasContext;

typedef struct {
    uint32_t mask;
    uint32_t value;
} SRA32Cond;

void sra32_translate_init(void)
{
    cpu_pc = tcg_global_mem_new_i32(tcg_env, offsetof(CPUSRA32State, pc), "pc");
    cpu_flags = tcg_global_mem_new_i32(tcg_env, offsetof(CPUSRA32State, flags), "flags");
    for (int i = 0; i < REG_COUNT; i++)
        cpu_regs[i] = tcg_global_mem_new_i32(tcg_env, offsetof(CPUSRA32State, regs[i]), reg_names[i]);
    for (int i = 0; i < CR_COUNT; i++)
        cpu_crs[i] = tcg_global_mem_new_i32(tcg_env, offsetof(CPUSRA32State, crs[i]), cr_names[i]);
}

static void sra32_tr_init_disas_context(DisasContextBase *dcbase, CPUState *cs)
{
    int bound = -(dcbase->pc_first | TARGET_PAGE_MASK) / 4;

    dcbase->max_insns = MIN(dcbase->max_insns, bound);
}

static void sra32_tr_tb_start(DisasContextBase *dcbase, CPUState *cs)
{
}

static void sra32_tr_insn_start(DisasContextBase *dcbase, CPUState *cs)
{
    tcg_gen_insn_start(dcbase->pc_next, 0, 0);
}

static void gen_goto_tb(DisasContext *dc, unsigned tb_slot_idx, vaddr dest)
{
    if (translator_use_goto_tb(&dc->base, dest)) {
        tcg_gen_goto_tb(tb_slot_idx);
        tcg_gen_movi_i32(cpu_pc, dest);
        tcg_gen_exit_tb(dc->base.tb, tb_slot_idx);
    } else {
        tcg_gen_movi_i32(cpu_pc, dest);
        tcg_gen_lookup_and_goto_ptr();
    }
    dc->base.is_jmp = DISAS_NORETURN;
}

static void update_flags(TCGv_i32 z, TCGv_i32 n, TCGv_i32 c, TCGv_i32 v)
{
    tcg_gen_shli_i32(n, n, 1);

    if (c != NULL)
        tcg_gen_shli_i32(c, c, 2);
    if (v != NULL)
        tcg_gen_shli_i32(v, v, 3);

    tcg_gen_andi_i32(cpu_flags, cpu_flags, ~(FLAG_Z | FLAG_N | FLAG_C | FLAG_V));
    tcg_gen_or_i32(cpu_flags, cpu_flags, z);
    tcg_gen_or_i32(cpu_flags, cpu_flags, n);

    if (c != NULL)
        tcg_gen_or_i32(cpu_flags, cpu_flags, c);
    if (v != NULL)
        tcg_gen_or_i32(cpu_flags, cpu_flags, v);
}

static void nop(DisasContext *dc, uint32_t code)
{
}

static void alu_add(TCGv_i32 dst, TCGv_i32 tmp, TCGv_i32 src)
{
    TCGv_i32 t0 = tcg_temp_new_i32();
    tcg_gen_add_i32(t0, tmp, src);

    TCGv_i32 z = tcg_temp_new_i32(),
        n = tcg_temp_new_i32(),
        c = tcg_temp_new_i32(),
        v = tcg_temp_new_i32();

    tcg_gen_setcondi_i32(TCG_COND_EQ, z, t0, 0);   // Z = t0 == 0
    tcg_gen_shri_i32(n, t0, 31); // N = t0 >> 31
    tcg_gen_setcond_i32(TCG_COND_LTU, c, t0, tmp); // C = t0 < A

    // V = (~(A ^ B) & (A ^ t0)) >> 31
    TCGv_i32 t1 = tcg_temp_new_i32();
    tcg_gen_xor_i32(t1, tmp, src);
    tcg_gen_not_i32(t1, t1);
    tcg_gen_xor_i32(v, tmp, t0);
    tcg_gen_and_i32(v, t1, v);
    tcg_gen_shri_i32(v, v, 31);

    update_flags(z, n, c, v);

    tcg_gen_mov_i32(dst, t0);
}

static void alu_sub(TCGv_i32 dst, TCGv_i32 tmp, TCGv_i32 src) {
    TCGv_i32 t0 = tcg_temp_new_i32();
    tcg_gen_sub_i32(t0, tmp, src);

    TCGv_i32 z = tcg_temp_new_i32(),
        n = tcg_temp_new_i32(),
        c = tcg_temp_new_i32(),
        v = tcg_temp_new_i32();

    tcg_gen_setcondi_i32(TCG_COND_EQ, z, t0, 0);   // Z = t0 == 0
    tcg_gen_shri_i32(n, t0, 31); // N = t0 >> 31
    tcg_gen_setcond_i32(TCG_COND_GTU, c, tmp, src); // C = A > B

    // V = ((A ^ B) & (A ^ t0)) >> 31
    TCGv_i32 t1 = tcg_temp_new_i32();
    tcg_gen_xor_i32(t1, tmp, src);
    tcg_gen_xor_i32(v, tmp, t0);
    tcg_gen_and_i32(v, t1, v);
    tcg_gen_shri_i32(v, v, 31);

    update_flags(z, n, c, v);

    tcg_gen_mov_i32(dst, t0);
}

static void alu(DisasContext *dc, uint32_t code)
{
    uint8_t funct, rd, rt;
    TCGv_i32 t0;

    if (INSTR_GET_HINT(code) == HINT_I) {
        funct = INSTR_GET_FUNCT_I(code);
        rd = INSTR_GET_RD_I(code);
        rt = INSTR_GET_RT_I(code);
        uint32_t imm = INSTR_GET_IMM(code);

        t0 = tcg_temp_new_i32();
        if (funct <= FUNCT_SUBC)
            tcg_gen_movi_i32(t0, sextract32(imm, 0, 12));
        else
            tcg_gen_movi_i32(t0, imm);
    } else {
        funct = INSTR_GET_FUNCT_R(code);
        rd = INSTR_GET_RD_R(code);
        rt = INSTR_GET_RT_R(code);
        t0 = cpu_regs[INSTR_GET_RS_R(code)];
    }

    if (funct == FUNCT_ADD) {
        alu_add(cpu_regs[rd], cpu_regs[rt], t0);
        return;
    } else if (funct == FUNCT_SUB) {
        alu_sub(cpu_regs[rd], cpu_regs[rt], t0);
        return;
    }

    TCGv_i32 z = tcg_temp_new_i32(),
        n = tcg_temp_new_i32();

    switch (funct) {
        case FUNCT_MUL:
        case FUNCT_UMUL:
            tcg_gen_mul_i32(cpu_regs[rd], cpu_regs[rt], t0);
            break;
        case FUNCT_REM:
            tcg_gen_rem_i32(cpu_regs[rd], cpu_regs[rt], t0);
            break;
        case FUNCT_UREM:
            tcg_gen_remu_i32(cpu_regs[rd], cpu_regs[rt], t0);
            break;
        case FUNCT_DIV:
            gen_helper_div(cpu_regs[rd], tcg_env, cpu_regs[rt], t0);
            break;
        case FUNCT_UDIV:
            gen_helper_divu(cpu_regs[rd], tcg_env, cpu_regs[rt], t0);
            break;
        case FUNCT_AND:
            tcg_gen_and_i32(cpu_regs[rd], cpu_regs[rt], t0);
            break;
        case FUNCT_OR:
            tcg_gen_or_i32(cpu_regs[rd], cpu_regs[rt], t0);
			break;
		case FUNCT_XOR:
            tcg_gen_xor_i32(cpu_regs[rd], cpu_regs[rt], t0);
			break;
		case FUNCT_SHL:
            tcg_gen_shl_i32(cpu_regs[rd], cpu_regs[rt], t0);
			break;
		case FUNCT_SHR:
            tcg_gen_shr_i32(cpu_regs[rd], cpu_regs[rt], t0);
			break;
		case FUNCT_SAR:
            tcg_gen_sar_i32(cpu_regs[rd], cpu_regs[rt], t0);
			break;
		case FUNCT_NOT:
            tcg_gen_not_i32(cpu_regs[rd], t0);
			break;
    }

    tcg_gen_setcondi_i32(TCG_COND_EQ, z, cpu_regs[rd], 0);
    tcg_gen_shri_i32(n, cpu_regs[rd], 31);

    update_flags(z, n, NULL, NULL);
}

static void sext(DisasContext *dc, uint32_t code)
{
    uint8_t funct = INSTR_GET_FUNCT_R(code);

    TCGv_i32 rd = cpu_regs[INSTR_GET_RD_R(code)],
        rs = cpu_regs[INSTR_GET_RS_R(code)];

    if (funct == 0)
        tcg_gen_ext8s_i32(rd, rs);
    else
        tcg_gen_ext16s_i32(rd, rs);
}

static void mov(DisasContext *dc, uint32_t code)
{
    // TODO: Conditional move
    tcg_gen_mov_i32(cpu_regs[INSTR_GET_RD_R(code)], cpu_regs[INSTR_GET_RS_R(code)]);
}

static void cmp(DisasContext *dc, uint32_t code)
{
    TCGv_i32 tmp = tcg_temp_new_i32(),
        t1 = tcg_temp_new_i32(),
        z = tcg_temp_new_i32(),
        n = tcg_temp_new_i32(),
        c = tcg_temp_new_i32(),
        v = tcg_temp_new_i32();

    TCGv_i32 ra = cpu_regs[INSTR_GET_RD_R(code)],
        rb = cpu_regs[INSTR_GET_RS_R(code)];

    // Sets Z, N, C, V flag

    tcg_gen_sub_i32(tmp, ra, rb);

    tcg_gen_setcondi_i32(TCG_COND_EQ, z, tmp, 0);  // z = tmp == 0
    tcg_gen_setcond_i32(TCG_COND_GEU, c, ra, rb); // c = A >= B
    tcg_gen_shri_i32(n, tmp, 31); // n = tmp >> 31
    // v = (ra ^ rb) & (ra ^ tmp) >> 31
    tcg_gen_xor_i32(v, ra, rb);   // v = ra ^ rb
    tcg_gen_xor_i32(t1, ra, tmp); // t1 = ra ^ tmp
    tcg_gen_and_i32(v, v, t1);    // v = v & t1
    tcg_gen_shri_i32(v, v, 31);   // v = v >> 31

    update_flags(z, n, c, v);
}

static void stk(DisasContext *dc, uint32_t code)
{
    uint8_t funct = INSTR_GET_FUNCT_R(code);
    TCGv_i32 reg = cpu_regs[INSTR_GET_RD_R(code)];
    TCGv_i32 tmp = tcg_temp_new_i32();
    tcg_gen_mov_i32(tmp, cpu_regs[REG_SP]);
    if (funct == 0) {
        // Push
        tcg_gen_subi_i32(tmp, tmp, 4);
        tcg_gen_qemu_st_i32(reg, tmp, 0, MO_LEUL);
    } else {
        // Pop
        tcg_gen_qemu_ld_i32(reg, tmp, 0, MO_LEUL);
        tcg_gen_addi_i32(tmp, tmp, 4);
    }
    tcg_gen_mov_i32(cpu_regs[REG_SP], tmp);
}

static void flg(DisasContext *dc, uint32_t code)
{
    uint8_t funct = INSTR_GET_FUNCT_R(code);
    TCGv_i32 reg = cpu_regs[INSTR_GET_RD_R(code)];
    if (funct == 0)
        // Read flags
        tcg_gen_mov_i32(reg, cpu_flags);
    else {
        // Write flags (TODO: Handle protection)
        gen_helper_write_flg(tcg_env, reg);
    }
}

static void spc(DisasContext *dc, uint32_t code)
{
    uint8_t rd = INSTR_GET_RD_R(code);
    tcg_gen_mov_i32(cpu_pc, cpu_regs[rd]);
    dc->base.is_jmp = DISAS_JUMP;
}

static void iret(DisasContext *dc, uint32_t code)
{
    // TODO: Handle protection.
    tcg_gen_mov_i32(cpu_pc, cpu_crs[CR_SVPC]);
    tcg_gen_mov_i32(cpu_flags, cpu_crs[CR_SVFL]);
    dc->base.is_jmp = DISAS_JUMP;
}

// Imm format

static void cr(DisasContext *dc, uint32_t code)
{
    uint8_t funct = INSTR_GET_FUNCT_I(code);
    uint8_t cr_num = INSTR_GET_IMM(code) & 0xF;
    TCGv_i32 reg = cpu_regs[INSTR_GET_RD_I(code)];
    TCGv_i32 cr = cpu_crs[cr_num];
    if (funct == 0)
        // Read cr
        tcg_gen_mov_i32(reg, cr);
    else {
        // Write cr
        TCGv_i32 tmp = tcg_temp_new_i32();
        tcg_gen_movi_i32(tmp, cr_num);
        gen_helper_write_cr(tcg_env, tmp, reg);
    }
}

static MemOp mem_sizes[] = { MO_UB, MO_LEUW, MO_LEUL };

static void ld(DisasContext *dc, uint32_t code)
{
    uint8_t funct = INSTR_GET_FUNCT_I(code);
    int32_t imm = sextract32(INSTR_GET_IMM(code), 0, 12);
    TCGv_i32 addr = tcg_temp_new_i32();

    tcg_gen_addi_i32(addr, cpu_regs[INSTR_GET_RT_I(code)], imm);

    // TODO: Check for unaligned memory access.

    tcg_gen_qemu_ld_i32(cpu_regs[INSTR_GET_RD_I(code)], addr, 0, mem_sizes[funct]);
}

static void st(DisasContext *dc, uint32_t code)
{
    uint8_t funct = INSTR_GET_FUNCT_I(code);
    int32_t imm = sextract32(INSTR_GET_IMM(code), 0, 12);
    TCGv_i32 addr = tcg_temp_new_i32();

    tcg_gen_addi_i32(addr, cpu_regs[INSTR_GET_RT_I(code)], imm);

    tcg_gen_qemu_st_i32(cpu_regs[INSTR_GET_RD_I(code)], addr, 0, mem_sizes[funct]);
}

// Upper format

static void sui(DisasContext *dc, uint32_t code)
{
    uint32_t upper = INSTR_GET_IMM_U(code) << 12;
    tcg_gen_ori_i32(cpu_regs[INSTR_GET_RD_U(code)], cpu_regs[INSTR_GET_RD_U(code)], upper);
}

// Jump format

static void get_flag(TCGv_i32 dst, uint32_t flag)
{
    tcg_gen_andi_i32(dst, cpu_flags, flag);
    tcg_gen_setcondi_i32(TCG_COND_NE, dst, dst, 0);          /* 0 or 1 */
}

static void gen_taken(TCGv_i32 r, uint8_t funct)
{
    TCGv_i32 z = tcg_temp_new_i32();
    TCGv_i32 a = tcg_temp_new_i32();
    TCGv_i32 b = tcg_temp_new_i32();

    get_flag(z, FLAG_Z);

    if (funct & FUNCT_UCOND) {
        get_flag(a, FLAG_C);

        switch (funct & FUNCT_COND_MASK) {
            case FUNCT_COND_EQ:
                tcg_gen_mov_i32(r, z);
                break;
            case FUNCT_COND_NE:
                tcg_gen_xori_i32(r, z, 1);
                break;
            case FUNCT_COND_LT:
                tcg_gen_xori_i32(r, a, 1);
                break;
            case FUNCT_COND_GTE:
                tcg_gen_mov_i32(r, a);
                break;
            case FUNCT_COND_GT:
                tcg_gen_andc_i32(r, a, z);
                break;
            case FUNCT_COND_LTE:
                tcg_gen_xori_i32(b, a, 1);
                tcg_gen_or_i32(r, b, z);
                break;
            default:
                tcg_gen_movi_i32(r, 0);
                break;
        }
    } else {
        get_flag(a, FLAG_N);
        get_flag(b, FLAG_V);
        tcg_gen_xor_i32(a, a, b);

        switch (funct & FUNCT_COND_MASK) {
            case FUNCT_COND_EQ:
                tcg_gen_mov_i32(r, z);
                break;
            case FUNCT_COND_NE:
                tcg_gen_xori_i32(r, z, 1);
                break;
            case FUNCT_COND_LT:
                tcg_gen_mov_i32(r, a);
                break;
            case FUNCT_COND_GTE:
                tcg_gen_xori_i32(r, a, 1);
                break;
            case FUNCT_COND_GT: 
                tcg_gen_or_i32(r, z, a);
                tcg_gen_xori_i32(r, r, 1);
                break;
            case FUNCT_COND_LTE:
                tcg_gen_or_i32(r, z, a);
                break;
            default:
                tcg_gen_movi_i32(r, 0);
                break;
        }
    }
}

static void br(DisasContext *dc, uint32_t code)
{
    uint8_t funct = INSTR_GET_FUNCT_J(code);
    int32_t off = sextract32(INSTR_GET_IMM_J(code), 0, 24) * 4;
    uint32_t dest = dc->pc + off;

    if ((funct & FUNCT_COND_MASK) == 0) {
        gen_goto_tb(dc, 0, dest);
        return;
    }

    TCGLabel *not_taken = gen_new_label();
    TCGv_i32 t = tcg_temp_new_i32();

    gen_taken(t, funct);
    tcg_gen_brcondi_i32(TCG_COND_EQ, t, 0, not_taken);
    gen_goto_tb(dc, 0, dest);
    gen_set_label(not_taken);
    gen_goto_tb(dc, 1, dc->pc + 4);
}

static void call(DisasContext *dc, uint32_t code)
{
    int32_t addr = INSTR_GET_IMM_J(code);
    if (addr & 0x800000) addr -= 0x1000000;
    addr <<= 2;

    tcg_gen_movi_i32(cpu_regs[REG_RP], dc->pc + 4);
    gen_goto_tb(dc, 0, dc->pc + addr);
}

static const void *lookup_table[4][16] = {
	{nop, alu, sext, mov, cmp, stk, flg, spc, NULL, iret},
	{alu, cr, ld, st},
	{sui},
	{br, call}
};

static void sra32_tr_translate_insn(DisasContextBase *dcbase, CPUState *cs)
{
    DisasContext *dc = container_of(dcbase, DisasContext, base);
    CPUSRA32State *env = cpu_env(cs);

    uint32_t code, pc;
    uint8_t hint, opc = 0;

    pc = dcbase->pc_next;
    dc->pc = pc;
    dcbase->pc_next = pc + 4;

    // Decode instruction
    code = translator_ldl_end(env, dcbase, pc, MO_LE);

    hint = INSTR_GET_HINT(code);
    switch (hint) {
        case HINT_R:
            opc = INSTR_GET_OPC_R(code);
            break;
        case HINT_I:
            opc = INSTR_GET_OPC_I(code);
            break;
        case HINT_J:
            opc = INSTR_GET_OPC_J(code);
            break;
    }

    if (opc >= 16 || hint >= 4) {
        return;
    }

    void (*handler)(DisasContext*, uint32_t) = lookup_table[hint][opc];
    if (handler == NULL) {
        return;
    }

    handler(dc, code);
}

static void sra32_tr_tb_stop(DisasContextBase *dcbase, CPUState *cs)
{
    DisasContext *ctx = container_of(dcbase, DisasContext, base);

    switch (ctx->base.is_jmp) {
    case DISAS_NEXT:
    case DISAS_TOO_MANY:
        gen_goto_tb(ctx, 0, dcbase->pc_next);
        break;
    case DISAS_JUMP:
        tcg_gen_lookup_and_goto_ptr();
        break;
    case DISAS_UPDATE:
        tcg_gen_movi_i32(cpu_pc, ctx->base.pc_next);
        /* fallthrough */
    case DISAS_EXIT:
        tcg_gen_exit_tb(NULL, 0);
        break;
    case DISAS_NORETURN:
        break;
    default:
        g_assert_not_reached();
    }
}

static const TranslatorOps sra32_tr_ops = {
    .init_disas_context = sra32_tr_init_disas_context,
    .tb_start           = sra32_tr_tb_start,
    .insn_start         = sra32_tr_insn_start,
    .translate_insn     = sra32_tr_translate_insn,
    .tb_stop            = sra32_tr_tb_stop,
};

void sra32_translate_code(CPUState *cs, TranslationBlock *tb, int *max_insns, vaddr pc, void *host_pc)
{
    DisasContext ctx;

    translator_loop(cs, tb, max_insns, pc, host_pc, &sra32_tr_ops, &ctx.base);
}
