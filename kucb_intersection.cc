#pragma once
#include "common.hh"
#include "kcobuchi.cc"
#include "bsg.cc"
#include <queue>

typedef std::vector<unsigned> counting_fn;
typedef std::vector<std::vector<counting_fn>> kucb_counting_fns;

#define DEBUG if constexpr (print_debug)

//TODO: the below could be made a lot faster if we used
// my walker to avoid BDDs

spot::internal::const_universal_dests get_univ_or_single_edge(twa_graph_ptr g, unsigned edge_id) {
  if (g->is_univ_dest(edge_id)) {
    return g->univ_dests(edge_id);
  } else {
    return {edge_id};
  }
}

enum PartialOrderResult { equal, less, greater, incomparable };

template <typename T>
PartialOrderResult partial_order_compare_vectors(const vector<T> & vec_a, const vector<T> & vec_b, const unsigned starting_idx = 0) {
  // assume for now they are equal
  PartialOrderResult result = PartialOrderResult::equal;

  // iterate through the list. for each element, if "a" is lesser,
  // that is fine if everything previous was lesser or equal.
  // same logic for greater.
  // as soon as you have both greater and lesser, they are incomparible.
  for (unsigned i=0; i<vec_a.size()-starting_idx; ++i) {
    auto ai = vec_a[i];
    auto bi = vec_b[i];
    if (ai < bi) {
      if (result == PartialOrderResult::greater) return incomparable;
      result = PartialOrderResult::less;
    } else if (ai > bi) {
      if (result == PartialOrderResult::less) return incomparable;
      result = PartialOrderResult::greater;
    }
  }
  return result;
};

#define KEEP_GREATER
// #define KEEP_LESSER

PartialOrderResult add_counting_function(const counting_fn & new_cf, vector<counting_fn> & cfs) {
  // we assume that no cfs in the existing set subsume eachother, so we can
  // just check the new one against existing ones.

  // starting index is actually kinda relative ending index; 1 means we ignore
  // the last element.
  const constexpr unsigned starting_idx = 1;

  const unsigned size = cfs.size();
  for (unsigned i=0; i<size; ++i) {
    const counting_fn & cf = cfs[i];
    auto po = partial_order_compare_vectors(new_cf, cf, starting_idx);
    if (po == PartialOrderResult::incomparable) {
      // these are incomparable, so we need to check the next one.
      continue;
    } else if (po == PartialOrderResult::greater) {
      // this cf is subsumed by an existing one. nothing to add, and we are done.
      #ifdef KEEP_GREATER
        // cout << "    overriding cf" << endl;
        cfs[i] = new_cf;
      #endif
    } else if (po == PartialOrderResult::equal) {
      // we don't need to add anything, it's already in there.
    } else if (po == PartialOrderResult::less) {
      // this one subsumes an existing one.
      #ifdef KEEP_LESSER
        // cout << "    overriding cf" << endl;
        cfs[i] = new_cf;
      #endif
    }
    return po;
  }
  // it didn't interact with any in the existing set, so we need to add it.
  cfs.push_back(new_cf);
  return PartialOrderResult::incomparable;
};

void print_cf(counting_fn c) {
  cout << "[";
    for (unsigned i=0; i<c.size()-1; ++i) {
      unsigned x = c[i];
      if (x == 0) cout << "_";
      else cout << (x-1);
    }
    cout << "]. (" << c[c.size()-1] << ")" << endl;
};

bool cf_equal(const counting_fn & cf_a, const counting_fn & cf_b, const unsigned starting_idx=0) {
  const unsigned cf_a_size = cf_a.size();
  if (cf_a_size != cf_b.size()) return false;
  for (unsigned i=0; i<cf_a_size-starting_idx; i++) {
    if (cf_a[i] != cf_b[i]) return false;
  }
  return true;
}

bool moore_kucb_intersection(twa_graph_ptr moore, twa_graph_ptr ucb, unsigned k) {
  constexpr bool print_debug = true;
  const unsigned kp1 = k+1;
  const unsigned kp2 = k+2;

  // note that we use 0 to mean BOTTOM, meaning we need to subtract 1 from our counts.
  // so everything starts at 1, or 2 if it is accepting.
  auto ucb_asc = make_accepting_state_cache(ucb);
  auto moore_sink_idx_ptr = moore->get_named_prop<store_unsigned>("sink-idx");
  unsigned moore_sink_idx = moore_sink_idx_ptr->val;

  DEBUG cout << " >> Moore sink index = " << moore_sink_idx << endl;

  const unsigned moore_size = moore->num_states();
  const unsigned ucb_size = ucb->num_states();
  auto counts = kucb_counting_fns(moore_size);

  if (moore_sink_idx >= moore_size)
    throw std::runtime_error("we broke something...");

  if (moore_sink_idx == 0)
    throw std::runtime_error("we might have broken somethign? apparently state 0 is a sink. that is possible but not very likely.");

  // initial counting function
  const unsigned ucb_init_state = ucb->get_init_state_number();
  const unsigned init_state_init_count = is_accepting_state(ucb_init_state, ucb_asc) ? 2 : 1;
  auto initial_counting_fn = counting_fn(ucb_size+1, 0);
  initial_counting_fn[ucb_init_state] = init_state_init_count;

  counts[ucb_init_state].push_back(initial_counting_fn);


  // DECISIONS FOR HOW THIS SHOULD RUN:
  // - do we leave counting fns at the source state when we take an edge?
  // - when two fns are comparable, do we take the higher or lower?
  // - success condtn: - all <= k
  //                   - all >  k
  //                   - any <= k
  //                   - any >  k

  /*
    An answer to this, we are checking *INCLUSION*, which means we don't want just
    any counterexample: we only want a counterexample where:
    - The Moore Machine REJECTS the word
    - The kUCB ACCEPTS the word (it is safe in the kUCB)

    We know there is none when we reach a fixed-point.

    But do we want to keep the lesser or greater when they collide? We are trying to
    find something that is SAFE in the UCB, which I think means we need to keep the
    LESSER.
  */

  function<string(bdd)> bdd_str = [&](bdd b){
    stringstream ss;
    ss << bdd_to_formula(b, moore->get_dict());
    return ss.str();
  };

  // we start with a counting function, and we figure out what the next one will be,
  // given an edge condition.
  function<counting_fn(counting_fn &, bdd, unsigned)> subsequent_counting_fn = [&](const counting_fn & cf, bdd cond, const unsigned cf_tag) {
    DEBUG cout << "*** subseq ***" << endl;
    auto next_cf = counting_fn(ucb_size+1, 0);
    next_cf[ucb_size] = cf_tag;
    for (unsigned i=0; i<cf.size()-1; ++i) {
      const unsigned cfi = cf[i];
      // we only progress from states that already have a count
      if (cfi == 0) continue;
      if (cfi >= kp2) {
        // this cf has k+1 touched already. I'm tempted to not progress anything here now,
        // but i think that for fixpoint reasons we need to.
        //TODO: experiment with this.
      }

      DEBUG cout << "i = " << i << ", cfi = " << cfi << endl;

      // TODO: should totally have some caching here
      DEBUG cout << "moore edge cond " << bdd_str(cond) << endl;
      for (const auto & e : ucb->out(i)) {
        if (bdd_overlap(cond, e.cond)) {
        DEBUG cout << "  overlapping ucb edge cond " << bdd_str(e.cond) << endl;
        // cout << "  overlap? " << ((bdd_overlap(cond, e.cond)) ? "yes" : "no") << endl ;
          for (const auto & dst : get_univ_or_single_edge(ucb, e.dst)) {
            unsigned & cur_val_in_next_cf = next_cf[dst];
            if (cur_val_in_next_cf == 0 || cfi >= cur_val_in_next_cf) { // == 0 is redundant here right?
              cur_val_in_next_cf = cfi;
              if (ucb_asc[dst] && cfi < kp2) ++cur_val_in_next_cf;
            }
          }
        }
      }
    }
    DEBUG { cout << " subseq cf: "; print_cf(next_cf); }
    return next_cf;
  };

  function<bool(counting_fn &)> is_safe = [kp1](const counting_fn & cf){
    for (unsigned i=0; i<cf.size()-1; ++i) {
    // for (const auto & cf_val : cf) {
      if (cf[i] > kp1) return false;
    }
    return true;
  };

  function<bool()> take_step = [&](){
    // cout << "intersection taking step" << endl;
    bool reached_fixpoint = true; // set this to false if we change any counting fns/sets

    // we start with any states that currently have counting functions on them.
    for (unsigned mi=0; mi<moore_size; ++mi) {
      cout << " <> mi = " << mi << endl;
      if (mi == moore_sink_idx) {
        // we are in an accepting state in the moore machine.
        // for now i will still not bother progressing the counting
        // functions from here - i believe this will never yield anything useful.
        continue;
      }

      const auto & cf_set = counts[mi];
      if (cf_set.size() == 0) continue;

      // for each outgoing edge, we get the new set of edges stepped to in the ucb
      for (auto cf : cf_set) {
        for (const auto & e : moore->out(mi)) {
          const bdd & cond = e.cond;
          const unsigned edge_idx = moore->edge_number(e);
          counting_fn next_cf = subsequent_counting_fn(cf, cond, edge_idx);
          PartialOrderResult res = add_counting_function(next_cf, counts[e.dst]);
          if (e.dst == moore_sink_idx) {
            // cout << endl << "&&& we have a cf at the sink state" << endl;
            // cout << "k = " << k << endl;
            // print_cf(next_cf);
            // cout << (is_safe(next_cf) ? "safe" : "unsafe") << endl;
            // cout << endl << endl;
            if (is_safe(next_cf)) return false;
          }
          
          
          if (res == PartialOrderResult::greater || res == PartialOrderResult::incomparable) {
            // we made some change
            reached_fixpoint = false;
          }
        }
      }
    }
    return reached_fixpoint;
  };

  // check if we have currently found a counterexample. This would mean
  // that, at the moore sink state, we have a safe counting function.
  function<bool()> check_for_counterexample = [&](){
    vector<counting_fn> cfs = counts[moore_sink_idx];
    for (auto cf : cfs) if (is_safe(cf))
      return true;
    return false;
  };

  // given that we know we have a counterexample, actually extract it.
  function<shared_ptr<vector<bdd>>()> extract_counterexample = [&](){
    auto counterexample = make_shared<vector<bdd>>();
    const auto edges = moore->edge_vector();
    for (auto & cf : counts[moore_sink_idx]) if (is_safe(cf)) {
      // the "final" cf we are backtracking from is the safe cf at
      // the sink state.
      unsigned cur_idx = moore_sink_idx;
      counting_fn * cur_cf = &cf;
      while (cur_idx != moore->get_init_state_number()) {
        // get the edge that led to this cf
        const unsigned edge_idx = (*cur_cf)[cur_cf->size()-1];
        const auto e = edges[edge_idx];
        const bdd & cond = e.cond;
        const unsigned src = e.src;

        // add the appropriate bdd to (the end of) the word
        counterexample->push_back(cond);

        // take a look at the state this cf came from, and find a counting
        // function that is a valid immediate predecessor of this cf.
        // that must be the cf we came from.

        cur_idx = src;
        bool found = false;
        for (auto & cf : counts[src]) {
          const auto subseq = subsequent_counting_fn(cf, cond, edge_idx);
          if (cf_equal(subseq, *cur_cf, 1)) {
            // this is the predecessor
            cur_cf = &cf;
            found = true;
            break;
          }
        }
        if (found == false) {
          throw std::runtime_error("didn't find predecessor cf!");
        }
      }
    }
    return counterexample;
  };


  function<void()> print_current_cfs = [&](){
    cout << "current counting function state:" << endl;
    for (unsigned i=0; i<moore_size; ++i) {
      vector<counting_fn> cfs = counts[i];
      cout << ((i == moore_sink_idx) ? "** " : "   ") << i << ": { ";
      for (auto cf : cfs) {
        cout << "[";
        for (unsigned ii=0; ii<cf.size()-1; ++ii) {
          unsigned x = cf[ii];
          if (x == 0) cout << "_";
          else cout << (x-1);
        }
        cout << "] (" << cf[cf.size()-1] << "), ";
      }
      cout << " }" << endl;
    }
  };
  
  DEBUG print_current_cfs();
  unsigned max_iter = 2000;
  while(true && (--max_iter > 1)) {
    const bool reached_fixpoint = take_step();
    DEBUG print_current_cfs();
    if (reached_fixpoint) {
      // we have reached a fixpoint. this means there is no counterexample?
      cout << "reached fix point" << endl;
      return false;
    } else {
      // haven't reached a fixpoint. check if a counterexample has been found.
      cout << "checking for counterexample" << endl;
      if (check_for_counterexample()) {
        auto ce = extract_counterexample();
        cout << "found a counterexample" << endl;
        for (const auto & x : *ce) {
          cout << "   ***   " << bdd_str(x) << endl;
        }
        return true;
      }
    }
  }

  // (g_0 & g_1 & r_0 & r_1 & r_2) | (g_0 & g_2 & r_0 & r_1 & r_2) | (g_1 & g_2 & r_0 & r_1 & r_2)

  throw std::runtime_error("too many iterations while checking for intersection.");
}





unsigned count_deterministisation_states(const twa_graph_ptr & ucb, const unsigned k, const ap_info & apinfo) {
  constexpr bool debug = true;
  const unsigned kp1 = k+1;
  const unsigned kp2 = k+2;

  // note that we use 0 to mean BOTTOM, meaning we need to subtract 1 from our counts.
  // so everything starts at 1, or 2 if it is accepting.
  auto ucb_asc = make_accepting_state_cache(ucb);

  const unsigned ucb_size = ucb->num_states();
  // auto counts = kucb_counting_fns(moore_size);

  // initial counting function
  const unsigned ucb_init_state = ucb->get_init_state_number();
  const unsigned init_state_init_count = is_accepting_state(ucb_init_state, ucb_asc) ? 2 : 1;
  auto initial_counting_fn = counting_fn(ucb_size, 0);
  initial_counting_fn[ucb_init_state] = init_state_init_count;

  auto ucb_bsg = BSG(ucb, apinfo, true);

  assert (ucb_bsg.num_states() == ucb_size);

  auto next = [&](const counting_fn & cf, const unsigned & letter) {
    auto next_cf = counting_fn(ucb_size, 0);
    // start with any state currently active in the CF
    for (unsigned s=0; s<ucb_size; ++s) {
      const unsigned cfs = cf[s];
      if (cfs >= kp2) {
        // we are already at kp2 so we don't continue any further.
        next_cf[s] = kp2;
      } else {
        for (const auto & e : ucb_bsg.out_matching_letter(s, letter)) {
          if (cfs >= next_cf[e.dest]) {
            next_cf[e.dest] = cfs;
            if (ucb_asc[e.dest] && cfs < kp2)
              ++next_cf[e.dest];
          }
        }
      }
    }
    return next_cf;
  };


  auto waiting = std::queue<counting_fn>({initial_counting_fn});
  auto visited = std::set<counting_fn>();
  unsigned count = 1;

  while (!waiting.empty()) {
    auto F = waiting.front();
    waiting.pop();
    // for each possible input+output letter
    for (unsigned letter=0; letter<apinfo.letter_count; ++letter) {
      auto Fprime = next(F, letter);
      if (visited.find(Fprime) == visited.end()) {
        visited.insert(Fprime);
        ++count;
        waiting.push(Fprime);
      }
    }
  }
  return count;
}