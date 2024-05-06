#pragma once

#define GEN_PAGE_BASIC 1
#define GEN_PAGE_DETAILS 1
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