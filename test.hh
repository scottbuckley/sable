#include "common.hh"

bool membership_query(formula spec, string word);
formula get_ltl();
twa_graph_ptr get_buchi(formula ltl);
int main();
void display_graph_info(twa_graph_ptr g, string name = "");
void generate_graph_image(twa_graph_ptr g, bool render_via_cmd = false, string name);
