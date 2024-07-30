#pragma once
#include "common.hh"
#include <spot/twa/formula2bdd.hh>

using namespace std;
using namespace spot;

/*struct ShiftedIterator {
  unsigned value_max;
  unsigned shift_bits;

  struct shift_iter_state {
    unsigned cur_value = 0;
    unsigned shift_bits = 0;
    shift_iter_state(unsigned value, unsigned shift_bits) : cur_value{value}, shift_bits{shift_bits} {}
    bool operator!=( const shift_iter_state &other ) const noexcept { return cur_value != other.cur_value; }
    shift_iter_state operator++() noexcept { ++cur_value; return *this; }
    unsigned operator*() noexcept { return cur_value << shift_bits; }
  };

  ShiftedIterator(unsigned value_max, unsigned shift_bits)
    : value_max{value_max}, shift_bits{shift_bits} {}

  shift_iter_state begin() const { return shift_iter_state(0, shift_bits); }
  shift_iter_state end()   const { return shift_iter_state(value_max, shift_bits); }
};*/

bdd make_bdd_from_bits(unsigned bits, const std::vector<unsigned> & ind_to_var) {
  bdd out = bddtrue;
  for (const auto & var : ind_to_var) {
    if (bits & 1)
      out &= bdd_ithvar(var);
    else
      out &= bdd_nithvar(var);
    bits >>= 1;
  }
  return out;
}

struct ap_info {
  const bdd_dict_ptr dict;

  std::vector<bool> var_is_input;
  std::vector<unsigned> ind_to_var;
  std::vector<unsigned> var_to_ind;
  std::vector<string> ind_to_string;

  vector<bdd> bdd_alphabet;
  range_iter masked_input_letters;
  shifted_range_iter masked_output_letters;

  bdd bdd_all_aps;
  bdd bdd_input_aps;
  bdd bdd_output_aps;

  unsigned ap_count = 0;
  unsigned input_ap_count = 0;
  unsigned output_ap_count = 0;

  unsigned letter_count = 0;
  unsigned input_letter_count = 0;
  unsigned output_letter_count = 0;

  unsigned all_mask = 0;
  unsigned input_mask = 0;
  unsigned output_mask = 0;

  ~ap_info() {
    dict->bdd_map.clear();
    dict->var_map.clear();
    dict->acc_map.clear();
  }

  ap_info(const bdd_dict_ptr & dict_in, std::vector<string> & input_aps)
    : dict{copy_bdd_dict_ptr(dict_in)} {
      
    // some preprocessing
    unsigned max_var = 0;
    for (auto [f, v] : dict->var_map) {
      ap_count++;
      if (v >= max_var) {
        max_var = v;
        var_is_input.resize(v+1);
      }
      if (vec_contains(input_aps, force_string(f))) {
        var_is_input[v] = true;
        input_ap_count++;
      } else {
        output_ap_count++;
      }
    }

    

    /*** SET UP THE BVM AND RBVM ***/

    // predict sizes of stuff
    letter_count = 1 << ap_count;
    input_letter_count = 1 << input_ap_count;
    output_letter_count = 1 << output_ap_count;

    ind_to_var.reserve(ap_count);
    var_to_ind = vector(max_var+1, UINT_MAX);
    bdd_alphabet.reserve(letter_count);
    ind_to_string.reserve(ap_count);
    masked_input_letters = range_iter(0, input_letter_count);
    masked_output_letters = shifted_range_iter(0, output_letter_count, input_ap_count);
    // masked_output_letters = ShiftedIterator(output_letter_count, output_ap_count);

    bdd_all_aps = bdd_true();
    bdd_input_aps = bdd_true();
    bdd_output_aps = bdd_true();

    // the input aps
    unsigned bitmask = 1;
    for (auto [f, v] : dict->var_map) if (var_is_input[v]) {
      input_ap_count += 1;
      var_to_ind[v] = ind_to_var.size();
      ind_to_var.push_back(v);
      ind_to_string.push_back(force_string(f));
      bdd_all_aps &= bdd_ithvar(v);
      bdd_input_aps &= bdd_ithvar(v);
      input_mask |= bitmask;
      bitmask <<= 1;
    }
    // the output aps
    for (auto [f, v] : dict->var_map) if (!var_is_input[v]) {
      output_ap_count += 1;
      var_to_ind[v] = ind_to_var.size();
      ind_to_var.push_back(v);
      ind_to_string.push_back(force_string(f));
      bdd_all_aps &= bdd_ithvar(v);
      bdd_output_aps &= bdd_ithvar(v);
      output_mask |= bitmask;
      bitmask <<= 1;
    }

    all_mask = input_mask | output_mask;

    /* ALPHABETS */
    for (unsigned i=0; i<letter_count; ++i)
      bdd_alphabet.push_back(make_bdd_from_bits(i, ind_to_var));
    
    // cout << "testing output letters" << endl;
    // masked_output_letters_foreach([](unsigned o){
    //   cout << o << endl;
    // });
    // for (const auto z : masked_output_letters) {
    //   cout << z << endl;
    // }
  }

  string bdd_to_string(const bdd & b) const {
    stringstream ss;
    ss << spot::bdd_to_formula(b, dict);
    return ss.str();
  }

  void masked_output_letters_foreach(function<void(unsigned)> fn) {
    for (unsigned i=0; i<output_letter_count; ++i) {
      fn(i << input_ap_count);
    }
  }

  // delete the copy constructors.
  // ap_info (const ap_info&) = delete;
  // ap_info& operator= (const ap_info&) = delete;
};

ap_info make_fake_apinfo() {
  formula f = parse_formula("A & B & C & D & E & H");
  translator trans;
  twa_graph_ptr aut = trans.run(f);
  bdd_dict_ptr dict = aut->get_dict();
  std::vector<string> input_aps = {"A", "B", "C", "D"};
  ap_info out = ap_info(dict, input_aps);
  return out;
}