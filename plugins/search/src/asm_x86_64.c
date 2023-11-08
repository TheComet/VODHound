#include "search/asm.h"
#include "search/dfa.h"
#include "search/match.h"

#include "vh/vec.h"

#include <stdio.h>

#if defined(_WIN32)
#   define WIN32_LEAN_AND_MEAN
#   include <Windows.h>
#else
#   include <unistd.h>
#   include <stdarg.h>
#   include <sys/mman.h>
#endif

static int
get_page_size(void)
{
#if defined(_WIN32)
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return info.dwPageSize;
#else
    return sysconf(_SC_PAGESIZE);
#endif
}
static void*
alloc_page_rw(int size)
{
#if defined(_WIN32)
    return VirtualAlloc(NULL, size, MEM_COMMIT, PAGE_READWRITE);
#else
    return mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
}
static void
protect_rx(void* addr, int size)
{
#if defined(_WIN32)
    DWORD old_protect;
    BOOL result = VirtualProtect(addr, size, PAGE_EXECUTE_READ, &old_protect);
#else
    mprotect(addr, size, PROT_READ | PROT_EXEC);
#endif
}
static void
free_page(void* addr, int size)
{
#if defined(_WIN32)
    (void)size;
    VirtualFree(addr, 0, MEM_RELEASE);
#else
    munmap(addr, size);
#endif
}

static int
write_asm(struct vec* code, int n, ...)
{
    va_list va;
    va_start(va, n);
    while (n--)
    {
        uint8_t* byte = vec_emplace(code);
        if (byte == NULL)
            return -1;
        *byte = (uint8_t)va_arg(va, int);
    }
    return 0;
}
static int
write_asm_u64(struct vec* code, uint64_t value)
{
    int i = 8;
    while (i--)
    {
        uint8_t* byte = vec_emplace(code);
        if (byte == NULL)
            return -1;
        *byte = value & 0xFF;
        value >>= 8;
    }
    return 0;
}
static void
patch_asm_u32(struct vec* code, vec_size offset, uint32_t value)
{
    vec_size i;
    for (i = 0; i != 4; ++i)
    {
        *(uint8_t*)vec_get(code, offset + i) = value & 0xFF;
        value >>= 8;
    }
}
static int
write_asm_u32(struct vec* code, uint32_t value)
{
    int i = 4;
    while (i--)
    {
        uint8_t* byte = vec_emplace(code);
        if (byte == NULL)
            return -1;
        *byte = value & 0xFF;
        value >>= 8;
    }
    return 0;
}
static int
write_asm_u16(struct vec* code, uint16_t value)
{
    int i = 2;
    while (i--)
    {
        uint8_t* byte = vec_emplace(code);
        if (byte == NULL)
            return -1;
        *byte = value & 0xFF;
        value >>= 8;
    }
    return 0;
}
static int
write_asm_u8(struct vec* bytes, uint8_t value)
{
    uint8_t* byte = vec_emplace(bytes);
    if (byte == NULL)
        return -1;
    *byte = value;
    return 0;
}

static uint8_t RAX(void) { return 0; }   static uint8_t EAX(void) { return 0; }
static uint8_t RCX(void) { return 1; }   static uint8_t ECX(void) { return 1; }
static uint8_t RDX(void) { return 2; }   static uint8_t EDX(void) { return 2; }
static uint8_t RBX(void) { return 3; }   static uint8_t EBX(void) { return 3; }
static uint8_t RSP(void) { return 4; }   static uint8_t ESP(void) { return 4; }
static uint8_t RBP(void) { return 5; }   static uint8_t EBP(void) { return 5; }
static uint8_t RSI(void) { return 6; }   static uint8_t ESI(void) { return 6; }
static uint8_t RDI(void) { return 7; }   static uint8_t EDI(void) { return 7; }

static uint8_t SCALE8(void) { return 3; }
static uint8_t SCALE4(void) { return 2; }
static uint8_t SCALE2(void) { return 1; }
static uint8_t SCALE1(void) { return 0; }

/* movabs */
static int MOV_r64_i64(struct vec* code, uint8_t reg, uint64_t value)
    { return write_asm(code, 2, 0x48, 0xb8 | reg) || write_asm_u64(code, value); }
/* mov: 0100 1000 1000 1001 11xx xyyy, xxx=from, yyy=to*/
static int MOV_r64_r64(struct vec* code, uint8_t dst, uint8_t src)
    { return write_asm(code, 3, 0x48, 0x89, 0xC0 | (src << 3) | dst); }
static int MOV_r32_r32(struct vec* code, uint8_t dst, uint8_t src)
    { return write_asm(code, 2, 0x89, 0xC0 | (src << 3) | dst); }
/* 1000 1011 00xx x100 ssyy yzzz, xxx=dst, yyy=offset, zzz=base, ss=scale */
static int MOVSX_r32_dword_ptr_base_offset_scale(struct vec* code, uint8_t dst, uint8_t base, uint8_t offset, uint8_t scale)
    { return write_asm(code, 3, 0x8B, 0x04 | (dst << 3), (scale << 6) | (offset << 3) | base); }
/* 0000 1111 1011 1111 00xx x100 ssyy yzzz, xxx=dst, yyy=offset, zzz=base, ss=scale */
static int MOVSX_r32_word_ptr_base_offset_scale(struct vec* code, uint8_t dst, uint8_t base, uint8_t offset, uint8_t scale)
    { return write_asm(code, 4, 0x0F, 0xBF, 0x04 | (dst << 3), (scale << 6) | (offset << 3) | base); }
/* 0000 1111 1011 1110 00xx x100 ssyy yzzz, xxx=dst, yyy=offset, zzz=base, ss=scale */
static int MOVSX_r32_byte_ptr_base_offset_scale(struct vec* code, uint8_t dst, uint8_t base, uint8_t offset, uint8_t scale)
    { return write_asm(code, 4, 0x0F, 0xBE, 0x04 | (dst << 3), (scale << 6) | (offset << 3) | base); }
static int AND_r64_r64(struct vec* code, uint8_t dst, uint8_t src)
    { return write_asm(code, 3, 0x48, 0x21, 0xC0 | (src << 3) | dst); }
static int AND_r64_i32_sign_extend(struct vec* code, uint8_t reg, uint32_t value)
    { return write_asm(code, 3, 0x48, 0x81, 0xE0 | reg) || write_asm_u32(code, value); }
static int AND_r32_i32(struct vec* code, uint8_t reg, uint32_t value)
    { return write_asm(code, 2, 0x81, 0xE0 | reg) || write_asm_u32(code, value); }
static int CMP_r64_r64(struct vec* code, uint8_t dst, uint8_t src)
    { return write_asm(code, 3, 0x48, 0x39, 0xC0 | (src << 3) | dst); }
static int CMP_r32_i32(struct vec* code, uint8_t reg, uint32_t value)
    { return write_asm(code, 2, 0x81, 0xF8 | reg) || write_asm_u32(code, value); }
static int XOR_r32_r32(struct vec* code, uint8_t dst, uint8_t src)
    { return write_asm(code, 2, 0x31, 0xC0 | (src << 3) | dst); }
static int JE_rel8(struct vec* code, int8_t offset)
    { return write_asm(code, 2, 0x74, (uint8_t)(offset - 2)); }
static int JE_rel32(struct vec* code, int32_t dst)
    { return write_asm(code, 2, 0x0F, 0x84) || write_asm_u32(code, (uint32_t)(dst - 6)); }
static void JE_rel32_patch(struct vec* code, int32_t offset, int32_t dst)
    { patch_asm_u32(code, offset + 2, (uint32_t)(dst - 6)); }
/* 0100 1000 1000 1101 00xx x101, xxx=to */
static int LEA_r64_RSP_plus_i32(struct vec* code, uint8_t reg, uint32_t offset)
    { return write_asm(code, 3, 0x48, 0x8D, 0x05 | (reg << 3)) || write_asm_u32(code, offset); }
static int RET(struct vec* code)
    { return write_asm(code, 1, 0xC3); }
static int PUSH_r64(struct vec* code, uint8_t reg)
    { return write_asm(code, 1, 0x50 | reg); }
static int POP_r64(struct vec* code, uint8_t reg)
    { return write_asm(code, 1, 0x58 | reg); }

static int
assemble_transition_lookup_table(struct vec* code, const struct dfa_table* dfa, int column)
{
    int r;
    uint8_t arg0_reg;

#if defined(_MSC_VER)
    arg0_reg = RCX();
#else
    arg0_reg = RDI();
#endif

    /*
     * Make sure to pop RBX again before generating the next instructions,
     * because otherwise the offset to the lookup table for the LEA instruction
     * will differ on Windows.
     */
#if defined(_MSC_VER)
    POP_r64(code, RBX());
#endif

    if (dfa->tt.rows < 128)
    {
        LEA_r64_RSP_plus_i32(code, RAX(), 5);  /* MOVSX (4) + RET (1) */
        MOVSX_r32_byte_ptr_base_offset_scale(code, EAX(), RAX(), arg0_reg, SCALE1());
    }
    else if (dfa->tt.rows < 32768)
    {
        LEA_r64_RSP_plus_i32(code, RAX(), 5);  /* MOVSX (4) + RET (1) */
        MOVSX_r32_word_ptr_base_offset_scale(code, EAX(), RAX(), arg0_reg, SCALE2());
    }
    else
    {
        LEA_r64_RSP_plus_i32(code, RAX(), 4);  /* MOVSX (3) + RET (1) */
        MOVSX_r32_dword_ptr_base_offset_scale(code, EAX(), RAX(), arg0_reg, SCALE4());
    }

    RET(code);

    /* Lookup table data */
    for (r = 0; r != dfa->tt.rows; ++r)
    {
        if (dfa->tt.rows < 128)
            write_asm_u8(code, (uint8_t)*(int*)table_get(&dfa->tt, r, column));
        else if (dfa->tt.rows < 32768)
            write_asm_u16(code, (uint16_t)*(int*)table_get(&dfa->tt, r, column));
        else
            write_asm_u32(code, (uint32_t)*(int*)table_get(&dfa->tt, r, column));
    }

    return 0;
}

int
asm_compile(struct asm_dfa* assembly, const struct dfa_table* dfa)
{
    int c;
    int page_size = get_page_size();
    int have_wildcard = 0;
    struct vec code;
    struct vec jump_offsets;

    vec_init(&code, sizeof(uint8_t));
    vec_init(&jump_offsets, sizeof(vec_size));

    if (vec_reserve(&code, page_size) < 0)
        goto push_failed;

    /*
     * Linux (System V AMD64 ABI):
     *   Integer args : RDI, RSI, RDX, RCX, R8, R9
     *   Volatile     :
     *
     * Windows:
     *   Integer args : RCX, RDX, R8, R9
     *   Volatile     : RAX, RCX, RDX, R8-R11, XMM0-XMM5
     */

    /* RBX is not volatile on Windows, but we need 2 working registers. */
#if defined(_MSC_VER)
    PUSH_r64(&code, RBX());
#endif

    for (c = 0; c != dfa->tt.cols; ++c)
    {
        const struct matcher* m = vec_get(&dfa->tf, c);

        /*
         * In the special case where the transition table has wildcard matchers,
         * we don't have to generate any code for matching the symbol (by
         * definition the wildcard matches any symbol). Generate lookup table
         * for state transition and return. The last jump will not exist in
         * this case.
         */
        if (c == dfa->tt.cols - 1 && matches_wildcard(m))
        {
            have_wildcard = 1;
            assemble_transition_lookup_table(&code, dfa, c);

            /* Don't generate match code for last column */
            break;
        }

        /* if (matcher->symbol.u64 & matcher->mask.u64) == (input_symbol & matcher->mask.u64) */
#if defined(_MSC_VER)
        MOV_r64_i64(&code, RAX(), m->mask.u64);
        MOV_r64_i64(&code, RBX(), m->symbol.u64);
        AND_r64_r64(&code, RAX(), RDX());  /* RDX = 2nd function parameter (symbol) */
        CMP_r64_r64(&code, RAX(), RBX());  /* Compare (RDX & mask) with symbol */
#else
        MOV_r64_i64(&code, RAX(), m->mask.u64);
        MOV_r64_i64(&code, RDX(), m->symbol.u64);
        AND_r64_r64(&code, RAX(), RSI());  /* RSI = 2nd function parameter (symbol) */
        CMP_r64_r64(&code, RAX(), RDX());  /* Compare (RSI & mask) with symbol */
#endif
        vec_push(&jump_offsets, &vec_count(&code));  /* Don't know destination address of jump yet */
        JE_rel32(&code, 0);
    }

    /*
     * Return 0 if nothing matched -- this block is not reached if a wildcard
     * exists
     */
    if (have_wildcard == 0)
    {
#if defined(_MSC_VER)
        POP_r64(&code, RBX());
#endif
        XOR_r32_r32(&code, EAX(), EAX());
        RET(&code);
    }

    for (c = 0; c != vec_count(&jump_offsets); ++c)
    {
        /* Patch in jump addresses from earlier */
        vec_size target = vec_count(&code);
        vec_size offset = *(vec_size*)vec_get(&jump_offsets, c);
        JE_rel32_patch(&code, offset, target - offset);

        assemble_transition_lookup_table(&code, dfa, c);
    }

#if defined(EXPORT_DOT)
    {
        FILE* fp = fopen("asm.bin", "w");
        fwrite(vec_data(&code), vec_count(&code), 1, fp);
        fclose(fp);
    }
#endif

    while (page_size < (int)vec_count(&code))
        page_size *= 2;

    void* mem = alloc_page_rw(page_size);
    memcpy(mem, vec_data(&code), vec_count(&code));
    protect_rx(mem, page_size);

    vec_deinit(&jump_offsets);
    vec_deinit(&code);
    assembly->next_state = (asm_func)mem;
    assembly->size = page_size;
    return 0;

push_failed:
    vec_deinit(&jump_offsets);
    vec_deinit(&code);
    return -1;
}

void
asm_deinit(struct asm_dfa* assembly)
{
    free_page((void*)assembly->next_state, assembly->size);
}

static int
asm_run(const struct asm_dfa* assembly, const union symbol* symbols, struct range r)
{
    int state;
    int idx;
    int last_accept_idx = r.start;

    state = 0;
    for (idx = r.start; idx != r.end; idx++)
    {
        state = assembly->next_state(state < 0 ? -state : state, symbols[idx].u64);

        /*
         * Transitioning to state 0 indicates the state machine has entered the
         * "trap state", i.e. no match was found for the current symbol. Stop
         * execution.
         */
        if (state == 0)
            break;

        /*
         * Negative states indicate an accept condition.
         * We want to match as much as possible, so instead of returning
         * immediately here, save this index as the last known accept condition.
         */
        if (state < 0)
            last_accept_idx = idx + 1;
    }

    return last_accept_idx;
}

struct range
asm_find_first(const struct asm_dfa* assembly, const union symbol* symbols, struct range window)
{
    for (; window.start != window.end; ++window.start)
    {
        int end = asm_run(assembly, symbols, window);
        if (end > window.start)
        {
            window.end = end;
            break;
        }
    }

    return window;
}

int
asm_find_all(struct vec* ranges, const struct asm_dfa* assembly, const union symbol* symbols, struct range window)
{
    for (; window.start != window.end; ++window.start)
    {
        int end = asm_run(assembly, symbols, window);
        if (end > window.start)
        {
            struct range* r = vec_emplace(ranges);
            if (r == NULL)
                return -1;
            r->start = window.start;
            r->end = end;
            window.start = end - 1;
        }
    }

    return 0;
}
