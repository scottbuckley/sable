#pragma once
#include "common.hh"
#include <spot/parseaut/public.hh>
#include <spot/twaalgos/game.hh>
#include <spot/misc/escape.hh>
#include <spot/twaalgos/sccinfo.hh>

using namespace std;
using namespace spot;

/* not currently used - this was just used during testing
twa_graph_ptr un_universalise(twa_graph_ptr input) {
    twa_graph_ptr output = make_twa_graph(input->get_dict());
    output->copy_acceptance_of(input);
    output->copy_ap_of(input);
    output->prop_state_acc(1);
    output->copy_named_properties_of(input);
    const unsigned ns = input->num_states();
    output->new_states(ns);
    for (unsigned s=0; s<ns; ++s) {
        for (auto e : input->out(s)) {
            if (!input->is_univ_dest(e.dst)) {
                output->new_edge(s, e.dst, e.cond, e.acc);
            } else {
                for (auto d : input->univ_dests(e.dst))
                    output->new_edge(s, d, e.cond, e.acc);
            }
        }
    }
    output->prop_universal(false);
    output->merge_edges();
    return output;
} */

bdd apmap_mask (std::vector<bool> apmap, bdd_dict_ptr dict) {
    bdd mask = bddfalse;
    unsigned i = 0;
    for (auto [f, idx] : dict->var_map) {
        if (!apmap[i])
            mask |= bdd_ithvar(idx);
        ++i;
    }
    return mask;
}


typedef std::vector<std::unordered_set<unsigned>> strat_v;

twa_graph_ptr highlight_strategy_vec(twa_graph_ptr& aut, int player0_color, int player1_color) {
    region_t* winners = aut->get_named_prop<region_t>("state-winner");
    strat_v* strategy = aut->get_named_prop<strat_v>("strategy-vec");
    if (!winners)
        throw std::runtime_error ("highlight_strategy(): winning region unavailable, solve the game first");
    if (!strategy)
        throw std::runtime_error ("highlight_strategy(): strategy unavailable, solve the game first");

    unsigned ns = aut->num_states();
    auto* hl_edges = aut->get_or_set_named_prop<std::map<unsigned, unsigned>>("highlight-edges");
    auto* hl_states = aut->get_or_set_named_prop<std::map<unsigned, unsigned>>("highlight-states");

    if (unsigned sz = std::min(winners->size(), strategy->size()); sz < ns)
    ns = sz;

    for (unsigned n = 0; n < ns; ++n) {
        int color = (*winners)[n] ? player1_color : player0_color;
        if (color == -1) continue;
        (*hl_states)[n] = color;
        for (unsigned e : (*strategy)[n])
            (*hl_edges)[e] = color;
    }

    return aut;
}

inline int bdd_compat(const bdd &l, const bdd &r) noexcept {
    return (bdd_implies(l, r) || bdd_implies(r, l));
}

// solve the safety game on a graph where players' states are not separated; instead
// consider APs covered by `apmap` to belong to Input/Player1, and all other APs to 
// belong to Output/Player2. This logic considers that Input/Player1 plays first, such
// that for each transition, the system has a chance to react to the input to choose an output.
// note: this version assumes that all edges are minimal unique AP configurations (as would be
// generated from an L*-style table), and that they are also input and output complete.
bool solve_safety_ap_game(const twa_graph_ptr& game, std::vector<bool> & apmap) {
    if (game->prop_universal()) {
        // we ain't work with universal automata
        throw std::runtime_error ("solve_safety_ap_game(): arena should not be universal");
    }

    auto apmask = apmap_mask(apmap, game->get_dict());
    // cout << "ap mask: " << apmask << endl;

    const unsigned ns = game->num_states();
    auto winners = new region_t(ns, true);
    game->set_named_prop("state-winner", winners);
    auto strategy = new strat_v(ns);
    game->set_named_prop("strategy-vec", strategy);

    // transposed is a reversed copy of game to compute predecessors
    // more easily.  It also keep track of the original edge index and condition.
    struct edge_data {
      unsigned edgeidx;
      bdd cond;
    };
    digraph<void, edge_data> transposed;
    // Reverse the automaton, compute the out degree of
    // each state, and save dead-states in queue.
    transposed.new_states(ns);
    std::vector<unsigned> dead_queue;
    for (unsigned s = 0; s < ns; ++s) {
        for (auto& e: game->out(s)) {
            transposed.new_edge(e.dst, e.src, game->edge_number(e), e.cond);
        }
        //FIXME: why does this need to be negated?
        if (!game->state_is_accepting(s)) {
            (*winners)[s] = false;
            dead_queue.push_back(s);
            // mark any self-edges from this accepting dead state as part of the strategy.
            for (auto & e : game->out(s)) if (e.dst == s) {
                (*strategy)[s].insert(game->edge_number(e));
            }
        }
    }
    
    // cout << "initial queue: "; print_vector(dead_queue);
    // dead_queue.push_back(3);
    // (*winners)[3] = false;
    
    // consider all states in the queue of dead states (and sometimes add more as we go).
    auto dead_border = std::vector<bool>(ns, false);
    while (!dead_queue.empty()) {
        // empty the "dead border", to rebuild again
        dead_border.assign(ns, false);
        while (!dead_queue.empty()) {
            unsigned s = dead_queue.back();
            dead_queue.pop_back();
            // cout << "- considering state " << s << endl;
            // look at all the *incoming* edges to s
            for (auto& e: transposed.out(s)) {
                unsigned pred = e.dst;
                if (pred == s) {
                    // we have a self-edge, so we mark it as being part of the dead border,
                    // so that the self edge will possibly be included in the strategy.
                    dead_border[pred] = true;
                }
                // if this edge comes from something already marked as dead, don't worry about it.
                if (!(*winners)[pred]) continue;
                // just mark it as being on the dead border.
                dead_border[pred] = true;
            }
        }
        
        for (unsigned s=0; s<ns; ++s) if (dead_border[s] && (*winners)[s]) {
            // now we need to consider if the system can make a choice that
            // avoids a dead state. we now consider the edges from this state (NOT the transposed edges)
            // cout << "- considering border state " << s << endl;
            bdd unsafe_cond = bddtrue;
            for (auto& e: game->out(s)) {
                if ((*winners)[e.dst]) {
                    // cout << "  - edge to " << succ << endl;
                    // cout << "    - cond: " << bdd_exist(e.cond, apmask) << endl;
                    unsafe_cond -= e.cond;
                    // cout << "away edge: " << bdd_to_formula(e.cond, game->get_dict()) << endl;
                    // cout << "    - total cond so far: " << unsafe_cond << endl;
                }
            }
            // we are only looking at the input APs
            // cout << "unsafe:" << bdd_to_formula(unsafe_cond, game->get_dict()) << endl;
            unsafe_cond = bdd_forall(unsafe_cond, apmask);
            // cout << "unsafe:" << bdd_to_formula(unsafe_cond, game->get_dict()) << endl;
            if (unsafe_cond != bddfalse) {
                // this means there is some input that has no edges to safe states.
                // cout << "  - safe edges from " << s << " do not cover all inputs. unsafe input: " << bdd_to_formula(unsafe_cond, game->get_dict()) << endl;
                (*winners)[s] = false;
                dead_queue.push_back(s);

                // now mark that we would take any outgoing edge that has these inputs
                for (auto& e: game->out(s)) {
                    if (!(*winners)[e.dst]) {
                        if (bdd_compat(unsafe_cond, e.cond)) {
                            (*strategy)[s].insert(game->edge_number(e));
                        }
                    }
                }
            }
        }
    }

    const bool losing_strategy_found = !(*winners)[game->get_init_state_number()];
    // now we want the player 1 strategy.
    // the simplest solution is to allow all edges to other safe states, but
    // we can reduce the number of edges a little.
    // FIXME: we probably want to do more optimisation here, the heuristic below doesn't 
    // remove all edges that could be removed.
    if (!losing_strategy_found) {
        for (unsigned s = 0; s < ns; ++s) {
            if ((*winners)[s]) {
                // bdd inputs_covered = bddfalse;
                for (auto& e: game->out(s)) {
                    if ((*winners)[e.dst]) {
                        (*strategy)[s].insert(game->edge_number(e));
                        // inputs_covered |= bdd_exist(e.cond, apmask);
                        // if the added edges already cover all inputs, stop adding edges.
                        // note: this is a pretty weak optimisation - for example if there is an
                        // output edge with condition TRUE, but it is not the first edge we look at,
                        // it won't be the only edge we add.
                        // if (inputs_covered == bddtrue) break;
                    }
                }
            }
        }
    }
    return !losing_strategy_found;
}

// solve the safety game on a graph where players' states are not separated; instead
// consider APs covered by `apmap` to belong to Input/Player1, and all other APs to 
// belong to Output/Player2. This logic considers that Input/Player1 plays first, such
// that for each transition, the system has a chance to react to the input to choose an output.
// note: this version is optimised for merged edges - there is another version that is optimised
// (and assumes) that each edge is a distinct minimal AP configuration, as is generated as a hypothesis
// automaton during Lsafe.
bool solve_safety_ap_game_merged_edges(const twa_graph_ptr& game, std::vector<bool> & apmap) {
    if (game->prop_universal()) {
        // we ain't work with universal automata
        throw std::runtime_error ("solve_safety_ap_game(): arena should not be universal");
    }

    auto apmask = apmap_mask(apmap, game->get_dict());
    // cout << "ap mask: " << apmask << endl;

    unsigned ns = game->num_states();
    auto winners = new region_t(ns, true);
    game->set_named_prop("state-winner", winners);
    auto strategy = new strat_v(ns);
    game->set_named_prop("strategy-vec", strategy);

    // transposed is a reversed copy of game to compute predecessors
    // more easily.  It also keep track of the original edge index.
    struct edge_data {
      unsigned edgeidx;
      bdd cond;
    };
    digraph<void, edge_data> transposed;
    // Reverse the automaton, compute the out degree of
    // each state, and save dead-states in queue.
    transposed.new_states(ns);
    std::vector<unsigned> dead_queue;
    for (unsigned s = 0; s < ns; ++s) {
        for (auto& e: game->out(s)) {
            transposed.new_edge(e.dst, e.src, game->edge_number(e), e.cond);
        }
        //FIXME: why does this need to be negated?
        if (!game->state_is_accepting(s)) {
            (*winners)[s] = false;
            dead_queue.push_back(s);
        }
    }
    
    // cout << "initial queue: "; print_vector(dead_queue);
    // dead_queue.push_back(3);
    // (*winners)[3] = false;
    
    // consider all states in the queue of dead states (and sometimes add more as we go).
    auto dead_border = new std::unordered_set<unsigned>();
    while (!dead_queue.empty()) {
        dead_border->clear();
        while (!dead_queue.empty()) {
            unsigned s = dead_queue.back();
            dead_queue.pop_back();
            // look at all the *incoming* edges to s
            // cout << "- considering state " << s << endl;
            for (auto& e: transposed.out(s)) {
                unsigned pred = e.dst;
                // if this edge comes from something already marked as dead, don't worry about it.
                if (!(*winners)[pred]) continue;
                // cout << "  - considering edge " << e.edgeidx << " to state " << e.dst << endl;
                // cout << "  - cond: " << e.cond << endl;
                if (bdd_existcomp(e.cond, apmask) == bddtrue) {
                    // cout << "  - this edge is forcable!" << endl;
                    (*winners)[pred] = false;
                    dead_queue.push_back(pred);
                    (*strategy)[pred].insert(e.edgeidx);
                } else {
                    dead_border->insert(pred);
                }
            }
        }
        // avoid checking the same border state twice on this iteration
        for (auto s : *dead_border) {
            if (!(*winners)[s]) continue;

            // now we need to consider if the system can make a choice that
            // avoids a dead state. we now consider the edges from this state (NOT the transposed edges)
            // cout << "- considering border state " << s << endl;
            bdd unsafe_cond = bddtrue;
            for (auto& e: game->out(s)) {
                const unsigned succ = e.dst;
                if ((*winners)[succ]) {
                    // cout << "  - edge to " << succ << endl;
                    // cout << "    - cond: " << bdd_exist(e.cond, apmask) << endl;
                    unsafe_cond -= bdd_exist(e.cond, apmask);
                    // cout << "    - total cond so far: " << unsafe_cond << endl;
                }
            }
            if (unsafe_cond != bddfalse) {
                // cout << "  - safe edges do not cover all inputs. unsafe input: " << unsafe_cond << endl;
                (*winners)[s] = false;
                dead_queue.push_back(s);
                for (auto& e: game->out(s)) {
                    const unsigned succ = e.dst;
                    if (!(*winners)[succ]) {
                        if (bdd_implies(e.cond, unsafe_cond)) {
                            (*strategy)[s].insert(game->edge_number(e));
                        }
                    }
                }
            }
        }
    }

    const bool losing_strategy_found = !(*winners)[game->get_init_state_number()];
    // now we want the player 1 strategy.
    // the simplest solution is to allow all edges to other safe states, but
    // we can reduce the number of edges a little.
    if (!losing_strategy_found) {
        for (unsigned s = 0; s < ns; ++s) if ((*winners)[s]) {
            // bdd inputs_covered = bddfalse;
            for (auto& e: game->out(s)) {
                if ((*winners)[e.dst]) {
                    (*strategy)[s].insert(game->edge_number(e));
                    // inputs_covered |= bdd_exist(e.cond, apmask);
                    // if the added edges already cover all inputs, stop adding edges.
                    // note: this is a pretty weak optimisation - for example if there is an
                    // output edge with condition TRUE, but it is not the first edge we look at,
                    // it won't be the only edge we add.
                    // if (inputs_covered == bddtrue) break;
                }
            }
        }
    }

    return !losing_strategy_found;
}

twa_graph_ptr get_strategy_machine(twa_graph_ptr input) {
    twa_graph_ptr output = make_twa_graph(input->get_dict());
    std::unordered_map<unsigned, unsigned> statemap;

    auto emplace_new_state = [&](unsigned s) {
        auto maybe = statemap.find(s);
        if (maybe == statemap.end()) {
            return statemap[s] = output->new_state();
        }
        return (*maybe).second;
    };

    const unsigned input_init_state = input->get_init_state_number();
    strat_v* strategy = input->get_named_prop<strat_v>("strategy-vec");
    region_t* winners = input->get_named_prop<region_t>("state-winner");
    const bool strategy_is_winning = (*winners)[input_init_state];

    unsigned ns = input->num_states();
    region_t visited_states = region_t(ns, false);
    auto& input_edges = input->edge_vector();
    function<void(unsigned, unsigned)> visit_state = [&](unsigned input_s, unsigned output_s) {
        visited_states[input_s] = true;
        for (auto eidx : strategy->at(input_s)) {
            auto& e = input_edges[eidx];
            auto new_dest = emplace_new_state(e.dst);
            output->new_edge(output_s, new_dest, e.cond);
            if (!visited_states[e.dst])
                visit_state(e.dst, new_dest);
        }
    };

    visit_state(emplace_new_state(input_init_state), input_init_state);
    return output;
}

std::tuple<bool, twa_graph_ptr> solve_safety(twa_graph_ptr g, ap_map apmap) {
    bdd_dict_ptr dict = g->get_dict();
    
    const bool winning = solve_safety_ap_game(g, apmap);

    twa_graph_ptr machine = get_strategy_machine(g);
    machine->merge_edges();

    return {winning, machine};

    // highlight_strategy_vec(g, 5, 4);
    // generate_graph_image(g, true);
    generate_graph_image(machine, true, "mealy");

}