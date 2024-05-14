#pragma once

#define GEN_PAGE_BASIC 0
#define GEN_PAGE_DETAILS 0
#define GEN_PAGE_TIMER 1
#define GEN_K_TIMERS 1

#include <spot/tl/parse.hh>
#include <spot/tl/print.hh>
#include <spot/twaalgos/translate.hh>
#include <spot/twaalgos/neverclaim.hh>
#include <spot/twaalgos/hoa.hh>
#include <spot/twaalgos/word.hh>
#include <spot/twaalgos/dot.hh>

#include "graphs_page.cc"

using namespace std;
using namespace spot;

typedef std::vector<bool> ap_map;
typedef shared_ptr<vector<bdd>> alphabet_vec;

void debug(string msg) {
    cout << msg << '\n';
}

template <typename T>
inline const bool vec_contains(vector<T> vec, T elem) {
    return (std::find(vec.begin(), vec.end(), elem) != vec.end());
}

template<typename T>
void print_vector(vector<T> v) {
    bool first = true;
    cout << "[";
    for (T b : v) {
        if (!first) cout << ", ";
        cout << b;
        first = false;
    }
    cout << "]" << endl;
}

template<typename T>
void print_vector(std::unordered_set<T> v) {
    bool first = true;
    cout << "[";
    for (T b : v) {
        if (!first) cout << ", ";
        cout << b;
        first = false;
    }
    cout << "]" << endl;
}

template<typename T>
string to_string(vector<T> v) {
    stringstream out;
    bool first = true;
    out << "[";
    for (T b : v) {
        if (!first) out << ", ";
        out << b;
        first = false;
    }
    out << "]" << endl;
    return out.str();
}

template<typename T>
string force_string(T v) {
    stringstream out;
    out << v;
    return out.str();
}

string to_string(formula f) {
    stringstream x;
    x << f;
    return x.str();
}

const inline string pad_number(unsigned n, unsigned width) {
  auto n_str = to_string(n);
  return std::string(width - n_str.length(), ' ') + n_str;
}

unsigned count_true(const vector<bool> & vec) {
  unsigned count = 0;
  for (const auto & b : vec)
    if (b) count++;
  return count;
}

inline unsigned count_false(const vector<bool> & vec) {
  return vec.size() - count_true(vec);
}

// NOTE: we have that the letter "a & !b" satisfies the condition "a", but
// we DO NOT HAVE that the letter "a" (which is not really a letter)
// satisfies the condition "a & !b".
inline bool letter_satisfies_cond(const bdd & letter, const bdd & cond) {
  // cout << "letter: " << letter << ", cond: " << cond << " = ";
  const bool result = bdd_apply(letter, cond, bddop_imp) == bddtrue;
  // cout << result << endl;
  return (result);
}


// since the bdd ids assigned to variables aren't going to be consecutive,
// we want to give them new indices so we can more mathematically iterate
// through them (and maybe do some bitwise trickery with them).
typedef std::vector<tuple<formula, unsigned>> better_var_map;
typedef std::shared_ptr<better_var_map> better_var_map_ptr;
inline better_var_map_ptr empty_better_var_map(){
  return std::make_shared<better_var_map>();
}

better_var_map_ptr get_better_var_map(bdd_dict_ptr dict) {
  better_var_map_ptr bvm = empty_better_var_map();
  bvm->reserve(dict->var_map.size());
  for (auto [f, i] : dict->var_map)
    bvm->push_back({f, i});
  return bvm;
}

bdd make_varset_bdd (better_var_map_ptr bvm) {
  bdd out = bddtrue;
  for (auto & [f, v] : *bvm)
    out &= bdd_ithvar(v);
  return out;
}

bdd make_bdd_from_bits(unsigned bits, better_var_map_ptr bvm) {
  bdd out = bddtrue;
  for (auto it = bvm->begin(); it != bvm->end(); ++it) {
    const auto [f, ind] = *it;
    if (bits & 1)
      out &= bdd_ithvar(ind);
    else
      out &= bdd_nithvar(ind);
    bits >>= 1;
  }
  return out;
}


inline bdd complete_letter(const bdd & letter, const bdd & varset) {
  return bdd_satoneset(letter, varset, bddtrue);
}

unsigned make_bits_from_bdd(bdd b, better_var_map_ptr bvm, const bdd & varset, alphabet_vec alphabet) {
  // first we need to refine this bdd to a specific bdd - a letter index can
  // only reference a single AP configuration, but the supplied BDD might
  // be a formula that allows for a few options.
  bdd specific_b = complete_letter(b, varset);

  // now we check each AP, if it's true in the bdd, we add that bit to the
  // index bits. This is how the alphabet was built, so it should work the
  // same way backwards. I should test this though lol.
  unsigned bits = 0;
  unsigned mask = 1;
  for (auto it = bvm->begin(); it != bvm->end(); ++it) {
    const auto [f, ind] = *it;
    if (bdd_implies(specific_b, bdd_ithvar(ind))) {
      bits |= mask;
    }
    mask <<= 1;
  }
  // the below is a debug check - should be removed at some point.
  if ((*alphabet)[bits] != specific_b) {
    throw std::logic_error("looks like theres a problem with this algorithm");
  }
  return bits;
}
