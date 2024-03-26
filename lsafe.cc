#include "common.hh"
#include "kcobuchi.cc"
#include <spot/twa/formula2bdd.hh>
#include "btree.cc"
#include "membership.cc"
#include "safety_games.cc"
#include <spot/twaalgos/complement.hh>
#include <spot/twaalgos/complete.hh>

#define longest_counterexample 1000
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

bdd make_varset_bdd (better_var_map_ptr bvm) {
    bdd out = bddtrue;
    for (auto & [f, v] : *bvm)
        out &= bdd_ithvar(v);
    return out;
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

finite_word_ptr copy_word(finite_word_ptr word) {
    finite_word_ptr out = make_finite_word();
    auto end = word->end();
    for (auto it = word->begin(); it!=end; ++it) {
        out->push_back(*it);
    }
    return out;
}

string dump_lsafe_state(bdd_dict_ptr dict, std::vector<prefix> P, std::vector<suffix> S, btree * tbl) {
    stringstream out;
    tbl->print(out);
    out << endl << "P:" << endl;
    for (unsigned i=0; i<P.size(); ++i) {
        out << " (" << i << ") " << prefix_to_string(dict, P[i]) << endl;
    }
    out << "S:" << endl;
    for (prefix s : S) {
        out << " - " << prefix_to_string(dict, s) << endl;
    }
    return out.str();
}

// void dump_lsafe_state(bdd_dict_ptr dict, std::vector<prefix> P, std::vector<suffix> S, btree * tbl) {
//     cout << endl << "dumping LSafe state: " << endl;
//     cout << "P:" << endl;
//     for (prefix p : P) {
//         cout << " - " << prefix_to_string(dict, p) << endl;
//     }
//     cout << "S:" << endl;
//     for (prefix s : S) {
//         cout << " - " << prefix_to_string(dict, s) << endl;
//     }
//     tbl->print();
// }

bool word_not_empty(twa_word_ptr word) {
    return (word != nullptr);
}

// this uses a known infinite counterexample to check against two machines, to find the shortest prefix
// that is a counterexample. Actually this is silly, we aren't using the knowledge that this is known to
// be a counterexample, so we already know it will succeed on the "machine". so this function is silly lol
finite_word_ptr find_finite_counterexample(twa_graph_ptr machine, twa_graph_ptr kucb, twa_word_ptr counterexample) {
    UCBWalker walk_machine = UCBWalker(machine);
    UCBWalker walk_kucb = UCBWalker(kucb);
    finite_word_ptr out = make_finite_word();
    for (bdd letter : counterexample->prefix) {
        out->push_back(letter);
        if (walk_machine.step(letter) != walk_kucb.step(letter))
            return out;
    }
    for (unsigned i=0; i<longest_counterexample; i++) {
        for (bdd letter : counterexample->cycle) {
            out->push_back(letter);
            if (walk_machine.step(letter) != walk_kucb.step(letter))
                return out;
        }
    }
    throw std::runtime_error("we didnt' find a counterexample after " + to_string(longest_counterexample) + " steps. something wrong here?");
}

inline bdd complete_letter(bdd letter, bdd varset) {
    return bdd_satoneset(letter, varset, bddtrue);
}

bool fully_specify_word(twa_word_ptr word, bdd & varset) {
    bool change_made = false;
    for (bdd & letter : word->prefix) {
        bdd newletter = complete_letter(letter, varset);
        if (newletter != letter) change_made = true;
        letter = newletter;
    }
    for (bdd & letter : word->cycle) {
        bdd newletter = complete_letter(letter, varset);
        if (newletter != letter) change_made = true;
        letter = newletter;
    }
    return change_made;
}

// this assumes that the given counterexample is in fact a counterexample, so it will
// eventually fail when executing on the machine "g"
finite_word_ptr find_shortest_failing_prefix(twa_graph_ptr g, twa_word_ptr counterexample) {
    UCBWalker walker = UCBWalker(g);
    finite_word_ptr out = make_finite_word();
    for (bdd & letter : counterexample->prefix) {
        out->push_back(letter);
        if (walker.step(letter)) return out;
    }
    for (unsigned i=0; i<longest_counterexample; i++) {
        for (bdd & letter : counterexample->cycle) {
            out->push_back(letter);
            if (walker.step(letter)) return out;
        }
    }
    throw std::runtime_error("we didnt' find a counterexample after " + to_string(longest_counterexample) + " steps. something wrong here?");
}

twa_graph_ptr get_buchi(formula ltl) {
    translator trans;
    trans.set_type(postprocessor::Buchi);
    trans.set_level(postprocessor::Low); // stopping optimisation for now
    trans.set_pref(postprocessor::SBAcc + postprocessor::Complete);
    twa_graph_ptr aut = trans.run(ltl);
    return aut;
}

std::tuple<bool, twa_graph_ptr> Lsafe_for_k(twa_graph_ptr kucb, ap_map apmap);

void LSafe(formula ltl, ap_map apmap) {

    const unsigned max_k = 10;

    formula ltl_neg = formula::Not(ltl);
    page_text(to_string(ltl_neg), "&not;LTL");

    twa_graph_ptr buchi_neg = get_buchi(ltl_neg);
    page_graph(buchi_neg, "NonDet Buchi of negated LTL");

    twa_graph_ptr cobuchi = dualize_to_univ_coBuchi(buchi_neg);
    page_graph(cobuchi, "Universal CoBuchi of LTL");
    

    for (unsigned k=0; k<=max_k; ++k) {
        cout << "Iterating with K = " << k << endl;
        if (k == 0) page_heading("Starting with K = " + to_string(k));
        else        page_heading("Incrementing K to " + to_string(k));

        twa_graph_ptr kcobuchi = kcobuchi_expand(cobuchi, k);
        page_graph(kcobuchi, "Expanded K-CoBuchi for K="+to_string(k));

        auto [realisable, machine] = Lsafe_for_k(kcobuchi, apmap);
        if (realisable) {
            page_text("We have a solution for K = " + to_string(k));
            page_text("todo");
            return;
        } else {
            page_text("We have a proof of unrealisability for K = " + to_string(k));
            twa_word_ptr counterexample = machine->intersecting_word(buchi_neg);
            if (word_not_empty(counterexample)) {
                page_text("However this is not a proof for the general formula. We need to increment K.");
                page_text(force_string(*counterexample), "Counterexample");
                continue;
            } else {
                page_text("This is also a proof for the general formula. We have proven the specification to be unrealisable.");
                return;
            }
        }
    }
    page_text("We tried K up to " + to_string(max_k) + ", and didn't find any solution.", "Giving up");
    cout << "K maxed out. giving up." << endl;
}

// perform LSafe for some kucb.
std::tuple<bool, twa_graph_ptr> Lsafe_for_k(twa_graph_ptr kucb, ap_map apmap) {
    if (!is_reasonable_ucb(kucb)) throw invalid_argument("provided graph must be a UCB.");

    const bdd_dict_ptr dict = kucb->get_dict();
    better_var_map_ptr bvm = get_better_var_map(dict);
    bdd varset = make_varset_bdd(bvm);
    alphabet_vec alphabet = alphabet_from_bvm(bvm);
    unsigned letter_count = alphabet.size();
    UCBWalker walk = UCBWalker(kucb);
    const unsigned h_fin_set = 0;

    twa_graph_ptr kucb_complement = spot::complement(kucb);
    page_graph(kucb_complement, "kUCB complement");

    const bool init_state_accepted = !walk.failed();
    page_text_bool(init_state_accepted, "The empty word is accepted.", "The empty word is not accepted.");


    std::vector<prefix> P = {empty_word()}; // set of states (in terms of state consistency)
    std::vector<suffix> S = {empty_word()};; // set of separating suffixes

    // set up blank table - we start with only empty in row and col,
    // which is true iff the kucb accepts the empty word
    btree * tbl = btreebool(init_state_accepted, 0);

    // function<bool(prefix)> find_or_create_row = [&](prefix prefix) {
    //     for (suffix suffix : T) {
            
    //     }
    //     return false;
    // };

    auto get_or_create_sink_row = [&]() {
        // cout << "we found an empty row to create: " << i << endl;
        // tbl->print();
        btree * cur_node = tbl;
        for (unsigned i=0; i<S.size(); ++i)
            cur_node = cur_node->force_false();
        return cur_node;
        // if (cur_node->has_rowid()) {
        //     // it already exists.
        //     return cur_node->get_rowid();
        // } else {
        //     cur_node->set_rowid(id_if_creating);
        //     return id_if_creating;
        // }
    };

    function<twa_graph_ptr()> close_table = [&]() {
        // cout << "  closing table (FROM SCRATCH - this should be made a lot sexier)" << endl;
        
        // take a look at each row, and see if it can "step" into another row after one letter.
        twa_graph_ptr H = make_twa_graph(dict);
        H->set_acceptance(acc_cond::acc_code::cobuchi());
        H->prop_state_acc(true);
        for (unsigned i=0; i<P.size(); i++) {
            prefix prefix = P[i];
            // cout << "- considering prefix index = " << i << ", word = " << prefix_to_string(dict, prefix) << endl;
            // tbl->print();
            auto new_state_num = H->new_state();
            if (new_state_num != i) throw std::logic_error("the assumption that these states will have ids that align with my code here was obvbiously wrong");
            walk.reset();
            walk.walk(prefix);
            if (!walk.failed()) {
                auto prefix_state = walk.get_position();
                bdd prevletter = bddtrue;
                for (unsigned l=0; l<letter_count; ++l) {
                    walk.set_position(prefix_state);
                    bdd letter = alphabet[l];
                    // cout << "  - considering letter = " << bdd_to_formula(letter, dict) << endl;
                    walk.step(letter);
                    if (!walk.failed()) {
                        auto prefix_letter_state = walk.get_position();
                        btree * cur_node = tbl;
                        for (suffix suffix : S) {
                            // cout << "    - considering suffix = " << prefix_to_string(dict, suffix) << endl;
                            walk.set_position(prefix_letter_state);
                            walk.walk(suffix);
                            // cout << "      - " << !walk.failed() << endl;
                            cur_node = cur_node->force_bool(!walk.failed());
                        }
                        if (!cur_node->has_rowid()) {
                            // cout << "      - " << "[making new row]" << endl;
                            cur_node->set_rowid(P.size());
                            P.push_back(finite_word_append(prefix, letter));
                        }
                        H->new_edge(i, cur_node->get_rowid(), letter);
                    } else {
                        // the prefix already fails. no need to do any more checks, but
                        // need to fill out the table anyway.
                        btree* empty_row = get_or_create_sink_row();
                        if (!empty_row->has_rowid()) {
                            empty_row->set_rowid(P.size());
                            P.push_back(finite_word_append(prefix, letter));                            
                        }
                        H->new_edge(i, empty_row->get_rowid(), letter);
                    }
                }
            } else {
                // this prefix immediately fails. it is the empty row, so it should always point
                // only to itself.
                btree* empty_row = get_or_create_sink_row();
                if (!empty_row->has_rowid()) {
                    empty_row->set_rowid(i);
                }
                H->new_acc_edge(i, i, bddtrue, true);
            }
        }
        // now we are going to set acceptance on the appropriate states.
        // for (btree::BTIterator it = tbl->begin(); it!=tbl->end(); ++it) {
        //     const bool is_accepting = false;
        //     cout << *it << endl;
        // }
        return H;
    };


    auto extend_table_with_suffix = [&](suffix suffix) {
        S.push_back(suffix);

        // consider all existing leaves in the tree, and extend them
        // with the suffix.
        // potential optimisation: if i considered each leaf all at once, and walked each
        // of them forward one letter at a time (of the suffix), to get to the point at which it is
        // "enough" - what exactly is the definition of "enough"? I guess that it creates more states in the graph.
        auto end = tbl->end();
        for (auto it = tbl->begin(); it != end; ++it) {
            unsigned idx = *it;
            btree* bt = it.get_btree();
            prefix prefix = P[idx];
            walk.reset();
            walk.walk(prefix);
            walk.walk(suffix);
            bt->set_rowid(-1);
            if (walk.failed()) bt->bfalse = new btree(idx);
            else               bt->btrue = new btree(idx);
        }
    };

    // put the new prefix at the end of the table.
    // throw an exception if it matches any existing rows.
    // note: i'm not certain that it's a really a problem for it to match any
    // existing rows :P
    // note: this is not currently used
    auto extend_table_with_prefix = [&](prefix prefix) {
        const unsigned rowid = P.size();
        P.push_back(prefix);
        btree* bt = tbl;
        walk.reset();
        walk.walk(prefix);
        auto prefix_state = walk.get_position();
        for (suffix suffix : S) {
            walk.set_position(prefix_state);
            const bool accepted = !walk.walk(suffix);
            bt = bt->force_bool(accepted);
        }
        if (bt->has_rowid()) throw std::logic_error("oh no, we are adding a row that is the same as all the others. that is no good");
        bt->set_rowid(rowid);
    };


    for (unsigned i=1; i<=10; i++) {
        if (i==10) {
            page_text("stopping at 10 iterations");
            cout << "stopping at 10 iterations" << endl;
        }

        page_heading("LSafe iteration #" + to_string(i));
        cout << "  LSafe Iteration " << i << endl;
        // close the table
        // dump_lsafe_state(dict, P, S, tbl);
        page_text("Closing table ...");
        twa_graph_ptr h = close_table();
        page_code("Hypothesis table:", dump_lsafe_state(dict, P, S, tbl));
        // page_graph(h, "Hypothesis graph (unmerged)");
        // h->merge_edges();
        page_graph(h, "Hypothesis graph");

        auto [realizable, machine] = solve_safety(h, apmap);
        if (realizable) {
            page_text("Hypothesis is realisable.");
            highlight_strategy_vec(h, 5, 4);
            page_graph(h, "Strategy");
            page_graph(machine, "Mealy machine");

            twa_word_ptr counterexample = machine->intersecting_word(kucb_complement);
            if (word_not_empty(counterexample)) {
                string orig_counterexample_string = force_string(*counterexample);
                if (fully_specify_word(counterexample, varset)) {
                    page_text(orig_counterexample_string, "Original (symbolic) counterexample");
                }
                page_text(force_string(*counterexample), "Counterexample");
                finite_word_ptr finite_counterexample = find_shortest_failing_prefix(kucb, counterexample);
                page_text(prefix_to_string(dict, finite_counterexample), "Shortest finite counterexample");

                // ok im trying just adding the whole thing as a suffix, except the first letter.
                // page_text("Right now we are adding the entire counterexample, minus its first letter, as a suffix.");
                page_text("Adding all suffixes of the counterexample to S.");
                while (finite_counterexample->size()>1) {
                    finite_counterexample->pop_front();
                    // make a copy
                    extend_table_with_suffix(copy_word(finite_counterexample));
                }
                page_code("Table after accounting for counterexample:", dump_lsafe_state(dict, P, S, tbl));
            } else {
                page_text("No counterexample found. I guess we are done?");
                break;
            }
        } else {
            page_text("Hypothesis is NOT realisable.");
            highlight_strategy_vec(h, 5, 4);
            page_graph(h, "Strategy");
            page_graph(machine, "Moore machine");

            twa_word_ptr counterexample = machine->intersecting_word(kucb);
            if (word_not_empty(counterexample)) {
                // fully_specify_word(counterexample, varset);
                page_text(force_string(*counterexample), "Counterexample");
                page_text("todo: find counterexample against hypothesis here");
                // finite_word_ptr finite_counterexample = find_shortest_failing_prefix(h, counterexample);
                // page_text(prefix_to_string(dict, finite_counterexample), "Shortest finite counterexample");
            } else {
                page_text("No counterexample found. For this K, the formula is not realisable.");
                // now we check in general, to see if it's a total counterexample or if we need to
                // increment K.
                return {false, machine};
            }

            page_text("todo: complete this logic");
            break;
        }
    }


    // generate_graph_image(h, true);
    // inclusion_query_etc(h);

    // auto x = h->intersecting_word(kucb_complement);
    // cout << "counterexample:" << endl;
    // cout << *x << endl;

    // auto xx = h->intersecting_run(kucb_complement);
    //note: "take most permissive strategy" - make this a global option
    
    // auto z = x->as_automaton();
    // generate_graph_image(z, true, "intersection");

    // cout << "checking containment" << endl;
    // bool x = spot::contains(kucb, h);
    // cout << "answer: " << x << endl;


    
    // table closing seems to work for now (although it's not currently creating a graph)
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