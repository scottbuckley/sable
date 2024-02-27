#include "common.hh"
#include <spot/twa/bddprint.hh>

#include <boost/tuple/tuple.hpp>
#include <boost/unordered_map.hpp>

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

twa_graph_ptr kcobuchi_expand(twa_graph_ptr input, unsigned k) {
    // check that the input graph is universal cobuchi with exactly one fin set.
    if (input->prop_universal() != true)
        throw std::invalid_argument("input graph must be deterministic.");
    acc_cond acc = input->get_acceptance();
    if (!acc.is_co_buchi())
        throw std::invalid_argument("input graph must use coBuchi acceptance.");
    if (acc.num_sets() != 1)
        throw std::invalid_argument("input graph must have exactly one acceptance set.");

    unsigned fin_set = acc.fin_one();
    // unsigned fin_set = 0;

    // create the output graph
    twa_graph_ptr output = make_twa_graph(input->get_dict());
    output->copy_acceptance_of(input);
    output->copy_ap_of(input);
    output->prop_state_acc(1); // mark it as having state-based acceptance
    output->copy_named_properties_of(input); // i don't know what named properties are

    // keep track of newly-created states, and what they represent
    state_cache created_states;

    // "visit" a state on the new graph, considering the transitions of the equivalent state
    // on the input graph, potentially creating new states and edges as you go.
    function<void(unsigned, unsigned, unsigned)> visit_state
    = [&](unsigned curstate_input, unsigned curstate_output, unsigned curcount) {
        unsigned next_count = curcount + is_accepting_state(input, curstate_input, fin_set);

        // keep track of newly created states that need to be visited
        vector<tuple<unsigned,unsigned>> states_to_visit;

        // for each outgoing edge ...
        for (auto& e: input->out(curstate_input)) {
            if (!input->is_univ_dest(e.dst)) {
                // ... add an edge to its equivalent state (which might need to be created) ...
                const auto [state_created, next_state] = get_or_create_state(output, created_states, e.dst, next_count);
                // ... and potentially mark it for later visitation.
                if (state_created) {
                    if (next_count <= k) {
                        states_to_visit.push_back({e.dst, next_state});
                    } else {
                        trapify_state(output, next_state, fin_set);
                    }
                }
                output->new_edge(curstate_output, next_state, e.cond);
            } else {
                // ... if it's universal, do this for each of the universal destinations, keeping
                // track of the new destinations needed for the new universal edge ...
                vector<unsigned> new_univ_dests;
                for (auto d: input->univ_dests(e.dst)) {
                    const auto [state_created, next_state] = get_or_create_state(output, created_states, d, next_count);
                    new_univ_dests.push_back(next_state);
                    // ... and potentially mark each of them for later visitation.
                    if (state_created) {
                        if (next_count <= k) {
                            states_to_visit.push_back({d, next_state});
                        } else {
                            trapify_state(output, next_state, fin_set);
                        }
                    }
                }
                output->new_univ_edge(curstate_output, new_univ_dests.begin(), new_univ_dests.end(), e.cond);
            }
        }
        // now recursively visit all the newly created states we marked for visitation.
        for (auto [si, so] : states_to_visit)
            visit_state(si, so, next_count);
    };

    // now, set up the new initial state ...
    const unsigned init_state = input->get_init_state_number();
    const bool init_accepting = is_accepting_state(input, init_state, fin_set);
    const unsigned new_init_state = output->new_state();
    created_states[{init_state, init_accepting}] = new_init_state;
    // ... and get the ball rolling by visiting it.
    visit_state(init_state, new_init_state, init_accepting);

    print_expansion_key(created_states);

    return output;
}


twa_graph_ptr kcobuchi_expand2(twa_graph_ptr input, unsigned k) {
    // check that the input graph is universal cobuchi with exactly one fin set.
    if (input->prop_universal() != true)
        throw std::invalid_argument("input graph must be deterministic.");
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
    unsigned trap_state;
    function<unsigned()> get_trap_state
    = [&]() {
        if (!trap_state_created) {
            trap_state = output->new_state();
            trapify_state(output, trap_state, fin_set);
        }
        return trap_state;
    };

    // "visit" a state on the new graph, considering the transitions of the equivalent state
    // on the input graph, potentially creating new states and edges as you go.
    function<void(unsigned, unsigned, unsigned)> visit_state
    = [&](unsigned curstate_input, unsigned curstate_output, unsigned curcount) {
        unsigned next_count = curcount + is_accepting_state(input, curstate_input, fin_set);

        // keep track of newly created states that need to be visited
        vector<tuple<unsigned,unsigned>> states_to_visit;

        // for each outgoing edge ...
        for (auto& e: input->out(curstate_input)) {
            if (!input->is_univ_dest(e.dst)) {
                // next branch would be a 
                
                // ... add an edge to its equivalent state (which might need to be created) ...
                const auto [state_created, next_state] = get_or_create_state(output, created_states, e.dst, next_count);
                // ... and potentially mark it for later visitation.
                if (state_created) {
                    if (next_count <= k) {
                        states_to_visit.push_back({e.dst, next_state});
                    } else {
                        trapify_state(output, next_state, fin_set);
                    }
                }
                output->new_edge(curstate_output, next_state, e.cond);
            } else {
                // ... if it's universal, do this for each of the universal destinations, keeping
                // track of the new destinations needed for the new universal edge ...
                vector<unsigned> new_univ_dests;
                for (auto d: input->univ_dests(e.dst)) {
                    const auto [state_created, next_state] = get_or_create_state(output, created_states, d, next_count);
                    new_univ_dests.push_back(next_state);
                    // ... and potentially mark each of them for later visitation.
                    if (state_created) {
                        if (next_count <= k) {
                            states_to_visit.push_back({d, next_state});
                        } else {
                            trapify_state(output, next_state, fin_set);
                        }
                    }
                }
                output->new_univ_edge(curstate_output, new_univ_dests.begin(), new_univ_dests.end(), e.cond);
            }
        }
        // now recursively visit all the newly created states we marked for visitation.
        for (auto [si, so] : states_to_visit)
            visit_state(si, so, next_count);
    };

    // now, set up the new initial state ...
    const unsigned init_state = input->get_init_state_number();
    const bool init_accepting = is_accepting_state(input, init_state, fin_set);
    const unsigned new_init_state = output->new_state();
    created_states[{init_state, init_accepting}] = new_init_state;
    // ... and get the ball rolling by visiting it.
    visit_state(init_state, new_init_state, init_accepting);

    print_expansion_key(created_states);

    return output;
}