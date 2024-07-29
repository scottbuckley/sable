#pragma once
#include "common.hh"
#include "safety_games.cc"
#include "bsg.cc"



std::vector<boost::dynamic_bitset<>> incompatible_state_pairs(const BSG & hyp, const ap_info & apinfo) {
  constexpr bool print_debug = true;

  // make a vector of bitsets, all set to 0 for now, so we have a 2d array of bits defining
  // which pairs of states have been identified as incompatible.
  const unsigned num_states = hyp.num_states();
  auto incompat = std::vector<boost::dynamic_bitset<>>(num_states, boost::dynamic_bitset<>(num_states));

  // for each pair of states, we remember the set of pair states that they can step to if they were merged.
  // auto onestep_pairs = std::vector<std::vector<shared_ptr<std::vector<std::tuple<unsigned, unsigned>>>>>();
  // for (unsigned i=0; i<num_states; ++i)
    // onestep_pairs.emplace_back(num_states, nullptr);

  bool new_incompats_found = true;
  while (new_incompats_found == true) {
    new_incompats_found = false;

    cout << "incompats table" << endl;
    for (unsigned s1=0; s1<num_states; ++s1) {
      for (unsigned s2=0; s2<num_states; ++s2) {
        cout << incompat[s1][s2] << " ";
      }
      cout << endl;
    }

    bool break_for_this_pair = false;

    // look at all pairs of states.
    for (unsigned s1=0; s1<num_states; ++s1) {
      for (unsigned s2=s1+1; s2<num_states; ++s2) {
        if constexpr (print_debug) cout << "state " << s1 << " and state " << s2 << endl;
        // we already know this pair is incompatible, we don't need to check it.
        if (incompat[s1][s2]) continue;

        // Reminder: I want to try to prove that these two states are NOT comptible.
        // This means I am kinda searching for combatibility (in the context of assumptions about
        // other states being compatible), but this affects how i short-circuit.
        // look at all pairs of edges.
        eBSC cond_covered_by_connections = eBSC::eBSC_false();
        for (const auto & e1 : hyp.out(s1)) {
          // each edge from S1
          for (const auto & e2 : hyp.out(s2)) {
            // each edge from S2

            // if these two edges can't step to a compatible pair, there is no way they will
            // contribute to proving compatibility / incompatibility.
            if (incompat[e1.dest][e2.dest]) continue;

            // do these two edges share some input?
            BSC edge_input_intersection = BSC::masked_intersection(e1.cond, e2.cond, apinfo.input_mask);

            if constexpr (print_debug) cout << "  edge " << e1.cond << " and edge " << e2.cond << " : " << edge_input_intersection << endl;

            // if there's no shared input on these two edges, they won't contribute.
            if (edge_input_intersection.is_false()) continue;

            if constexpr (print_debug) cout << "    there is some input intersection" << endl;

            // if there's also no shared output for the shared input, these edges won't contribute.
            if (!e1.cond.intersects(e2.cond)) continue;

            if constexpr (print_debug) cout << "    there is also output intersection" << endl;

            // at this point, we know that the destinations are currently considered compatible,
            // and that the input and output APs intersect between the two edges.
            // we can record that the collision of the inputs are covered for this pair of states.
            cond_covered_by_connections |= edge_input_intersection;
            if constexpr (print_debug) cout << "    the next states are also (thus far) compatible" << endl;
            if constexpr (print_debug) cout << "    input covered: " << cond_covered_by_connections << endl;

            // if we have already covered all of the input language, we probably can stop looking
            // at edges.
            if (cond_covered_by_connections.is_true()) {
              // break_for_this_pair = true;
              // break;
            }
          }
        }

        // we considered all edges, now we need to check if the inputs that could share
        // edges from both states to other compatible states covers all inputs. if it
        // doesn't, we are also not compatible.
        if (!cond_covered_by_connections.is_true()) {
          // if constexpr (print_debug) cout << "found incompatibility between " << s1 << " and " << s2 << ", due to not covering all inputs" << endl;
          // if constexpr (print_debug) cout << cond_covered_by_connections.s(apinfo) << endl;
          incompat[s1][s2] = true;
          incompat[s2][s1] = true;
          new_incompats_found = true;
        }
      }
    }
  }

  cout << "final incompat table" << endl;
  for (unsigned s1=0; s1<num_states; ++s1) {
    for (unsigned s2=0; s2<num_states; ++s2) {
      cout << incompat[s1][s2] << " ";
    }
    cout << endl;
  }

  return incompat;
}

void get_compat_sets_including(const unsigned & s, const std::vector<boost::dynamic_bitset<>> & incompat) {
  // we are finding the maximal subsets that include state s.
  // in this case, we define 'maximal' to mean that we can't add any more states to this set.

  const unsigned num_states = incompat.size();

  auto grow_set = [&](const std::vector<unsigned> & set) {
    std::vector<unsigned> new_states;
    for (unsigned se=0; se<num_states; ++se) { // se = state external (to the current set)
      bool could_add_to_set = true;
      for (const unsigned si : set)
        if (se == si || incompat[se][si]) {
          could_add_to_set = false;
          break;
        }
      if (could_add_to_set)
        new_states.push_back(se);
    }
    return new_states;
  };

  auto init_set = std::vector<unsigned>(1, 0);
  auto next = grow_set(init_set);
}

auto get_greedy_compat_set(const unsigned & s, const std::vector<boost::dynamic_bitset<>> & incompat) {
  // we are making a "maximal" compatible set, starting with `s`, greedily adding the first
  // compatible states we see, without any backtracking.
  const unsigned num_states = incompat.size();
  // add another state to the current set if you can
  auto grow_set = [&](std::vector<unsigned> & set) {
    for (unsigned se=0; se<num_states; ++se) {
      bool could_add_to_set = true;
      for (const unsigned & si : set) {
        if (se == si || incompat[se][si]) {
          could_add_to_set = false;
          break;
        }
      }
      if (could_add_to_set) {
        set.push_back(se);
        return true;
      }
    }
    return false;
  };

  std::vector<unsigned> state_set = {s};
  while (grow_set(state_set));
  return state_set;
}

void ensure_input_completeness(const std::vector<unsigned> & stateset, const BSG & og, const mutable_BSG & new_g, const ap_info & apinfo) {
  // each input letter needs an output edge from this set
  for (const unsigned & l : apinfo.masked_input_letters) {
    cout << l << endl;
  }
}

void build_greedy_graph(const BSG & og, const std::vector<boost::dynamic_bitset<>> & incompat, const ap_info & apinfo) {
  auto init_stateset = get_greedy_compat_set(0, incompat);
  auto states_in_sets = boost::dynamic_bitset<>(og.num_states(), false);
  for (const auto & i : init_stateset) states_in_sets.set(i);

  auto new_g = mutable_BSG();
  unsigned cur_state = new_g.add_state();

  ensure_input_completeness(init_stateset, og, new_g, apinfo);

}

bool single_state_strategy_exists(const BSG & g, const ap_info & apinfo) {
  for (const unsigned & input_letter : apinfo.masked_input_letters) {
    // for each letter we check what outputs are globally available.
    eBSC this_letter_outputs_avail = eBSC::eBSC_true();
    for (unsigned s=0; s<g.num_states(); ++s) {
      eBSC this_state_outputs_avail = eBSC::eBSC_false();
      for (const auto & e : g.out(s)) {
        if (e.cond.accepts_letter(input_letter))
          this_state_outputs_avail |= e.cond.masked(apinfo.output_mask);
      }
      // this_letter_outputs_avail &= this_state_outputs_avail;
    }

  }
  return false;
}

void debug_find_smaller_substrategy(twa_graph_ptr hyp, const ap_info & apinfo) {
  page_text("trying to find a smaller substrategy from the final solved hypothesis ...");

  auto strat = get_strategy_machine(hyp);
  strat->merge_edges();
  page_graph(strat, "most permissive strat");

  auto bsg = BSG(strat, apinfo);

  twa_graph_ptr g = bsg.to_twa_graph(strat->get_dict(), apinfo);
  page_graph(g, "round-trip graph");

  auto incompat = incompatible_state_pairs(bsg, apinfo);
  build_greedy_graph(bsg, incompat, apinfo);


}