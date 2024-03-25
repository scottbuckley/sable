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
#define k 0

twa_graph_ptr get_buchi(formula ltl) {
    translator trans;
    trans.set_type(postprocessor::Buchi);
    trans.set_level(postprocessor::Low); // stopping optimisation for now
    trans.set_pref(postprocessor::SBAcc + postprocessor::Complete);
    twa_graph_ptr aut = trans.run(ltl);
    return aut;
}

string to_string(formula f) {
    stringstream x;
    x << f;
    return x.str();
}

int main() {

    page_start ("Sable Debug");

    // get the formula and print it and its negation
    formula ltl = parse_formula(starting_ltl);
    page_text(to_string(ltl), "LTL");

    formula ltl_neg = formula::Not(ltl);
    page_text(to_string(ltl_neg), "&not;LTL");

    // set up an "ap map" to dictate which APs are inputs, and which are outputs.
    // the example below (unless somebody changes it) says that the AP with BDD VAR INDEX zero
    // (these indices aren't contiguous though) is an input AP, and all others are output APs.
    ap_map apmap = std::vector<bool>(2, false);
    apmap[0] = true;
    page_text(to_string(apmap), "AP Map");

    twa_graph_ptr buchi_neg = get_buchi(ltl_neg);
    page_graph(buchi_neg, "NonDet Buchi of negated LTL");

    twa_graph_ptr cobuchi = dualize_to_univ_coBuchi(buchi_neg);
    page_graph(cobuchi, "Universal CoBuchi of LTL");
    // twa_graph_ptr cobuchi_normaldualize = dualize(buchi_neg);
    // twa_graph_ptr cobuchi_dualize2 = dualize2(buchi_neg);
    // unsigned k = 5;
    twa_graph_ptr kcobuchi = kcobuchi_expand(cobuchi, k);
    page_graph(kcobuchi, "Expanded K-CoBuchi for K="+to_string(k));
    
    Lsafe(kcobuchi, apmap);
    // cout << "lsafe finished" << endl;

    // generate a bunch of images
    // generate_graph_image(buchi_neg, true, "buchi");
    // generate_graph_image(cobuchi_normaldualize, true, "dualized");
    // generate_graph_image(cobuchi_dualize2, true, "dualize2d");
    // generate_graph_image(cobuchi, true, "cobuchi");
    // generate_graph_image(kcobuchi, true);

    // solve_safety(un_universalise(cobuchi), apmap);

    

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