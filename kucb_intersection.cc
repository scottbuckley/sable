#pragma once
#include "common.hh"
#include "kcobuchi.cc"

typedef std::vector<unsigned> counting_fn;
typedef std::vector<std::vector<counting_fn>> kucb_counting_fns;


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
PartialOrderResult partial_order_compare_vectors(const vector<T> & vec_a, const vector<T> & vec_b) {
  // assume for now they are equal
  PartialOrderResult result = PartialOrderResult::equal;

  // iterate through the list. for each element, if "a" is lesser,
  // that is fine if everything previous was lesser or equal.
  // same logic for greater.
  // as soon as you have both greater and lesser, they are incomparible.
  for (unsigned i=0; i<vec_a.size(); ++i) {
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

PartialOrderResult add_counting_function(const counting_fn & new_cf, vector<counting_fn> & cfs) {
  // we assume that no cfs in the existing set subsume eachother, so we can
  // just check the new one against existing ones.
  const unsigned size = cfs.size();
  for (unsigned i=0; i<size; ++i) {
    const counting_fn & cf = cfs[i];
    auto po = partial_order_compare_vectors(new_cf, cf);
    if (po == PartialOrderResult::incomparable) {
      // these are incomparable, so we need to check the next one.
      continue;
    } else if (po == PartialOrderResult::greater) {
      // this one subsumes an existing one.
      cfs[i] = new_cf;
    } else if (po == PartialOrderResult::equal) {
      // we don't need to add anything, it's already in there.
    } else if (po == PartialOrderResult::less) {
      // this cf is subsumed by an existing one. nothing to add, and we are done.
    }
    return po;
  }
  // it didn't interact with any in the existing set, so we need to add it.
  cfs.push_back(new_cf);
  return PartialOrderResult::incomparable;
};

bool moore_kucb_intersection(twa_graph_ptr moore, twa_graph_ptr ucb, unsigned k) {
  const unsigned kp1 = k+1;
  const unsigned kp2 = k+2;

  // note that we use 0 to mean BOTTOM, meaning we need to subtract 1 from our counts.
  // so everything starts at 1, or 2 if it is accepting.
  auto asc = make_accepting_state_cache(moore);

  const unsigned moore_size = moore->num_states();
  const unsigned ucb_size = ucb->num_states();
  auto counts = kucb_counting_fns(moore_size);

  // initial counting function
  const unsigned init_state = moore->get_init_state_number();
  const unsigned init_state_init_count = is_accepting_state(init_state, asc) ? 2 : 1;
  auto initial_counting_fn = counting_fn(ucb_size);
  initial_counting_fn[init_state] = init_state_init_count;

  counts[init_state].push_back(initial_counting_fn);


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

    We know there is none when we reach a fixed-point
  */

  // 



  function<counting_fn(counting_fn*, bdd)> subsequent_counting_fn = [&](counting_fn* cf, bdd cond) {
    auto next_cf = counting_fn(ucb_size, 0);
    for (unsigned i=0; i<cf->size(); ++i) {
      const unsigned cfi = (*cf)[i];
      // we only progress from states that already have a count
      if (cfi == 0) continue;

      // TODO: should totally have some caching here
      for (const auto & e : ucb->out(i)) {
        if (bdd_implies(cond, e.cond)) {
          for (const auto & dst : get_univ_or_single_edge(ucb, e.dst)) {
            unsigned & cur_val_in_next_cf = next_cf[dst];
            if (cur_val_in_next_cf == 0 || cfi >= cur_val_in_next_cf) { // == 0 is redundant here right?
              cur_val_in_next_cf = cfi;
              if (asc[dst] && cfi < kp2) ++cur_val_in_next_cf;
            }
          }
        }
      }
    }
    return next_cf;
  };

  function<bool()> take_step = [&](){
    bool reached_fixpoint = true; // set this to false if we change any counting fns/sets

    // we start with any states that currently have counting functions on them.
    for (unsigned mi=0; mi<moore_size; ++mi) {
      if (is_accepting_state(mi, asc)) {
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
          counting_fn next_cf = subsequent_counting_fn(&cf, cond);
          PartialOrderResult res = add_counting_function(next_cf, counts[e.dst]);
          if (res == PartialOrderResult::less || res == PartialOrderResult::incomparable) {
            // we made some change
            reached_fixpoint = false;
          }
        }
      }
    }
    return reached_fixpoint;
  };

  function<bool(counting_fn *)> is_safe = [kp1](const counting_fn * cf){
    for (const auto & cf_val : (*cf)) {
      if (cf_val > kp1) return false;
    }
    return true;
  };

  function<bool()> check_for_counterexample = [&](){
    for (size_t i=asc.find_first(); i!=accepting_state_cache::npos; i=asc.find_next(i)) {
      for (auto cf : counts[i]) {
        if (is_safe(&cf)) {
          return true;
        }
      }
    }
    return false;
  };
  
  while(true) {
    if (take_step()) {
      // we have reached a fixpoint. this means there is no counterexample?
      cout << "reached fix point" << endl;
      return false;
    } else {
      // haven't reached a fixpoint. check if a counterexample has been found.
      if (check_for_counterexample()) {
        cout << "found a counterexample" << endl;
        return true;
      }
    }
  }
}