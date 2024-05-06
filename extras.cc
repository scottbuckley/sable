#pragma once
#include "common.hh"
#include "kcobuchi.cc"
#include <string>
#include <spot/tl/relabel.hh>

bdd get_dead_letters(twa_graph_ptr g) {
  accepting_state_cache asc = make_accepting_state_cache(g);
  bdd dead_letters = bddtrue;
  for (unsigned i=0; i<g->num_states(); ++i) {
    for (auto & e : g->out(i)) {
      if (g->is_univ_dest(e.dst)) {
        for (auto dst : g->univ_dests(e.dst)) {
          if (is_accepting_state(g, dst, asc) && is_sink(g, dst)) {
            dead_letters = bdd_and(dead_letters, e.cond);
          }
        }
      } else if (is_accepting_state(g, e.dst, asc) && is_sink(g, e.dst)) {
        dead_letters = bdd_and(dead_letters, e.cond);
      }
    }
  }
  return dead_letters;
}

vector<bool> get_dead_letter_map(twa_graph_ptr g, alphabet_vec alphabet) {
  bdd dead_letters = get_dead_letters(g);
  auto bitmap = vector<bool>(alphabet.size(), true);
  for (unsigned i=0; i<alphabet.size(); ++i) {
    if (bdd_implies(alphabet[i], dead_letters)) {
      bitmap[i] = false;
    }
  }
  return bitmap;
}


void rename_aps_in_formula(formula & ltl, vector<string> & input_aps) {
  auto relabel_map = new std::map<formula, formula>();
  ltl = spot::relabel(ltl, Abc, relabel_map);
  #if GEN_PAGE_BASIC  
    stringstream ss;
    for (auto & [new_ap, old_ap] : *relabel_map) {
      ss << new_ap << " = " << old_ap << endl;
    }
    cout << ss.str();
    page_code("LTL formula renaming map:", ss.str());
  #endif
  // make new input_aps list
  auto new_input_aps = vector<string>();
  for (auto & [new_ap, old_ap] : *relabel_map) {
    string old_ap_string = force_string(old_ap);
    if (std::any_of(input_aps.begin(), input_aps.end(), [old_ap_string](string s){return s==old_ap_string;})) {
      new_input_aps.push_back(force_string(new_ap));
    }
  }
  input_aps.swap(new_input_aps);
}