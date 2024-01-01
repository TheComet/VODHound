#pragma once

#include "search/range.h"
#include "search/state.h"
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

struct dfa_table;
union symbol;
struct vec;

typedef union state (*asm_func)(union state state, uint64_t symbol);

struct asm_dfa
{
    asm_func next_state;
    int size;
};

/*!
 * \brief Initializes the assembly structure. Call this before doing anything
 * else.
 */
void
asm_init(struct asm_dfa* assembly);

/*!
 * \brief Frees all memory if necessary. You must call asm_init() again if you
 * want to re-use the structure.
 */
void
asm_deinit(struct asm_dfa* assembly);

/*!
 * \brief Compile a DFA into executable code.
 * \param[in] assembly Structure that has been initialized with asm_init(). If
 * the structure is already holding a compiled expression, it will be freed
 * first.
 * \return Returns 0 on success or negative on error.
 */
int
asm_compile(struct asm_dfa* assembly, const struct dfa_table* dfa);

static inline int
asm_is_compiled(struct asm_dfa* assembly)
    { return assembly->next_state != (void*)0; }

/*!
 * \brief Finds the first match using a compiled expression.
 * \param[in] assembly A compiled expression from asm_compile().
 * \param[in] symbols Array of symbols to search on.
 * \param[in] window Start and end indices into "symbols" to run the search on.
 * \return Returns a range into "symbols" matching the compiled expression. If
 * no match is found, then range.start == range.end.
 */
struct range
asm_find_first(const struct asm_dfa* assembly, const union symbol* symbols, struct range window);

/*!
 * \brief Finds all matches using a compiled expression.
 * \param[in] assembly A compiled expression from asm_compile().
 * \param[in] symbols Array of symbols to search on.
 * \param[in] window Start and end indices into "symbols" to run the search on.
 * \return Returns a range into "symbols" matching the compiled expression. If
 * no match is found, then range.start == range.end.
 */
int
asm_find_all(struct vec* ranges, const struct asm_dfa* assembly, const union symbol* symbols, struct range window);

#if defined(__cplusplus)
}
#endif
