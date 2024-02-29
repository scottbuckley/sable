#pragma once

#include "common.hh"
#include <spot/twa/bddprint.hh>

#include <boost/tuple/tuple.hpp>
#include <boost/unordered_map.hpp>
#include <spot/twa/acc.hh>

#include <spot/twaalgos/dualize.hh>

// Pair with hash adapted from https://stackoverflow.com/a/3612272
// doing it this way so that i can use a fast hashmap containing pairs,
// as std::tuple doesn't hash, or something.
struct Pair {
  Pair(unsigned x1, unsigned x2) : tuple(x1,x2) {}
  boost::tuples::tuple<unsigned, unsigned> tuple;
};

bool operator==(const Pair &a, const Pair &b) {
  return a.tuple.get<0>() == b.tuple.get<0>() &&
         a.tuple.get<1>() == b.tuple.get<1>();
}

std::size_t hash_value(const Pair &e) {
  std::size_t seed = 0;
  boost::hash_combine(seed, e.tuple.get<0>());
  boost::hash_combine(seed, e.tuple.get<1>());
  return seed;
}

// to be able to use Arrays in an unordered_map. taken from https://stackoverflow.com/a/42701911
struct ArrayHasher {
    std::size_t operator()(const std::array<int, 3>& a) const {
        std::size_t h = 0;

        for (auto e : a) {
            h ^= std::hash<int>{}(e)  + 0x9e3779b9 + (h << 6) + (h >> 2); 
        }
        return h;
    }   
};

typedef tuple<bool, unsigned> bool_unsigned;
typedef boost::unordered_map<Pair, unsigned> state_cache;


// turn a state into an acceptance trap
void trapify_state(twa_graph_ptr g, unsigned state, unsigned fin_set) {
    g->new_edge(state, state, bddtrue, {fin_set});
}

// check if a state is accepting, in that it has the accepting set 'fin_set'.
// yes, it's insane that you have to do it this way, but it's the same in the spot source;
// acceptance info is only stored in edges.
bool is_accepting_state(twa_graph_ptr g, unsigned state, unsigned fin_set) {
    // look at the first edge, and look through its accepting sets to find 'fin_set'.
    for (auto& t: g->out(state)) {
        for (auto v: t.acc.sets())
            if (v == fin_set) return true;
        break;
    }
    return false;
};

bool is_accepting_state(twa_graph_ptr g, unsigned state) {
    for (auto& t: g->out(state)) {
        for (auto v: t.acc.sets())
            return true;
        break;
    }
    return false;
};

bool is_accepting_sink(twa_graph_ptr g, unsigned state, bool assume_state_is_accepting=false) {
    if (!assume_state_is_accepting && !is_accepting_state(g, state)) return false;
    // return false if you find any branches that leave this state
    for (auto e : g->out(state))
        if (!g->is_univ_dest(e.dst))
            { if (e.dst != state) return false; }
        else for (auto d : g->univ_dests(e.dst))
            if (d != state) return false;
    return true;
}

void print_expansion_key(state_cache dict) {
    //TODO: sort the output?
    for (auto& [key, output_state]: dict) {
        auto& [input_state, count] = key.tuple;
        std::cout << output_state << " = (" << input_state << ", c=" << count << ")" << endl; }
}

// create the new state if it needs to be created (return {true, _} if it was created)
// and return its id.
bool_unsigned get_or_create_state(twa_graph_ptr g, state_cache & created_states, unsigned stateid_input, unsigned count) {
    unsigned state_id;
    bool state_is_new = false;
    Pair tokey = {stateid_input, count};

    // did the state already exist?
    if (created_states.count(tokey) == 0) {
        // no. need to construct this new state
        state_id = g->new_state();
        created_states[tokey] = state_id;
        state_is_new = true;
    } else {
        // yes. just return its id.
        state_id = created_states[tokey];
        state_is_new = false;
    }
    return (bool_unsigned){state_is_new, state_id};
}

twa_graph_ptr kcobuchi_expand(twa_graph_ptr input, unsigned k, bool unroll_accepting_sinks=false) {
    // check that the input graph is universal cobuchi with exactly one fin set.
    if (input->prop_universal() != true)
        throw std::invalid_argument("input graph must be universal.");
    acc_cond acc = input->get_acceptance();
    if (!acc.is_co_buchi())
        throw std::invalid_argument("input graph must use coBuchi acceptance.");
    if (acc.num_sets() != 1)
        throw std::invalid_argument("input graph must have exactly one acceptance set.");

    unsigned fin_set = acc.fin_one();

    // create the output graph
    twa_graph_ptr output = make_twa_graph(input->get_dict());
    output->copy_acceptance_of(input);
    output->copy_ap_of(input);
    output->prop_state_acc(1); // mark it as having state-based acceptance
    output->copy_named_properties_of(input); // i don't know what named properties are

    // keep track of newly-created states, and what they represent
    state_cache created_states;

    bool trap_state_created = false;
    unsigned trap_state_cache;
    function<unsigned()> get_trap_state
    = [&]() {
        if (!trap_state_created) {
            trap_state_created = true;
            trap_state_cache = output->new_state();
            trapify_state(output, trap_state_cache, fin_set);
            created_states[{999, k+1}] = trap_state_cache;
        }
        return trap_state_cache;
    };

    // "visit" a state on the new graph, considering the transitions of the equivalent state
    // on the input graph, potentially creating new states and edges as you go.
    // note: `curcount` is the count BEFORE entering this state.
    function<void(unsigned, unsigned, unsigned)> visit_state
    = [&](unsigned curstate_input, unsigned curstate_output, unsigned curcount) {
        cout << "visit(" << curstate_input << ", " << curstate_output << ", " << curcount << ")" << endl;
        // we visited this state, which means we have already decided that we do not already
        // exceed the k limit.
        bool is_accepting = is_accepting_state(input, curstate_input);
        curcount = curcount + is_accepting;

        // keep track of newly created states that need to be visited
        vector<tuple<unsigned,unsigned>> states_to_visit;

        // for each outgoing edge ...
        for (auto& e: input->out(curstate_input)) {
            if (!input->is_univ_dest(e.dst)) {
                // once we step to the next state, what will the count be?
                unsigned next_count = curcount + is_accepting_state(input, e.dst);
                if (next_count > k || is_accepting_sink(input, e.dst)) {
                    // the next state will have a count > k, so it should be a (the) trap state
                    const unsigned next_state = get_trap_state();
                    output->new_edge(curstate_output, next_state, e.cond);
                } else {
                    // make an edge to the next (maybe new) state
                    const auto [state_created, next_state] = get_or_create_state(output, created_states, e.dst, next_count);
                    output->new_edge(curstate_output, next_state, e.cond);
                    if (state_created)
                        states_to_visit.push_back({e.dst, next_state});
                }
            } else {
                // ... if it's universal, do this for each of the universal destinations, keeping
                // track of the new destinations needed for the new universal edge ...
                vector<unsigned> new_univ_dests;
                bool trap_already_added = false;
                for (auto d: input->univ_dests(e.dst)) {
                    unsigned next_count = curcount + is_accepting_state(input, d);
                    //TODO: there is an optimisation we could do here, where if any of the
                    // universal destinations lead to a trap, they should all lead to a trap.
                    if (next_count > k  || is_accepting_sink(input, d)) {
                        if (!trap_already_added) {
                            trap_already_added = true;
                            new_univ_dests.push_back(get_trap_state());
                        }
                    } else {
                        const auto [state_created, next_state] = get_or_create_state(output, created_states, d, next_count);
                        new_univ_dests.push_back(next_state);
                        if (state_created)
                            states_to_visit.push_back({d, next_state});
                    }
                }
                output->new_univ_edge(curstate_output, new_univ_dests.begin(), new_univ_dests.end(), e.cond);
            }
        }
        // now recursively visit all the newly created states we marked for visitation.
        for (auto [si, so] : states_to_visit)
            visit_state(si, so, curcount);
    };

    // now, set up the new initial state ...
    const unsigned init_state = input->get_init_state_number();
    const bool init_accepting = is_accepting_state(input, init_state);
    const unsigned new_init_state = output->new_state();
    if (init_accepting && k<1) {
        // weird case where the input state is accepting but k=0, meaning
        // we must immediately fail.
        trapify_state(output, new_init_state, fin_set);
        cout << "replaced entire graph with single trap state, as k<1 and the initial state was accepting." << endl;
    } else {
        created_states[{init_state, init_accepting}] = new_init_state;
        visit_state(init_state, new_init_state, 0);
        print_expansion_key(created_states);
    }
    return output;
}

// FIXME: can I do this more efficiently with a generator?
vector<unsigned> get_all_dests(twa_graph_ptr g, unsigned s) {
    vector<unsigned> dests;
    for (auto e : g->out(s)) {
        if (g->is_univ_dest(e.dst)) {
            for (auto d : g->univ_dests(e.dst))
                dests.push_back(d);
        } else {
            dests.push_back(e.dst);
        }
    }
    return dests;
}


// given a set of edges, return a bdd representing unrepresented edge possibilities.
bdd get_unspecified_edge_cond(spot::internal::state_out<twa_graph::graph_t> es) {
    bdd conditions = bddtrue;
    for (auto e : es)
        conditions -= e.cond;
    return conditions;
}

void complete_coBuchi_here(twa_graph_ptr g) {
    acc_cond acc = g->get_acceptance();

    // get the acceptance set we should use
    unsigned fin_set = 0;
    if (acc.num_sets() == 0) {
        // make this cobuchi if it isn't already.
        g->set_acceptance(acc_cond(acc_cond::acc_code::fin({fin_set})));
    } else {
        fin_set = acc.fin_one();
    }
    
    bool trap_state_created = false;
    unsigned trap_state;

    unordered_set<unsigned> visited;
    function<void(unsigned)> visit_state = [&](unsigned curstate) {
        if (visited.count(curstate) == 0) {
            visited.insert(curstate);
            // first we visit all other edges, so we don't have to exclude this new
            // edge as we traverse.
            for (unsigned d : get_all_dests(g, curstate))
                visit_state(d);
            bdd unspecified = get_unspecified_edge_cond(g->out(curstate));
            if (unspecified != bddfalse) {
                // not everything is defined for this output state
                if (!trap_state_created) {
                    // create the trap state
                    trap_state = g->new_state();
                    g->new_edge(trap_state, trap_state, bddtrue, {fin_set});
                    trap_state_created = true;
                }
                g->new_edge(curstate, trap_state, unspecified);
            }
        }
    };
    visit_state(g->get_init_state_number());
}

twa_graph_ptr dualize_to_univ_coBuchi(twa_graph_ptr g) {
    acc_cond acc = g->get_acceptance();
    if (!acc.is_buchi())
        throw std::invalid_argument("input graph must use Buchi acceptance.");
    twa_graph_ptr dual = dualize(g);
    acc = dual->get_acceptance();
    if (acc.is_all())
        complete_coBuchi_here(dual);
    dual->prop_universal(1);
    return dual;
}

twa_graph_ptr dualize2(twa_graph_ptr g) {
    twa_graph_ptr output = make_twa_graph(g, twa::prop_set::all());
    output->set_acceptance(output->get_acceptance().complement());
    return output;
}