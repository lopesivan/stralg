
#ifndef BWT_H
#define BWT_H

#include <remap.h>
#include <suffix_array.h>

struct bwt_table {
    // I don't add bwt_table here.
    // I don't need it for searching
    // after I have built the other
    // tables.
    size_t *c_table;
    size_t *o_table;
};

static inline size_t o_index(unsigned char a, size_t i,
                             struct suffix_array *sa)
{
    return a * sa->length + i;
}

// for these to work, sa must have been build
// from a remapped string.
void init_bwt_table   (struct bwt_table    *bwt_table,
                       struct suffix_array *sa,
                       struct remap_table  *remap_table);
void dealloc_bwt_table(struct bwt_table *bwt_table);

struct exact_bwt_match_iter {
    struct suffix_array *sa;
    size_t L;
    size_t i;
    size_t R;
};
struct exact_bwt_match {
    size_t pos;
};
void init_exact_bwt_match_iter   (struct exact_bwt_match_iter *iter,
                                  struct bwt_table            *bwt_table,
                                  struct suffix_array         *sa,
                                  const char                  *remapped_pattern);
bool next_exact_bwt_match        (struct exact_bwt_match_iter *iter,
                                  struct exact_bwt_match      *match);
void dealloc_exact_bwt_match_iter(struct exact_bwt_match_iter *iter);

#endif
