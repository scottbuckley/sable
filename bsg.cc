#pragma once
#include "common.hh"
#include "safety_games.cc"
#include <spot/twa/formula2bdd.hh>
#include <spot/misc/minato.hh>

#include <bit>
#include <bitset>
#include <cmath>
#include <iostream>

#define ASSERT_NOT_FALSE


#ifdef ASSERT_NOT_FALSE
  #define CANT_BE_FALSE(bsc) assert(!bsc.is_false())
  #define THIS_CANT_BE_FALSE assert(!is_false())
#else
  #define CANT_BE_FALSE(bsc) 
  #define THIS_CANT_BE_FALSE 
#endif

// bitset graphs

// bitset condition
class BSC {
  friend class eBSC;

private:
  size_t truth = 0; // should each AP be true (if we care)
  size_t pathy = 0; // do we care about each AP?

  inline void set_false() {
    pathy = 0;
    truth = 1;
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



  static std::vector<BSC> from_any_bdd(const bdd & bdd_cond, const ap_info & apinfo, BSC bsc = BSC()) {
    constexpr bool debug_print = false;
    bdd b = bdd_cond;
    if constexpr (debug_print) {
      cout << "looking at new BDD" << endl;
      cout << bdd_cond << endl;
    }
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

      if constexpr (debug_print) {
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
    THIS_CANT_BE_FALSE; CANT_BE_FALSE(other);
    // if the two conditions don't care about any of the same bits, then
    // this is trivially true. if they do, we only need to check that the bits
    // that both conditions care about are the same.
    const unsigned common_pathy = pathy & other.pathy;
    if (common_pathy == 0) return true;
    return ((truth & common_pathy) == (other.truth & common_pathy));
  }

  static BSC intersection(const BSC & bsc, const BSC & other) {
    CANT_BE_FALSE(bsc); CANT_BE_FALSE(other);
    const unsigned common_pathy = bsc.pathy & other.pathy;
    if (common_pathy != 0 && ((bsc.truth & common_pathy) != (other.truth & common_pathy))) {
      // we contradict.
      return bsc_false();
    } else {
      // we don't contradict.
      return BSC(bsc.truth | other.truth, bsc.pathy | other.pathy);
    }
  }

  static BSC masked_intersection(const BSC & bsc, const BSC & other, const unsigned & mask) {
    CANT_BE_FALSE(bsc); CANT_BE_FALSE(other);
    const unsigned common_pathy = bsc.pathy & other.pathy & mask;
    if (common_pathy != 0 && ((bsc.truth & common_pathy) != (other.truth & common_pathy))) {
      // we contradict.
      return bsc_false();
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

  // whether or not a condition has some intersection with another condition.
  bool intersects_(const BSC & other) const {
    // we consider the false case first, saying that false does not intersect
    // with anything except false, then we defer to the normal method.
    if (is_false()) return other.is_false();
    if (other.is_false()) return false;
    return intersects(other);
  }

  // a totally apathetic BSC is the 'true' condition.
  inline static BSC bsc_true() {
    return BSC(0, 0);
  }

  // we can't naturally encode 'false' in a BSC, so we have the special case
  // of total apathy but the first bit set to true to indicate 'false'. this
  // is a special case that is NOT accounted for in most cases, so if you are
  // using a BSC that might be false, you will want to use the _ version of
  // any operations you use.
  inline static BSC bsc_false() {
    return BSC(1, 0);
  }

  void conj_with(const BSC & other) {
    THIS_CANT_BE_FALSE; CANT_BE_FALSE(other);
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

  BSC conj(const BSC & other) const {
    const unsigned common_pathy = pathy & other.pathy;
    if (common_pathy != 0) {
     if ((truth & common_pathy) != (other.truth & common_pathy)) return bsc_false();
    } else {
      if (is_false() || other.is_false()) return bsc_false();
    }

    // we don't contradict.
    return BSC(truth | other.truth, pathy | other.pathy);
  }

  BSC operator&(const BSC & other) const {
    return conj(other);
  }

  void conj_with_(const BSC & other) {
    if (is_false() || other.is_false())
      set_false();
    conj_with(other);
  }

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

  // remove any configurations from this BSC that are in the other BSC.
  // could result in 'false' if there is nothing left.
  void subtract(const BSC & other, const BSC & intersection) {
    if (equals(intersection))
      return set_false();

    const unsigned new_pathy = other.pathy & ~pathy;
    if (new_pathy == 0) {
      throw std::runtime_error("this shouldn't happen");
    }
    truth |= (~other.truth) & new_pathy;
    pathy |= new_pathy;
  }

  void subtract(const BSC & other) {
    subtract(other, conj(other));
  }


  void single_disj_with(const BSC & other) {
    THIS_CANT_BE_FALSE; CANT_BE_FALSE(other);
    if ((pathy ^ other.pathy) != 0) {
      // there are some bits that each BSC have different
      // pathy for - this means there's not a single BSC that
      // can represent their disjunctions. we just return the
      // special value false in this case.
      set_false();
    } else {
      const unsigned common_pathy = pathy & other.pathy;
      if (common_pathy != 0) {
        // there are bits that both sides care about. if they
        // agree, nothing needs to change, but if they differ
        // we must stop caring about this bit.
        const unsigned new_apathy = (truth ^ other.truth) & common_pathy;
        pathy ^= new_apathy;
        // we need to set truth to zero for any newly apathetic bits
        truth &= pathy;
      } else {
        // this is a weird case - there is no common or uncommon pathy,
        // meaning both are apathetic. in this case we are just true.
        truth = 0;
      }
    }
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

  // Perform a single-step disjunction with the BSC 'other'.
  // Return true iff this BSC was changed as part of the process.
  // The result is something like "this = this - other", where possible.
  // * If TRUE is returned, there is NEVER any left over
  //     condition to deal with.
  // * If FALSE is returned, 'other' is modified in-place
  //     to be the leftover condition (which is the False BSC
  //     if we didn't change but there's still nothing left to 
  //     disj with). Frequently 'other' will be unchanged when
  //     False is returned.
  bool disj_with(BSC & other) {
    // the semantics are clear that returning TRUE means there
    // is no condition left over, but we may or may not want to
    // still set 'other' to BSC_FALSE before returning TRUE.
    constexpr bool set_false_before_return_true = false;
    if (other.is_false()) return false;

    if (pathy == 0) {
      if (truth == 0) {
        // we are true - we subsume whatever you throw at us.
        other.set_false();
        return false;
      } else {
        // we are false - we become whatever you throw at us.
        assert(truth == 1);
        pathy = other.pathy;
        truth = other.truth;
        if (set_false_before_return_true) other.set_false();
        return true;
      }
    }

    // quick check to see if we are identical. - this behaviour is covered elsewhere
    // if (truth == other.truth && pathy == other.pathy) {
    //   // we are identical, so we kinda subsume the other one
    //   other.set_false();
    //   return false;
    // }

    const unsigned common_pathy = pathy & other.pathy;
    if ((truth & common_pathy) != (other.truth & common_pathy)) {
      // the things we both care about are not identical: there is no intersection.

      // special case when there is only one bit of difference.
      unsigned pathy_diff;
      if (pathy == other.pathy && (has_single_bit(pathy_diff = (truth ^ other.truth)))) {
        pathy ^= pathy_diff;
        truth &= ~pathy_diff;
        if (set_false_before_return_true) other.set_false();
        return true;
      }

      // otherwise there's nothing we can do here.
      // we make no change to the input BSC.
      return false;
    } else {
      // the things we both care about are identical - there might be stuff we can do

      if (pathy == common_pathy) {
        // the other one is more specific, so we include it. we don't need to do anything.
        other.set_false();
        return false;
      } else if (other.pathy == common_pathy) {
        // we are more specific, so the other one includes us. we assume the shape of the other one.
        pathy = other.pathy;
        truth = other.truth;
        if (set_false_before_return_true) other.set_false();
        return true;
      } else {
        // we know that the BSCs are not the same, don't include eachother, but do have an intersection.

        // we perform a subtraction, returning (other - this).
        // other gets smaller, we don't change.
        const unsigned new_pathy = pathy & ~other.pathy;
        if (new_pathy == 0) {
          // i dont really remember why this shouldnt happen. seems probably fine to me.
          // i think in this context, since we know there is an intersection but not an inclusion, it must be true?
          throw std::runtime_error("this shouldn't happen");
        }
        if (has_single_bit(new_pathy)) {
          //this is where we can do a simple subtraction
          other.truth |= (~truth & new_pathy);
          other.pathy |= new_pathy;
          return false;
        } else {
          // throw std::runtime_error("what do i do here");
          return false;
        }
      }
    }
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

  // void print_to_generic_ss(ostream & out) const {
  //   unsigned truth_copy = truth;
  //   unsigned pathy_copy = pathy;

  //   if (pathy == 0) {
  //     if (truth == 0)
  //       out << "TRUE";
  //     else if (truth == 1)
  //       out << "FALSE";
  //     else
  //       out << "BROKEN_CONDITION";
  //   }

  //   char yes = 'A';
  //   char no  = 'a';
  //   while (truth_copy != 0 || pathy_copy != 0) {
  //     if (pathy_copy & 0b1) {
  //       if (truth_copy & 0b1)
  //         out << yes;
  //       else
  //         out << '^' << yes;
  //     }
  //     truth_copy >>= 1;
  //     pathy_copy >>= 1;
  //     yes++;
  //   }
  // }

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

};

struct BSG_Edge {
  BSC cond;
  unsigned dest;

  BSG_Edge(const bdd & cond, unsigned dest, const ap_info & apinfo)
  : cond(cond, apinfo), dest{dest} {}

  BSG_Edge(const BSC & cond, unsigned dest)
  : cond{cond}, dest{dest} {}
};

struct BSG_State {
  unsigned first_edge_idx;
  unsigned num_edges;
  BSG_State(const unsigned & first_edge_idx, const unsigned & num_edges) : first_edge_idx{first_edge_idx}, num_edges{num_edges} {}
};

class BSG {
private:
  std::vector<BSG_State> states;
  std::vector<BSG_Edge> edges;
  // std::span<BSG_Edge> edges_span;
public:
  BSG(twa_graph_ptr g, const ap_info & apinfo) {
    const unsigned num_states = g->num_states();
    states.reserve(num_states); // we will need exactly this many nodes
    edges.reserve(g->num_edges()); // we will need at least this many edges (if it gets too much, it means we are splitting edges, which we haven't implemented yet)
    // edges_span = std::span(edges);

    if (g->get_init_state_number() != 0)
      throw std::runtime_error("error: initial state is not zero, we have not accounted for this");
    
    //TODO: add accepting state info (probably)

    for (unsigned s=0; s<num_states; ++s) {
      const unsigned first_edge_idx = edges.size();
      unsigned edge_count = 0;
      for (const auto & e : g->out(s)) {
        for (const auto & bsc : BSC::from_any_bdd(e.cond, apinfo)) {
          edges.emplace_back(bsc, e.dst);
          ++edge_count;
        }
      }
      states.emplace_back(first_edge_idx, edge_count);
    }
  }

  span_iter<BSG_Edge> out(const unsigned & s) const {
    return span_iter<BSG_Edge>(&edges, states[s].first_edge_idx, states[s].num_edges);
  }

  const unsigned num_states() const {
    return states.size();
  }
  
  twa_graph_ptr to_twa_graph(const bdd_dict_ptr & dict, const ap_info & apinfo) {
    auto g = make_twa_graph(dict);
    const unsigned num_states = states.size();
    g->new_states(num_states);
    for (unsigned s=0; s<num_states; ++s) {
      for (const auto & e : out(s)) {
        g->new_edge(s, e.dest, e.cond.to_bdd(apinfo));
      }
    }
    return g;
  }
};



class mutable_BSG {
private:
  unsigned state_count;
  std::vector<std::vector<BSG_Edge>> edges;
  // std::span<BSG_Edge> edges_span;
public:
  mutable_BSG() {
  }

  unsigned add_state() {
    edges.emplace_back();
    return state_count++;
  }
  
  void add_edge(unsigned src, unsigned dest, const BSC & cond) {
    edges[src].emplace_back(cond, dest);
  }

};







/* extended BSGs */

struct eBSC {
  std::list<BSC> options;
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
  }

public:
  ~eBSC() {
    options.clear();
  }

  static eBSC eBSC_false() {
    return eBSC();
  }

  static eBSC eBSC_true() {
    eBSC out = eBSC();
    out.options.push_front(BSC::bsc_true());
    return out;
  }

  static eBSC from_BSC(BSC bsc) {
    if (bsc.not_false()) {
      eBSC out = eBSC();
      out.options.push_front(bsc);
      return out;
    } else {
      return eBSC_false();
    }
  }


  #define LIST_FOREACH_WITH_PREV(list_name, item_name) \
    auto prev = list_name.cbefore_begin();\
    auto it = list_name.cbegin();\
    for (; it!= list_name.cend(); prev=it, ++it) { \
      auto item_name = *it;

  #define LIST_FOREACH_WITH_PREV_REF(list_name, item_name) \
    auto prev = list_name.before_begin();\
    auto it = list_name.begin();\
    for (; it!= list_name.end(); prev=it, ++it) { \
      auto & item_name = *it;

private:
  // void condition_collapse(const forward_list<BSC>::iterator & it) {
  void condition_collapse(BSC & bsc) {
    for (auto it = options.begin(); it != options.end(); ++it) {
      if (it->pathy != bsc.pathy) continue;
      const unsigned truth_diff = it->truth ^ bsc.truth;
      if (has_single_bit(truth_diff)) {
        bsc.pathy ^= truth_diff;
        bsc.truth &= bsc.pathy;
        options.erase(it);
      }
    }
  }

  inline void become_copy_of(const eBSC & other) {
    copy_list_into(other.options, options);
  }

public:
  // bool disj_with(const BSC & other) {
  //   disj_with(new BSC(other));
  // }

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

  // perform a proper disjunction with 'other'.
  // return TRUE iff this eBSC was modified by the disjunction,
  // FALSE if this eBSC subsumed 'other'.
  bool disj_with(BSC other) {
    // cout << "dw " << s() << "," << other.s() << endl;
    if (other.is_false()) return false;
    for (auto it = options.begin(); it != options.end(); ++it) {
      BSC & bsc = *it;
      if (bsc.disj_with(other)) {
        // we are done. Perhaps we will need to do a condition
        // collapse though, after one of the conditions has changed.
        // cout << "  this one changed; cond_collapse" << endl;
        condition_collapse(bsc);
        return true;
      }
      if (other.is_false()) {
        // cout << "  got subsumed" << endl;
        // the condition has been subsumed
        return false;
      }
      // cout << "  no change, no subsumption. next..." << endl;
      // cout << "    " << other.s() << endl;
    }
    if (other.not_false()) {
      // cout << "    appending " << other.s() << endl;
      // there is some leftover
      options.emplace_back(other);
      // cout << "      final: " << s() << endl;
      return true;
    }
    assert(false); // i don't think we should ever get here.
    return false;
  }

  bool disj_with(const eBSC & other) {
    // cout << "doing disj_with" << endl << s() << endl << other.s() << endl << endl;
    if (is_false()) {
      // just copy their options
      become_copy_of(other);
      return true;
    }
    if(other.is_false()) {
      return false;
    }
    bool changed = false;
    for (const BSC & bsc : other.options)
      if (disj_with(bsc)) changed = true;
    // cout << "finished disj and changed=" << changed << endl;
    return changed;
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



  
  // void i_just_changed() {
  //   // bsc.print_to_ss(cout);
  //   // cout << " just changed. checking deeper." << endl;
  //   // subsume a later condition if you can.
  //   while (try_subsume_later_ebsc()) {}
  // }
  // 
  // check if you can now subsume any later conditions.
  // return true if you did.
  // bool try_subsume_later_ebsc() {
  //   eBSC * ebsc_to_check = next;
  //   while (ebsc_to_check != nullptr) {
  //     // i know for sure that all future conditions are strictly disjoint with me.
  //     // the only way we could combine with any of those conditions is with the "off-by-one" condition.
  //     const BSC & other_bsc = ebsc_to_check->bsc;
  //     const unsigned truth_diff = bsc.truth ^ other_bsc.truth;
  //     // if (bsc.pathy == other_bsc.pathy && (std::popcount(pathy_diff) == 1)) {
  //     if (bsc.pathy =truth_diff= other_bsc.pathy && exactly_one_set_bit(truth_diff)) {
  //       bsc.pathy ^= truth_diff;
  //       bsc.truth &= ~truth_diff;
  //       delete_later_ebsc(ebsc_to_check);
  //       return true;
  //     }
  //     ebsc_to_check = ebsc_to_check->next;
  //   }
  //   return false;
  // }

  // void delete_this_disj_option() {
  //   if (next == nullptr) {
  //     set_false();
  //   } else {
  //     eBSC * temp = next;
  //     bsc = temp->bsc;
  //     next = temp->next;
  //     delete temp;
  //   }
  // }

  // void delete_later_ebsc(eBSC * ebsc_to_delete) {
  //   assert (ebsc_to_delete != nullptr);
  //   if (next == ebsc_to_delete) {
  //     next = ebsc_to_delete->next;
  //     delete ebsc_to_delete;
  //   } else {
  //     next->delete_later_ebsc(ebsc_to_delete);
  //   }
  // }

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

  // void print_to_ss(ostream & out) const {
  //   bsc.print_to_generic_ss(out);
  //   if (next != nullptr) {
  //     out << " || ";
  //     next->print_to_ss(out);
  //   }
  // }

  inline bool is_true() const {
    return options.front().is_true();
  }

  inline bool is_false() const {
    return options.empty();
  }

  void set_false() {
    options.clear();
  }

  /*
    A & (C | D)
    = (A & C) | (A & D)
    
    (A | B) & (C | D)
    = (A & C) | (A & D) | (B & C) | (B & D)
    
    = (A & (C | D)) | (B & (C | D))
  */

  // conj with basic BSC
  void conj_with(const BSC & other) {
    if (is_false()) return;
    // is_true is efficiently captured by the normal semantics.
    if (other.pathy == 0) {
      if (other.truth == 1) set_false();
      return;
    }

    for (auto it = options.begin(); it != options.end(); ++it) {
      it->conj_with(other);
      if (it->is_false()) options.erase(it);
    }
  }

  void conj_with(const eBSC & other) {
    if (is_false())       return;
    if (is_true())        return become_copy_of(other);
    if (other.is_false()) return set_false();
    if (other.is_true())  return;

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

inline unsigned random_under(const unsigned & max_excl) {
   return std::rand() % max_excl;
}

inline bool random_chance(const unsigned & perc_chance) {
  return (std::rand() % 100) < perc_chance;
}

BSC random_BSC(const unsigned & num_bits = 4, const unsigned & perc_false = 10) {
  if (random_chance(perc_false)) return BSC::bsc_false();
  unsigned max_excl = 1 << num_bits;
  unsigned pathy = random_under(max_excl);
  unsigned truth = random_under(max_excl) & pathy;
  return BSC(truth, pathy);
}

eBSC random_eBSC(const unsigned & max_depth = 4, const unsigned & num_bits = 4, const unsigned & perc_false = 10) {
  eBSC out = eBSC::eBSC_false();
  unsigned depth = 1 + random_under(max_depth);
  while (depth-- > 0) {
    out |= random_BSC(num_bits, perc_false);
  }
  return out;
}

void ebsc_test() {
  const ap_info apinfo = make_fake_apinfo();
  // std::srand(std::time(nullptr));


  auto eBSC_eq_BDD = [&](const eBSC & ebsc, const bdd & b) {
    if (b == bdd_true())  return ebsc.is_true();
    if (b == bdd_false()) return ebsc.is_false();
    return !!(ebsc.to_bdd(apinfo) == b);
  };

  auto BSC_eq_BDD = [&](const BSC & bsc, const bdd & b) {
    if (b == bdd_true())  return bsc.is_true();
    if (b == bdd_false()) return bsc.is_false();
    return !!(bsc.to_bdd(apinfo) == b);
  };

  auto assert_disj_same = [&](const eBSC & a, const eBSC & b){
    bdd a_ = a.to_bdd(apinfo);
    bdd b_ = b.to_bdd(apinfo);
    eBSC aorb = a | b;
    bdd a_orb_ = a_ | b_;

    if (!eBSC_eq_BDD(aorb, a_orb_)) {
      cout << "a: " << a.s(apinfo) << endl;
      cout << "b: " << b.s(apinfo) << endl;
      cout << "a_: " << apinfo.bdd_to_string(a_) << endl;
      cout << "b_: " << apinfo.bdd_to_string(b_) << endl;
      cout << "a | b: " << (a | b).s(apinfo) << endl;
      // cout << "a | b (as bdd): " << apinfo.bdd_to_string((a | b).to_bdd(apinfo)) << endl;
      cout << "a_ | b_: " << apinfo.bdd_to_string(a_ | b_) << endl;

      cout << " *** FAILED *** " << endl;
      exit(1);
    } else {
      // cout << "test passed" << endl;
    }
  };

  auto assert_conj_same = [&](const eBSC & a, const eBSC & b){
    bdd a_ = a.to_bdd(apinfo);
    bdd b_ = b.to_bdd(apinfo);
    eBSC aorb = a & b;
    bdd a_orb_ = a_ & b_;

    if (!eBSC_eq_BDD(aorb, a_orb_)) {
      cout << "a: " << a.s(apinfo) << endl;
      cout << "b: " << b.s(apinfo) << endl;
      cout << "a_: " << apinfo.bdd_to_string(a_) << endl;
      cout << "b_: " << apinfo.bdd_to_string(b_) << endl;
      cout << "a & b: " << (a & b).s(apinfo) << endl;
      // cout << "a & b (as bdd): " << apinfo.bdd_to_string((a & b).to_bdd(apinfo)) << endl;
      cout << "a_ & b_: " << apinfo.bdd_to_string(a_ & b_) << endl;

      cout << " *** FAILED *** " << endl;
      exit(1);
    } else {
      // cout << "test passed" << endl;
    }
  };

  auto assert_single_conj_same = [&](const BSC & a, const BSC & b){
    a.check_validity();
    b.check_validity();
    bdd a_ = a.to_bdd(apinfo);
    bdd b_ = b.to_bdd(apinfo);
    BSC aandb = a & b;
    bdd a_andb_ = a_ & b_;

    if (!BSC_eq_BDD(aandb, a_andb_)) {
      cout << "a: " << a.s(apinfo) << endl;
      cout << "a.is_false(): " << a.is_false() << endl;
      cout << "b: " << b.s(apinfo) << endl;
      cout << "a_: " << apinfo.bdd_to_string(a_) << endl;
      cout << "b_: " << apinfo.bdd_to_string(b_) << endl;
      cout << "a & b: " << (a & b).s(apinfo) << endl;
      cout << "(a & b).is_false(): " << (a & b).is_false() << endl;
      // cout << "a & b (as bdd): " << apinfo.bdd_to_string((a & b).to_bdd(apinfo)) << endl;
      cout << "a_ & b_: " << apinfo.bdd_to_string(a_ & b_) << endl;

      cout << " *** FAILED *** " << endl;
      exit(1);
    } else {
      // cout << "test passed" << endl;
    }
  };

  auto assert_single_disj_same = [&](const BSC & a, const BSC & b){
    bdd a_ = a.to_bdd(apinfo);
    bdd b_ = b.to_bdd(apinfo);
    eBSC aandb = a | b;
    bdd a_andb_ = a_ | b_;

    /*
      a: ^A^BD
      a.truth: 0001
      a.pathy: 1101

      b: ^B^C
      b.truth: 0000
      b.pathy: 0110
    
    */


    // bdd bii = bdd_xor(a_andb_, aandb_bdd);
    
    // bdd bii = bdd_not(bdd_biimp(a_andb_, aandb.to_bdd(apinfo)));
    // int bii = bdd_implies(aandb.to_bdd(apinfo),a_andb_);
    // int bii = bdd_implies(a_andb_, aandb.to_bdd(apinfo));
    // cout << "bii: " << apinfo.bdd_to_string(bii) << endl;
    // cout << "bii: " << bii << endl;

    bdd aandb_bdd = aandb.to_bdd(apinfo);
    if (!eBSC_eq_BDD(aandb, a_andb_)) {
      cout << "a: " << a.s(apinfo) << endl;
      cout << "b: " << b.s(apinfo) << endl;
      cout << "a_: " << apinfo.bdd_to_string(a_) << endl;
      cout << "b_: " << apinfo.bdd_to_string(b_) << endl;
      cout << "a | b: " << (aandb).s(apinfo) << endl;
      cout << "a | b (as bdd): " << apinfo.bdd_to_string(aandb_bdd) << endl;
      cout << "a_ | b_: " << apinfo.bdd_to_string(a_andb_) << endl;
      cout << "a_ zzz b_: " << apinfo.bdd_to_string(bdd_xor(a_, b_)) << endl;

      cout << " *** FAILED *** " << endl;
      exit(1);
    } else {
      // cout << "test passed" << endl;
    }
  };

  // std::srand(std::time(nullptr));
  // print_vector(apinfo.dict->var_map);

  std::srand(123);
  constexpr const unsigned num_tests = 50000;
  const unsigned progress_interval = 10000000;

  cout << "single conj tests ..." << endl;
  for (unsigned i=1; i<=num_tests; ++i) {
    assert_single_conj_same(random_BSC(), random_BSC());
    if (i % progress_interval == 0) cout << i << endl;
  }
  cout << "single conj tests passed." << endl << endl;

  cout << "single disj tests ..." << endl;
  for (unsigned i=1; i<=num_tests; ++i) {
    assert_single_disj_same(random_BSC(), random_BSC());
    if (i % progress_interval == 0) cout << i << endl;
  }
  cout << "single disj tests passed." << endl << endl;


  cout << "complex conj tests ..." << endl;
  for (unsigned i=1; i<=num_tests; ++i) {
    assert_conj_same(random_eBSC(), random_eBSC());
    if (i % progress_interval == 0) cout << i << endl;
  }
  cout << "complex conj tests passed." << endl << endl;


  cout << "complex disj tests ..." << endl;
  for (unsigned i=1; i<=num_tests; ++i) {
    assert_disj_same(random_eBSC(), random_eBSC());
    if (i % progress_interval == 0) cout << i << endl;
  }
  cout << "complex disj tests passed." << endl << endl;
  
  cout << "passed " << num_tests << " tests." << endl;
  
}