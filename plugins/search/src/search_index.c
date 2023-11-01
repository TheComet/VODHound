#include "search/search_index.h"
#include "search/symbol.h"

#include "vh/frame_data.h"

#include <stddef.h>
#include <stdio.h>

struct fighter_index
{
    struct vec symbols;
    struct vec frame_start_idxs;
    struct vec frame_end_idxs;
};

void
search_index_init(struct search_index* index)
{
    vec_init(&index->fighters, sizeof(struct fighter_index));
}

void
search_index_deinit(struct search_index* index)
{
    search_index_clear(index);
    vec_deinit(&index->fighters);
}

static int
search_index_build_fighter(struct fighter_index* fidx, int me_idx, const struct frame_data* fdata)
{
    int frame;
    for (frame = 0; frame != fdata->frame_count; ++frame)
    {
        int op_idx = fdata->fighter_count - me_idx - 1;
        int prev_frame = frame > 0 ? frame - 1 : 0;

        char me_dead = 0;  /* TODO */
        char me_hitlag =
            (fdata->flags[op_idx][frame] & FRAME_DATA_ATTACK_CONNECTED) &&
            (fdata->hitstun[me_idx][frame] == fdata->hitstun[me_idx][prev_frame]);
        char me_hitstun =
            !me_hitlag &&
            (fdata->hitstun[me_idx][frame] > 0);
        char me_shieldlag =
            (fdata->status[me_idx][frame] == 30);  /* FIGHTER_STATUS_KIND_GUARD_DAMAGE */
        char me_rising =
            (fdata->posy[me_idx][frame] > fdata->posy[me_idx][prev_frame]);
        char me_falling =
            (fdata->posy[me_idx][frame] < fdata->posy[me_idx][prev_frame]);
        char me_buried = 0;   /* TODO */
        char me_phantom = 0;  /* TODO */

        char op_dead = 0;  /* TODO */
        char op_hitlag =
            (fdata->flags[me_idx][frame] & FRAME_DATA_ATTACK_CONNECTED) &&
            (fdata->hitstun[op_idx][frame] == fdata->hitstun[op_idx][prev_frame]);
        char op_hitstun =
            !op_hitlag &&
            (fdata->hitstun[op_idx][frame] > 0);
        char op_shieldlag =
            (fdata->status[op_idx][frame] == 30);  /* FIGHTER_STATUS_KIND_GUARD_DAMAGE */
        char op_rising =
            (fdata->posy[op_idx][frame] > fdata->posy[op_idx][prev_frame]);
        char op_falling =
            (fdata->posy[op_idx][frame] < fdata->posy[op_idx][prev_frame]);
        char op_buried = 0;   /* TODO */
        char op_phantom = 0;  /* TODO */

        /*
         * Only add a symbol if it is meaningfully different from the previously
         * added symbol.
         *
         * Currently, the granularity of symbols is based on the motion value.
         * We make sure that successive motion values are always different.
         *
         * The consequence of this is we'll need to merge some of the state
         * flags.
         */
        uint64_t motion = fdata->motion[me_idx][frame];
        if (vec_count(&fidx->symbols) > 0)
        {
            union symbol* last_sym = vec_back(&fidx->symbols);
            if (last_sym->motionl == (motion & 0xFFFFFFFF) &&
                last_sym->motionh == (motion >> 32))
            {
                /*
                 * It's possible that a move starts before it hits a shield or
                 * connects with the opponent. If this happpens, modify the
                 * flag on the existing symbol so it looks like the move always
                 * hit.
                 */
                if (me_hitlag)    last_sym->me_hitlag = 1;
                if (me_hitstun)   last_sym->me_hitstun = 1;
                if (me_shieldlag) last_sym->me_shieldlag = 1;
                if (op_hitlag)    last_sym->op_hitlag = 1;
                if (op_hitstun)   last_sym->op_hitstun = 1;
                if (op_shieldlag) last_sym->op_shieldlag = 1;

                /* It probably makes sense to handle rising and falling moves
                 * the same way */
                if (me_rising)    last_sym->me_rising = 1;
                if (me_falling)   last_sym->me_falling = 1;
                if (op_rising)    last_sym->op_rising = 1;
                if (op_falling)   last_sym->op_falling = 1;

                /* The last symbol had the same motion value as the current, skip */
                continue;
            }
        }

        union symbol* new_sym = vec_emplace(&fidx->symbols);
        int* frame_start_idx = vec_emplace(&fidx->frame_start_idxs);
        int* frame_end_idx = vec_emplace(&fidx->frame_end_idxs);
        if (new_sym == NULL || frame_start_idx == NULL || frame_end_idx == NULL)
            return -1;
        *new_sym = symbol_make(motion,
            me_dead, me_hitlag, me_hitstun, me_shieldlag, me_rising, me_falling, me_buried, me_phantom,
            op_dead, op_hitlag, op_hitstun, op_shieldlag, op_rising, op_falling, op_buried, op_phantom);
        *frame_start_idx = frame;
        *frame_end_idx = frame + 1;  /* Should be updated on the next iteration, but just in case */

        /* Update the previous symbol's frame end index */
        if (vec_count(&fidx->frame_end_idxs) > 1)
            *(int*)vec_get_back(&fidx->frame_end_idxs, 2) = frame;  /* 2nd last element */
    }

    /* Update the previous symbol's frame end index */
    if (vec_count(&fidx->frame_end_idxs) > 1)
        *(int*)vec_back(&fidx->frame_end_idxs) = fdata->frame_count;

    return 0;
}

int
search_index_build(struct search_index* index, const struct frame_data* fdata)
{
    int fighter;
    for (fighter = 0; fighter != fdata->fighter_count; ++fighter)
    {
        struct fighter_index* fidx = vec_emplace(&index->fighters);
        if (fidx == NULL)
            goto fail;
        vec_init(&fidx->symbols, sizeof(union symbol));
        vec_init(&fidx->frame_start_idxs, sizeof(int));
        vec_init(&fidx->frame_end_idxs, sizeof(int));
        if (search_index_build_fighter(vec_get(&index->fighters, fighter), fighter, fdata) < 0)
            goto fail;
    }

    return 0;

fail:
    search_index_clear(index);
    return -1;
}

void
search_index_clear(struct search_index* index)
{
    VEC_FOR_EACH(&index->fighters, struct fighter_index, fighter)
        vec_deinit(&fighter->symbols);
        vec_deinit(&fighter->frame_start_idxs);
        vec_deinit(&fighter->frame_end_idxs);
    VEC_END_EACH
    vec_clear(&index->fighters);
}

int
search_index_symbol_count(struct search_index* index, int fighter_idx)
{
    struct fighter_index* fighter = vec_get(&index->fighters, fighter_idx);
    return vec_count(&fighter->symbols);
}

const union symbol*
search_index_symbols(const struct search_index* index, int fighter_idx)
{
    struct fighter_index* fighter = vec_get(&index->fighters, fighter_idx);
    return vec_data(&fighter->symbols);
}
