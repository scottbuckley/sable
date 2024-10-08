#pragma once
#include "common.hh"
#include "safety_games.cc"
#include "bsg.cc"
#include "bdd.cc"

std::vector<boost::dynamic_bitset<>> incompatible_state_pairs(const BSG & hyp, const ap_info & apinfo) {
  bool debug = true;

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

    //print the table
    cout << "incompats table" << endl;
    for (unsigned s1=0; s1<num_states; ++s1) {
      for (unsigned s2=0; s2<num_states; ++s2) {
        cout << incompat[s1][s2] << " ";
      }
      cout << endl;
    }

    // look at all pairs of states.
    for (unsigned s1=0; s1<num_states; ++s1) {
      for (unsigned s2=s1+1; s2<num_states; ++s2) {
        bool debug = (s1 == 2 && s2 == 9);
        ddout << "state " << s1 << " and state " << s2 << endl;
        // we already know this pair is incompatible, we don't need to check it.
        if (incompat[s1][s2]) continue;


        // Reminder: I want to try to prove that these two states are NOT comptible.
        // This means I am kinda searching for combatibility (in the context of assumptions about
        // other states being compatible), but this affects how i short-circuit.

        // Look at all pairs of edges.
        BSDD cond_covered_by_connections = BSDD(false);
        for (const auto & e1 : hyp.out(s1)) {
          // each edge from S1
          for (const auto & e2 : hyp.out(s2)) {
            // each edge from S2

            // if these two edges can't step to a compatible pair, there is no way they will
            // contribute to proving compatibility / incompatibility.
            if (incompat[e1.dest][e2.dest]) continue;

            // do these two edges share some input?
            BSC edge_input_intersection = BSC::masked_intersection(e1.cond, e2.cond, apinfo.input_mask);

            ddout << "  edge " << e1.cond << " and edge " << e2.cond << " : " << edge_input_intersection << endl;

            // if there's no shared input on these two edges, they won't contribute.
            if (edge_input_intersection.is_false()) continue;

            ddout << "    there is some input intersection" << endl;

            // if there's also no shared output for the shared input, these edges won't contribute.
            if (!e1.cond.intersects(e2.cond)) continue;

            ddout << "    there is also output intersection" << endl;

            // at this point, we know that the destinations are currently considered compatible,
            // and that the input and output APs intersect between the two edges.
            // we can record that the collision of the inputs are covered for this pair of states.
            cond_covered_by_connections |= edge_input_intersection;
            ddout << "    the next states are also (thus far) compatible" << endl;
            ddout << "    input covered: " << cond_covered_by_connections.to_string() << endl;

            // if we have already covered all of the input language, we probably can stop looking
            // at edges.
            if (cond_covered_by_connections.is_true()) {
              // todo: this can be a short circuit.
              // break_for_this_pair = true;
              // break;
            }
          }
        }

        // we considered all edges, now we need to check if the inputs that could share
        // edges from both states to other compatible states covers all inputs. if it
        // doesn't, we are also not compatible.
        if (!cond_covered_by_connections.is_true()) {
          // dout << "found incompatibility between " << s1 << " and " << s2 << ", due to not covering all inputs" << endl;
          // dout << cond_covered_by_connections.s(apinfo) << endl;
          incompat[s1][s2] = true;
          incompat[s2][s1] = true;
          new_incompats_found = true;
        }
      }
    }
  }

  cout << "final incompat table" << endl;
  for (unsigned s1=0; s1<num_states; ++s1) {
    cout <<  s1 << ": ";
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

auto get_greedy_compat_set(const unsigned & starting_state, const std::vector<boost::dynamic_bitset<>> & incompat, boost::dynamic_bitset<> & states_in_sets) {
  // we are making a "maximal" compatible set, starting with `s`, greedily adding the first
  // compatible states we see, without any backtracking.
  const unsigned num_states = incompat.size();

  std::vector<unsigned> state_set = {starting_state};

  for (unsigned i=0; i<num_states; ++i) {
    if (states_in_sets[i]) continue;
    bool could_add = true;
    for (const auto & s : state_set) {
      if (s == i || incompat[s][i])
        could_add = false;
    }
    if (could_add) {
      state_set.push_back(i);
      states_in_sets.set(i);
    }
  }

  // while (grow_set(state_set));
  return state_set;
}

void ensure_input_completeness(const std::vector<unsigned> & stateset, const BSG & og, const mutable_BSG & new_g, const ap_info & apinfo) {
  // each input letter needs an output edge from this set
  for (const unsigned & l : apinfo.masked_input_letters) {
    cout << l << endl;
  }
}

// input og is a "most permissive strategy"
// incompat is a bitset table telling us which states are incompatible in that graph
void build_greedy_graph(const BSG & og, const std::vector<boost::dynamic_bitset<>> & incompat, const ap_info & apinfo) {

  // THIS IS THE PART WHERE WE DECIDE WHICH STATES TO MERGE

  const unsigned num_states = og.num_states();
  auto states_in_sets = boost::dynamic_bitset<>(num_states, false);
  auto og_idx_to_ss_idx = std::vector<unsigned>(num_states, 0);
  std::vector<std::vector<unsigned>> state_sets;

  const auto build_new_stateset = [&](const unsigned & start_ind){
    const auto new_stateset = get_greedy_compat_set(start_ind, incompat, states_in_sets);
    const unsigned new_stateset_idx = state_sets.size();
    cout << "stateset idx=" << new_stateset_idx << endl;
    cout << "  contains: ";
    for (const auto & i : new_stateset) {
      cout << i << ", ";
      states_in_sets.set(i);
      og_idx_to_ss_idx[i] = new_stateset_idx;
    }
    cout << endl;
    state_sets.emplace_back(new_stateset);
  };
  // note: this can probably be done faster with the dynamic bitset iterator
  for (unsigned s=0; s<num_states; ++s) {
    if (states_in_sets[s]) continue;
    build_new_stateset(s);
  }

  // all states should be in sets now. lets print them i guess

  cout << "greedy graph results: " << endl;
  cout << "num of statesets: " << state_sets.size() << endl;


  // THIS IS THE PART WHERE WE BUILD IT INTO A NEW GRAPH

  // auto merged_og = make_merged_mutable_BSG(og);

  auto new_g = mutable_BSG();
  const unsigned new_state_count = state_sets.size();
  new_g.add_states(new_state_count);

  // for each new stateset (and therefore new state, with the same index)
  for (unsigned ss_idx=0; ss_idx<new_state_count; ++ss_idx) {
    const auto & ss = state_sets[ss_idx];
    // assert (ss.size()>1);

    // let's manually check this state_set to make sure everything is
    // marked as compatible.
    for (unsigned a=0; a<ss.size(); ++a) {
      for (unsigned b=0; b<ss.size(); ++b) {
        if (incompat[ss[a]][ss[b]]) {
          cout << "state set " << ss_idx << " doesn't seem intercompatible" << endl;
          cout << "counterexample: " << ss[a] << " and " << ss[b] << endl;
          assert(false);
        }
      }
    }

    // for each input letter...
    for (const auto & letter : apinfo.masked_input_letters) {
      // look through each merged_og state, gathering the *intersection* of
      // all possible conditions that lead to all possible destinations.
      // we only accept a destination if all states in the stateset can go to it
      // with this input letter, and we keep the condition that allows this, but
      // there could be multiple destinations that match this requirement, so
      // it's a bit tricky to keep track of.

      auto letter_cond = BSC(letter, apinfo.input_mask);

      std::list<BSG_Edge> current_matches;
      for (const auto & e : og.out_matching(ss[0], letter_cond))
        current_matches.emplace_back(e.cond, og_idx_to_ss_idx[e.dest]);


      for (unsigned s=1; s<ss.size(); ++s) {
        // intersect 'current_matches' with matching edges in 'state'.
        // cout << "ss[s]=" << ss[s] << endl;

        // cout << endl << "current_matches:" << endl;
        // for (const auto & e : current_matches)
        //   cout << " - " << e << endl;
        // og.dump_state_matching(ss[s], letter_cond);

        for (auto it=current_matches.begin(); it!=current_matches.end(); ++it) {
          bool found_match = false;
          // cout << " xx " << endl;
          for (const auto & e : og.out_matching(ss[s], letter_cond)) {
            // cout << "matching edge: " << e << endl;
            // cout << "transformed dest: " << og_idx_to_ss_idx[e.dest] << endl;
            if (og_idx_to_ss_idx[e.dest] == it->dest && it->cond.intersects(e.cond)) {
              it->cond &= e.cond;
              found_match = true;
              // cout << "  yes" << endl;
            } else {
              // cout << "  no" << endl;
            }
          }
          // this edge option in 'current_matches' doesn't match anything in this
          // state's matching edges. we need to therefore remove it.
          if (!found_match)
            current_matches.erase(it);
        }
      }
      if (current_matches.empty()) {
        // something went wrong here
        cout << "og state count = " << num_states << endl;
        cout << "ss count = " << state_sets.size() << endl;
        cout << "this ss length = " << ss.size() << endl;
        cout << "ss_idx = " << ss_idx << endl;

        cout << "current_matches:" << endl;
        for (const auto & e : current_matches)
          cout << " - " << e << endl;

        cout << "some specific debug info" << endl;

        og.dump_state(2);
        og.dump_state(9);
        og.dump_state_matching(2, letter_cond);
        og.dump_state_matching(9, letter_cond);

        throw std::runtime_error("looks like we built a stateset that isn't actually intercompatible?");
      }
      
      // at this point, 'current_matches' holds edge(s) that satisfy all states in the
      // state set. we can just pick any one destination and use that for our edge.
      unsigned edge_dest = current_matches.front().dest;
      BSC edge_cond = current_matches.front().cond;
      edge_cond.lock_values_within_mask(apinfo.output_mask);
      new_g.add_edge(ss_idx, og_idx_to_ss_idx[edge_dest], edge_cond);
    }
    // ok now every letter is done.
  }
  // and every stateset is done
}


    // iterate through the edges of the first state
    // for (const auto & og_edge : og.out(ss[0])) {
    //   const unsigned & edge_dest = og_edge.dest;

    //   BSC unanimous_edge_cond = og_edge.cond;

    //   // iterate through all output edges of all other states in this
    //   // stateset that share the same destination and have a colliding condition.
    //   for (unsigned other_state=1; other_state<num_states; ++other_state) {
    //     for (const auto & other_edge : og.out(ss[other_state])) {
    //       if (other_edge.dest == edge_dest) {
    //         if (other_edge.cond.intersects(unanimous_edge_cond)) {
    //           unanimous_edge_cond.conj_with(other_edge.cond);
    //           assert(unanimous_edge_cond.not_false());
    //           // we found an agreeing edge, we dont need to look at any
    //           // more edges from this state.
    //           break;
    //         }
    //       }
    //     }
    //   }

    //   // unanimous_edge_cond is shared by an edge on all other states in 
    //   // this stateset. we can create an edge with it.
    //   BSC new_edge_cond = unanimous_edge_cond;

    //   // choose one output letter
    //   // new_edge_cond.lock_values_within_mask(apinfo.output_mask);

    //   new_g.add_edge(ss_idx, edge_dest, new_edge_cond);
      

    // }


    // // we consider each possible *input* letter
    // for (const auto & letter : apinfo.masked_input_letters) {

    // }

  //   }
  // }

  // ensure_input_completeness(init_stateset, og, new_g, apinfo); // is it possible that it isn't?


bool single_state_strategy_exists(const BSG & g, const ap_info & apinfo) {
  for (const unsigned & input_letter : apinfo.masked_input_letters) {
    // for each letter we check what outputs are globally available.
    eBSC this_letter_outputs_avail = eBSC::True();
    for (unsigned s=0; s<g.num_states(); ++s) {
      eBSC this_state_outputs_avail = eBSC::False();
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

  // twa_graph_ptr g = bsg.to_twa_graph(strat->get_dict(), apinfo);
  // page_graph(g, "round-trip graph");

  auto incompat = incompatible_state_pairs(bsg, apinfo);
  build_greedy_graph(bsg, incompat, apinfo);


}