#pragma once

#include <spot/twaalgos/word.hh>
#include <spot/tl/parse.hh>
#include <spot/twa/formula2bdd.hh>
#include <spot/tl/print.hh>

#include <sstream>

using namespace spot;
using namespace std;

typedef std::list<bdd> finite_word; // finite word (comprised of bdds)
typedef std::shared_ptr<finite_word> finite_word_ptr;

inline finite_word_ptr make_finite_word() {
    return std::make_shared<finite_word>();
}


typedef shared_ptr<vector<unsigned>> index_word_ptr;

const index_word_ptr empty_word() {
  return make_shared<vector<unsigned>>();
}

index_word_ptr copy_word(index_word_ptr word) {
  index_word_ptr out = empty_word();
  auto end = word->end();
  for (auto it = word->begin(); it!=end; ++it) {
    out->push_back(*it);
  }
  return out;
}

// template <>
// struct hash<bdd>{
//   std::size_t operator()( bdd const& val ) const {
//     return 100;
//   }
// };

/* here we assume that only one alphabet will be used in this run. if not, then UH OH.
    but i'll put in a hash check just in case i guess */
// alphabet_vec cached_alphabet = nullptr;
// auto reverse_alphabet_cache = boost::unordered_map<bdd, unsigned>();

//TODO: this should be reverse-engineerable from taking a look at the APs. The unsigned that
// represents the bdd has its bits mapping directly to those APs, so I should be able to do this
// using that kinda method.
index_word_ptr bdd_word_to_index_word(finite_word_ptr word, const ap_info & apinfo) {
  // if (cached_alphabet == nullptr) cached_alphabet = alphabet;
  // else if (cached_alphabet != alphabet) throw logic_error("we are using a different alphabet now?!");
  
  auto out = empty_word();
  for (const auto & bdd_letter : *word) {
    // auto found = reverse_alphabet_cache.find(bdd_letter);
    // if (found == reverse_alphabet_cache.end()) {
      // doesn't exist in the cache
      auto index_letter = make_bits_from_bdd(bdd_letter, apinfo);
      out->push_back(index_letter);
    // }
  }
  return out;
}

finite_word_ptr string_to_prefix(bdd_dict_ptr dict, string wordstring) {
    finite_word_ptr fword = make_finite_word();
    bdd current_bdd = bddtrue;
    bool is_negating = false;
    for (char c : wordstring) {
        if (c == ' ') {
            fword->push_back(current_bdd);
            current_bdd = bddtrue;
            is_negating = false;
        } else if (c == '!') {
            is_negating = true;
        } else {
            const std::string cs{c};
            const formula f = formula::ap(cs);
            try {
                if (is_negating) {
                    current_bdd &= bdd_nithvar(dict->var_map.at(f));
                } else {
                    current_bdd &= bdd_ithvar(dict->var_map.at(f));
                }
            } catch (std::out_of_range e) {
                throw std::invalid_argument("input string contains characters not in the dictionary: " + cs);
            }
            is_negating = false;
        }
    }
    fword->push_back(current_bdd);
    return fword;
}

void print_prefix(std::ostream& os, const bdd_dict_ptr dict, const finite_word_ptr fword) {
    for (auto c : *fword) {
        os << bdd_to_formula(c, dict) << " ";
    }
}

string prefix_to_string(bdd_dict_ptr dict, finite_word_ptr fword) {
    stringstream out;
    bool first = true;
    out << "[";
    for (auto letter : *fword) {
        if (first) first = false;
        else out << ", ";
        out << bdd_to_formula(letter, dict);
    }
    out << "]";
    return out.str();
}

//FIXME: this might be more efficient if we weren't using List, bit instead Vector.
finite_word_ptr finite_word_append(finite_word_ptr prefix, bdd letter) {
    finite_word_ptr new_word = make_finite_word();
    for (bdd l : *prefix)
        new_word->push_back(l);
    new_word->push_back(letter);
    return new_word;
}