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

#define HARD_EXAMPLE 1

int main() {

    std::vector<string> input_aps;

    #if HARD_EXAMPLE == 1
        // Full Arbiter n=2
        const string starting_ltl = "G ((grant0 & G !request0) -> (F !grant0))"
            "&& G ((grant1 & G !request1) -> (F !grant1))"
            "&& G ((grant2 & G !request2) -> (F !grant2))"
            "&& G ((grant0 & X (!request0 & !grant0)) -> X (request0 R !grant0))"
            "&& G ((grant1 & X (!request1 & !grant1)) -> X (request1 R !grant1))"
            "&& G ((grant2 & X (!request2 & !grant2)) -> X (request2 R !grant2))"
            "&& G ((!grant0 & !grant1) | ((!grant0 | !grant1) & !grant2))"
            "&& request0 R !grant0"
            "&& request1 R !grant1"
            "&& request2 R !grant2"
            "&& G (request0 -> F grant0)"
            "&& G (request1 -> F grant1)"
            "&& G (request2 -> F grant2)";
        input_aps = {"request0", "request1", "request2"};
    #endif
    #if HARD_EXAMPLE == 2
        const string starting_ltl = "G(a -> Fga) && G(c -> Fgc) && G(!ga || !gc) && (G!a -> G!ga) && (G!c -> G!gc) && (G(ga -> X!ga)) && (G(gc -> X!gc))";
        input_aps = {"a", "c"};
    #endif
    #if HARD_EXAMPLE == 3
        const string starting_ltl = "(F (p0 & F (p1 & F (p2 & X p3))) & F (q0 & F (q1 & F (q2 & X q3)))) <-> G F acc";
        input_aps = {"p0", "p1", "p2", "p3", "q0", "q1", "q2", "q3"};
    #endif
    
    // const string starting_ltl = "G(a -> Fga) && G(c -> Fgc) && G(!ga || !gc)"; // should get us to only 3 states
    // const string starting_ltl = "!(G(a -> Xb -> XX!a) && GFb)"; // not realisable




    // get the formula and print it and its negation
    formula ltl = parse_formula(starting_ltl);

    cout << "beginning process" << endl;
    page_start ("Sable Debug");
    
    auto timers = StopwatchSet();
    LSafe(ltl, input_aps, timers);
    timers.report();
    timers.draw_page_donut("LSafe total", {
        "K expansion",
        "initialisation",
        "Table closing",
        "Inclusion Checks",
        "Safety Game Solving"
    });
    
    page_finish();
    return 0;
}


void display_graph_info(twa_graph_ptr g, string name) {
    if (!name.empty())
        cout << "Graph: " << name << '\n';
    print_hoa(cout, g);
}