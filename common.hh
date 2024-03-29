#pragma once


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