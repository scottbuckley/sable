#pragma once
#include "common.hh"

#include <fstream>

#define CONFIG_ALWAYS_REGENERATE_LTL 0

tuple<string, vector<string>> get_example_from_tlsf(const string & tlsf_path) {
  ifstream ltl_file;
  ifstream ins_file;

  ltl_file.open(tlsf_path);
  if (!ltl_file.good()) {
    cout << "could not find/open the file \"" << tlsf_path << "\"" << endl;
    exit(1);
  }
  ltl_file.close();

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
  
  // at this point we have ensured that the .ltl file exists,
  // so the .ins file must also exist.
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

// check if a detcount file exists, and has an entry for k.
// if it exists, return the number, otherwise return 0.
unsigned check_detcount_file_contains_k(const string & tlsf_path, const unsigned & k) {
  ifstream detcount_file;

  detcount_file.open(tlsf_path + ".detcount");
  if (!detcount_file.good()) {
    return 0;
  }
  
  stringstream detcount_file_contents;
  detcount_file_contents << detcount_file.rdbuf();

  string tmp;
  while(getline(detcount_file_contents, tmp, '\n')){
    std::string k_str = tmp.substr(0, tmp.find(" "));
    if (strtoul(k_str.c_str(), nullptr, 10) == k) {
      tmp.erase(0, tmp.find(" ") + 1);
      return strtoul(tmp.c_str(), nullptr, 10);
    }
  }
  return 0;
}

void write_to_detcount_file(const string & tlsf_path, const unsigned & k, const unsigned & detcount) {
  ofstream detcount_file;

  detcount_file.open(tlsf_path + ".detcount", std::ios_base::app);
  detcount_file << k << " " << detcount << '\n';
  detcount_file.close();
}

