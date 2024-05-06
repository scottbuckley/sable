#include "test.hh"
#include "common.hh"
#include "kcobuchi.cc"
#include "words.cc"
#include "membership.cc"
#include "lsafe.cc"
#include "safety_games.cc"
#include "getinput.cc"

#include <spot/tl/dot.hh>
#include <spot/twaalgos/dualize.hh>
#include <spot/twaalgos/complete.hh>
#include <spot/twaalgos/isdet.hh>
#include <spot/twaalgos/contains.hh>

#include <spot/tl/relabel.hh>

#define CONFIG_RELABEL_FORMULA 1


// #define EXAMPLE_FILE "../benchmarks_syntcomp/tlsf/amba/amba/parametric/amba_case_study.tlsf"
// #define EXAMPLE_FILE "../benchmarks_syntcomp/tlsf/amba/amba_decomposed/amba_decomposed_decode.tlsf"
// #define EXAMPLE_FILE "../benchmarks_syntcomp/tlsf/amba/amba_decomposed/amba_decomposed_shift.tlsf"
// #define EXAMPLE_FILE "../benchmarks_syntcomp/tlsf/amba/amba_decomposed/amba_decomposed_tburst4.tlsf"
// #define EXAMPLE_FILE "../benchmarks_syntcomp/tlsf/amba/amba_decomposed/amba_decomposed_tincr.tlsf"
// #define EXAMPLE_FILE "../benchmarks_syntcomp/tlsf/amba/amba_decomposed/amba_decomposed_tsingle.tlsf"
// #define EXAMPLE_FILE "../benchmarks_syntcomp/tlsf/ltl2dba/parametric/ltl2dba_R.tlsf"
// #define EXAMPLE_FILE "../benchmarks_syntcomp/tlsf/full_arbiter/parametric/full_arbiter.tlsf"
#define EXAMPLE_FILE "../benchmarks_syntcomp/tlsf/full_arbiter/parametric/simple_arbiter.tlsf"

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