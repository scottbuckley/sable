#pragma once

#define CONFIG_RELABEL_FORMULA 0
#define CONFIG_SOLVE_GAME_DETERMINISTIC 1

#define CONFIG_REDUNDANT_CHECK_MOORE_INTERSECTION_AGAINST_SPOT 1

#define PAUSE_REGULARLY 0

#include <iostream>
#include <fstream>
#include <string>

#include <spot/tl/parse.hh>
#include <spot/tl/print.hh>
#include <spot/twaalgos/translate.hh>
#include <spot/twaalgos/neverclaim.hh>
#include <spot/twaalgos/hoa.hh>
#include <spot/twaalgos/word.hh>
#include <spot/twaalgos/dot.hh>

#include "command_line_args.cc"

#include "graphs_page.cc"
#include "util.hh"
#include "apinfo.cc"


// NOTE: we have that the letter "a & !b" satisfies the condition "a", but
// we DO NOT HAVE that the letter "a" (which is not really a letter)
// satisfies the condition "a & !b".
inline bool letter_satisfies_cond(const bdd & letter, const bdd & cond) {
  // cout << "letter: " << letter << ", cond: " << cond << " = ";
  const bool result = bdd_apply(letter, cond, bddop_imp) == bddtrue;
  // cout << result << endl;
  return (result);
}

inline bdd complete_letter(const bdd & letter, const ap_info & apinfo) {
  return bdd_satoneset(letter, apinfo.bdd_all_aps, bddtrue);
}

unsigned make_bits_from_bdd(bdd b, const ap_info & apinfo) {
  // first we need to refine this bdd to a specific bdd - a letter index can
  // only reference a single AP configuration, but the supplied BDD might
  // be a formula that allows for a few options.
  bdd specific_b = complete_letter(b, apinfo);

  // now we check each AP, if it's true in the bdd, we add that bit to the
  // index bits. This is how the alphabet was built, so it should work the
  // same way backwards. I should test this though lol.
  unsigned bits = 0;
  unsigned mask = 1;
  // for (auto it = bvm->begin(); it != bvm->end(); ++it) {
    // const auto [f, ind] = *it;
  for (const auto & v : apinfo.ind_to_var) {
    if (bdd_implies(specific_b, bdd_ithvar(v))) {
      bits |= mask;
    }
    mask <<= 1;
  }
  // the below is a debug check - should be removed at some point.
  if ((apinfo.bdd_alphabet)[bits] != specific_b) {
    throw std::logic_error("looks like theres a problem with this algorithm");
  }
  return bits;
}

inline bool bdd_overlap(const bdd &l, const bdd &r) noexcept {
  return ((l & r) != bddfalse);
}

inline int bdd_compat(const bdd &l, const bdd &r) noexcept {
  return (bdd_implies(l, r) || bdd_implies(r, l));
}




struct store_unsigned {
  unsigned val = 0;
  store_unsigned(unsigned val) : val{val} {};
  store_unsigned * copy() {
    return new store_unsigned(val);
  }
};




#if PAUSE_REGULARLY
  #define PAUSE wait()
#else
  #define PAUSE
#endif

string line;

inline void wait() {
  cout << "press enter to continue ... ";
  std::getline(std::cin, line);
}

inline void wait(string s) {
  cout << s;
  std::getline(std::cin, line);
}

string binary_string(unsigned v, unsigned num_bits = 12) {
  stringstream x;
  for (unsigned i=0; i<num_bits; ++i) {
    x << (v & 0b1);
    v >>= 1;
  }
  return x.str();
}

  // void print_to_ss(ostream & out) const {
  //   unsigned truth_copy = truth;
  //   unsigned pathy_copy = pathy;

  //   if (pathy == 0)
  //     out << "TRUE";

  //   char yes = 'A';
  //   char no  = 'a';
  //   while (truth_copy != 0 || pathy_copy != 0) {
  //     if (pathy_copy & 0b1) {
  //       if (truth_copy & 0b1)
  //         out << yes;
  //       else
  //         out << '^' << yes;
  //     }
  //     truth_copy >>= 1;
  //     pathy_copy >>= 1;
  //     yes++; no++;
  //   }
  // }
// }