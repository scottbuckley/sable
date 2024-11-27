#pragma once
#include "common.hh"
#include "safety_games.cc"
#include "bsg.cc"
#include "bdd.cc"

bool simulates_single(const unsigned & s1, const unsigned & s2, const BSG & bsg) {
  // s1 simulates s2

  // each safe action in s1 can also be performed by s2.
  bool found_truth = false;
  for (const auto & e1 : bsg.out(s1)) {
    // for each cond in s1 ...
    found_truth = false;
    for (const auto & e2 : bsg.out(s2)) {
      // find a matching cond in s2
      if (e2.cond.get_truth() == e1.cond.get_truth()) {
        // we found a match
        found_truth = true;
        break;
      }
    }
    if (!found_truth) return false;
  }
  return true;
}

void print_sim(const std::vector<boost::dynamic_bitset<>> & sim) {
  const unsigned size = sim.size();
    for (unsigned s1=0; s1<size; ++s1) {
      cout << s1 << ": ";
      for (unsigned s2=0; s2<size; ++s2) {
        cout << sim[s1][s2] << " ";
      }
      cout << endl;
    }
}

// assumes g is totally unmerged.
std::vector<boost::dynamic_bitset<>> compute_initial_sim(const BSG & g) {
  const unsigned num_states = g.num_states();

  auto sim = std::vector<boost::dynamic_bitset<>>(num_states, boost::dynamic_bitset<>(num_states));

  // sim_0
  for (unsigned s1=0; s1<num_states; ++s1) {
    for (unsigned s2=0; s2<num_states; ++s2) {
      if (s1 == s2) {
        sim[s1][s2] = true;
      } else {
        sim[s1][s2] = simulates_single(s1, s2, g);
      }
    }
  }
  return sim;
}

bool simulates_iter(const unsigned & s1, const unsigned & s2, const BSG & bsg, const std::vector<boost::dynamic_bitset<>> & sim) {
  // s1 simulates s2

  // each safe action in s2 can also be performed by s1, AND
  // in a way that points to simulating states.
  bool found_match = false;
  for (const auto & e1 : bsg.out(s1)) {
    // for each cond in s2 ...
    found_match = false;
    for (const auto & e2 : bsg.out(s2)) {

      if ((e2.cond.get_truth() == e1.cond.get_truth()) && sim[e1.dest][e2.dest]) {
        // matching edges point to simulable states.
        found_match = true;
        break;
      }
    }
    if (!found_match) return false;
  }
  return true;
}

bool iterate_sim(const BSG & g, std::vector<boost::dynamic_bitset<>> & sim) {
  const unsigned num_states = g.num_states();
  bool changed = false;

  for (unsigned s1=0; s1<num_states; ++s1) {
    for (unsigned s2=0; s2<num_states; ++s2) if (s1 != s2) {
      if (sim[s1][s2]) {
        // s1 simulates s2 in s_(n-1)
        if (!simulates_iter(s1, s2, g, sim)) {
          sim[s1][s2] = false;
          changed = true;
        }
      }
    }
  }
  return changed;
}

// unsigned count_1s(const std::vector<boost::dynamic_bitset<>> & sim) {
//   unsigned sum = 0;
//   for (const auto & bs : sim)
//     sum += bs.count();
//   return sum;
// }

std::vector<boost::dynamic_bitset<>> compute_sim(const BSG & g, const ap_info & apinfo, int sink_idx = -1) {

  //FIXME. REMOVE THIS CHECK. once we know that it will always hold
  // this is an expensive check. only for debug runs.
  if ((sink_idx==-1 && !g.check_totally_unmerged(apinfo.all_mask))
   || (sink_idx!=-1 && !g.check_totally_unmerged(apinfo.all_mask, sink_idx))) {
    // not all edges are fully defined (for both input and output)
    cout << "problem! we are trying to compute SIM on a graph that's partially merged." << endl;
    g.dump_all_states();
    assert(false);
  }

  auto sim = compute_initial_sim(g);

  // cout << "init sim:" << endl;
  // print_sim(sim);

  while(iterate_sim(g, sim)) {
    // cout << "subseq sim:" << endl;
    // print_sim(sim);
  }
  return sim;
}

unsigned compute_dominant(const unsigned & s, const boost::dynamic_bitset<> & win, const std::vector<boost::dynamic_bitset<>> & sim) {
  unsigned t0 = s;

  // find something that simulates t0.
  bool could_have_simulator = true;
  while (could_have_simulator) {
    could_have_simulator = false;
    for (size_t t1=win.find_first(); t1!=win.npos; t1=win.find_next(t1)) {
      if (!sim[t0][t1] && sim[t1][t0]) {
        could_have_simulator = true;
        t0 = t1;
        break;
      }
    }
  }
  return t0;
}

std::vector<unsigned> compute_abstraction(const std::vector<boost::dynamic_bitset<>> & sim) {
  const unsigned num_states = sim.size();
  auto win = boost::dynamic_bitset<>();
  win.resize(num_states, true);
  // cout << "init win" << endl;
  // cout << win << endl;
  auto alpha = std::vector<unsigned>(num_states); // 0 = what?
  while (win.any()) {
    // while there are any states left in win
    const unsigned s = win.find_first();
    // cout << "  looking at " << s << endl;
    const unsigned s_dominant = compute_dominant(s, win, sim);
    // cout << "  dominannt = " << s_dominant << endl;
    // now we have the supersimulator. for every state that s_dominant
    // simulates, we will add a mapping to alph and remove that state
    // from win.
    // auto & sim_dominant_row = sim[s_dominant];
    for (size_t s2=win.find_first(); s2!=win.npos; s2=win.find_next(s2)) {
      // cout << "    against " << s2 << endl;
      if (sim[s_dominant][s2]) {
        // cout << "    adding" << endl;
        alpha[s2] = s_dominant;
        win.reset(s2);
      }
    }
  }
  return alpha;
}

mutable_BSG compute_strat(const BSG & g, const std::vector<boost::dynamic_bitset<>> & sim, const std::vector<unsigned> & alpha, const ap_info & apinfo) {
  mutable_BSG new_g = mutable_BSG();
  std::vector<unsigned> new_state_to_dominant;
  auto to_develop = std::queue<unsigned>();
  auto added_to_new_state = boost::dynamic_bitset<>(g.num_states(), false);

  // set up initial state and add it to "to_develop"
  const unsigned q0 = new_g.add_state();
  assert (q0 == 0);
  new_state_to_dominant.push_back(alpha[0]);
  added_to_new_state.set(alpha[0]);
  to_develop.push(0);

  while (!to_develop.empty()) {
    const unsigned new_q = to_develop.front();
    const unsigned q = new_state_to_dominant[new_q];
    to_develop.pop();
    
    // now we have some q to consider.
    // let us consider every input letter
    for (const auto & i : apinfo.masked_input_letters) {
      // now we create a set of all options of (output + destination)
      // that are viable options from q.
      bool found_ideal_edge = false;
      for (const auto & e : g.out(q)) {
        // for each output edge from q, if it's for this input letter
        if (e.cond.intersects(BSC(i, apinfo.input_mask))) {
          const unsigned dest_dominant = alpha[e.dest];
          if (added_to_new_state[dest_dominant]) {
            // this is an edge to a state already added to the new graph.
            // we will use this one. but we have to just find which one it is.
            for (unsigned new_s=0; new_s<new_state_to_dominant.size(); ++new_s) {
              if (new_state_to_dominant[new_s] == dest_dominant) {
                new_g.add_edge(new_q, new_s, e.cond);
                found_ideal_edge = true;
                break;
              }
            }
            break;
          } else {
            // the destination state doesn't have its dominant in the graph yet,
            // but it's still possible that one of the existing states
            // simulates it. we search for that possibility here.
            for (unsigned new_s=0; new_s<new_state_to_dominant.size(); ++new_s) {
              if (sim[new_state_to_dominant[new_s]][e.dest]) {
                new_g.add_edge(new_q, new_s, e.cond);
                found_ideal_edge = true;
                break;
              }
            }
            if (found_ideal_edge) break;
          }
        }
      }

      if (!found_ideal_edge) {
        // we couldn't find an edge to a state we are already using, so we
        // will just have to choose one at random.
        for (const auto & e : g.out(q)) {
          // for each output edge from q, if it's for this input letter
          if (e.cond.intersects(BSC(i, apinfo.input_mask))) {
            const unsigned & dest_dominant = alpha[e.dest];
            const unsigned new_dest_idx = new_g.add_state();
            added_to_new_state.set(dest_dominant);
            new_state_to_dominant.push_back(dest_dominant);
            new_g.add_edge(new_q, new_dest_idx, e.cond);
            to_develop.push(new_dest_idx);
            break;
          }
        }
      }
    }
  }
  return new_g;
}

twa_graph_ptr find_smaller_simulation(const twa_graph_ptr & strat, const ap_info & apinfo) {
  // page_text("trying to find a smaller substrategy from the final solved hypothesis, via simulation ...");

  // auto strat = get_strategy_machine(hyp);
  // strat->merge_edges(); // do we want to do this?
  // page_graph(strat, "most permissive strat");

  auto bsg = BSG(strat, apinfo);

  // cout << "dumping initial graph ..." << endl;
  // bsg.dump_all_states();
  // cout << endl << endl << endl;

  auto sim = compute_sim(bsg, apinfo);
  // cout << "sim*:" << endl;
  // print_sim(sim);

  auto alpha = compute_abstraction(sim);
  // cout << "alpha:" << endl;
  // print_vector(alpha);
  auto new_g = compute_strat(bsg, sim, alpha, apinfo);

  // cout << "new graph formed. dumping ..." << endl;
  // new_g.dump_all_states();
  // cout << endl << endl << endl;

  const auto & new_twa_graph = new_g.to_twa_graph(apinfo, strat->get_dict());
  new_twa_graph->merge_edges();
  // page_graph(new_twa_graph, "littlised graph");
  return new_twa_graph;
}

mutable_BSG compute_strat_moore(const BSG & g, const std::vector<boost::dynamic_bitset<>> & sim, const std::vector<unsigned> & alpha, unsigned moore_sink_idx, const ap_info & apinfo) {
  mutable_BSG new_g = mutable_BSG();
  std::vector<unsigned> new_state_to_dominant;
  auto to_develop = std::queue<unsigned>();
  auto added_to_new_state = boost::dynamic_bitset<>(g.num_states(), false);

  auto add_to_graph = [&](const unsigned & old_s){
    const unsigned new_s = new_g.add_state();
    new_state_to_dominant.push_back(alpha[old_s]);
    added_to_new_state.set(alpha[old_s]);
    return new_s;
  };

  // set up initial state and add it to "to_develop"
  add_to_graph(0);
  to_develop.push(0);

  // also add the sink state to the new machine, but we don't
  // find its dominant state; we use it as-is.
  const unsigned new_sink = new_g.add_state();
  new_state_to_dominant.push_back(moore_sink_idx);
  added_to_new_state.set(moore_sink_idx);
  new_g.add_edge(new_sink, new_sink, BSC::True());

  while (!to_develop.empty()) {
    const unsigned new_q = to_develop.front();
    const unsigned q = new_state_to_dominant[new_q];
    to_develop.pop();

    // now we have some q to consider.
    // first we collect all the input letters that are legal from here
    unordered_set<unsigned> inputs;
    for (const auto & e : g.out(q)) {
      inputs.insert(e.cond.get_truth() & apinfo.input_mask);
    }


    // we want to know the new total number of states if we chose each input
    unsigned current_best_input = 0;
    unsigned current_best_input_size = UINT_MAX;
    for (const auto & i : inputs) {
      
      auto existing_states = std::vector<unsigned>(new_state_to_dominant);
      auto input_match = BSC(i, apinfo.input_mask);
      for (const auto & e : g.out(q)) if (e.cond.intersects(input_match)) {
        bool dest_already_created = false;
        // if its dominant is already in the graph, we won't need to add anything
        if (added_to_new_state[alpha[e.dest]]) continue;
        if (e.dest == moore_sink_idx) continue;

        // otherwise, we check if anything in the graph simulates it
        for (const auto & s : existing_states) {
          if (sim[s][e.dest]) {
            // we already have something in the graph that simulates this destination
            dest_already_created = true;
            break;
          }
        }
        if (!dest_already_created)
          existing_states.push_back(alpha[e.dest]);
      }

      // if the new total number of states is the new minimum, this is the input we will go for.
      if (existing_states.size() < current_best_input_size) {
        current_best_input_size = existing_states.size();
        current_best_input = i;
      }
    }

    const unsigned & chosen_input = current_best_input;
    const auto input_match = BSC(current_best_input, apinfo.input_mask);

    // we have chosen the input we will use, now we need to actually add it
    // all to the graph
    for (const auto & e : g.out(q)) if (e.cond.intersects(input_match)) {
      // again we need to see what this dest is going to.
      const unsigned & dest = e.dest;
      const unsigned & dest_dominant = alpha[dest];

      if (dest == moore_sink_idx) {
        new_g.add_edge(new_q, new_sink, e.cond);
      } else if (added_to_new_state[alpha[dest]]) {
        // we know that the destination is already included in the graph, just
        // gotta find the right state that is made from that dominant.
        bool added_edge = false;
        for (unsigned state_in_new_graph=0; state_in_new_graph<new_state_to_dominant.size(); ++state_in_new_graph) {
          if (new_state_to_dominant[state_in_new_graph] == alpha[dest]) {
            // s is the new state we want
            new_g.add_edge(new_q, state_in_new_graph, e.cond);
            added_edge = true;
            break;
          }
        }
        assert(added_edge);
      } else {
        // we need to look through all the current states and see if any of them
        // simulate this dest.
        bool found_existing_simulator = false;
        for (unsigned state_in_new_graph=0; state_in_new_graph<new_state_to_dominant.size(); ++state_in_new_graph) {
          if (sim[new_state_to_dominant[state_in_new_graph]][dest]) {
            // state_in_new_graph simulates the destination, we can just point to that
            new_g.add_edge(new_q, state_in_new_graph, e.cond);
            found_existing_simulator = true;
            break;
          }
        }
        if (!found_existing_simulator) {
          // none of the existing states simulate the destination,
          // so we need to create a new state.
          unsigned new_state = add_to_graph(dest_dominant);
          new_g.add_edge(new_q, new_state, e.cond);
          to_develop.push(new_state);
        }
      }
    }
  }
  return new_g;
}

twa_graph_ptr find_smaller_simulation_moore(const twa_graph_ptr & strat, const ap_info & apinfo) {
  // page_text("trying to find a smaller substrategy from the final solved hypothesis, via simulation ...");

  // auto strat = get_strategy_machine(hyp);
  // strat->merge_edges(); // do we want to do this?
  // page_graph(strat, "most permissive strat");

  auto bsg = BSG(strat, apinfo);

  // page_graph(strat, "moore strat before minimisation");

  unsigned moore_sink_idx = strat->get_named_prop<store_unsigned>("sink-idx")->val;

  // cout << "dumping initial graph ..." << endl;
  // bsg.dump_all_states();
  // cout << endl << endl << endl;

  // same as mealy
  auto sim = compute_sim(bsg, apinfo, moore_sink_idx);
  // modify "sim" so that the sink only simulates itself
  sim[moore_sink_idx].reset();
  sim[moore_sink_idx].set(moore_sink_idx);
  // cout << "sim*:" << endl;
  // print_sim(sim);


  // same as mealy
  auto alpha = compute_abstraction(sim);
  // cout << "alpha:" << endl;
  // print_vector(alpha);

  // different to mealy
  auto new_g = compute_strat_moore(bsg, sim, alpha, moore_sink_idx, apinfo);

  // cout << "new graph formed. dumping ..." << endl;
  // new_g.dump_all_states();
  // cout << endl << endl << endl;

  const auto & new_twa_graph = new_g.to_twa_graph(apinfo, strat->get_dict());
  new_twa_graph->set_named_prop<store_unsigned>("sink-idx", new store_unsigned(1));
  // new_twa_graph->copy_named_properties_of(strat);
  // new_twa_graph->merge_edges();
  // page_graph(new_twa_graph, "littlised graph");
  return new_twa_graph;
  

}