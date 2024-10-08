#pragma once
#include "bsc.cc"
#include "ebsc.cc"

class BSDD {

  const unsigned false_idx = 0;
  const unsigned true_idx = 1;

  struct BSDD_Node {
    unsigned var_mask;
    unsigned low_idx;
    unsigned high_idx;

    BSDD_Node(const unsigned & var_mask, const unsigned & low_idx, const unsigned & high_idx)
    : var_mask{var_mask}, low_idx{low_idx}, high_idx{high_idx} {}

    BSDD_Node()
    : var_mask{0}, low_idx{0}, high_idx{0} {}

    inline BSC var_BSC_true() const {
      return BSC(var_mask, var_mask);
    }

    inline BSC var_BSC_false() const {
      return BSC(0, var_mask);
    }
  };
  
  struct BDD_Memory {
    std::vector<BSDD_Node> nodes = std::vector<BSDD_Node>(2);
  };

private:
  shared_ptr<BDD_Memory> memory;

  eBSC idx_to_eBSC(const unsigned & idx) const {
    if (idx == true_idx) {
      return eBSC::True();
    } else if (idx == false_idx) {
      return eBSC::False();
    }
    const BSDD_Node & node = memory->nodes[idx];

    eBSC low_eBSC = idx_to_eBSC(node.low_idx);
    low_eBSC.conj_with(node.var_BSC_false());

    eBSC high_eBSC = idx_to_eBSC(node.high_idx);
    high_eBSC.conj_with(node.var_BSC_true());

    return low_eBSC | high_eBSC;

    // if (node.low_idx == false_idx) {
    //   if (node.high_idx == true_idx) {
    //     return eBSC::from_BSC(node.var_BSC_true());
    //   } else {
    //     eBSC rest = idx_to_eBSC(node.high_idx);
    //     rest.conj_with(node.var_BSC_true());
    //     return rest;
    //   }
    // } else if (node.high_idx == false_idx) {
    //   if (node.low_idx == true_idx) {
    //     return eBSC::from_BSC(node.var_BSC_false());
    //   } else {
    //     eBSC rest = idx_to_eBSC(node.low_idx);
    //     rest.conj_with(node.var_BSC_false());
    //     return rest;
    //   }
    // } else if (node.low_idx == true_idx) {
    //   // low is true. high isn't true or false.
    //   eBSC rest = idx_to_eBSC(node.high_idx);
    //   rest.conj_with(node.var_BSC_true());
    //   rest.disj_with(node.var_BSC_false());
    //   return rest;
    // } else if (node.high_idx == true_idx) {
    //   // high is true. low isn't true or false
    //   eBSC rest = idx_to_eBSC(node.low_idx);
    //   rest.conj_with(node.var_BSC_false());
    //   rest.disj_with(node.var_BSC_true());
    //   return rest;
    // }
  }

  // string node_to_string(const BSDD_Node & node) const {
  //   const string A = "" + std::to_string(node.var_mask);
  //   const string not_A = "!" + std::to_string(node.var_mask);
    
  //   if (node.low_idx >= 2 && node.high_idx >= 2) {
  //     // complex
  //   } else if (node.low_idx == true_idx) {
  //     return A + " & " + node_to_string(memory->nodes[node.high_idx]);
  //   } else if (node.low_idx == false_idx) {
  //     return not_A + " | ( " + A + " & " + node_to_string(memory->nodes[node.high_idx]) + ")";
  //   } else if (node.high_idx == true_idx) {
  //     return A + " | ( " + not_A + " & " + node_to_string(memory->nodes[node.high_idx]) + ")";
  //   } else if (node.high_idx == false_idx) {
  //     return not_A + " & " + node_to_string(memory->nodes[node.high_idx]);
  //   }
  //   return "meow";
  // }

  // string to_string(const unsigned & idx) const {
  //   // cout << "converting to string ... " << endl;
  //   // cout << idx << endl;
  //   // if (idx == true_idx) return "T";
  //   // if (idx == false_idx) return "F";
  //   // cout << "printing nontrivial bsdd ... " << endl;
  //   // return node_to_string(memory->nodes[idx]);
  // }

  void dump_debug(const unsigned & idx) {
    if (idx == true_idx) {
      // cout << "     TRUE" << endl << endl;
      return;
    } else if (idx == false_idx) {
      // cout << "     FALSE" << endl << endl;
      return;
    }
    cout << "idx: " << idx << endl;
    const BSDD_Node & node = memory->nodes[idx];
    cout << "var: " << node.var_BSC_true().s() << endl;
    if (node.low_idx == false_idx)
      cout << "low: FALSE" << endl;
    else if (node.low_idx == true_idx)
      cout << "low: TRUE" << endl;
    else
      cout << "low: " << node.low_idx << endl;

    if (node.high_idx == false_idx)
      cout << "hi: FALSE" << endl;
    else if (node.high_idx == true_idx)
      cout << "hi: TRUE" << endl;
    else
      cout << "hi:  " << node.high_idx << endl;
    cout << endl;
    dump_debug(node.low_idx);
    dump_debug(node.high_idx);
  }

public:

  void dump_debug() {
    dump_debug(root_idx);
  }

  string to_string() const {
    auto e = idx_to_eBSC(root_idx);
    return e.s();
  }

  eBSC to_eBSC() const {
    return idx_to_eBSC(root_idx);
  }

  // std::vector<BSDD_Node> nodes = std::vector<BSDD_Node>(2);
  
  unsigned root_idx = true_idx;

  // use the provided BDD Memory
  BSDD(const bool & truth, const shared_ptr<BDD_Memory> & mem) {
    memory = mem;
    root_idx = (truth ? true_idx : false_idx);
  }

  // use the BDD memory from another BDD
  BSDD(const bool & truth, const BSDD & mem_bdd) {
    memory = mem_bdd.memory;
    root_idx = (truth ? true_idx : false_idx);
  }

  // make a fresh BDD memory
  BSDD(const bool & truth) {
    memory = make_shared<BDD_Memory>();
    root_idx = (truth ? true_idx : false_idx);
  }

  static BSDD from_eBSC(const eBSC & e) {
    auto out = BSDD(false);
    for (const auto & opt : e.options) {
      out |= opt;
    }
    return out;
  }

  unsigned bdd_from_bsc(const BSC & bsc) const {
    unsigned pathy = bsc.get_pathy();
    const unsigned & truth = bsc.get_truth();
    if (pathy == 0) {
      if (bsc.is_true()) return true_idx;
                         return false_idx;
    }
    std::stack<unsigned> masks;
    while (pathy) {
      const unsigned np_bit = pathy & -pathy;
      masks.emplace(np_bit);
      pathy ^= np_bit;
    }

    unsigned top_idx = true_idx;
    while (!masks.empty()) {
      const unsigned var_mask = masks.top(); masks.pop();
      const unsigned new_idx = memory->nodes.size();
      if (var_mask & truth) // true
        memory->nodes.emplace_back(var_mask, false_idx, top_idx);
      else // false
        memory->nodes.emplace_back(var_mask, top_idx, false_idx);
      top_idx = new_idx;
    }
    return top_idx;
  }

  unsigned make_node(const unsigned & var_mask, const unsigned & low, const unsigned & high) const {
    if (low == high) return low;
    unsigned idx = 0;
    for (const auto & node : memory->nodes) {
      if (node.var_mask == var_mask && node.low_idx == low && node.high_idx == high)
        return idx;
      ++idx;
    }
    memory->nodes.emplace_back(var_mask, low, high);
    return idx;
  }

  unsigned disj_with_bsc(const unsigned & U_idx, const BSC & bsc, string indent = "") const {
    if (U_idx == true_idx)  return true_idx;
    if (bsc.is_true())      return true_idx;
    if (bsc.is_false())     return U_idx;
    if (U_idx == false_idx) return bdd_from_bsc(bsc);

    // U = the existing BSDD
    // V = the new condition (bsc)

    constexpr const bool debug = false;

    auto [V, bsc_next] = bsc.split_at_first_var();
    unsigned V_var_mask = V.get_pathy();

    BSDD_Node U = memory->nodes[U_idx];

    // dout << indent << U.var_mask << endl;
    // dout << indent << V_var_mask << endl;

    unsigned new_low_idx;
    unsigned new_high_idx;
    unsigned new_var_mask = U.var_mask;

    if (V.get_truth()) {
      // dout << indent << "a" << endl;
      if (U.var_mask == V_var_mask) {
        // dout << indent << "a1" << endl;
        new_low_idx  = U.low_idx;
        new_high_idx = disj_with_bsc(U.high_idx, bsc_next, indent+"  ");
      } else if (U.var_mask < V_var_mask) {
        // dout << indent << "a2" << endl;
        new_low_idx  = disj_with_bsc(U.low_idx, bsc, indent+"  ");
        // dout << indent << "--" << endl;
        new_high_idx = disj_with_bsc(U.high_idx, bsc, indent+"  ");
      } else {
        // dout << indent << "a3" << endl;
        new_low_idx  = U_idx;
        new_high_idx = disj_with_bsc(U_idx, bsc_next, indent+"  ");
        new_var_mask = V_var_mask;
      }
    } else {
      // dout << indent << "b" << endl;
      if (U.var_mask == V_var_mask) {
        // dout << indent << "b1" << endl;
        new_low_idx  = disj_with_bsc(U.low_idx, bsc_next, indent+"  ");
        new_high_idx = U.high_idx;
      } else if (U.var_mask < V_var_mask) {
        // dout << indent << "b2" << endl;
        new_low_idx  = disj_with_bsc(U.low_idx, bsc, indent+"  ");
        // dout << indent << "--" << endl;
        new_high_idx = disj_with_bsc(U.high_idx, bsc, indent+"  ");
      } else {
        // dout << indent << "b3" << endl;
        new_low_idx  = disj_with_bsc(U_idx, bsc_next, indent+"  ");
        new_high_idx = U_idx;
        new_var_mask = V_var_mask;
      }
    }

    const unsigned x = make_node(new_var_mask, new_low_idx, new_high_idx);
    // dout << indent << "*" << x << endl;
    return x;
  }

  void operator |=(const BSC & bsc) {
    root_idx = disj_with_bsc(root_idx, bsc);
  }

  bool is_true() const {
    return root_idx == true_idx;
  }

  bool is_false() const {
    return root_idx == false_idx;
  }

private:
  bool node_accepts_letter(unsigned node_idx, const unsigned & letter) const {
    while (node_idx > 1) {
      if ((memory->nodes[node_idx].var_mask & letter) == 0)
        node_idx = memory->nodes[node_idx].low_idx;
      else
        node_idx = memory->nodes[node_idx].high_idx;
    }
    if (node_idx == true_idx) return true;
    if (node_idx == false_idx) return false;
    assert(false);
  }
public:

  bool accepts_letter(const unsigned & letter) const {
    return node_accepts_letter(root_idx, letter);
  }

  
};