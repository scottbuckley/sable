#pragma once
#include "bsc.cc"
#include "bsg.cc"

#include <bit>
#include <bitset>
#include <cmath>
#include <iostream>

#define ASSERT_NOT_FALSE

#define KEEP_DISJ_STRICTLY_DISJOINT 1

#define dout if constexpr (debug) cout
#define ddout if (debug) cout

#ifdef ASSERT_NOT_FALSE
  #define CANT_BE_FALSE(bsc) assert(!bsc.is_false())
  #define THIS_CANT_BE_FALSE assert(!is_false())
#else
  #define CANT_BE_FALSE(bsc) 
  #define THIS_CANT_BE_FALSE 
#endif



/* extended BSGs */

struct eBSC {
  std::list<BSC> options;

  // the minimisation feature may not be needed really, since we have
  // taut checking now. but for now we do a minimisation every now and then.
  static const unsigned changes_between_minimisation = 100;
  unsigned potentially_simpifiable_changes = 0;

  // tracks whether it may be worth performing a (new) taut check.
  unsigned growth_since_taut_check = false;

  /* note: the inverse is an OVERAPPROXIMATION of the inverse.
    The invariant is that inverse contains some truth that is not held
    in this eBSC (if there is one). */
  std::list<BSC> inverse;
  
  // BSC bsc;
  // eBSC * next = nullptr;

  // eBSC() {
  //   // the default eBCS is FALSE
  // }

  // eBSC(BSC bsc) {
  //   if (bsc.not_false()) options.push_front(bsc);
  // }

private:
  eBSC() {
    // the default eBSC is FALSE
    inverse.emplace_back(0,0); // inverse = true
  }

  void set_false() {
    // set to FALSE
    potentially_simpifiable_changes = 0;
    options.clear();
    inverse.clear();
    inverse.emplace_back(0,0); // inverse = true
  }

  void set_true() {
    // set to FALSE
    potentially_simpifiable_changes = 0;
    options.clear();
    inverse.clear();
    options.emplace_back(0,0); // options = true
  }

  void invalidate_inverse_cache() {
    inverse.clear();
    inverse.emplace_back(0,0); // inverse = true
  }

public:

  inline static eBSC False() {
    return eBSC();
  }

  static eBSC True() {
    eBSC out = eBSC();
    out.options.emplace_front(0,0);
    out.inverse.clear();
    return out;
  }

  static eBSC from_BSC(BSC bsc) {
    if (bsc.not_false()) {
      eBSC out = eBSC();
      out.options.emplace_front(bsc);
      return out;
    } else {
      return eBSC::False();
    }
  }


  /* attempt to minimise this  eBSC. return true if you did any
    "hard work" in the process. */
  bool minimise() {
    if (potentially_simpifiable_changes >= changes_between_minimisation) {
      full_minimise();
      return true;
    }
    return false;
  }

private:
  void full_minimise() {
    constexpr bool debug = false;
    // assert(!is_simple_true());
    dout << "--- minimising ";
    dout << s() << endl;
    
    auto it_primary = options.begin();
    auto it_secondary = options.begin();
    ++it_secondary;

    bool primary_changed = false;
    bool secondary_changed = false;

    bool reuse_primary = false; // r = retry this primary option.

    while (it_primary != options.end()) {
      while (it_secondary != options.end()) {
        if (it_secondary == it_primary) {
          ++it_secondary;
          continue;
        }

        dout << "minim compare: " << it_primary->s() << " | " << it_secondary->s() << endl;

        unsigned x = it_primary->pathy + it_secondary->pathy;

        string pre = it_primary->s() + " vs " + it_secondary->s();
        disj_with_single_disjoint_inputs(*it_primary, *it_secondary, primary_changed, secondary_changed);

        // making sure that primary points to the leftmost item.
        if (primary_changed || secondary_changed) {
          dout << "action: " << pre << endl;
          dout << "now:    " << it_primary->s() << " vs " << it_secondary->s() << endl;
          dout << s() << endl;

          if (it_primary->is_true() || it_secondary->is_true()) return set_true();

          if (it_secondary->is_false()) {
            options.erase(it_secondary);
            reuse_primary = true;
            dout << "secondary deleted" << endl;
            dout << s() << endl;
            break;
          } else {
            // only one has changed - if they both changed, secondary would be false.
            assert (!primary_changed || !secondary_changed);
            if (primary_changed) {
              reuse_primary = true;
              dout << "primary changed" << endl;
              break;
            } else if (secondary_changed) {
              // move 'secondary' to just before 'primary'.
              options.insert(it_primary, *it_secondary);
              options.erase(it_secondary);
              --it_primary;
              reuse_primary = true;
              dout << "secondary changed. weird." << endl;
              dout << s() << endl;
              break;
            }
          }
        } else {
          dout << "no change" << endl;
        }
        ++it_secondary;
      }

      if (reuse_primary) {
        reuse_primary = false;
        it_secondary = options.begin();
      } else {
        ++it_primary;
        it_secondary = it_primary;
        it_secondary++;
      }
    }
  }

  inline void become_copy_of(const eBSC & other) {
    copy_list_into(other.options, options);
    potentially_simpifiable_changes = other.potentially_simpifiable_changes;
    copy_list_into(other.inverse, inverse);
    growth_since_taut_check = true;
  }

public:
  void check_validity() const {
    unsigned cur_index = 0;
    bool has_true = false;
    for (const auto & bsc : options) {
      bsc.check_validity();
      if (bsc.is_false()) {
        throw runtime_error("this eBSC contains False options");
      } else if (bsc.is_true()) {
        has_true = true;
      }
      cur_index++;
    }
    if (has_true && cur_index > 1) {
      throw runtime_error("this eBSC contains True option, but isn't the True eBSC.");
    }
  }

private:
  inline void note_simplifiable_change() noexcept {
    ++potentially_simpifiable_changes;
  }


  std::list<BSC> ensure_disjoint(const BSC & bsc, const BSC & other) {
    auto out = std::list<BSC>();
    const unsigned common_pathy = bsc.pathy & other.pathy;
    unsigned new_pathy = bsc.pathy & ~other.pathy;
    assert (new_pathy != 0);
    unsigned cumul_pathy = 0;
    unsigned cumul_truth = 0;
    while (new_pathy) {
      // first set bit of new_pathy
      const unsigned np_bit = new_pathy & -new_pathy;
      new_pathy ^= np_bit;
      auto new_cond = BSC(other.truth | cumul_truth | (np_bit & ~bsc.truth), other.pathy | cumul_pathy | np_bit);
      out.emplace_back(new_cond);
      cumul_pathy |= np_bit;
      cumul_truth |= (np_bit & bsc.truth);
    }  
    return out;
  }
  

  /* Perform a "single-step" disjunction between 'bsc' and the condition(s) in 'others'.
     We assume that this condition is not TRUE or FALSE, and that 'other' does not
     contain TRUE or FALSE.
     'bsc' and 'others' may me modified in-place. 'other' is emptied if there are
     no more conditions needed to disjoin.
     'bsc_changed' and 'other_changed' are set to true if 'bsc' and 'others' are changed.
  */
  void disj_with_single(BSC & bsc, std::list<BSC> & others, bool & bsc_changed, bool & other_changed) {
    other_changed = false;
    bsc_changed = false;

    constexpr bool debug = false;

    dout << "performing single disj. bsc = " << bsc.s() << endl;

    for (auto it = others.begin(); it != others.end(); ++it) {
      auto & other = *it;
      dout << " against: " << other.s() << endl;

      const unsigned common_pathy = bsc.pathy & other.pathy;
      const unsigned truth_diff = bsc.truth ^ other.truth;
      const unsigned common_truth_diff = truth_diff & common_pathy;

      if (common_truth_diff) {
        dout << "c1" << endl;
        // we disagree on at least one AP - no intersection.
        if (has_single_bit(common_truth_diff)) {
          dout << "s1" << endl;
          // there is exactly one AP that we disagree on - we can do some
          // clever stuff here.
          if (bsc.pathy == other.pathy) {
            dout << "sa" << endl;
            // eg: ABC | AB^C = AB
            bsc.pathy ^= common_truth_diff;
            bsc.truth &= bsc.pathy;
            others.erase(it);
            bsc_changed = true;
            other_changed = true;
          // } else if (other.pathy == common_pathy) {
          } else if (false) {
            dout << "sb" << endl;
            // eg: ^ABC^D | BD = ^ABC | BD <- colliding still!
            // eg: ^ABC^D | BD = ^ABC | ABD | B^CD <- this is better
            unsigned new_pathy = bsc.pathy ^ common_pathy;

            bsc.pathy ^= common_truth_diff;
            bsc.truth &= bsc.pathy;
            bsc_changed = true;
            // the bits we care about that the other condition doesn't.
            
          } else if (false) {
          // } else if (bsc.pathy == common_pathy) {
            dout << "sc" << endl;
            // eg: ^B | ^ABC = ^B | ^AC
            other.pathy ^= common_truth_diff;
            other.truth &= other.pathy;
            other_changed = true;
          } else {
            dout << "sd__" << endl;
            // eg: ABC | B^CD
            // eg: ^A^BC | BD  
            // no simplification to be made here
          }
        } else {
          dout << "s0__" << endl;
          // there is no collision and no simple trick.
          // nothing we can do here.
        }
      } else {
        dout << "c0" << endl;
        // there is no difference in the things we both care about, which means
        // there is an intersection of conditions.
        if (bsc.pathy == common_pathy) {
          dout << "c1" << endl;
          // 'bsc' subsumes 'other'
          // eg: A^B | A^BC = A^B
          // eg: A^BC | A^BC = A^BC
          others.erase(it);
          other_changed = true;
        } else if (other.pathy == common_pathy) {
          dout << "c2" << endl;
          // 'other' subsumes 'bsc'
          // eg: A^BC | A^B = A^B
          bsc.pathy = other.pathy;
          bsc.truth = other.truth;
          others.erase(it);
          bsc_changed = true;
          other_changed = true;
        } else {
          // eg: ABC | BD = ABC | ^ABD | B^CD
          dout << "tricky" << endl;
          // now we know the conditions have an intersection, but don't subsume
          // one another. This is a tricky case - we want to perform a kind of
          // subtraction: other = other - this, except this might mean that
          // there is more than one new 'other' to deal with. however, all of the
          // new 'other's will be strictly disjunctive, so we won't need to start again.
          unsigned new_pathy = bsc.pathy & ~other.pathy;
          assert (new_pathy != 0);
          unsigned cumul_truth = other.truth;
          unsigned cumul_pathy = other.pathy;
          bool first = true;
          while (new_pathy) {
            // first set bit of new_pathy
            const unsigned np_bit = new_pathy & -new_pathy;
            new_pathy ^= np_bit;
            const unsigned new_truth = cumul_truth | (np_bit & ~bsc.truth);
            const unsigned new_pathy = cumul_pathy | np_bit;
            if (first) {
              first = false;
              other.truth = new_truth;
              other.pathy = new_pathy;
            } else {
              others.emplace_front(new_truth, new_pathy);
            }
            cumul_truth |= (np_bit & bsc.truth);
            cumul_pathy |= np_bit;
          }
          other_changed = true;
        }
      }
    }
    if (bsc_changed || other_changed) note_simplifiable_change();
  }

  /* a version that assumes that the input conditions do not collide.
     this is used for minimisation. */
  void disj_with_single_disjoint_inputs(BSC & bsc, BSC & other, bool & bsc_changed, bool & other_changed) {
    other_changed = false;
    bsc_changed = false;

    constexpr bool debug = true;

    const unsigned common_pathy = bsc.pathy & other.pathy;
    const unsigned truth_diff = bsc.truth ^ other.truth;
    const unsigned common_truth_diff = truth_diff & common_pathy;

    if (common_truth_diff == 0) {
      cout << "this shouldn't happen" << endl;
      cout << bsc.s() << endl;
      cout << other.s() << endl;
      cout << "common_pathy " << binary_string(common_pathy, 5) << endl;
      cout << "truth_diff " << binary_string(truth_diff, 5) << endl;
      cout << "common_truth_diuff " << binary_string(common_truth_diff, 5) << endl;
      assert(false);
      assert (common_truth_diff != 0);
    }

    if (!has_single_bit(common_truth_diff)) return;

    // there is exactly one AP that we disagree on - we can do some
    // clever stuff here.
    if (bsc.pathy == other.pathy) {
      // eg: ABC | AB^C = AB
      bsc.pathy ^= common_truth_diff;
      bsc.truth &= bsc.pathy;
      bsc_changed = true;
      other.set_false();
      other_changed = true;
    } else {
      // eg: ABC | B^CD
      // no simplification to be made here
    }

    if (bsc_changed || other_changed) note_simplifiable_change();
  }

public:
  /* not currently used */
  void disj_with(BSC cond) {
    const constexpr bool debug = false;
    dout << "performing full disj. current: " << s() << endl;
    dout << " against " << cond.s() << endl;
    if (cond.is_false()) return;
    // others_top = conditions that need to be compared with all conditions.
    std::stack<BSC> others_top;
    others_top.push(cond);
    bool bsc_changed = false;
    bool other_changed = false;

    growth_since_taut_check = true;

    while (!others_top.empty()) {
      // others = conditions that need to be compared with following conditions.
      std::list<BSC> others = {others_top.top()}; others_top.pop();

      for (auto it = options.begin(); it != options.end(); ++it) {
        BSC & bsc = *it;
        disj_with_single(bsc, others, bsc_changed, other_changed);
        if (bsc_changed) {
          if (bsc.is_true()) return set_true();
          others_top.push(bsc);
          options.erase(it);
        }
        if (other_changed && others.empty()) break;
      }
      if (!others.empty()) {
        // there's at least one leftover - add the first
        options.push_back(others.front());
        others.pop_front();
        while (!others.empty()) {
          // there are even more leftovers - add them to 'others_top' so
          // they will be compared to everything again (this is overkill - they
          // really only need to be compared to the ones that are getting added
          // at the end.
          others_top.push(others.front());
          others.pop_front();
        }
      }
      // if (!others.empty()) {
      //   // options.insert(options.end(), others.begin(), others.end());
      // }
    }
  }

public:
  // perform a proper disjunction with 'other'.
  // return TRUE iff this eBSC was modified by the disjunction,
  // FALSE if this eBSC subsumed 'other'.
  void disj_with(const eBSC & other) {
    if (is_false())
      return become_copy_of(other);

    if(other.is_false())
      return;

    bool changed = false;
    for (const BSC & bsc : other.options)
      disj_with(bsc);
  }

  bdd to_bdd(const ap_info & apinfo) const {
    bdd out = bdd_false();
    for (const auto & bsc : options)
      out |= bsc.to_bdd(apinfo);
    return out;
  }

  void operator|=(const BSC & other) {
    disj_with(other);
  }

public:
  string s(const ap_info & apinfo) const {
    if (is_false()) return "FALSE";
    stringstream out;
    bool first = true;
    for (const auto & bsc : options) {
      if (!first) out << " || ";
      out << bsc.s(apinfo);
      first = false;
    }
    return out.str();
  }

  string s() const {
    if (is_false()) return "FALSE";
    stringstream out;
    bool first = true;
    for (const auto & bsc : options) {
      if (!first) out << " || ";
      out << bsc.s();
      first = false;
    }
    return out.str();
  }

private:
  bool is_simple_true() const {
    return options.front().is_true();
  }

public:

  bool check_taut() {
    if (is_simple_true()) {
      growth_since_taut_check = false;
      return true;
    }
    if (growth_since_taut_check) {
      if (full_check_tautology()) {
        return true;
      } else {
        // minimise();
        return false;
      }
    }
    return false;
  }

  inline bool is_false() const {
    return options.empty();
  }

  // conj with basic BSC
  void conj_with(const BSC & other) {
    if (is_false()) return;
    // is_true is efficiently captured by the normal semantics.
    if (other.pathy == 0) {
      if (other.truth == 1) set_false();
      return;
    }

    // since the condition can now 'grow'.
    invalidate_inverse_cache();

    for (auto it = options.begin(); it != options.end(); ++it) {
      it->conj_with(other);
      if (it->is_false()) options.erase(it);
    }
  }

  void conj_with(const eBSC & other) {
    if (is_false())       return;
    if (is_simple_true())        return become_copy_of(other);
    if (other.is_false()) return set_false();
    if (other.is_simple_true())  return;

    // since the condition can now 'grow'.
    invalidate_inverse_cache();

    std::list<BSC> orig_options;
    orig_options.swap(options);

    for (const auto & my_option : orig_options) {
      for (const auto & their_option : other.options) {
        auto conj_option = my_option & their_option;
        if (conj_option.not_false())
          disj_with(my_option & their_option);
      }
    }
  }

  bool full_check_tautology() {
    growth_since_taut_check = false;
    constexpr const bool debug = false;

    assert(!is_simple_true());

    if constexpr (debug) {
      dout << "checking taut on " << s() << endl;
      dout << "inv:" << endl;
      for (const auto & inv : inverse) {
        dout << " - " << inv.s() << endl;
      }
    }

    std::stack<std::tuple<std::list<BSC>::iterator, std::list<BSC>::const_iterator>> check_stack;

    auto opt_begin = options.cbegin();
    auto opt_end   = options.cend();

    for (auto it = inverse.begin(); it != inverse.end(); ++it)
      check_stack.emplace(it, opt_begin);

    while (!check_stack.empty()) {
      auto [inv_it, opt_it] = check_stack.top();
      check_stack.pop();
      for (; opt_it != opt_end; ++opt_it) {
        const unsigned common_pathy = opt_it->pathy & inv_it->pathy;

        // if there is no intersection
        if ((opt_it->truth & common_pathy) != (inv_it->truth & common_pathy)) {
          dout << "a" << endl;
          // eg: ABC - A^BC
          // check the next option
          continue;
        }

        // if they are identical -- we can delete this inverse
        if (opt_it->pathy == inv_it->pathy) {
          // eg: (inv)ABC - (opt)ABC
          inverse.erase(inv_it);
          break;
        }

        // reminder: the condition with the common pathy is MORE GENERAL

        // the option is more general - we can delete this inverse
        if (opt_it->pathy == common_pathy) {
          dout << "b" << endl;
          // eg: (inv)ABC - (opt)AB
          inverse.erase(inv_it);
          break;
        }

        // if the inverse totally covers the option
        // .. or if they don't cover eachother
        // eg: (inv)AB - (opt)ABC
        // eg: (inv)AB - (opt)BC

        dout << "c" << endl;

        unsigned new_pathy = opt_it->pathy & ~inv_it->pathy;

        assert (new_pathy != 0);

        unsigned cumul_truth = inv_it->truth;
        unsigned cumul_pathy = inv_it->pathy;
        bool first = true;
        while (new_pathy) {
          dout << "x" << endl;
          // first set bit of new_pathy
          const unsigned np_bit = new_pathy & -new_pathy;
          new_pathy ^= np_bit;
          unsigned new_truth = cumul_truth | (np_bit & (~opt_it->truth));
          unsigned new_pathy = cumul_pathy | np_bit;

          if (first) {
            first = false;
            // first one - just modify the existing one
            inv_it->truth = new_truth;
            inv_it->pathy = new_pathy;
            // cout << "updated entry to " << inv_it->s() << endl;
          } else {
            // others - add them to the stack and store their iterators
            inverse.emplace_front(new_truth, new_pathy);
            check_stack.emplace(inverse.begin(), opt_it);
            // cout << "added entry " << inverse.front().s() << endl;
          }

          cumul_pathy |= np_bit;
          cumul_truth |= (np_bit & opt_it->truth);
          // cout << "cumul " << BSC(cumul_truth, cumul_pathy).s() << endl;
        }
      }
      if (opt_it == opt_end)  {
        // we got to the end with some counterexample.
        // this is not a tautology.
        dout << "taut = false" << endl;
        dout << "inv:" << endl;
        if constexpr (debug) {
          for (const auto & inv : inverse) {
            dout << " - " << inv.s() << endl;
          }
        }
        return false;
      }
    }

    // we never found a counterexample; this must be tautologous.
    set_true();
    dout << "taut = true" << endl;
    return true;
  }
};

ostream & operator << (ostream &out, const BSC & bsc) {
  bsc.print_to_ss(out);
  return out;
}

ostream & operator << (ostream &out, const eBSC & ebsc) {
  out << ebsc.s();
  // ebsc.print_to_ss(out);
  return out;
}

eBSC operator|(eBSC bsc1, const eBSC & bsc2) {
  bsc1.disj_with(bsc2);
  return bsc1;
}

eBSC operator|(const BSC & bsc1, const BSC & bsc2) {
  eBSC out = eBSC::from_BSC(bsc1);
  out.disj_with(bsc2);
  return out;
}

eBSC operator&(eBSC bsc1, const eBSC & bsc2) {
  bsc1.conj_with(bsc2);
  return bsc1;
}
eBSC random_eBSC(const unsigned & max_depth = 4, const unsigned & num_bits = 4, const unsigned & perc_false = 10) {
  eBSC out = eBSC::False();
  unsigned depth = 1 + random_under(max_depth);
  while (depth-- > 0) {
    out |= random_BSC(num_bits, perc_false);
  }
  return out;
}

eBSC vrandom_eBSC(const unsigned & max_depth = 4, const unsigned & num_bits = 4, const unsigned & perc_false = 10) {
  eBSC out = eBSC::False();
  unsigned depth = 1 + random_under(max_depth);
  constexpr const bool debug = false;
  while (depth-- > 0) {
    auto bsc = random_BSC(num_bits, perc_false);
    dout << "  adding bsc: " << bsc.s() << endl;
    out |= bsc;
    dout << "    is now: " << out.s() << endl;
  }
  return out;
}