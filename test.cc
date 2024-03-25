#include "test.hh"
#include "kcobuchi.cc"
#include "words.cc"
#include "membership.cc"
#include "lsafe.cc"
#include "safety_games.cc"

#include <spot/tl/dot.hh>
#include <spot/twaalgos/dualize.hh>
#include <spot/twaalgos/complete.hh>
#include <spot/twaalgos/isdet.hh>
#include <spot/twaalgos/contains.hh>

#define starting_ltl "G(a -> Xb -> XX!a) && GFb"
// #define starting_ltl "G(a -> Fb) && G(c -> Fd) && G(!b || !d)" // should get us to only 3 states

int main() {

    page_start ("Sable Debug");

    // get the formula and print it and its negation
    formula ltl = parse_formula(starting_ltl);
    page_text(to_string(ltl), "LTL");

    // set up an "ap map" to dictate which APs are inputs, and which are outputs.
    // the example below (unless somebody changes it) says that the AP with BDD VAR INDEX zero
    // (these indices aren't contiguous though) is an input AP, and all others are output APs.
    ap_map apmap = std::vector<bool>(2, false);
    apmap[0] = true;
    page_text(to_string(apmap), "AP Map");
    
    LSafe(ltl, apmap);

    // auto w1 = string_to_prefix(kcobuchi->get_dict(), "a !ab");
    // auto w2 = string_to_prefix(kcobuchi->get_dict(), "!ab ab");
    // auto w3 = string_to_prefix(kcobuchi->get_dict(), "a!b ab");
    // auto walker = make_walker(kcobuchi);
    // if (walker->walk(w1)) {
    //        cout << "w1 fails" << endl;
    // } else cout << "w1 is ok" << endl;

    // auto st = walker->get_position();

    // if (walker->walk(w2)) {
    //        cout << "w1+w2 fails" << endl;
    // } else cout << "w1+w2 is ok" << endl;

    // walker->set_position(st);

    // if (walker->walk(w3)) {
    //        cout << "w1+w3 fails" << endl;
    // } else cout << "w1+w3 is ok" << endl;
    // cout << prefix_to_string(kcobuchi->get_dict(), w) << endl;
    // cout << member(w, kcobuchi) << endl;
    // cout << are_equivalent(buchi_neg, formula::tt());
    // display_graph_info(cobuchi, "cobuchi");

    // btree * x = new btree();
    // x->add_at_path({1,1,0,1}, 1101);
    // x->add_at_path({1,0,0,0}, 1000);
    // x->add_at_path({0,0,1,0}, 9010);
    // x->add_at_path({0,1,1,1,1,1,1,1,0}, 444);
    // for (auto it : *x) {
    //     cout << "item: " << it << endl;
    // }
    
    page_finish();
    return 0;
}


void display_graph_info(twa_graph_ptr g, string name) {
    if (!name.empty())
        cout << "Graph: " << name << '\n';
    print_hoa(cout, g);
}