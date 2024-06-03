#pragma once
#include "common.hh"
#include "kcobuchi.cc"

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

#define KEEP_GREATER
// #define KEEP_LESSER

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
    for (auto x : c) {
      if (x == 0) cout << "_";
      else cout << (x-1);
    }
    cout << "]" << endl;;
};

bool moore_kucb_intersection(twa_graph_ptr moore, twa_graph_ptr ucb, unsigned k) {
  constexpr bool print_debug = false;
  const unsigned kp1 = k+1;
  const unsigned kp2 = k+2;

  // note that we use 0 to mean BOTTOM, meaning we need to subtract 1 from our counts.
  // so everything starts at 1, or 2 if it is accepting.
  auto ucb_asc = make_accepting_state_cache(ucb);
  auto moore_sink_idx_ptr = moore->get_named_prop<store_unsigned>("sink-idx");
  unsigned moore_sink_idx = moore_sink_idx_ptr->val;

  const unsigned moore_size = moore->num_states();
  const unsigned ucb_size = ucb->num_states();
  auto counts = kucb_counting_fns(moore_size);

  if (moore_sink_idx >= moore_size)
    throw std::runtime_error("we broke something...");

  // initial counting function
  const unsigned ucb_init_state = ucb->get_init_state_number();
  const unsigned init_state_init_count = is_accepting_state(ucb_init_state, ucb_asc) ? 2 : 1;
  auto initial_counting_fn = counting_fn(ucb_size);
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
  function<counting_fn(counting_fn*, bdd)> subsequent_counting_fn = [&](counting_fn* cf, bdd cond) {
    DEBUG cout << "*** subseq ***" << endl;
    auto next_cf = counting_fn(ucb_size, 0);
    for (unsigned i=0; i<cf->size(); ++i) {
      const unsigned cfi = (*cf)[i];
      // we only progress from states that already have a count
      if (cfi == 0) continue;
      if (cfi >= kp2) {
        // this cf has k+1 touched already. I'm tempted to not progress anything here now,
        // but i think that for fixpoint reasons we need to.
        //TODO: experiment with this.
      }

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

  function<bool()> take_step = [&](){
    // cout << "intersection taking step" << endl;
    bool reached_fixpoint = true; // set this to false if we change any counting fns/sets

    // we start with any states that currently have counting functions on them.
    for (unsigned mi=0; mi<moore_size; ++mi) {
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
          counting_fn next_cf = subsequent_counting_fn(&cf, cond);
          PartialOrderResult res = add_counting_function(next_cf, counts[e.dst]);
          if (res == PartialOrderResult::greater || res == PartialOrderResult::incomparable) {
            // we made some change
            reached_fixpoint = false;
          }
        }
      }
    }
    return reached_fixpoint;
  };

  function<bool(counting_fn *)> is_safe = [kp1](counting_fn * cf){
    for (const auto & cf_val : (*cf)) {
      if (cf_val > kp1) return false;
    }
    return true;
  };

  // check if we have currently found a counterexample. This would mean
  // that, at the moore sink state, we have a safe counting function.
  function<bool()> check_for_counterexample = [&](){
    vector<counting_fn> cfs = counts[moore_sink_idx];
    for (auto cf : cfs) {
      if (is_safe(&cf)) {
        return true;
      }
    }
    return false;
  };


  function<void()> print_current_cfs = [&](){
    cout << "current counting function state:" << endl;
    for (unsigned i=0; i<moore_size; ++i) {
      vector<counting_fn> cfs = counts[i];
      cout << ((i == moore_sink_idx) ? "** " : "   ") << i << ": { ";
      for (auto cf : cfs) {
        cout << "[";
        for (auto x : cf) {
          if (x == 0) cout << "_";
          else cout << (x-1);
        }
        cout << "], ";
      }
      cout << " }" << endl;
    }
  };
  
  // print_current_cfs();
  unsigned max_iter = 20;
  while(true && (--max_iter > 1)) {
    const bool reached_fixpoint = take_step();
    // print_current_cfs();
    if (reached_fixpoint) {
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

  // (g_0 & g_1 & r_0 & r_1 & r_2) | (g_0 & g_2 & r_0 & r_1 & r_2) | (g_1 & g_2 & r_0 & r_1 & r_2)

  throw std::runtime_error("too many iterations while checking for intersection.");
}