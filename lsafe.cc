#include "common.hh"
#include "kcobuchi.cc"
#include <spot/twa/formula2bdd.hh>
#include "btree.cc"
#include "membership.cc"
// #include <spot/twa/bdddict.hh>

/*
    Ideas:
    - Do we really need to consider all 2^ap transitions? There is probably some analysis we can do
      on the original graph that determines a minimal set of "letters" that actually are used to distinguish
      the semantics of the machine.
    - rows represented as a binary tree could certainly work, especially given all rows are the same length.
*/

bool is_reasonable_ucb(twa_graph_ptr kucb) {
    //FIXME: should we check stuff here?
    return true;
}

typedef std::vector<bool> Lrow;
typedef std::vector<Lrow> Ltable;

bool empty_is_member(twa_graph_ptr g) {
    return is_accepting_state(g, g->get_init_state_number());
}

typedef std::vector<string> qtset;

twa_graph_ptr make_hypothesis_machine(qtset q, qtset t, Ltable tbl, bdd_dict_ptr dict) {
    twa_graph_ptr out = make_twa_graph(dict);
    unsigned init_state = out->new_state();
    return out;
}

typedef std::vector<bdd> alphabet_vec;

// since the bdd ids assigned to variables aren't going to be consecutive,
// we want to give them new indices so we can more mathematically iterate
// through them (and maybe do some bitwise trickery with them).

typedef std::vector<tuple<formula, unsigned>> better_var_map;

typedef std::shared_ptr<better_var_map> better_var_map_ptr;
inline better_var_map_ptr empty_better_var_map(){
    return std::make_shared<better_var_map>();
}

better_var_map_ptr get_better_var_map(bdd_dict_ptr dict) {
    better_var_map_ptr bvm = empty_better_var_map();
    bvm->reserve(dict->var_map.size());
    for (auto [f, i] : dict->var_map)
        bvm->push_back({f, i});
    return bvm;
}

bdd make_bdd_from_bits(unsigned bits, better_var_map_ptr bvm) {
    bdd out = bddtrue;
    for (auto it = bvm->begin(); it != bvm->end(); ++it) {
        const auto [f, ind] = *it;
        if (bits & 1)
            out &= bdd_ithvar(ind);
        else
            out &= bdd_nithvar(ind);
        bits >>= 1;
    }
    return out;
}

// build an alphabet from a bdd varmap.
// consider every combination of APs, ordered such that their presence
// gives the bit truth of their bvm index (not their bdd var number)
// in the vector's index.
alphabet_vec alphabet_from_bvm(better_var_map_ptr bvm) {
    alphabet_vec alphabet;
    const unsigned num_letters = (1 << bvm->size());
    for (unsigned i=0; i<num_letters; i++)
        alphabet.push_back(make_bdd_from_bits(i, bvm));
    return alphabet;
}

// void make_closed(qtset & Q, qtset & T, Ltable & tbl, alphabet_vec & alphabet, twa_graph_ptr kucb) {

// }

typedef finite_word_ptr prefix;
typedef finite_word_ptr suffix;

const finite_word_ptr empty_word() {
    return make_finite_word();
}

void Lsafe(twa_graph_ptr kucb) {
    if (!is_reasonable_ucb(kucb)) throw invalid_argument("provided graph must be a UCB.");

    const bdd_dict_ptr dict = kucb->get_dict();
    better_var_map_ptr bvm = get_better_var_map(dict);
    alphabet_vec alphabet = alphabet_from_bvm(bvm);
    unsigned letter_count = alphabet.size();

    // std::vector<prefix> Q; // set of states (in terms of state consistency)
    // std::vector<suffix> T; // set of separating suffixes

    // // initial values for Q and T
    // Q[0] = empty_word();
    // T[0] = empty_word();

    // set up blank table - we start with only empty in row and col,
    // which is true iff the kuch accepts the empty word
    // Ltable tbl = {{empty_is_member(kucb)}};
    
    // btree tbl = *btreebool(empty_is_member(kucb), 0);

    // function<bool(prefix)> find_or_create_row = [&](prefix prefix) {
    //     for (suffix suffix : T) {
            
    //     }
    //     return false;
    // };

    // function<void()> close_table = [&]() {
    //     // take a look at each row, and see if it can "step" into another row after one letter.
    //     for (unsigned row : tbl) {
    //         prefix prefix = Q[row];
    //         for (unsigned letter=0; letter<letter_count; ++letter) {
    //             bdd letter_bdd = alphabet[letter];

    //             // if you're in state "row", and then add letter "letter", we need to be able to step to
    //             // another row-state.
    //             Lrow next_row;
    //             // for ()
    //         }
    //     }

    // };



    // close the table


    // twa_graph_ptr H = make_hypothesis_machine(Q, T, tbl, dict);





    

    // std::vector<x>
    /*
    - what do i need?
    - build a table
        - rows: prefixes
        - cols: suffixes
        - cells:binary
    - .. 2d bit matrix.
    - representation:
        - vector of vectors? fuck no
        - 2d array of bools?
            - could be much more mem efficient with bitvectors in unsigned longs, but don't need that for now.
        - indexed array of unordered_sets? what is a lookup like on this?
    - NB: want to be able to easily compare rows. this lends itself to bitvectors. but scalability issues.
        - map prefix -> (set suffix) ??
        - what is the complexity of a suffix lookup in a set?
        - tree representation makes lots of sense...

    */
}