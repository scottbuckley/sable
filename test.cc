#include "test.hh"
#include "kcobuchi.cc"
#include "words.cc"
#include "membership.cc"
#include "lsafe.cc"

#include <spot/tl/dot.hh>
#include <spot/twaalgos/dualize.hh>
#include <spot/twaalgos/complete.hh>
#include <spot/twaalgos/isdet.hh>
#include <spot/twaalgos/contains.hh>


#define starting_ltl "G(a -> Xb -> XX!b)"

/*
    TODO:
    - fix first-count error DONE
    - don't expand bad sinks DONE
    - optimise good sinks DONE
    - change membership query to:
        - track a set of states (optimisation)
        - follow alternating automata semantics ("OR" between edges, "AND" between univ dests)
*/

twa_graph_ptr get_buchi(formula ltl) {
    translator trans;
    trans.set_type(postprocessor::Buchi);
    trans.set_level(postprocessor::Low); // stopping optimisation for now
    trans.set_pref(postprocessor::SBAcc + postprocessor::Complete);
    twa_graph_ptr aut = trans.run(ltl);
    return aut;
}

void generate_graph_image(twa_graph_ptr g, bool render_via_cmd, string name) {
    ofstream outdata;
    outdata.open("graph.dot");
    print_dot(outdata, g, "arf(Lato)C(#ffffaa)b");
    outdata.close();
    if (render_via_cmd) {
        std::string command = "dot -Tpng graph.dot -o " + name + ".png";
        system(command.c_str());
    }
}

int main() {
    // get the formula and print it and its negation
    formula ltl = parse_formula(starting_ltl);
    formula ltl_neg = formula::Not(ltl);
    twa_graph_ptr buchi_neg = get_buchi(ltl_neg);
    twa_graph_ptr cobuchi = dualize_to_univ_coBuchi(buchi_neg);
    // twa_graph_ptr cobuchi_normaldualize = dualize(buchi_neg);
    // twa_graph_ptr cobuchi_dualize2 = dualize2(buchi_neg);
    twa_graph_ptr kcobuchi = kcobuchi_expand(cobuchi, 5);

    

    // generate a bunch of images
    generate_graph_image(buchi_neg, true, "buchi");
    // generate_graph_image(cobuchi_normaldualize, true, "dualized");
    // generate_graph_image(cobuchi_dualize2, true, "dualize2d");
    generate_graph_image(cobuchi, true, "cobuchi");
    generate_graph_image(kcobuchi, true);

    Lsafe(kcobuchi);

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
    

    return 0;
}


void display_graph_info(twa_graph_ptr g, string name) {
    if (!name.empty())
        cout << "Graph: " << name << '\n';
    print_hoa(cout, g);
}