#include <iostream>
#include <spot/twaalgos/hoa.hh>
#include <spot/twa/twagraph.hh>

int main(void)
{
  // The bdd_dict is used to maintain the correspondence between the
  // atomic propositions and the BDD variables that label the edges of
  // the automaton.
  spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  // This creates an empty automaton that we have yet to fill.
  spot::twa_graph_ptr aut = make_twa_graph(dict);

//   // Since a BDD is associated to every atomic proposition, the
//   // register_ap() function returns a BDD variable number
//   // that can be converted into a BDD using bdd_ithvar().
//   bdd p1 = bdd_ithvar(aut->register_ap("p1"));
//   bdd p2 = bdd_ithvar(aut->register_ap("p2"));

//   // Set the acceptance condition of the automaton to Inf(0)&Inf(1)
//   aut->set_generalized_buchi(2);

//   // States are numbered from 0.
//   aut->new_states(3);
//   // The default initial state is 0, but it is always better to
//   // specify it explicitly.
//   aut->set_init_state(0U);

//   // new_edge() takes 3 mandatory parameters: source state,
//   // destination state, and label.  A last optional parameter can be
//   // used to specify membership to acceptance sets.
//   aut->new_edge(0, 1, p1);
//   aut->new_edge(1, 1, p1 & p2, {0});
//   aut->new_edge(1, 2, p2, {1});
//   aut->new_edge(2, 1, p1 | p2, {0, 1});

//   // Print the resulting automaton.
//   print_hoa(std::cout, aut);
  return 0;
}