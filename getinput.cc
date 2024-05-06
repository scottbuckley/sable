#pragma once
#include "common.hh"

#include <fstream>

#define CONFIG_ALWAYS_REGENERATE_LTL 1

tuple<string, vector<string>> get_example_from_tlsf(const string & tlsf_path) {
  ifstream ltl_file;
  ifstream ins_file;

  #if (CONFIG_ALWAYS_REGENERATE_LTL)
    string command = "./make_ltl.sh " + tlsf_path;
    auto ret = system(command.c_str());
    ltl_file.open(tlsf_path + ".ltl");
  #else
    ltl_file.open(tlsf_path + ".ltl");
    if (!ltl_file.good()) {
      // the file doesnt exist, we need to generate it
      string command = "./make_ltl.sh " + tlsf_path;
      auto ret = system(command.c_str());
      // cout << "return thing was " << WEXITSTATUS(x) << endl;
      ltl_file.open(tlsf_path + ".ltl");
    }
  #endif
  
  ins_file.open(tlsf_path + ".ins");

  stringstream ltl_file_contents;
  ltl_file_contents << ltl_file.rdbuf();

  stringstream ins_file_contents;
  ins_file_contents << ins_file.rdbuf();

  // parse contents of the ins file into a vector of strings
  auto input_aps = vector<string>();
  string tmp;
  while(getline(ins_file_contents, tmp, ',')){
    tmp.erase(std::remove(tmp.begin(), tmp.end(), ' '), tmp.end());
    tmp.erase(std::remove(tmp.begin(), tmp.end(), '\n'), tmp.end());
    input_aps.push_back(tmp);
  }

  return {ltl_file_contents.str(), input_aps};
}



/* these are the old hard-coded examples
#define HARD_EXAMPLE 4

  std::vector<string> input_aps;
  #if HARD_EXAMPLE == 0
    // very simple example
    const string starting_ltl = "G(a -> Xb -> XX!a) && GFb";
    input_aps = {"a"};
  #endif
  #if HARD_EXAMPLE == 1
    // Full Arbiter n=3
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
    const string starting_ltl = "G(a -> Fga) && G(c -> Fgc) && G(!ga || !gc)"; // should get us to only 3 states
    input_aps = {"a", "c"};
  #endif
  #if HARD_EXAMPLE == 3
    const string starting_ltl = "G(a -> Fga) && G(c -> Fgc) && G(!ga || !gc) && (G!a -> G!ga) && (G!c -> G!gc) && (G(ga -> X!ga)) && (G(gc -> X!gc))";
    input_aps = {"a", "c"};
  #endif
  #if HARD_EXAMPLE == 4
    // this one explodes in time, doing inclusion checks
    const string starting_ltl = "(F (p0 & F (p1 & F (p2 & X p3))) & F (q0 & F (q1 & F (q2 & X q3)))) <-> G F acc";
    input_aps = {"p0", "p1", "p2", "p3", "q0", "q1", "q2", "q3"};
  #endif
  #if HARD_EXAMPLE == 5
    const string starting_ltl = "(G ((grant0 & G !request0) -> (F !grant0))) "
      "&& (G ((grant1 & G !request1) -> (F !grant1))) "
      "&& (G ((grant2 & G !request2) -> (F !grant2))) "
      "&& (G ((grant0 & X (!request0 & !grant0)) -> X (request0 R !grant0))) "
      "&& (G ((grant1 & X (!request1 & !grant1)) -> X (request1 R !grant1))) "
      "&& (G ((grant2 & X (!request2 & !grant2)) -> X (request2 R !grant2))) "
      "&& (G ((!grant0 & !grant1) | ((!grant0 | !grant1) & !grant2))) "
      "&& (request0 R !grant0) "
      "&& (request1 R !grant1) "
      "&& (request2 R !grant2) "
      "&& (G (request0 -> F grant0)) "
      "&& (G (request1 -> F grant1)) "
      "&& (G (request2 -> F grant2))";
      input_aps = {"request0", "request1", "request2"};
  #endif

// const string starting_ltl = "G(a -> Fga) && G(c -> Fgc) && G(!ga || !gc)"; // should get us to only 3 states
  // const string starting_ltl = "!(G(a -> Xb -> XX!a) && GFb)"; // not realisable

   */