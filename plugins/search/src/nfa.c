#include "search/ast.h"
#include "search/nfa.h"

#include "vh/hash40.h"
#include "vh/hm.h"
#include "vh/mem.h"
#include "vh/log.h"
#include "vh/str.h"

#include <stdio.h>
#include <inttypes.h>

#define max(a, b) ((a) < (b) ? (b) : (a))

/*
 * Represents a subset of the final NFA.
 *
 * Each fragment holds a list of unresolved incoming and outgoing transitions.
 * As the AST is compiled, fragments are assembled together by connecting these
 * transitions to each other in order to form larger and larger fragments, until
 * the final NFA is completed.
 *
 * The "in" and "out" lists are indices into the vector of states/matchers.
 * The "bridge" flag handles a special case where a fragment can have a direct
 * transition from input to output without going through an internal state/matcher.
 */
struct fragment
{
    struct vec in;
    struct vec out;
    char direct;
};

static void
fragment_init(struct fragment* f)
{
    vec_init(&f->in, sizeof(int));
    vec_init(&f->out, sizeof(int));
    f->direct = 0;
}
static void
fragment_deinit(struct fragment* f)
{
    vec_deinit(&f->in);
    vec_deinit(&f->out);
}
static int
node_duplicate(int node_idx, struct vec* nodes, struct hm* index_map)
{
    struct nfa_node* node;
    struct nfa_node* new_node;
    int* hm_value;

    switch (hm_insert(index_map, &node_idx, (void**)&hm_value))
    {
        case 0: return 0;
        case 1: *hm_value = vec_count(nodes); break;
        default: return -1;
    }

    node = vec_get(nodes, node_idx);
    new_node = vec_emplace(nodes);
    if (new_node == NULL)
        return -1;
    vec_init(&new_node->next, sizeof(int));
    if (vec_push_vec(&new_node->next, &node->next) < 0)
        return -1;
    new_node->matcher = node->matcher;

    VEC_FOR_EACH(&node->next, int, conn)
        if (node_duplicate(*conn, nodes, index_map) < 0)
            return 1;
    VEC_END_EACH

    return 0;
}
static int
fragment_duplicate(struct fragment* dst, const struct fragment* src, struct vec* nodes)
{
    struct hm index_map;

    /* map indices of old nodes to the newly inserted nodes */
    if (hm_init(&index_map, sizeof(int), sizeof(int)) < 0)
        return -1;
    VEC_FOR_EACH(&src->in, int, conn)
        node_duplicate(*conn, nodes, &index_map);
    VEC_END_EACH

    /* Fix transitions of all new nodes */
    HM_FOR_EACH(&index_map, int, int, src_idx, dst_idx)
        struct nfa_node* dst_node = vec_get(nodes, *dst_idx);
        VEC_FOR_EACH(&dst_node->next, int, conn)
            *conn = *(int*)hm_find(&index_map, conn);
        VEC_END_EACH
    HM_END_EACH

    /* Fragment inputs/outputs */
    VEC_FOR_EACH(&src->in, int, conn)
        if (vec_push(&dst->in, hm_find(&index_map, conn)) < 0)
            goto fail;
    VEC_END_EACH
    VEC_FOR_EACH(&src->out, int, conn)
        if (vec_push(&dst->out, hm_find(&index_map, conn)) < 0)
            goto fail;
    VEC_END_EACH

    hm_deinit(&index_map);
    return 0;

fail:
    hm_deinit(&index_map);
    return -1;
}

static int
nfa_compile_recurse(
    const struct ast* ast,
    int n,
    struct vec* nodes,
    struct vec* fstack,
    struct vec* qstack)
{
    switch (ast->nodes[n].info.type)
    {
        case AST_STATEMENT: {
            struct fragment* right;
            struct fragment* left;

            if (nfa_compile_recurse(ast, ast->nodes[n].statement.child, nodes, fstack, qstack) < 0) return -1;
            if (nfa_compile_recurse(ast, ast->nodes[n].statement.next, nodes, fstack, qstack) < 0) return -1;
            if (vec_count(fstack) < 2)
            {
                log_err("Failed to compile AST: Incomplete statement\n");
                return -1;
            }

            /*
             * Connect all outgoing connections of the left-hand fragment
             * with the incoming connections of the right-hand fragment.
             *
             *       ___        ___
             *   o--|   |--  --|   |--o
             *   o--|___|--  --|___|--o
             *      Left       Right
             */
            right = vec_get_back(fstack, 1);
            left = vec_get_back(fstack, 2);
            VEC_FOR_EACH(&left->out, int, left_out)
                VEC_FOR_EACH(&right->in, int, right_in)
                    struct nfa_node* node = vec_get(nodes, *left_out);
                    if (vec_push(&node->next, right_in) < 0)
                        return -1;
                VEC_END_EACH
            VEC_END_EACH

            /*
             * The left fragment is replace by the result of combining the two
             * fragments, while the right fragment is destroyed. Thus, the
             * outgoing connections of the right-hand fragment are now the
             * outgoing connections of the combined fragment.
             *
             * In the special case of the right-hand fragment having direct
             * connections between its inputs and outputs, the outputs of the
             * left-hand fragment are visible on the output side of the right-hand
             * fragment, and thus need to be merged instead of replaced.
             */
            if (right->direct)
                vec_push_vec(&left->out, &right->out);
            else
                vec_steal_vector(&left->out, &right->out);

            /*
             * In the special case of the left-hand fragment having direct
             * connections between its inputs and outputs, the inputs of the
             * right-hand fragment are visible on the input side of the left-hand
             * fragment, and thus need to be merged instead of replaced.
             */
            if (left->direct)
            {
                VEC_FOR_EACH(&right->in, int, conn)
                    vec_push(&left->in, conn);
                VEC_END_EACH
                left->direct = 0;
            }

            /* Right-hand fragment has been completely merged, destroy it */
            fragment_deinit(vec_pop(fstack));
        } break;

        case AST_REPETITION: {
            struct fragment* f;
            int min_reps, max_reps;
            if (nfa_compile_recurse(ast, ast->nodes[n].repetition.child, nodes, fstack, qstack) < 0) return -1;
            if (vec_count(fstack) < 1)
            {
                log_err("Failed to compile AST: Incomplete repetition\n");
                return -1;
            }

            f = vec_back(fstack);
            min_reps = ast->nodes[n].repetition.min_reps;
            max_reps = ast->nodes[n].repetition.max_reps;

            /* Invalid values */
            if (min_reps < 0)
            {
                log_err("Cannot repeat from \"%d\" times\n", min_reps);
                return -1;
            }

            /*
             * If the minimum repetition count is 0, then create a direct
             * connection from input to output.
             */
            if (min_reps == 0 || max_reps == 0)
                f->direct = 1;

            /* No upper bound, e.g. ".*" */
            if (max_reps == -1)
            {
                int n;
                struct vec fragments;

                /*
                 * We will need min-1 duplicates of the current fragment in order
                 * to implement the repetition logic.
                 */
                vec_init(&fragments, sizeof(struct fragment));
                for (n = 1; n < min_reps; ++n)
                {
                    struct fragment* new_fragment = vec_emplace(&fragments);
                    if (new_fragment == NULL)
                        goto dup_fragments_failed;
                    fragment_init(new_fragment);

                    if (fragment_duplicate(new_fragment, f, nodes) < 0)
                        goto dup_fragments_failed;
                }

                /* Add repeat to fragment */
                VEC_FOR_EACH(&f->out, int, out)
                    VEC_FOR_EACH(&f->in, int, in)
                        struct nfa_node* node = vec_get(nodes, *out);
                        if (vec_push(&node->next, in) < 0)
                            goto dup_fragments_failed;
                    VEC_END_EACH
                VEC_END_EACH

                /* Wire up outputs among duplicates */
                for (n = 1; n < vec_count(&fragments); ++n)
                {
                    struct fragment* f1 = vec_get(&fragments, n - 1);
                    struct fragment* f2 = vec_get(&fragments, n);
                    VEC_FOR_EACH(&f1->in, int, in)
                        VEC_FOR_EACH(&f2->out, int, out)
                            struct nfa_node* node = vec_get(nodes, *out);
                            if (vec_push(&node->next, in) < 0)
                                goto dup_fragments_failed;
                        VEC_END_EACH
                    VEC_END_EACH
                }

                if (vec_count(&fragments) > 0)
                {
                    struct fragment* f0 = vec_front(&fragments);
                    struct fragment* fn = vec_back(&fragments);

                    VEC_FOR_EACH(&f->in, int, in)
                        VEC_FOR_EACH(&f0->out, int, out)
                            struct nfa_node* node = vec_get(nodes, *out);
                            if (vec_push(&node->next, in) < 0)
                                goto dup_fragments_failed;
                        VEC_END_EACH
                    VEC_END_EACH

                    vec_steal_vector(&f->in, &fn->in);
                }

                VEC_FOR_EACH(&fragments, struct fragment, fragment)
                    fragment_deinit(fragment);
                VEC_END_EACH
                vec_deinit(&fragments);
                break;

            dup_fragments_failed:
                VEC_FOR_EACH(&fragments, struct fragment, fragment)
                    fragment_deinit(fragment);
                VEC_END_EACH
                vec_deinit(&fragments);
                return -1;
            }
            else  /* With upper bound */
            {
                int n;
                struct vec fragments;

                /* Invalid values */
                if (max_reps < 0 || min_reps > max_reps)
                {
                    log_err("Cannot repeat from \"%d\" to \"%d\" times\n", min_reps, max_reps);
                    return -1;
                }

                /* Special case if maxreps is 0, remove all connections */
                if (max_reps == 0)
                {
                    vec_clear(&f->in);
                    vec_clear(&f->out);
                    break;
                }

                // Nothing to do
                if (max_reps == 1)
                    break;

                /*
                 * We will need max-1 duplicates of the current fragment to
                 * implement the repitition logic.
                 */
                vec_init(&fragments, sizeof(struct fragment));
                for (n = 1; n != max_reps; ++n)
                {
                    struct fragment* new_fragment = vec_emplace(&fragments);
                    if (new_fragment == NULL)
                        goto dup_fragments_failed2;
                    fragment_init(new_fragment);

                    if (fragment_duplicate(new_fragment, f, nodes) < 0)
                        goto dup_fragments_failed2;
                }

                /* Wire up outputs among duplicates */
                for (n = 1; n < vec_count(&fragments); ++n)
                {
                    struct fragment* f1 = vec_get(&fragments, n - 1);
                    struct fragment* f2 = vec_get(&fragments, n);
                    VEC_FOR_EACH(&f1->in, int, in)
                        VEC_FOR_EACH(&f2->out, int, out)
                            struct nfa_node* node = vec_get(nodes, *out);
                            if (vec_push(&node->next, in) < 0)
                                goto dup_fragments_failed2;
                        VEC_END_EACH
                    VEC_END_EACH
                }

                /* Wire up outputs to original fragment */
                {
                    struct fragment* f0 = vec_front(&fragments);
                    VEC_FOR_EACH(&f0->out, int, out)
                        VEC_FOR_EACH(&f->in, int, in)
                            struct nfa_node* node = vec_get(nodes, *out);
                            if (vec_push(&node->next, in) < 0)
                                goto dup_fragments_failed2;
                        VEC_END_EACH
                    VEC_END_EACH
                }

                /* Wire up inputs to original fragment */
                if (min_reps > 1)
                    vec_clear(&f->in);
                for (n = max(0, min_reps - 2); n != max_reps - 1; ++n)
                {
                    struct fragment* fn = vec_get(&fragments, n);
                    VEC_FOR_EACH(&fn->in, int, in)
                        if (vec_push(&f->in, in) < 0)
                            goto dup_fragments_failed2;
                    VEC_END_EACH
                }

                VEC_FOR_EACH(&fragments, struct fragment, fragment)
                    fragment_deinit(fragment);
                VEC_END_EACH
                vec_deinit(&fragments);
                break;

            dup_fragments_failed2:
                VEC_FOR_EACH(&fragments, struct fragment, fragment)
                    fragment_deinit(fragment);
                VEC_END_EACH
                vec_deinit(&fragments);
                return -1;
            }
        } break;

        case AST_UNION: {
            struct fragment* right;
            struct fragment* left;

            if (nfa_compile_recurse(ast, ast->nodes[n].union_.child, nodes, fstack, qstack) < 0) return -1;
            if (nfa_compile_recurse(ast, ast->nodes[n].union_.next, nodes, fstack, qstack) < 0) return -1;
            if (vec_count(fstack) < 2)
            {
                log_err("Failed to compile AST: Incomplete union\n");
                return -1;
            }

            right = vec_get_back(fstack, 1);
            left = vec_get_back(fstack, 2);

            if (vec_push_vec(&left->in, &right->in) < 0) return -1;
            if (vec_push_vec(&left->out, &right->out) < 0) return -1;
            left->direct = (left->direct || right->direct);

            /* Right-hand fragment has been completely merged, destroy it */
            fragment_deinit(vec_pop(fstack));
        } break;

        case AST_INVERSION: {
            if (nfa_compile_recurse(ast, ast->nodes[n].inversion.child, nodes, fstack, qstack) < 0) return -1;
        } break;

        case AST_WILDCARD: {
            int in, out;
            struct nfa_node* node;
            struct fragment* f = vec_emplace(fstack);
            if (f == NULL)
                return -1;
            fragment_init(f);

            in = vec_count(nodes);
            out = vec_count(nodes);
            if (vec_push(&f->in, &in) < 0) return -1;
            if (vec_push(&f->out, &out) < 0) return -1;

            if ((node = vec_emplace(nodes)) == NULL) return -1;
            vec_init(&node->next, sizeof(int));
            node->matcher = match_wildcard();
        } break;

        case AST_LABEL: {
            uint64_t motion;
            struct nfa_node* node;
            struct fragment* f;
            struct str_view label;
            int in, out;

            /* Assume label is a hex value */
            label = strlist_to_view(&ast->labels, ast->nodes[n].label.label);
            if (!cstr_starts_with(label, "0x") || str_hex_to_u64(label, &motion) != 0)
            {
                /* Assume label is a hash40 string */
                motion = hash40_str(label);
            }

            f = vec_emplace(fstack);
            if (f == NULL)
                return -1;
            fragment_init(f);

            in = vec_count(nodes);
            out = vec_count(nodes);
            if (vec_push(&f->in, &in) < 0) return -1;
            if (vec_push(&f->out, &out) < 0) return -1;

            if ((node = vec_emplace(nodes)) == NULL) return -1;
            vec_init(&node->next, sizeof(int));
            node->matcher = match_motion(motion);
        } break;

        case AST_MOTION: {
            struct nfa_node* node;
            struct fragment* f;
            int in, out;

            f = vec_emplace(fstack);
            if (f == NULL)
                return -1;
            fragment_init(f);

            in = vec_count(nodes);
            out = vec_count(nodes);
            if (vec_push(&f->in, &in) < 0) return -1;
            if (vec_push(&f->out, &out) < 0) return -1;

            if ((node = vec_emplace(nodes)) == NULL) return -1;
            vec_init(&node->next, sizeof(int));
            node->matcher = match_motion(ast->nodes[n].motion.motion);
        } break;

        case AST_CONTEXT_QUALIFIER: {
            if (nfa_compile_recurse(ast, ast->nodes[n].union_.child, nodes, fstack, qstack) < 0)return -1;
        } break;
    }

    return 0;
}

int
nfa_compile(struct nfa_graph* nfa, const struct ast* ast)
{
    struct vec fragment_stack;
    struct vec qualifier_stack;
    struct vec nodes;
    struct nfa_node* entry_node;
    struct fragment* final_fragment;
    int ret = -1;

    vec_init(&nodes, sizeof(struct nfa_node));
    vec_init(&fragment_stack, sizeof(struct fragment));
    vec_init(&qualifier_stack, sizeof(uint8_t));

    /*
     * The "entry" node is a special node that is not evaluated, but merely
     * acts as a container for holding all start nodes. This is necessary
     * because NFAs can have multiple start nodes. By convention, we make the
     * entry node at index 0, which is why it is inserted now.
     */
    entry_node = vec_emplace(&nodes);
    if (entry_node == NULL)
        goto out;
    vec_init(&entry_node->next, sizeof(int));
    entry_node->matcher = match_none();

    if (nfa_compile_recurse(ast, 0, &nodes, &fragment_stack, &qualifier_stack) != 0)
        goto out;

    /*
     * There should be a single fragment left on the stack, which is the final
     * state machine. The only thing left to do is to wire up the dangling
     * inputs/outputs of this final fragment.
     */
    if (vec_count(&fragment_stack) != 1)
        goto out;
    final_fragment = vec_front(&fragment_stack);

    /* Remove duplicate state transitions */
    VEC_FOR_EACH(&nodes, struct nfa_node, node)
        int a, b;
        for (a = 0; a != vec_count(&node->next); ++a)
            for (b = a + 1; b < vec_count(&node->next); ++b)
                if (*(int*)vec_get(&node->next, a) == *(int*)vec_get(&node->next, b))
                {
                    vec_erase_index(&node->next, b);
                    b--;
                }
    VEC_END_EACH

    /*
     * Patch in start nodes into the "entry node" we created earlier
     */
    entry_node = vec_front(&nodes);
    vec_steal_vector(&entry_node->next, &final_fragment->in);

    /*
     * Mark all nodes with dangling outgoing transitions with the accept
     * condition
     */
    VEC_FOR_EACH(&final_fragment->out, int, out_node)
        struct nfa_node* node = vec_get(&nodes, *out_node);
        node->matcher.is_accept = 1;
    VEC_END_EACH

    nfa->node_count = vec_count(&nodes);
    nfa->nodes = vec_steal_data(&nodes);
    ret = 0;

out:
    vec_deinit(&qualifier_stack);
    VEC_FOR_EACH(&fragment_stack, struct fragment, f)
        fragment_deinit(f);
    VEC_END_EACH
    vec_deinit(&fragment_stack);
    VEC_FOR_EACH(&nodes, struct nfa_node, node)
        vec_deinit(&node->next);
    VEC_END_EACH
    vec_deinit(&nodes);
    return ret;
}

void
nfa_deinit(struct nfa_graph* nfa)
{
    int n;
    for (n = 0; n != nfa->node_count; ++n)
        vec_deinit(&nfa->nodes[n].next);
    mem_free(nfa->nodes);
}

int
nfa_export_dot(const struct nfa_graph* nfa, const char* file_name)
{
    int n;
    FILE* fp = fopen(file_name, "w");
    if (fp == NULL)
        goto open_file_failed;

    fprintf(fp, "digraph {\n");
    fprintf(fp, "start [shape=\"point\", label=\"\", width=\"0.25\"];\n");

    for (n = 1; n < nfa->node_count; ++n)
    {
        fprintf(fp, "n%d [label=\"%d\"", n, n);
        if (nfa->nodes[n].matcher.is_accept)
            fprintf(fp, ", shape=\"doublecircle\"");
        fprintf(fp, "];\n");
    }

    for (n = 0; n != nfa->node_count; ++n)
    {
        VEC_FOR_EACH(&nfa->nodes[n].next, int, e)
            const struct matcher* m = &nfa->nodes[*e].matcher;
            if (n == 0)
                fprintf(fp, "start -> n%d [", *e);
            else
                fprintf(fp, "n%d -> n%d [", n, *e);

            fprintf(fp, "label=\"");
            if (matches_motion(m))
                fprintf(fp, "0x%" PRIx64, ((uint64_t)m->symbol.motionh << 32) | ((uint64_t)m->symbol.motionl));
            else if (matches_wildcard(m))
                fprintf(fp, "(.)");
            fprintf(fp, "\"];\n");
        VEC_END_EACH
    }

    fprintf(fp, "}\n");
    fclose(fp);
    return 0;

fail:
    fclose(fp);
open_file_failed:
    return -1;
}

#if 0
// ----------------------------------------------------------------------------
Matcher Matcher::start()
{
    return Matcher(
        rfcommon::FighterMotion::makeInvalid(),
        rfcommon::FighterStatus::makeInvalid(),
        0,
        0
    );
}

// ----------------------------------------------------------------------------
Matcher Matcher::wildCard(uint8_t ctxQualFlags)
{
    return Matcher(
        rfcommon::FighterMotion::makeInvalid(),
        rfcommon::FighterStatus::makeInvalid(),
        ctxQualFlags,
        0
    );
}

// ----------------------------------------------------------------------------
Matcher Matcher::motion(rfcommon::FighterMotion motion, uint8_t ctxQualFlags)
{
    return Matcher(
        motion,
        rfcommon::FighterStatus::makeInvalid(),
        ctxQualFlags,
        MATCH_MOTION
    );
}

// ----------------------------------------------------------------------------
Matcher::Matcher(
        rfcommon::FighterMotion motion,
        rfcommon::FighterStatus status,
        uint8_t ctxQualFlags,
        uint8_t matchFlags)
    : motion_(motion)
    , status_(status)
    , ctxQualFlags_(ctxQualFlags)
    , matchFlags_(matchFlags)
{}

// ----------------------------------------------------------------------------
bool Matcher::matches(const State& state) const
{
    if (!!(matchFlags_ & MATCH_STATUS))
        if (state.status != status_)
            return false;

    if (!!(matchFlags_ & MATCH_MOTION))
        if (state.motion != motion_)
            return false;

    if (ctxQualFlags_)
    {
        bool onHit = state.opponentInHitlag();
        bool onShield = state.opponentInShieldlag();
        bool onWhiff = !state.opponentInHitlag() && !state.opponentInShieldlag();
        uint8_t compare =
                (static_cast<uint8_t>(onHit) << 0)
              | (static_cast<uint8_t>(onWhiff) << 1)
              | (static_cast<uint8_t>(onShield) << 2);
        if (!(ctxQualFlags_ & compare))
            return false;
    }

    return true;
}

// ----------------------------------------------------------------------------
static void duplicateMatchers(int idx, rfcommon::Vector<Matcher>* matchers, rfcommon::HashMap<int, int>* indexMap)
{
    if (indexMap->insertIfNew(idx, matchers->count()) == indexMap->end())
        return;

    matchers->push(Matcher(matchers->at(idx)));
    for (int i : matchers->at(idx).next)
        duplicateMatchers(i, matchers, indexMap);
}
static Fragment duplicateFragment(const Fragment& f, rfcommon::Vector<Matcher>* matchers)
{
    Fragment dup;
    rfcommon::HashMap<int, int> indexMap;  // Map indices of old matchers to the newly inserted matchers
    for (int i : f.in)
        duplicateMatchers(i, matchers, &indexMap);

    // Fix transitions
    for (auto kv : indexMap)
    {
        Matcher& newMatcher = matchers->at(kv.value());
        for (int& i : newMatcher.next)
            i = indexMap.find(i)->value();
    }

    // Fragment inputs/outputs
    for (int i : f.in)
        dup.in.push(indexMap.find(i)->value());
    for (int i : f.out)
        dup.out.push(indexMap.find(i)->value());

    return dup;
}

// ----------------------------------------------------------------------------
static bool compileASTRecurse(
        const QueryASTNode* node,
        const rfcommon::MotionLabels* labels,
        rfcommon::FighterID fighterID,
        rfcommon::String* error,
        rfcommon::Vector<Matcher>* matchers,
        rfcommon::Vector<rfcommon::SmallVector<rfcommon::FighterMotion, 4>>* mergeMotions,
        rfcommon::SmallVector<Fragment, 16>* fstack,
        rfcommon::SmallVector<uint8_t, 16>* qstack)
{
    // OR all flags currently on the stack
    auto calcContextQualifierFlags = [](rfcommon::SmallVector<uint8_t, 16>* qstack) -> uint8_t {
        uint8_t contextQualifierFlags = 0;
        for (uint8_t flag : *qstack)
        {
            if (!!(flag & QueryASTNode::HIT))
                contextQualifierFlags |= Matcher::HIT;
            if (!!(flag & QueryASTNode::WHIFF))
                contextQualifierFlags |= Matcher::WHIFF;
            if (!!(flag & QueryASTNode::OS))
                contextQualifierFlags |= Matcher::SHIELD;
        }
        return contextQualifierFlags;
    };

    switch (node->type)
    {
    case QueryASTNode::STATEMENT: {
        if (!compileASTRecurse(node->statement.child, labels, fighterID, error, matchers, mergeMotions, fstack, qstack)) return false;
        if (!compileASTRecurse(node->statement.next, labels, fighterID, error, matchers, mergeMotions, fstack, qstack)) return false;
        if (fstack->count() < 2)
        {
            *error = "Incomplete statement";
            return false;
        }

        Fragment& right = fstack->back(1);
        Fragment& left = fstack->back(2);
        for (int o : left.out)
            for (int i : right.in)
                matchers->at(o).next.push(i);

        if (right.bridge)
            left.out.push(std::move(right.out));
        else
            left.out = std::move(right.out);

        if (left.bridge)
        {
            for (int i : right.in)
                left.in.push(i);
            left.bridge = false;
        }

        fstack->pop();
    } break;

    case QueryASTNode::REPITITION: {
        if (!compileASTRecurse(node->repitition.child, labels, fighterID, error, matchers, mergeMotions, fstack, qstack)) return false;
        if (fstack->count() < 1)
        {
            *error = "Incomplete repitition";
            return false;
        }

        // Invalid values
        if (node->repitition.minreps < 0)
        {
            *error = "Cannot repeat from \"" + rfcommon::String::decimal(node->repitition.minreps) + "\" times";
            return false;
        }

        Fragment& f = fstack->back();

        // Mark the entire fragment as optional if minreps or maxreps is 0
        if (node->repitition.minreps == 0 || node->repitition.maxreps == 0)
            f.bridge = true;

        if (node->repitition.maxreps == -1)
        {
            // We will need min-1 duplicates of the current fragment to implement the
            // repitition logic
            rfcommon::SmallVector<Fragment, 8> fragments;
            for (int n = 1; n < node->repitition.minreps; ++n)
                fragments.push(duplicateFragment(f, matchers));

            // Add repeat to fragment
            for (int a : f.out)
                for (int b : f.in)
                    matchers->at(a).next.push(b);

            // Wire up outputs among duplicates
            for (int n = 1; n < fragments.count(); ++n)
            {
                for (int o : fragments[n].out)
                    for (int i : fragments[n-1].in)
                        matchers->at(o).next.push(i);
            }

            if (fragments.count() > 0)
            {
                for (int i : f.in)
                    for (int o : fragments[0].out)
                        matchers->at(o).next.push(i);
                f.in = std::move(fragments.back().in);
            }
        }
        else
        {
            // Invalid values
            if (node->repitition.maxreps < 0 || node->repitition.minreps > node->repitition.maxreps)
            {
                *error = "Cannot repeat from \"" +
                        rfcommon::String::decimal(node->repitition.minreps) +
                        "\" to \"" +
                        rfcommon::String::decimal(node->repitition.maxreps) +
                        "\" times";
                return false;
            }

            // Special case if maxreps is 0, remove all connections
            if (node->repitition.maxreps == 0)
            {
                f.in.clear();
                f.out.clear();
                break;
            }

            // Nothing to do
            if (node->repitition.maxreps == 1)
                break;

            // We will need max-1 duplicates of the current fragment to implement the
            // repitition logic
            rfcommon::SmallVector<Fragment, 8> fragments;
            for (int n = 1; n != node->repitition.maxreps; ++n)
                fragments.push(duplicateFragment(f, matchers));

            // Wire up outputs among duplicates
            for (int n = 1; n != fragments.count(); ++n)
            {
                for (int o : fragments[n].out)
                    for (int i : fragments[n-1].in)
                        matchers->at(o).next.push(i);
            }

            // Wire up outputs to original fragment
            for (int o : fragments[0].out)
                for (int i : f.in)
                    matchers->at(o).next.push(i);

            // Wire up inputs to original fragment
            if (node->repitition.minreps > 1)
                f.in.clear();
            for (int n = std::max(0, node->repitition.minreps - 2); n != node->repitition.maxreps - 1; ++n)
            {
                for (int i : fragments[n].in)
                    f.in.push(i);
            }
        }
    } break;

    case QueryASTNode::UNION: {
        if (!compileASTRecurse(node->union_.child, labels, fighterID, error, matchers, mergeMotions, fstack, qstack)) return false;
        if (!compileASTRecurse(node->union_.next, labels, fighterID, error, matchers, mergeMotions, fstack, qstack)) return false;
        if (fstack->count() < 2)
        {
            *error = "Incomplete union";
            return false;
        }

        Fragment& f1 = fstack->back(1);
        Fragment& f2 = fstack->back(2);

        f2.in.push(std::move(f1.in));
        f2.out.push(std::move(f1.out));
        f2.bridge = (f1.bridge || f2.bridge);

        fstack->pop();
    } break;

    case QueryASTNode::INVERSION:
        if (!compileASTRecurse(node->inversion.child, labels, fighterID, error, matchers, mergeMotions, fstack, qstack)) return false;
        break;

    case QueryASTNode::WILDCARD: {
        fstack->push({{matchers->count()}, {matchers->count()}});
        matchers->push(Matcher::wildCard(calcContextQualifierFlags(qstack)));
    } break;

    case QueryASTNode::LABEL: {
        uint8_t ctxtQualFlags = calcContextQualifierFlags(qstack);

        // Assume label is a user label and maps to one or more motion
        // values
        auto motions = labels->toMotions(fighterID, node->labels.label.cStr());
        if (motions.count() > 0)
        {
            Fragment& fragment = fstack->emplace();
            for (const auto& motion : motions)
            {
                fragment.in.push(matchers->count());
                fragment.out.push(matchers->count());
                matchers->push(Matcher::motion(motion, ctxtQualFlags));
            }

            // If the user label maps to multiple motions a, b, c, then
            // connect them such that it matches [abc]+
            if (motions.count() > 1)
            {
                for (int a = 1; a <= motions.count(); ++a)
                    for (int b = 1; b <= motions.count(); ++b)
                        matchers->back(a).next.push(matchers->count() - b);
            }

            // Store list of motions so they can be used to merge states in
            // matched sequences
            mergeMotions->push(std::move(motions));
            break;
        }

        // Assume label is actually a hash40 string and maps to a single motion
        // value
        auto motion = labels->toMotion(node->labels.label.cStr());
        if (motion.isValid())
        {
            fstack->push({{matchers->count()}, {matchers->count()}});
            matchers->push(Matcher::motion(motion, ctxtQualFlags));
            break;
        }

        // Assume string is hex describing a hash40 motion value
        motion = rfcommon::FighterMotion::fromHexString(node->labels.label.cStr());
        if (motion.isValid())
        {
            fstack->push({{matchers->count()}, {matchers->count()}});
            matchers->push(Matcher::motion(motion, ctxtQualFlags));
            break;
        }

        *error = rfcommon::String("Motion \"") + node->labels.label + "\" not found";
        return false;
    } break;

    case QueryASTNode::CONTEXT_QUALIFIER: {
        qstack->push(node->contextQualifier.flags);
        if (!compileASTRecurse(node->contextQualifier.child, labels, fighterID, error, matchers, mergeMotions, fstack, qstack)) return false;
        qstack->pop();
    } break;
    }

    return true;
}

// ----------------------------------------------------------------------------
Query* Query::compileAST(const QueryASTNode* ast, const rfcommon::MotionLabels* labels, rfcommon::FighterID fighterID, rfcommon::String* error)
{
    std::unique_ptr<Query> query(new Query);
    query->matchers_.push(Matcher::start());

    rfcommon::SmallVector<Fragment, 16> fstack;  // "fragment stack"
    rfcommon::SmallVector<uint8_t, 16> qstack;   // "qualifier stack"

    if (!compileASTRecurse(ast, labels, fighterID, error, &query->matchers_, &query->mergeableLabels_, &fstack, &qstack))
        return nullptr;
    if (fstack.count() != 1)
        return nullptr;

    // Patch in starting matcher, which is always at index 0
    for (int i : fstack[0].in)
        query->matchers_[0].next.push(i);

    // Mark all matchers with dangling outgoing transitions with the accept condition
    for (int i : fstack[0].out)
        query->matchers_[i].setAcceptCondition();

    // Removes duplicate state transitions
    for (Matcher& matcher : query->matchers_)
    {
        try_again:
        for (int a = 0; a != matcher.next.count(); ++a)
            for (int b = a + 1; b < matcher.next.count(); ++b)
                if (matcher.next[a] == matcher.next[b])
                {
                    matcher.next.erase(b);
                    goto try_again;
                }
    }

    return query.release();
}

// ----------------------------------------------------------------------------
// Given a list of player states, begin executing the NFA on the state at "startIdx"
// either until it completes successfully, or "endIdx" is reached.
//
// Returns "startIdx" if a match was not found, otherwise returns the index after
// the last successfully matched state.
//
// This function expects "clist", "nlist" and "lastLists" to be arrays of size
// "matchers_.count()". They don't have to be initialized.
static int runNFA(
    const States& states,
    const rfcommon::Vector<Matcher>& matchers,
    const int startIdx,
    const int endIdx,
    int* clist,
    int* nlist,
    int* lastLists)
{
    //const int maxMatchLength = 500000;
    int stateIdx = startIdx;

    // Prepare current and next state lists. Current list contains all
    // start states of the NFA, which can be more than 1. We (mis-)use the
    // first matcher as a container for all of the starting states.
    int listid = 0;
    int clistLen = 0;
    int nlistLen = 0;
    for (int i : matchers[0].next)
        clist[clistLen++] = i;

    // List IDs start at 0
    memset(lastLists, 0, sizeof(*lastLists) * matchers.count());

    while (true)
    {
        // We use a running "list ID" to keep track of which matchers were
        // already visited. This avoids adding the same matcher to nlist
        // more than once
        listid++;
        nlistLen = 0;

        // Process each matcher in the current list to see if any node in the
        // NFA matches the current player state
        for (int clistIdx = 0; clistIdx != clistLen; ++clistIdx)
        {
            const Matcher& node = matchers[clist[clistIdx]];

            // Does not match -> don't add to nlist
            if (node.matches(states[stateIdx]) == false)
                continue;

            // The current NFA node matched, which means we need to explore
            // all of its children. Only add child to nlist if we haven't
            // already, by checking the list ID.
            for (int nextMatcherIdx : node.next)
                if (lastLists[nextMatcherIdx] != listid)
                {
                    lastLists[nextMatcherIdx] = listid;
                    nlist[nlistLen++] = nextMatcherIdx;
                }

            // We have run out of states to match
            if (stateIdx + 1 >= endIdx /*|| stateIdx >= startIdx + maxMatchLength*/)
            {
                if (node.isAcceptCondition())
                    return stateIdx + 1;  // Success, return the end of the matched range = last matched index + 1

                // the state machine is not complete, which means
                // we only have a partial match -> failure
                return startIdx;
            }

            if (node.isAcceptCondition())
            {
                // If there are still children that can match, continue
                for (int nextMatcherIdx : node.next)
                    if (matchers[nextMatcherIdx].matches(states[stateIdx + 1]))
                        goto skip_return;

                // Success, return the end of the matched range = last matched index + 1
                return stateIdx + 1;
            skip_return:;
            }
        }

        if (nlistLen == 0
            /*|| stateIdx >= startIdx + maxMatchLength*/
            || stateIdx + 1 >= endIdx)
        {
            return startIdx;  // Failed to match anything
        }

        // Advance
        stateIdx++;
        std::swap(clist, nlist);
        std::swap(clistLen, nlistLen);
    }
}

// ----------------------------------------------------------------------------
#define STACKMEMSIZE 64
Range Query::findFirst(const States& states, const Range& range) const
{
    int liststackmem[STACKMEMSIZE * 3];
    int* listmem = matchers_.count() > STACKMEMSIZE ? (int*)malloc(sizeof(int) * matchers_.count()) : liststackmem;

    // Nothing to do
    /*
    if (matchers_.count() == 0 || matchers_[0].next.count() == 0)
        return Range(0, 0);*/

    // Go through each state and try to run the NFA on it
    for (int startIdx = range.startIdx; startIdx < range.endIdx; ++startIdx)
    {
        const int endIdx = runNFA(states, matchers_, startIdx, range.endIdx, listmem, listmem + matchers_.count(), listmem + matchers_.count() * 2);
        if (endIdx > startIdx)
        {
            if (matchers_.count() > STACKMEMSIZE)
                free(listmem);
            return Range(startIdx, endIdx);
        }
    }

    if (matchers_.count() > STACKMEMSIZE)
        free(listmem);

    return Range(0, 0);
}

// ----------------------------------------------------------------------------
rfcommon::Vector<Range> Query::findAll(const States& states, const Range& range) const
{
    rfcommon::Vector<Range> result;
    int liststackmem[STACKMEMSIZE * 3];
    int* listmem = matchers_.count() > STACKMEMSIZE ? (int*)malloc(sizeof(int) * matchers_.count()) : liststackmem;

    // Nothing to do
    /*
    if (matchers_.count() == 0 || matchers_[0].next.count() == 0)
        return result;*/

    // We search the sequence of states rather than the graph, because we are
    // interested in matching sequences of decisions
    for (int startIdx = range.startIdx; startIdx < range.endIdx; ++startIdx)
    {
        const int endIdx = runNFA(states, matchers_, startIdx, range.endIdx, listmem, listmem + matchers_.count(), listmem + matchers_.count() * 2);
        if (endIdx > startIdx)
        {
            result.emplace(startIdx, endIdx);
            startIdx = endIdx - 1;
        }
    }

    if (matchers_.count() > STACKMEMSIZE)
        free(listmem);

    return result;
}

// ----------------------------------------------------------------------------
rfcommon::Vector<Range> Query::findAllOverlapping(
        const Query* otherQuery,
        const States& states, const Range& range,
        const States& otherStates, const Range& otherRange) const
{
    using rfcommon::FrameIndex;

    rfcommon::Vector<Range> result;
    int liststackmem[STACKMEMSIZE * 3];
    int* listmem = matchers_.count() > STACKMEMSIZE ? (int*)malloc(sizeof(int) * matchers_.count()) : liststackmem;

    int otherStartIdx = otherRange.startIdx;

    for (int startIdx = range.startIdx; startIdx < range.endIdx; ++startIdx)
    {
        // Run first search
        int endIdx = runNFA(
            states, matchers_,
            startIdx, range.endIdx,
            listmem, listmem + matchers_.count(), listmem + matchers_.count() * 2
        );
        if (endIdx == startIdx)
            continue;

        for (; otherStartIdx != otherRange.endIdx; ++otherStartIdx)
        {
            // The beginning of this second search is beyond the range
            // of the first search, so finding a union would be impossible
            if (endIdx < states.count() &&
                otherStates[otherStartIdx].sideData.frameIndex > states[endIdx].sideData.frameIndex)
                break;

            // Run second search
            const int otherEndIdx = runNFA(
                otherStates, otherQuery->matchers_,
                otherStartIdx, otherRange.endIdx,
                listmem, listmem + otherQuery->matchers_.count(), listmem + otherQuery->matchers_.count() * 2
            );
            if (otherEndIdx == otherStartIdx)
                continue;

            // Calculate intersection of ranges
            const FrameIndex start1 = states[startIdx].sideData.frameIndex;
            const FrameIndex start2 = otherStates[otherStartIdx].sideData.frameIndex;
            const FrameIndex end1 = endIdx < states.count() ?
                states[endIdx].sideData.frameIndex : FrameIndex::fromValue(std::numeric_limits<FrameIndex::Type>::max());
            const FrameIndex end2 = otherEndIdx < otherStates.count() ?
                otherStates[otherEndIdx].sideData.frameIndex : FrameIndex::fromValue(std::numeric_limits<FrameIndex::Type>::max());

            const FrameIndex frameStart = start1 > start2 ? start1 : start2;
            const FrameIndex frameEnd = end1 < end2 ? end1 : end2;

            // They do not intersect
            if (frameStart > frameEnd)
                continue;

            // Map range 2 indices to range 1 if necessary
            if (start2 > start1)
            {
                startIdx = std::lower_bound(
                    states.begin() + startIdx,
                    states.begin() + endIdx,
                    otherStates[otherStartIdx],
                    [](const State& a, const State& b) {
                        return a.sideData.frameIndex < b.sideData.frameIndex;
                    }
                ) - states.begin();
            }

            if (end2 < end1)
            {
                endIdx = std::lower_bound(
                    states.begin() + startIdx,
                    states.begin() + endIdx,
                    otherStates[otherEndIdx],
                    [](const State& a, const State& b) {
                        return a.sideData.frameIndex < b.sideData.frameIndex;
                    }
                ) - states.begin() + 1;
            }

            // Sometimes the difference in granularity of the two state arrays
            // is too great and the startIdx will equal the endIdx, even though
            // a valid intersection exist. A lot of code operates under the
            // assumption that all entries in the returned ranges satisfy
            // startIdx < endIdx. Try to relax the range a bit if this happens.
            if (startIdx == endIdx)
            {
                if (startIdx > 0)
                    startIdx--;
                else
                    endIdx++;
            }

            assert(endIdx <= states.count());
            result.emplace(startIdx, endIdx);
            startIdx = endIdx - 1;
        }
    }

    if (matchers_.count() > STACKMEMSIZE)
        free(listmem);

    return result;
}

// ----------------------------------------------------------------------------
void Query::exportDOT(const char* filename, const rfcommon::MotionLabels* labels, rfcommon::FighterID fighterID)
{
    FILE* fp = fopen(filename, "w");
    fprintf(fp, "digraph query {\n");

    auto toHash40OrHex = [labels](rfcommon::FighterMotion motion) -> rfcommon::String {
        if (const char* h40 = labels->toHash40(motion))
            return h40;
        return motion.toHex();
    };

    for (int i = 0; i != matchers_.count(); ++i)
    {
        rfcommon::String label =
                i == 0 ? "start" :
                matchers_[i].isWildcard() ? "." :
                toHash40OrHex(matchers_[i].motion_);
        const char* color = matchers_[i].isAcceptCondition() ? "red" : "black";
        fprintf(fp, "m%d [shape=\"record\",color=\"%s\",label=\"%s", i, color, label.cStr());

        if (matchers_[i].inContext(Matcher::HIT))
            fprintf(fp, " | HIT");
        if (matchers_[i].inContext(Matcher::WHIFF))
            fprintf(fp, " | WHIFF");
        if (matchers_[i].inContext(Matcher::SHIELD))
            fprintf(fp, " | OS");
        fprintf(fp, "\"];\n");
    }

    for (int i = 0; i != matchers_.count(); ++i)
    {
        for (int e : matchers_[i].next)
            fprintf(fp, "m%d -> m%d;\n", i, e);
    }

    fprintf(fp, "}\n");
    fclose(fp);
}
#endif
