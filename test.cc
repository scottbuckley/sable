#include "test.hh"
#include "kcobuchi.cc"

#include <spot/tl/dot.hh>
#include <spot/twaalgos/dualize.hh>
#include <spot/twaalgos/complete.hh>
#include <spot/twaalgos/isdet.hh>
#include <spot/twaalgos/contains.hh>

#define starting_ltl "G(b->X!b)"

twa_graph_ptr get_buchi(formula ltl) {
  translator trans;
  trans.set_type(postprocessor::Buchi);
  trans.set_level(postprocessor::Low); // stopping optimisation for now
  trans.set_pref(postprocessor::SBAcc + postprocessor::Complete);
  twa_graph_ptr aut = trans.run(ltl);
  return aut;
}

void generate_graph_image(twa_graph_ptr g, bool render_via_cmd) {
  ofstream outdata;
  outdata.open("graph.dot");
  print_dot(outdata, g, "arf(Lato)C(#ffffaa)b");
  outdata.close();
  if (render_via_cmd)
    system("dot -Tpng graph.dot -o graph.png");
}

int main() {
  // get the formula and print it and its negation
  formula ltl = parse_formula(starting_ltl);
  formula ltl_neg = formula::Not(ltl);
  twa_graph_ptr buchi_neg = get_buchi(ltl_neg);
  
  // buchi_neg->prop_universal(1); // this seems to give us the behaviour we want, but we don't know why
  twa_graph_ptr cobuchi = dualize(buchi_neg);
  
  // below is for k expansion, which is not the issue we are currently facing.
  // cobuchi->prop_universal(1); // mark as universal
  // cobuchi = kcobuchi_expand(cobuchi, 3);

  generate_graph_image(cobuchi, true);
  
  cout << are_equivalent(buchi_neg, formula::tt());
  display_graph_info(cobuchi, "cobuchi");

  return 0;
}


void display_graph_info(twa_graph_ptr g, string name) {
  if (!name.empty())
    cout << "Graph: " << name << '\n';
  print_hoa(cout, g);
}