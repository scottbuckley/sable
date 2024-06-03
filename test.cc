#include "test.hh"
#include "common.hh"
#include "kcobuchi.cc"
#include "words.cc"
#include "walkers.cc"
#include "lsafe.cc"
#include "safety_games.cc"
#include "getinput.cc"
#include "extras.cc"

#include <spot/tl/dot.hh>
#include <spot/twaalgos/dualize.hh>
#include <spot/twaalgos/complete.hh>
#include <spot/twaalgos/isdet.hh>
#include <spot/twaalgos/contains.hh>


// #define EXAMPLE_FILE "../benchmarks_syntcomp/tlsf/amba/amba/parametric/amba_case_study.tlsf"
// #define EXAMPLE_FILE "../benchmarks_syntcomp/tlsf/amba/amba_decomposed/amba_decomposed_decode.tlsf"
// #define EXAMPLE_FILE "../benchmarks_syntcomp/tlsf/amba/amba_decomposed/amba_decomposed_shift.tlsf"
// #define EXAMPLE_FILE "../benchmarks_syntcomp/tlsf/amba/amba_decomposed/amba_decomposed_tburst4.tlsf"
// #define EXAMPLE_FILE "../benchmarks_syntcomp/tlsf/amba/amba_decomposed/amba_decomposed_tincr.tlsf"
// #define EXAMPLE_FILE "../benchmarks_syntcomp/tlsf/amba/amba_decomposed/amba_decomposed_tsingle.tlsf"
// #define EXAMPLE_FILE "../benchmarks_syntcomp/tlsf/ltl2dba/parametric/ltl2dba_R.tlsf"
// #define EXAMPLE_FILE "../benchmarks_syntcomp/tlsf/full_arbiter/parametric/full_arbiter.tlsf"
// #define EXAMPLE_FILE "../benchmarks_syntcomp/tlsf/full_arbiter/parametric/simple_arbiter_2.tlsf"
// #define EXAMPLE_FILE "../benchmarks_syntcomp/tlsf/full_arbiter/parametric/simple_arbiter_3.tlsf"
// #define EXAMPLE_FILE "../benchmarks_syntcomp/tlsf/full_arbiter/parametric/simple_arbiter_4.tlsf"
#define EXAMPLE_FILE "../benchmarks_syntcomp/tlsf/full_arbiter/parametric/simple_arbiter_5.tlsf" // start with K=4, answer in 12 minutes, mostly walking, 10% unmeasured
// #define EXAMPLE_FILE "../benchmarks_syntcomp/tlsf/full_arbiter/parametric/simple_arbiter_6.tlsf"
// #define EXAMPLE_FILE "../benchmarks_syntcomp/tlsf/full_arbiter/parametric/simple_arbiter_7.tlsf"
// #define EXAMPLE_FILE "../benchmarks_syntcomp/tlsf/full_arbiter/parametric/simple_arbiter_8.tlsf"

int main() {
  // get the formula and print it and its negation
  auto [ltlstr, input_aps] = get_example_from_tlsf(
    EXAMPLE_FILE
  );
  // string ltlstr =
  //     // "(G ((grant0 & G !request0) -> (F !grant0))) "
  //     // "&& (G ((grant1 & G !request1) -> (F !grant1))) "
  //     // "&& (G ((grant2 & G !request2) -> (F !grant2))) "
  //     // "&& (G ((grant0 & X (!request0 & !grant0)) -> X (request0 R !grant0))) "
  //     // "&& (G ((grant1 & X (!request1 & !grant1)) -> X (request1 R !grant1))) "
  //     // "&& (G ((grant2 & X (!request2 & !grant2)) -> X (request2 R !grant2))) "
  //     "(G ((!grant0 & !grant1) | ((!grant0 | !grant1) & !grant2))) "
  //     // "&& (request0 R !grant0) "
  //     // "&& (request1 R !grant1) "
  //     // "&& (request2 R !grant2) "
  //     "&& (G (request0 -> F grant0)) "
  //     "&& (G (request1 -> F grant1)) "
  //     "&& (G (request2 -> F grant2))";
  // vector<string> input_aps = {"request0", "request1", "request2"};
  
  cout << "beginning process" << endl;
  page_start ("Sable Debug");

  formula ltl = parse_formula(ltlstr);

  #if CONFIG_RELABEL_FORMULA
    rename_aps_in_formula(ltl, input_aps);
  #endif
  
  auto timers = StopwatchSet();
  auto lsafe_timer = timers.make_timer("LSafe Total");
  lsafe_timer->flag_total();

  lsafe_timer->start();
    LSafe2(ltl, input_aps, timers);
  lsafe_timer->stop()->report();

  timers.draw_page_donut("Timers for total execution");
  
  page_finish();
  return 0;
}


void display_graph_info(twa_graph_ptr g, string name) {
  if (!name.empty())
    cout << "Graph: " << name << '\n';
  print_hoa(cout, g);
}