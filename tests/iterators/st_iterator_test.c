
#include <stralg.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define BUFFER_SIZE 1024

static char *match_string(size_t idx, const char *cigar)
{
    char *new_string = malloc(BUFFER_SIZE);
    sprintf(new_string, "%lu %s", idx, cigar);
    return new_string;
}

static void free_strings(string_vector *vec)
{
    for (int i = 0; i < vec->used; i++) {
        free(string_vector_get(vec, i));
    }
}



// MARK: Straightforward recursive implementation

struct search_data {
    struct suffix_tree *st;
    char *full_cigar_buf;
    char *cigar_buf;
    string_vector *results;
};

static void search_children(struct search_data *data,
                            struct suffix_tree_node *v,
                            char *cigar,
                            const char *p, int edits);

static void search_edge(struct search_data *data,
                        struct suffix_tree_node *v,
                        const char *x, const char *end,
                        const char *p,
                        char *cigar, int edits)
{
    if (edits < 0)
        return; // we ran out of edits
    
    if (*p == '\0') {
        // We have a match.
        *cigar = '\0';
        simplify_cigar(data->cigar_buf, data->full_cigar_buf);
        
        struct st_leaf_iter leaf_iter;
        init_st_leaf_iter(&leaf_iter, data->st, v);
        struct st_leaf_iter_result res;
        while (next_st_leaf(&leaf_iter, &res)) {
            uint32_t position = res.leaf->leaf_label;
            char *m = match_string(position, data->cigar_buf);
            string_vector_append(data->results, m);
        }
        dealloc_st_leaf_iter(&leaf_iter);

    } else if (x == end) {
        // We ran out of edge.
        search_children(data, v, cigar, p, edits);
        
    } else {
        // Recurse on different edits
        *cigar = 'M';
        int match_edit = (*p == *x) ? edits : edits - 1;
        search_edge(data, v, x + 1, end, p + 1, cigar + 1, match_edit);
        
        *cigar = 'D';
        search_edge(data, v, x + 1, end, p, cigar + 1, edits - 1);

        *cigar = 'I';
        search_edge(data, v, x, end, p + 1, cigar + 1, edits - 1);

    }
}

static void search_children(struct search_data *data,
                            struct suffix_tree_node *v,
                            char *cigar,
                            const char *p, int edits)
{
    struct suffix_tree *st = data->st;
    struct suffix_tree_node *child = v->child;
    while (child) {
        const char *x = st->string + child->range.from;
        const char *end = st->string + child->range.to;
        search_edge(data, child, x, end, p, cigar, edits);
        child = child->sibling;
    }

}

static void simple_match(struct suffix_tree *st,
                         const char *p,
                         const char *string,
                         int edits,
                         string_vector *results)
{
    size_t m = strlen(p) + 4*edits + 1; // one edit can max cost four characters
    
    struct search_data data;
    data.st = st;
    data.full_cigar_buf = malloc(m + 1);
    data.full_cigar_buf[0] = '\0';
    data.cigar_buf = malloc(m + 1);
    data.cigar_buf[0] = '\0';
    data.results = results;
    
    search_children(&data, st->root, data.full_cigar_buf, p, edits);
    
    free(data.full_cigar_buf);
    free(data.cigar_buf);
}


// MARK: Recursive implementation w/o flanking deletions
static void deleteless_match(struct suffix_tree *st,
                             const char *pattern, const char *string,
                             int edits,
                             string_vector *results)
{
}


// MARK: Iterator version
static void iter_match(struct suffix_tree *st,
                       const char *pattern, const char *string,
                       int edits,
                       string_vector *results)
{
    struct st_approx_match_iter iter;
    struct st_approx_match match;
    
    init_st_approx_iter(&iter, st, pattern, edits);
    while (next_st_approx_match(&iter, &match)) {
        char *m = match_string(match.match_label, match.cigar);
        string_vector_append(results, m);
    }
    
    dealloc_st_approx_iter(&iter);
}

// MARK: Test code
static bool equal_vectors(string_vector *first, string_vector *second)
{
    if (first->used != second->used) return false;
    
    for (int i = 0; i < first->used; ++i) {
        const char *s1 = string_vector_get(first, i);
        const char *s2 = string_vector_get(second, i);
        if (strcmp(s1, s2) != 0) return false;
    }
    
    return true;
}
static bool first_unique(string_vector *first, string_vector *second)
{
    string_vector first_unique;
    string_vector second_unique;
    init_string_vector(&first_unique, 10);
    init_string_vector(&second_unique, 10);
    
    split_string_vectors(first, second, &first_unique, &second_unique);
    bool res = second_unique.used == 0;
    
    dealloc_vector(&first_unique);
    dealloc_vector(&second_unique);
    return res;
}

static void test_matching(const char *pattern, const char *string, int edits)
{
    string_vector simple_results;
    init_string_vector(&simple_results, 100);
    string_vector deleteless_results;
    init_string_vector(&deleteless_results, 100);
    string_vector iter_results;
    init_string_vector(&iter_results, 100);

    struct suffix_tree *st = naive_suffix_tree(string);
    simple_match(st, pattern, string, edits, &simple_results);
    deleteless_match(st, pattern, string, edits, &deleteless_results);
    iter_match(st, pattern, string, edits, &iter_results);
    free_suffix_tree(st);
    
    sort_string_vector(&simple_results);
    sort_string_vector(&deleteless_results);
    sort_string_vector(&iter_results);
    
    printf("recursive\n");
    print_string_vector(&simple_results);
    printf("\niter\n");
    print_string_vector(&iter_results);
    printf("\n");
    
    // FIXME: test vectors
    //assert(first_unique(&simple_results, &deleteless_results))
    // assert(equal_vectors(&deleteless_results, &iter_results);
    
    free_strings(&simple_results);
    dealloc_vector(&simple_results);
    free_strings(&deleteless_results);
    dealloc_vector(&deleteless_results);
    free_strings(&iter_results);
    dealloc_vector(&iter_results);
}

int main(int argc, char **argv)
{
    const int edits[] = {
        0, 1, 2
    };
    size_t no_edits = sizeof(edits) / sizeof(*edits);

    if (argc == 3) {
        const char *pattern = argv[1];
        const char *fname = argv[2];
        
        char *string = load_file(fname);
        printf("did I get this far?\n");
        if (!string) {
            // LCOV_EXCL_START
            printf("Couldn't read file %s\n", fname);
            return EXIT_FAILURE;
            // LCOV_EXCL_STOP
        }
        
        for (size_t k = 0; k < no_edits; ++k)
            test_matching(pattern, string, edits[k]);
        
        free(string);
    } else {

#if 1
        test_matching("ac", "acacacg", 1);
#else
        char *strings[] = {
            "acacacg",
            "gacacacag",
            "acacacag",
            "acacaca",
            "acataca",
        };
        size_t no_strings = sizeof(strings) / sizeof(const char *);
        const char *patterns[] = {
            "aca", "ac", "ca", "a", "c", "acg", "cg", "g",
        };
        size_t no_patterns = sizeof(patterns) / sizeof(const char *);
        
        for (size_t i = 0; i < no_patterns; ++i) {
            for (size_t j = 0; j < no_strings; ++j) {
                for (size_t k = 0; k < no_edits; ++k) {
                    printf("%s in %s [%d]\n", patterns[i], strings[j], edits[k]);
                    test_matching(patterns[i], strings[j], edits[k]);

                }
            }
        }

#endif
    }

    
    return EXIT_SUCCESS;
}
