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

void debug(string msg) {
    cout << msg << '\n';
}