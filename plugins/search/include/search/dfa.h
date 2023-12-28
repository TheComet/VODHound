#pragma once

#include "search/range.h"
#include "vh/table.h"
#include "vh/vec.h"

#if defined(__cplusplus)
extern "C" {
#endif

union symbol;
struct nfa_graph;
struct vec;

struct dfa_table
{
    struct table tt;
    struct vec tf;
};

/*!
 * \brief Initializes the DFA structure. Call this before doing anything else.
 */
void
dfa_init(struct dfa_table* dfa);

/*!
 * \brief Frees all memory if necessary. You must call dfa_init() again if you
 * want to re-use the structure.
 */
void
dfa_deinit(struct dfa_table* dfa);

/*!
 * \brief Create a DFA from an NFA.
 * \param[in] dfa Structure that has been initialized with dfa_init(). If
 * the structure is already holding an existing DFA, it will be freed
 * first.
 * \return Returns 0 on success or negative on error.
 */
int
dfa_from_nfa(struct dfa_table* dfa, struct nfa_graph* nfa);

#if defined(EXPORT_DOT)
int
dfa_export_dot(const struct dfa_table* dfa, const char* file_name);
void
nfa_export_table(const struct table* tt, const struct vec* tf, const char* file_name);
void
dfa_export_table(const struct table* tt, const struct vec* tf, const char* file_name);
#else
#define dfa_export_dot(dfa, file_name)
#define nfa_export_table(tt, tf, file_name)
#define dfa_export_table(tt, tf, file_name)
#endif

/*!
 * \brief Finds the first match.
 * \param[in] assembly A compiled expression from asm_compile().
 * \param[in] symbols Array of symbols to search on.
 * \param[in] window Start and end indices into "symbols" to run the search on.
 * \return Returns a range into "symbols" matching the compiled expression. If
 * no match is found, then range.start == range.end.
 */
struct range
dfa_find_first(const struct dfa_table* dfa, const union symbol* symbols, struct range window);

/*!
 * \brief Finds all matches.
 * \param[in] assembly A compiled expression from asm_compile().
 * \param[in] symbols Array of symbols to search on.
 * \param[in] window Start and end indices into "symbols" to run the search on.
 * \return Returns a range into "symbols" matching the compiled expression. If
 * no match is found, then range.start == range.end.
 */
int
dfa_find_all(struct vec* ranges, const struct dfa_table* dfa, const union symbol* symbols, struct range window);

#if defined(__cplusplus)
}
#endif
