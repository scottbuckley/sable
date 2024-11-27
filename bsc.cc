#pragma once
#include "common.hh"
#include "time.cc"
#include "safety_games.cc"
#include <spot/twa/formula2bdd.hh>
#include <spot/misc/minato.hh>

#include <bit>
#include <bitset>
#include <cmath>
#include <iostream>

#define dout if constexpr (debug) cout
#define ddout if (debug) cout

#define IF_BSC_TRUE_OR_FALSE(t, f) if (pathy == 0) {if (truth == 0) {t;} else {f;}}
#define IF_BSC_REF_TRUE_OR_FALSE(b, t, f) if (b.pathy == 0) {if (b.truth == 0) {t;} else {f;}}


// bitset condition
class BSC {
  friend class eBSC;

private:
  size_t truth = 0; // should each AP be true (if we care)
  size_t pathy = 0; // do we care about each AP?

  inline void set_false() {
    pathy = 0;
    truth = 1;
    auto x = bdd_false;
  }

  inline void set_true() {
    pathy = 0;
    truth = 0;
  }

public:
  BSC() : truth{0}, pathy{0} {}

  BSC(const size_t & truth, const size_t & pathy)
  : truth{truth}, pathy{pathy} {}

  // now I need to handle disjunctions, but it means I might end up being two BSCs.
  BSC(const bdd & bdd_cond, const ap_info & apinfo) {
    bdd b = bdd_cond;
    while (b != bddtrue) {
      // this is the bdd var index at the top of this bdd tree
      unsigned const var = bdd_var(b);
      unsigned const var_idx = apinfo.var_to_ind[var];
      unsigned const var_mask = 1 << var_idx;

      pathy |= var_mask; // mark that we care about this bit

      // one branch of this bdd must be false - otherwise, it's not a conjunction. think about it.
      bdd high = bdd_high(b);
      if (high == bddfalse) {
        // if this var is high we fail, meaning this bit should be set to 0 (which it defaulted to)
        // the next part of this conjunction must be in the other branch
        b = bdd_low(b);
      } else {
        // if this var is high we don't fail, meaning this bit should be set to 1.
        truth |= var_mask;
        // if this is the case, it must be that the other branch is false, or else this isn't a conjunction.
        assert(bdd_low(b) == bddfalse);
        // the next part of this conjunction must be in this branch.
        b = high;
      } 
    }
  }

  inline unsigned get_pathy() const { return pathy; }
  inline unsigned get_truth() const { return truth; }

  static std::vector<BSC> from_any_bdd(const bdd & bdd_cond, const ap_info & apinfo, BSC bsc = BSC()) {
    constexpr bool debug = false;
    bdd b = bdd_cond;
      dout << "looking at new BDD" << endl;
      dout << bdd_cond << endl;
    while (b != bddtrue) {
      // this is the bdd var index at the top of this bdd tree
      unsigned const var = bdd_var(b);
      unsigned const var_idx = apinfo.var_to_ind[var];
      unsigned const var_mask = 1 << var_idx;

      // mark that we care about this bit
      bsc.pathy |= var_mask;

      // one branch of this bdd must be false - otherwise, it's not a conjunction. think about it.
      bdd high = bdd_high(b);
      bdd low  = bdd_low(b);

      if constexpr (debug) {
        cout << "var " << apinfo.ind_to_string[var_idx] << endl;
        if (high == bddfalse) cout << "high false" << endl;
        else if (high == bddtrue) cout << "high true" << endl;
        else cout << "high complex" << endl;

        if (low == bddfalse) cout << "low false" << endl;
        else if (low == bddtrue) cout << "low true" << endl;
        else cout << "low complex" << endl;
      }

      if (high == bddfalse) {
        // if this is high we fail, meaning this bit should be set to 0 (which we already recorded)
        // the next part of this conjunction must be in the other branch.
        b = low;
      } else if (low == bddfalse) {
        // if this is low we fail, meaning this bit should be set to 1.
        // the next part of this conjunction must be in the other branch.
        bsc.truth |= var_mask;
        b = high;
      } else {
        // neither are false.
        // here i want to say that neither of the children can be bddtrue, but i'm not sure.
        // i'll assert it for now and figure out what's wrong if the assertion is wrong.
        assert(high != bddtrue); assert (low != bddtrue);
        // the rest of everything should be handled in two passes i think
        auto false_branch = from_any_bdd(low, apinfo, bsc);
        bsc.truth |= var_mask;
        auto true_branch = from_any_bdd(high, apinfo, bsc);
        
        true_branch.reserve(true_branch.size()+false_branch.size());
        true_branch.insert(true_branch.end(), false_branch.begin(), false_branch.end());
        return true_branch;
      }
    }
    return {bsc};
  }

  inline bool is_false() const {
    return (pathy == 0 && truth == 1);
  }

  inline bool not_false() const {
    return (pathy != 0 || truth != 1);
  }

  inline bool is_true() const {
    return (pathy == 0 && truth == 0);
  }

  // whether or not a letter is accepted by this BSG.
  bool accepts_letter(const unsigned & letter) const {
    // "letter & pathy" masks away bits in letter that this BSC doesn't care about,
    // and the result is compared to `truth`, as we assume always that
    // truth & pathy == truth for a valid BSC.
    if (pathy == 0) return (truth != 1);
    return ((letter & pathy) == truth);
  }

  unsigned example_accepting_letter() const {
    assert (not_false());
    return truth;
  }

  void lock_values_within_mask(const unsigned & mask) {
    pathy |= mask;
  }

  bool is_fully_defined(const unsigned & mask) const {
    return ((pathy & mask) == mask);
  }

  void check_validity() const {
    if (pathy == 0) {
      if (truth == 0) return;
      if (truth == 1) return;
      throw runtime_error("this bdd is invalid! :(");
    }
    if ((truth & (~pathy)) != 0) {
      throw runtime_error("this bdd is invalid! :(");
    }
  }

  // bool accepts_letter(const unsigned & letter) const {
  //   THIS_CANT_BE_FALSE;
  //   return ((letter & pathy) == truth);
  // }

  // whether or not a condition has some intersection with another condition.
  bool intersects(const BSC & other) const {

    IF_BSC_TRUE_OR_FALSE(return other.not_false(), return false)

    // if the two conditions don't care about any of the same bits, then
    // this is trivially true. if they do, we only need to check that the bits
    // that both conditions care about are the same.
    const unsigned common_pathy = pathy & other.pathy;
    if (common_pathy == 0) return true;
    return ((truth & common_pathy) == (other.truth & common_pathy));
  }

  bool intersects_masked(const BSC & other, const unsigned & mask) const {
    const unsigned masked_common_pathy = pathy & other.pathy & mask;
    if (masked_common_pathy == 0) return true;
    return ((truth & masked_common_pathy) == (other.truth & masked_common_pathy));
  }

  static BSC intersection(const BSC & bsc, const BSC & other) {
    // if 'bsc' is true, return other. if 'bsc' is false, return false. similar for 'other'
    IF_BSC_REF_TRUE_OR_FALSE(bsc,   return other, return bsc)
    IF_BSC_REF_TRUE_OR_FALSE(other, return bsc,   return other)

    const unsigned common_pathy = bsc.pathy & other.pathy;
    if (common_pathy != 0 && ((bsc.truth & common_pathy) != (other.truth & common_pathy))) {
      // we contradict.
      return BSC::False();
    } else {
      // we don't contradict.
      return BSC(bsc.truth | other.truth, bsc.pathy | other.pathy);
    }
  }
  

  static BSC masked_intersection(const BSC & bsc, const BSC & other, const unsigned & mask) {
    // if 'bsc' is true, return other. if 'bsc' is false, return false. similar for 'other'
    IF_BSC_REF_TRUE_OR_FALSE(bsc, return other.masked(mask), return bsc)
    IF_BSC_REF_TRUE_OR_FALSE(other, return bsc.masked(mask), return other)

    const unsigned common_pathy = bsc.pathy & other.pathy & mask;
    if (common_pathy != 0 && ((bsc.truth & common_pathy) != (other.truth & common_pathy))) {
      // we contradict.
      return BSC::False();
    } else {
      // we don't contradict.
      return BSC(mask & (bsc.truth | other.truth), mask & (bsc.pathy | other.pathy));
    }
  }

  bool encapsulates(const BSC & other) {
    // do I encapsulate all configurations in 'other'?
    throw std::runtime_error("todo: implement me");
    return false;
  }

  // // whether or not a condition has some intersection with another condition.
  // bool intersects_(const BSC & other) const {
  //   // we consider the false case first, saying that false does not intersect
  //   // with anything except false, then we defer to the normal method.
  //   if (is_false()) return other.is_false();
  //   if (other.is_false()) return false;
  //   return intersects(other);
  // }

  // a totally apathetic BSC is the 'true' condition.
  inline static BSC True() noexcept {
    return BSC(0, 0);
  }

  // we can't naturally encode 'false' in a BSC, so we have the special case
  // of total apathy but the first bit set to true to indicate 'false'. this
  // is a special case that is NOT accounted for in most cases, so if you are
  // using a BSC that might be false, you will want to use the _ version of
  // any operations you use.
  inline static BSC False() noexcept {
    return BSC(1, 0);
  }

  void conj_with(const BSC & other) {
    IF_BSC_TRUE_OR_FALSE({
      // i'm true, so become a copy of the other
      pathy = other.pathy;
      truth = other.truth;
      return;
    }, return)

    IF_BSC_REF_TRUE_OR_FALSE(other, return, return set_false())

    const unsigned common_pathy = pathy & other.pathy;
    if (common_pathy != 0 && ((truth & common_pathy) != (other.truth & common_pathy))) {
      // we contradict.
      set_false();
    } else {
      // we don't contradict.
      pathy |= other.pathy;
      truth |= other.truth;
    }
  }

/*
  BSC conj(const BSC & other) const {
    const unsigned common_pathy = pathy & other.pathy;
    if (common_pathy != 0) {
     if ((truth & common_pathy) != (other.truth & common_pathy)) return BSC::False();
    } else {
      if (is_false() || other.is_false()) return BSC::False();
    }

    // we don't contradict.
    return BSC(truth | other.truth, pathy | other.pathy);
  }
*/

  BSC operator&(const BSC & other) const {
    BSC out = BSC(truth, pathy);
    out.conj_with(other);
    return out;
  }

  // void conj_with_(const BSC & other) {
  //   if (is_false() || other.is_false())
  //     set_false();
  //   conj_with(other);
  // }

  inline void operator&=(const BSC & other) {
    conj_with(other);
  }

  inline bool equals(const BSC & other) const {
    return (pathy == other.pathy && truth == other.truth);
  }

  inline bool operator==(const BSC & other) const {
    return (pathy == other.pathy && truth == other.truth);
  }

  inline bool operator!=(const BSC & other) const {
    return (truth != other.truth || pathy != other.pathy);
  }

  // checks whether the other BSC is included within this one.
  // that is to say any letter accepted by 'other' will also be
  // accepted by this.
  bool includes(const BSC & other) {
    const unsigned common_pathy = pathy & other.pathy;
    if ((truth & common_pathy) != (other.truth & common_pathy)) {
      // we don't intersect; we couldn't possibly include the other
      return false;
    } else if (pathy == common_pathy) {
      // the other one is more specific, so we include it.
      return true;
    }
    return false;
  }

  // mask in-place
  void mask(const unsigned & mask_bits) {
    pathy &= mask_bits;
    truth &= mask_bits;
  }

  // make a copy of this BSC, but masked
  BSC masked(const unsigned & mask_bits) const {
    return BSC(truth & mask_bits, pathy & mask_bits);
  }

  bdd to_bdd(const ap_info & apinfo) const {
    if (pathy == 0) {
      if (truth == 0) return bdd_true();
      return bdd_false();
    }
    bdd output = bdd_true();
    unsigned pathy_copy = pathy;
    unsigned truth_copy = truth;
    for (const auto & var : apinfo.ind_to_var) {
      if (pathy_copy & 1) {
        // we care about this bit
        if (truth_copy & 1) {
          output &= bdd_ithvar(var);
        } else {
          output &= bdd_nithvar(var);
        }
      }
      pathy_copy >>= 1;
      truth_copy >>= 1;
    }
    return output;
  }

  string s(const ap_info & apinfo) const {
    stringstream out;
    auto x = apinfo.ind_to_string.cbegin();
    print_to_ss(out, x);
    return out.str();
  }

  string s() const {
    stringstream out;
    print_to_ss(out);
    return out.str();
  }

  void print_to_ss(ostream & out) const {
    auto x = index_transform_iter<string, index_to_label>().begin();
    print_to_ss(out, x);
  }

  template<class iterator_type>
  void print_to_ss(ostream & out, iterator_type it) const {
    unsigned truth_copy = truth;
    unsigned pathy_copy = pathy;

    if (pathy == 0) {
      if (truth == 0)
        out << "TRUE";
      else if (truth == 1)
        out << "FALSE";
      else
        out << "BROKEN_CONDITION";
    }

    unsigned idx = 0;
    while (truth_copy != 0 || pathy_copy != 0) {
      string ap_string = it.operator*();
      if (pathy_copy & 0b1) {
        if (truth_copy & 0b1)
          out << ap_string;
        else
          out << '^' << ap_string;
      }
      truth_copy >>= 1;
      pathy_copy >>= 1;
      ++idx;
      ++it;
    }
  }

  std::tuple<BSC, BSC> split_at_first_var() const {
    assert(pathy != 0);
    const unsigned np_bit = pathy & -pathy;
    return make_tuple(BSC(truth & np_bit, np_bit), BSC(truth & ~np_bit, pathy ^ np_bit));
  }

};

inline unsigned random_under(const unsigned & max_excl) {
   return std::rand() % max_excl;
}

inline bool random_chance(const unsigned & perc_chance) {
  return (std::rand() % 100) < perc_chance;
}

BSC random_BSC(const unsigned & num_bits = 4, const unsigned & perc_false = 10) {
  if (random_chance(perc_false)) return BSC::False();
  unsigned max_excl = 1 << num_bits;
  unsigned pathy = random_under(max_excl);
  unsigned truth = random_under(max_excl) & pathy;
  return BSC(truth, pathy);
}