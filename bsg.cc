#pragma once
#include "bsc.cc"
#include "bdd.cc"

// bitset graphs

template <typename T>
struct BSG_Edge_gen {
  BSC cond;
  T dest;

  BSG_Edge_gen(const bdd & cond, T dest, const ap_info & apinfo)
  : cond(cond, apinfo), dest{dest} {}

  BSG_Edge_gen(const BSC & cond, T dest)
  : cond{cond}, dest{dest} {}

  // bool operator!=( const BSG_Edge_gen<T> &other ) const noexcept {
  //   return (dest != other.dest) && (cond != other.cond);
  // }
};
using BSG_Edge = BSG_Edge_gen<unsigned>;

template <typename T>
ostream & operator<< (ostream &out, const BSG_Edge_gen<T> & e) {
  out << "BSG_Edge: cond=" << e.cond << ", dest=" << e.dest;
  return out;
}

struct BSG_State {
  unsigned first_edge_idx;
  unsigned num_edges;
  BSG_State(const unsigned & first_edge_idx, const unsigned & num_edges) : first_edge_idx{first_edge_idx}, num_edges{num_edges} {}
};

class BSG {
private:
  std::vector<BSG_State> states;
  std::vector<BSG_Edge> edges;
  bool has_universal_behaviour = false;
public:
  BSG(const twa_graph_ptr & g, const ap_info & apinfo, const bool & allow_universal_behaviour = false) {
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
        if (g->is_univ_dest(e.dst)) {
          if (!allow_universal_behaviour)
            throw new runtime_error("this graph has universal edges, which was not specified to be allowed!");
          has_universal_behaviour = true;
          for (const auto & dest : g->univ_dests(e.dst)) {
            for (const auto & bsc : BSC::from_any_bdd(e.cond, apinfo)) {
              edges.emplace_back(bsc, dest);
              ++edge_count;
            }
          }
        } else {
          for (const auto & bsc : BSC::from_any_bdd(e.cond, apinfo)) {
            edges.emplace_back(bsc, e.dst);
            ++edge_count;
          }
        }
      }
      states.emplace_back(first_edge_idx, edge_count);
    }
  }

  span_iter<BSG_Edge> out(const unsigned & s) const {
    return span_iter<BSG_Edge>(&edges, states[s].first_edge_idx, states[s].num_edges);
  }

  span_filter_iter<BSG_Edge> out_matching(const unsigned & s, const BSC & cond) const {
    return span_filter_iter<BSG_Edge>(&edges, states[s].first_edge_idx, states[s].num_edges, [&](const BSG_Edge & e){return cond.intersects(e.cond);});
  }

  span_filter_iter<BSG_Edge_gen<unsigned>> out_matching_letter(const unsigned & s, const unsigned & letter) const { // e.cond.accepts_letter(letter)
    return span_filter_iter<BSG_Edge_gen<unsigned>>(&edges, states[s].first_edge_idx, states[s].num_edges, [&](const BSG_Edge_gen<unsigned> & e){return (static_cast<BSC>(e.cond)).accepts_letter(letter);});
  }

  void dump_all_states() const {
    const unsigned num_states = states.size();
    cout << "Dumping data on all " << num_states << " states:" << endl;
    for (unsigned i=0; i<num_states; ++i) {
      dump_state(i);
    }
  }

  void dump_state(const unsigned & s) const {
    cout << "State " << s << " Edges:" << endl;
    for (const auto & e : out(s))
      cout << " - " << e << endl;
  }

  void dump_state_matching(const unsigned & s, const BSC & cond) const {
    cout << "State " << s << " Edges matching cond " << cond.s() << ":" << endl;
    for (const auto & e : out_matching(s, cond))
      cout << " - " << e << endl;
  }

  const unsigned num_states() const {
    return states.size();
  }

  // this is an expensive operation, it should be avoided, only used
  // for debug purposes.
  bool check_totally_unmerged(const unsigned & mask) const {
    // we will check all edges on all states to make sure all conditions are fully defined.
    for (const auto & e : edges) {
      if (!e.cond.is_fully_defined(mask)) return false;
    }
    return true;
  }

  bool check_totally_unmerged(const unsigned & mask, unsigned except_state) const {
    // we will check all edges on all states to make sure all conditions are fully defined.
    for (unsigned s=0; s<num_states(); ++s) {
      if (s == except_state) continue;
      for (const auto & e : out(s))
        if (!e.cond.is_fully_defined(mask)) return false;
    }
    return true;
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

template <typename T>
class mutable_BSG_gen {
private:
  unsigned state_count = 0;
  std::vector<std::list<BSG_Edge_gen<T>>> edges;
public:
  mutable_BSG_gen() {}

  unsigned add_state() {
    edges.push_back({});
    return state_count++;
  }

  unsigned num_states() const {
    return state_count;
  }

  void add_states(const unsigned & new_state_count) {
    for (unsigned i=0; i<new_state_count; ++i) {
      edges.push_back({});
    }
    state_count += new_state_count;
  }
  
  void add_edge(const unsigned & src, const T & dest, const BSC & cond) {
    assert (src < state_count);
    edges[src].push_back(BSG_Edge_gen<T>(cond, dest));
  }

  std::list<BSG_Edge_gen<T>> & out(const unsigned & s) {
    return edges[s];
  }

  const std::list<BSG_Edge_gen<T>> & c_out(const unsigned & s) const {
    return edges[s];
  }

  std::vector<BSG_Edge_gen<T>> out_matching(const unsigned & s, const unsigned & letter) const {
    std::vector<BSG_Edge_gen<T>> out;
    for (const auto & e : edges[s]) {
      if (e.cond.accepts_letter(letter))
        out.emplace_back(e);
    }
    return out;
  }

  bool state_has_edge_matching_cond(const unsigned & s, const BSC & cond) const {
    for (const auto & e : edges[s])
      if (e.cond.intersects(cond))
        return true;
    return false;
  }

  twa_graph_ptr to_twa_graph(const ap_info & apinfo, const bdd_dict_ptr & dict) const {
    auto g = make_twa_graph(dict);
    g->new_states(state_count);
    for (unsigned s=0; s<state_count; ++s) {
      for (const auto & e : c_out(s)) {
        g->new_edge(s, e.dest, e.cond.to_bdd(apinfo));
      }
    }
    return g;
  }

  void dump_all_states() const {
    cout << "Dumping data on all " << state_count << " states:" << endl;
    for (unsigned i=0; i<state_count; ++i) {
      dump_state(i);
    }
    cout << endl << endl;
  }

  void dump_state(const unsigned & s) const {
    cout << "State " << s << " Edges:" << endl;
    for (auto & e : c_out(s))
      cout << " - " << e << endl;
  }
};
using mutable_BSG = mutable_BSG_gen<unsigned>;


struct BSDDG_Edge {
  BSDD cond;
  unsigned dest;

  BSDDG_Edge(const BSDD & cond, unsigned dest)
  : cond{cond}, dest{dest} {}

  // static mutable_BSG_Edge from_BSG_Edge(const BSG_Edge & e) {

  // }
};

class BSDDG {
private:
  unsigned state_count;
  std::vector<std::vector<BSDDG_Edge>> edges;
  BSDD mem = BSDD(true);
public:
  BSDDG() {
  }

  unsigned add_state() {
    edges.emplace_back();
    return state_count++;
  }

  void add_states(const unsigned & new_state_count) {
    for (unsigned i=0; i<new_state_count; ++i)
      edges.emplace_back();
    state_count += new_state_count;
  }
  
  void add_edge(unsigned src, unsigned dest, const BSC & cond) {
    for (auto & e : edges[src]) if (e.dest == dest) {
      e.cond |= cond;
      return;
    }
    // should these all share the same BDD memory or be separate?
    // right now they are sharing the memory
    edges[src].emplace_back(mem.bdd_from_bsc(cond), dest);
  }

  // const std::vector<mutable_BSG_Edge> & out(const unsigned & s) const {
  //   return edges[s];
  // }

  // std::vector<mutable_BSG_Edge> & out_matching(const unsigned & s, const unsigned & letter) const {
  //   std::vector<mutable_BSG_Edge> out;
  //   for (const auto & e : edges[s]) {
  //     if (e.cond.accepts_letter(letter))
  //       out.emplace_back(e);
  //   }
  //   return out;
  // }

};

mutable_BSG make_merged_mutable_BSG(const BSG & og) {
  const unsigned num_states = og.num_states();
  mutable_BSG g;
  g.add_states(num_states);
  for (unsigned s=0; s<num_states; ++s) {
    for (const auto & e : og.out(s)) {
      g.add_edge(s, e.dest, e.cond);
    }
  }
  return g;
}
