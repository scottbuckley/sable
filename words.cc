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