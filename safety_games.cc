#pragma once
#include "common.hh"
#include "extras.cc"
#include "substrategy_simulation.cc"
#include <spot/parseaut/public.hh>
#include <spot/twaalgos/game.hh>
#include <spot/misc/escape.hh>
#include <spot/twaalgos/sccinfo.hh>
#include <spot/twaalgos/complement.hh>
#include <queue>

using namespace std;
using namespace spot;

std::function<void(void*)> blank_des = [](void *p) noexcept {
  //delete static_cast<unsigned*>(p); 
};

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
  bdd mask = bddtrue;
  for (auto [f, idx] : dict->var_map) {
    if (apmap[idx])
      mask &= bdd_ithvar(idx);
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

// solve the safety game on a graph where players' states are not separated; instead
// consider APs covered by `apmap` to belong to Input/Player1, and all other APs to 
// belong to Output/Player2. This logic considers that Input/Player1 plays first, such
// that for each transition, the system has a chance to react to the input to choose an output.
// note: this version assumes that all edges are minimal unique AP configurations (as would be
// generated from an L*-style table), and that they are also input and output complete.
bool solve_safety_ap_game(const twa_graph_ptr& game, const ap_info & apinfo, bool verbose = false) {
  if (game->prop_universal()) {
    // we ain't work with universal automata
    throw std::runtime_error ("solve_safety_ap_game(): arena should not be universal");
  }
  // cout << "ap mask: " << bdd_to_formula(apmask, game->get_dict()) << endl;

  const unsigned ns = game->num_states();
  auto winners = new region_t(ns, true); //leak
  game->set_named_prop("state-winner", winners);
  auto strategy = new strat_v(ns); //leak
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
      // record that this is the sink state
      game->set_named_prop("sink-idx", new store_unsigned(s));

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
    
    // go through the border
    for (unsigned s=0; s<ns; ++s) if (dead_border[s] && (*winners)[s]) {
      // now we need to consider if the system can make a choice that
      // avoids a dead state. we now consider the edges from this state (NOT the transposed edges)
      // cout << "- considering border state " << s << endl;
      bdd unsafe_cond = bddtrue;
      for (auto& e: game->out(s)) {
        if ((*winners)[e.dst]) {
          // cout << "  - edge to " << succ << endl;
          unsafe_cond -= e.cond;
          // cout << "away edge: " << bdd_to_formula(e.cond, game->get_dict()) << endl;
          if (verbose) {
            cout << "  - cond: " << bdd_to_formula(e.cond, game->get_dict()) << endl;
            cout << "  - total cond so far: " << bdd_to_formula(unsafe_cond, game->get_dict()) << endl;
          }
        }
      }
      // we are only looking at the input APs
      // cout << "unsafe:" << bdd_to_formula(unsafe_cond, game->get_dict()) << endl;
      unsafe_cond = bdd_forallcomp(unsafe_cond, apinfo.bdd_input_aps);
      if (verbose) {
        cout << "  - final total cond: " << bdd_to_formula(unsafe_cond, game->get_dict()) << endl;
      }
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
          // cout << "  - cond: " << bdd_exist(e.cond, apmask) << endl;
          unsafe_cond -= bdd_exist(e.cond, apmask);
          // cout << "  - total cond so far: " << unsafe_cond << endl;
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

void translate_sink_idx(twa_graph_ptr src, twa_graph_ptr dest, function<unsigned(unsigned)> state_translate_fn) {
  unsigned new_sink_idx = state_translate_fn(src->get_named_prop<store_unsigned>("sink-idx")->val);
  dest->set_named_prop<store_unsigned>("sink-idx", new store_unsigned(new_sink_idx));
}

// produces a strategy (mealy/moore) machine from a hypothesis graph which
// has been solved already. produces the MOST PERMISSIVE strategy.
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

  if (!strategy_is_winning) {
    output->copy_named_properties_of(input);
    // if we are making a moore machine, we should record which state is the sink.
    translate_sink_idx(input, output, emplace_new_state);
  }

  return output;
}




twa_graph_ptr minimise_mealy_machine_pre_merge(twa_graph_ptr g, const ap_info & apinfo) {

  twa_graph_ptr output = make_twa_graph(g->get_dict());
  std::unordered_map<unsigned, unsigned> statemap;
  const unsigned init_state = g->get_init_state_number();

  auto emplace_new_state = [&](unsigned s) {
    auto maybe = statemap.find(s);
    if (maybe == statemap.end()) {
      return statemap[s] = output->new_state();
    }
    return (*maybe).second;
  };

  const unsigned ns = g->num_states();
  region_t visited_states = region_t(ns, false);
  
  auto states_to_visit = std::queue<unsigned>({init_state});

  function<void(unsigned, unsigned)> visit_state = [&](unsigned input_s, unsigned output_s) {
    if (visited_states[input_s]) return;
    visited_states[input_s] = true;
    bool first = true;
    bdd inputs_covered = bddfalse;
    for (const auto & e : g->out(input_s)) {
      bdd input_cond = bdd_existcomp(e.cond, apinfo.bdd_input_aps);
      if (bdd_implies(input_cond, inputs_covered)) {
        // this input is already covered. skip this edge
      } else {
        inputs_covered |= input_cond;
        const auto new_dest = emplace_new_state(e.dst);
        output->new_edge(output_s, new_dest, e.cond);
        if (!visited_states[e.dst])
          visit_state(e.dst, new_dest);
      }
    }
  };

  visit_state(emplace_new_state(init_state), init_state);
  return output;
}

twa_graph_ptr minimise_moore_machine_pre_merge(twa_graph_ptr g, const ap_info & apinfo) {

  twa_graph_ptr output = make_twa_graph(g->get_dict());
  std::unordered_map<unsigned, unsigned> statemap;
  const unsigned init_state = g->get_init_state_number();

  auto emplace_new_state = [&](unsigned s) {
    auto maybe = statemap.find(s);
    if (maybe == statemap.end()) {
      return statemap[s] = output->new_state();
    }
    return (*maybe).second;
  };

  const unsigned ns = g->num_states();
  region_t visited_states = region_t(ns, false);
  
  auto states_to_visit = std::queue<unsigned>({init_state});

  function<void(unsigned, unsigned)> visit_state = [&](unsigned input_s, unsigned output_s) {
    if (visited_states[input_s]) return;
    visited_states[input_s] = true;
    bool first = true;
    bdd input_choice;
    for (const auto & e : g->out(input_s)) {
      bdd input_cond = bdd_existcomp(e.cond, apinfo.bdd_input_aps);
      // cout << "cond " << bdd_to_formula(e.cond, g->get_dict()) << endl;
      // cout << "input cond " << bdd_to_formula(input_cond, g->get_dict()) << endl;
      // cout << (bdd_implies(input_choice, input_cond) ? "yes" : "no") << endl;
      if (first || bdd_implies(input_choice, input_cond)) {
        if (first) {
          first = false;
          input_choice = input_cond;
          // cout << "chosen input cond: " << input_choice << endl;
        }
        auto new_dest = emplace_new_state(e.dst);
        if (e.cond == bddtrue) {
          const bdd & spec_cond = bdd_satoneset(input_cond, apinfo.bdd_input_aps, bddtrue);
          output->new_edge(output_s, new_dest, spec_cond);
          // cout << "condition for sink: " << bdd_to_formula(spec_cond, g->get_dict()) << endl;
        } else {
          output->new_edge(output_s, new_dest, e.cond);
        }
        if (!visited_states[e.dst])
          visit_state(e.dst, new_dest);
      }
    }
  };
  visit_state(emplace_new_state(init_state), init_state);

  

  // copy over the sink index
  //set_named_prop(s, val, [](void *p) noexcept { delete static_cast<T*>(p); })
  translate_sink_idx(g, output, emplace_new_state);
  // auto sink_idx_ptr = g->get_named_prop<store_unsigned>("sink-idx");
  // unsigned new_sink_idx = emplace_new_state(sink_idx_ptr->val);
  // auto new_sink_idx_ptr = new store_unsigned(new_sink_idx);
  // output->set_named_prop("sink-idx", new_sink_idx_ptr);

  return output;
}

// twa_graph_ptr minimise_moore_machine_post_merge(twa_graph_ptr abstract, bdd & apmask) {
//   twa_graph_ptr concrete = make_twa_graph(abstract->get_dict());
//   concrete->copy_acceptance_of(abstract);

//   const unsigned state_count = abstract->num_states();

//   concrete->new_states(state_count);

//   const unsigned init_state = abstract->get_init_state_number();


//   // for each edge:
//   // isolate the INPUT condition
//   // specify it into a single satisfying bdd
//   // combine it again with the existing OUTPUT part of the original condition.
//   for (unsigned s=0; s<state_count; ++s) {
//   for (const auto & e : abstract->out(s)) {
//     bdd cond_input =  bdd_forallcomp(e.cond, apmask);
//     bdd cond_output = bdd_exist(e.cond, apmask);
//     bdd specific_input_cond = bdd_satone(cond_input);
//     concrete->new_edge(s, e.dst, specific_input_cond & cond_output);
//     // cout << "orig cond: " << e.cond << endl;
//     // cout << "input cond: " << cond_input << endl;
//     // cout << "output cond: " << cond_output << endl;
//     // cout << "specific: " << specific_input_cond << endl;
//     // cout << "merged: " << (specific_input_cond & cond_output) << endl;
//   }
//   };

//   return concrete;
// }


std::tuple<bool, twa_graph_ptr> solve_safety(twa_graph_ptr g, const ap_info & apinfo, bool verbose = false) {
  bdd_dict_ptr dict = g->get_dict();
  
  const bool winning = solve_safety_ap_game(g, apinfo, verbose);
  // #if CONFIG_SOLVE_GAME_DETERMINISTIC
    // twa_graph_ptr machine = get_strategy_machine_minimised(g, apmask);
  // #else
  twa_graph_ptr machine = get_strategy_machine(g);
  // #endif

  if (winning) {
    const unsigned first_state_count = machine->num_states();
    machine = find_smaller_simulation(machine, apinfo);
    const unsigned minimised_state_count = machine->num_states();

    IF_PAGE_DETAILS {
      page_text("Most-permissive strat had " + to_string(first_state_count) + " states, which we minimised to " + to_string(minimised_state_count));
    }

    machine->merge_edges(); // optional

    if (!is_valid_mealy_machine(machine, apinfo)) {
      cout << "Invalid mealy machine was created." << endl;
      page_graph(machine, "This is NOT a valid mealy machine!");
      page_finish();
      exit(1);
    }
  } else {
    const unsigned first_state_count = machine->num_states();
    // machine = find_smaller_simulation_moore(machine, apinfo);
    machine = minimise_moore_machine_pre_merge(machine, apinfo);
    const unsigned minimised_state_count = machine->num_states();

    IF_PAGE_DETAILS {
      page_text("Most-permissive strat had " + to_string(first_state_count) + " states, which we minimised to " + to_string(minimised_state_count));
    }

    // machine->merge_edges(); // optional

    if (!is_valid_moore_machine(machine, apinfo)) {
      cout << "OH NO it's not a valid moore you did something wrong :(";
      page_graph(machine, "This is NOT a valid moore machine!");
      page_finish();
      exit(1);
    }
  }

  return {winning, machine};
}



















twa_graph_ptr create_hardcoded_solution(bdd_dict_ptr dict) {
  auto g = make_twa_graph(dict);
  auto left_state = g->new_state();
  auto right_state = g->new_state();
  
  int ir0 = get_bdd_var_index(dict, "r0");
  int ir1 = get_bdd_var_index(dict, "r1");
  int igg = get_bdd_var_index(dict, "g");

  bdd r0 = bdd_ithvar(ir0);
  bdd _r0 = bdd_nithvar(ir0);
  bdd r1 = bdd_ithvar(ir1);
  bdd _r1 = bdd_nithvar(ir1);
  bdd gg = bdd_ithvar(igg);
  bdd _gg = bdd_nithvar(igg);

  // auto for_bdd = new bdd();
  bdd bdd_none = _r0 & _r1 & _gg;
  bdd bdd_r0_g = r0 & gg;
  bdd bdd_r1_g = r1 & gg;

  g->new_edge(left_state, left_state, bdd_none);
  g->new_edge(right_state, right_state, bdd_none);

  g->new_edge(left_state, right_state, bdd_r0_g);
  g->new_edge(right_state, left_state, bdd_r1_g);

  return g;
}

twa_graph_ptr create_hardcoded_solution2(bdd_dict_ptr dict) {
  auto g = make_twa_graph(dict);
  auto left = g->new_state();
  auto rght = g->new_state();
  
  int ir0 = get_bdd_var_index(dict, "r0");
  int ir1 = get_bdd_var_index(dict, "r1");
  int igg = get_bdd_var_index(dict, "g");

  const bdd  r0 = bdd_ithvar(ir0);
  const bdd _r0 = bdd_nithvar(ir0);
  const bdd  r1 = bdd_ithvar(ir1);
  const bdd _r1 = bdd_nithvar(ir1);
  const bdd  gg = bdd_ithvar(igg);
  const bdd _gg = bdd_nithvar(igg);

  g->new_edge(left, left, _r0 & _gg);
  g->new_edge(left, rght, r0  & _gg);

  g->new_edge(rght, rght, _r1 & _gg);
  g->new_edge(rght, left, r1 & gg);

  return g;
}

twa_graph_ptr create_shitty_hardcoded_solution(bdd_dict_ptr dict) {
  auto g = make_twa_graph(dict);
  auto left_state = g->new_state();
  
  int ir0 = get_bdd_var_index(dict, "r0");
  int ir1 = get_bdd_var_index(dict, "r1");
  int igg = get_bdd_var_index(dict, "g");

  bdd r0 = bdd_ithvar(ir0);
  bdd _r0 = bdd_nithvar(ir0);
  bdd r1 = bdd_ithvar(ir1);
  bdd _r1 = bdd_nithvar(ir1);
  bdd gg = bdd_ithvar(igg);
  bdd _gg = bdd_nithvar(igg);

  // auto for_bdd = new bdd();
  bdd bdd_none = _r0 & _r1 & _gg;
  bdd bdd_g = (r0 | r1) & gg;

  g->new_edge(left_state, left_state, bdd_none);
  g->new_edge(left_state, left_state, bdd_g);

  return g;
}



void debug_check_hardcoded_solution(twa_graph_ptr hyp, const ap_info & apinfo) {
  page_text("doing some debugging ...");
  auto strat = get_strategy_machine(hyp);
  page_graph(strat, "most permissive strat");

  auto small_solution = create_hardcoded_solution2(hyp->get_dict());
  page_graph(small_solution, "Ideal solution");
  if (!is_valid_mealy_machine(small_solution, apinfo)) {
    page_text("THIS IS NOT A VALID MEALY MACHINE");
    throw std::runtime_error("The hard-coded solution is not a valid mealy machine!");
  }

  auto strat_compl = spot::complement(strat);
  page_graph(strat_compl, "most permissive strat (complemented)");

  bool x = small_solution->intersects(strat_compl);

  if (x) {
    page_text("The ideal solution is NOT found in the most-permissive strategy of the final hypothesis.");
  } else {
    page_text("The ideal solution IS found in the most-permissive strategy of the final hypothesis.");
  }

  twa_word_ptr ce = small_solution->intersecting_word(strat_compl);
  if (ce == nullptr) {
    cout << "counterexample not found" << endl;
  } else {
    cout << *ce << endl;
  }
}