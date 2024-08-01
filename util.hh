#pragma once

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

using namespace std;
using namespace spot;

#define CONTAINS(coll, elem) (std::find(coll.begin(), coll.end(), elem) != coll.end())

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

template<typename T, typename S>
void print_vector(std::map<T, S> v) {
    bool first = true;
    cout << "[";
    for (const auto & b : v) {
        if (!first) cout << ", ";
        cout << "(" << b.first << ": " << b.second << ")";
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

template <typename T>
struct span_iter {
  const std::vector<T> * collection;
  const unsigned start;
  const unsigned count;

  span_iter(const std::vector<T> * collection, unsigned start, unsigned count)
    : collection{collection}, start{start}, count{count} {}

  struct iter_state {
    const std::vector<T> * collection;
    unsigned index = 0;
    iter_state(unsigned index, const std::vector<T> * collection) : index{index}, collection{collection} {}
    bool operator!=( const iter_state &other ) const noexcept { return index != other.index; }
    iter_state operator++() noexcept { ++index; return *this; }
    T operator*() noexcept { return (*collection)[index]; }
  };

  iter_state begin() const { return iter_state(start, collection); }
  iter_state end()   const { return iter_state(start + count, nullptr); }
};

struct range_iter {
  unsigned start = 0;
  unsigned end_val = 0;

  range_iter(unsigned start, unsigned end_val)
    : start{start}, end_val{end_val} {}
  
  // range_iter(range_iter& copy) : start{copy.start}, end_val{copy.end_val} {};
  range_iter() {}

  struct iter_state {
    unsigned current;
    iter_state(unsigned val) : current{val} {}
    bool operator!=( const iter_state &other ) const noexcept { return current != other.current; }
    iter_state operator++() noexcept { ++current; return *this; }
    unsigned operator*() noexcept { return current; }
  };

  iter_state begin() const { return iter_state(start); }
  iter_state end()   const { return iter_state(end_val); }
};

// #define INDEX_TRANSFORM_ITERATOR(iter_name, iter_type, index_expression)
// template <typename T>
template <typename T, T (F)(const unsigned &)>
struct index_transform_iter {
  unsigned index = 0;
  
  index_transform_iter() {}
  
  struct iter_state {
    unsigned index;
    iter_state(unsigned index) : index{index} {}
    bool operator!=( const iter_state &other ) const noexcept { return true; }
    iter_state operator++() noexcept { ++index; return *this; }
    T operator*() noexcept { return F(index); }
  };

  iter_state begin() const { return iter_state(0); }
  iter_state end()   const { return iter_state(0); }
};

string index_to_label(const unsigned & ind) {
  assert (ind <= 26);
  return string() + ("ABCDEFGHIJKLMNOPQRSTUVWXYZ"[ind]);
}



// INDEX_TRANSFORM_ITERATOR(label_iter, string, "meow")

struct shifted_range_iter {
  unsigned start;
  unsigned end_val;
  int shift_count;

  shifted_range_iter(unsigned start, unsigned end_val, int shift_count)
    : start{start}, end_val{end_val}, shift_count{shift_count} {}
  shifted_range_iter(){}

  struct iter_state {
    unsigned current;
    int shift_count;
    iter_state(unsigned val, int shift_count) : current{val}, shift_count{shift_count} {}
    bool operator!=( const iter_state &other ) const noexcept { return current != other.current; }
    iter_state operator++() noexcept { ++current; return *this; }
    unsigned operator*() noexcept { return current << shift_count; }
  };

  iter_state begin() const { return iter_state(start, shift_count); }
  iter_state end()   const { return iter_state(end_val, shift_count); }
};

//todo: benchmark this against other methods
// bool exactly_one_set_bit(unsigned bits) {
//   bool seen_1 = false;
//   if (bits == 0) return false;
//   // keep shifting until you see the first 1
//   while((bits & 0b1) != 1) {
//     bits >>= 1;
//   }
//   // shift one more time
//   bits >>= 1;
//   // the whole thing must be zero now, if there is only one set bit
//   return (bits == 0);
// }

inline bool has_single_bit(unsigned v) noexcept {
  return v != 0 && (((v & (v - 1)) == 0));
}

template <typename T>
void copy_list_into(const std::list<T> & from, std::list<T> & to) {
  to.clear();
  for (const auto & elem : from)
    to.emplace_back(elem);
}

template <typename T, typename S>
void copy_map_into(const std::map<T,S> & from, std::map<T,S> & to) {
  to.clear();
  for (auto it = from.begin(); it != from.end(); ++it) {
    to[it->first] = it->second;
  }
}

bdd_dict_ptr copy_bdd_dict_ptr(const bdd_dict_ptr & in) {
  auto out = make_bdd_dict();
  copy_map_into(in->var_map, out->var_map);
  copy_map_into(in->acc_map, out->acc_map);
  out->bdd_map = in->bdd_map;
  return out;
}