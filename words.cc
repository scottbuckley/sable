#include <spot/twaalgos/word.hh>
#include <spot/tl/parse.hh>
#include <spot/twa/formula2bdd.hh>
#include <spot/tl/print.hh>

#include <sstream>

using namespace spot;
using namespace std;


twa_word_ptr string_to_prefix(bdd_dict_ptr dict, string wordstring, bool unrecognised_is_not_first_ap=false) {
    twa_word_ptr word = make_twa_word(dict);

    auto m = dict->var_map;
    for (char c : wordstring) {
        std::string s{c};
        formula f = formula::ap(s);
        if (m.count(f)==0) {
            if (unrecognised_is_not_first_ap)
                word->prefix.push_back(bdd_nithvar(0));
            else
                std::cout << "unrecognised prop: " << s << endl;
        } else {
            word->prefix.push_back(bdd_ithvar(m.at(f)));
        }
    }
    return word;
}

string prefix_to_string(bdd_dict_ptr dict, twa_word_ptr word) {
    stringstream out;
    for (auto c : word->prefix) {
        out << bdd_to_formula(c, dict) << " ";
    }
    return out.str();
}