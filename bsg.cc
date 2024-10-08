#pragma once
#include "bsc.cc"
#include "bdd.cc"

// bitset graphs

struct BSG_Edge {
  BSC cond;
  unsigned dest;

  BSG_Edge(const bdd & cond, unsigned dest, const ap_info & apinfo)
  : cond(cond, apinfo), dest{dest} {}

  BSG_Edge(const BSC & cond, unsigned dest)
  : cond{cond}, dest{dest} {}
};

ostream & operator<< (ostream &out, const BSG_Edge & e) {
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
public:
  BSG(const twa_graph_ptr & g, const ap_info & apinfo) {
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

  std::vector<BSG_Edge> out_matching(const unsigned & s, const BSC & cond) const {
    std::vector<BSG_Edge> out;
    for (const auto & e : this->out(s))
      if (e.cond.intersects(cond))
        out.emplace_back(e);
    return out;
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
public:
  mutable_BSG() {
  }

  unsigned add_state() {
    edges.emplace_back();
    return ++state_count;
  }

  void add_states(const unsigned & new_state_count) {
    for (unsigned i=0; i<new_state_count; ++i)
      edges.emplace_back();
    state_count += new_state_count;
  }
  
  void add_edge(unsigned src, unsigned dest, const BSC & cond) {
    edges[src].emplace_back(cond, dest);
  }

  const std::vector<BSG_Edge> & out(const unsigned & s) const {
    return edges[s];
  }

  std::vector<BSG_Edge> out_matching(const unsigned & s, const unsigned & letter) const {
    std::vector<BSG_Edge> out;
    for (const auto & e : edges[s]) {
      if (e.cond.accepts_letter(letter))
        out.emplace_back(e);
    }
    return out;
  }

};




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
    return ++state_count;
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
