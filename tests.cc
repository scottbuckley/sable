#pragma once
#include "bdd.cc"


void ebsc_test() {
  const ap_info apinfo = make_fake_apinfo();
  // std::srand(std::time(nullptr));


  auto eBSC_eq_BDD = [&](eBSC ebsc, const bdd & b) {
    if (b == bdd_true())  return ebsc.check_taut();
    if (b == bdd_false()) return ebsc.is_false();
    return !!(ebsc.to_bdd(apinfo) == b);
  };

  auto assert_same_eBSC_bdd_BSDD = [&](eBSC ebsc, const bdd & b, const BSDD & bsdd) {
    if (b == bdd_true() || bsdd.is_true()) {
      if (!(b == bdd_true() && bsdd.is_true())) return false;
      return ebsc.check_taut();
    }
    bdd bsdd_as_bdd = bsdd.to_eBSC().to_bdd(apinfo);
    if (bsdd_as_bdd != b) return false;
    return !!(ebsc.to_bdd(apinfo) == b);
  };

  auto BSC_eq_BDD = [&](const BSC & bsc, const bdd & b) {
    if (b == bdd_true())  return bsc.is_true();
    if (b == bdd_false()) return bsc.is_false();
    return !!(bsc.to_bdd(apinfo) == b);
  };

  auto assert_disj_same = [&](const eBSC & a, const eBSC & b){
    bdd a_ = a.to_bdd(apinfo);
    bdd b_ = b.to_bdd(apinfo);
    eBSC aorb = a | b;
    bdd a_orb_ = a_ | b_;

    if (!eBSC_eq_BDD(aorb, a_orb_)) {
      cout << "a: " << a.s(apinfo) << endl;
      cout << "b: " << b.s(apinfo) << endl;
      cout << "a_: " << apinfo.bdd_to_string(a_) << endl;
      cout << "b_: " << apinfo.bdd_to_string(b_) << endl;
      cout << "a | b: " << (a | b).s(apinfo) << endl;
      // cout << "a | b (as bdd): " << apinfo.bdd_to_string((a | b).to_bdd(apinfo)) << endl;
      cout << "a_ | b_: " << apinfo.bdd_to_string(a_ | b_) << endl;

      cout << " *** FAILED *** " << endl;
      exit(1);
    } else {
      // cout << "test passed" << endl;
    }
  };

  auto conj_timers = StopwatchSet();
  auto conj_total = conj_timers.make_timer("Cumul BSC/BDD Total");
  conj_total->flag_total();
  auto conj_bdd_timer = conj_timers.make_timer("BDD Operations");
  auto conj_bsdd_timer = conj_timers.make_timer("BSDD Operations");
  auto conj_bsc_timer = conj_timers.make_timer("BSC Operations");

  auto assert_conj_same = [&](const eBSC & a, const eBSC & b){
    bdd a_ = a.to_bdd(apinfo);
    bdd b_ = b.to_bdd(apinfo);

    BSDD a__ = BSDD::from_eBSC(a);
    BSDD b__ = BSDD::from_eBSC(b);

    conj_bsc_timer->start();
      eBSC aorb = a & b;
    conj_bsc_timer->stop();

    // conj_bsdd_timer->start();
    //   BSDD a__orb__ = a__ & b__;
    // conj_bsdd_timer->stop();

    conj_bdd_timer->start();
      bdd a_orb_ = a_ & b_;
    conj_bdd_timer->stop();

    if (!eBSC_eq_BDD(aorb, a_orb_)) {
      cout << "a: " << a.s(apinfo) << endl;
      cout << "b: " << b.s(apinfo) << endl;
      cout << "a_: " << apinfo.bdd_to_string(a_) << endl;
      cout << "b_: " << apinfo.bdd_to_string(b_) << endl;
      cout << "a & b: " << (a & b).s(apinfo) << endl;
      // cout << "a & b (as bdd): " << apinfo.bdd_to_string((a & b).to_bdd(apinfo)) << endl;
      cout << "a_ & b_: " << apinfo.bdd_to_string(a_ & b_) << endl;

      cout << " *** FAILED *** " << endl;
      // exit(1);
      throw std::runtime_error("failed");
    } else {
      // cout << "test passed" << endl;
    }
  };

  auto simple_conj_timers = StopwatchSet();
  auto simple_conj_total = simple_conj_timers.make_timer("Cumul BSC/BDD Total");
  simple_conj_total->flag_total();
  auto conj_simple_bdd_timer = simple_conj_timers.make_timer("Simple BDD Operations");
  auto conj_simple_bsc_timer = simple_conj_timers.make_timer("Simple BSC Operations");

  auto assert_single_conj_same = [&](const BSC & a, const BSC & b){
    a.check_validity();
    b.check_validity();
    bdd a_ = a.to_bdd(apinfo);
    bdd b_ = b.to_bdd(apinfo);
    conj_simple_bsc_timer->start();
      BSC aandb = a & b;
    conj_simple_bsc_timer->stop();

    conj_simple_bdd_timer->start();
      bdd a_andb_ = a_ & b_;
    conj_simple_bdd_timer->stop();

    if (!BSC_eq_BDD(aandb, a_andb_)) {
      cout << "a: " << a.s(apinfo) << endl;
      cout << "a.is_false(): " << a.is_false() << endl;
      cout << "b: " << b.s(apinfo) << endl;
      cout << "a_: " << apinfo.bdd_to_string(a_) << endl;
      cout << "b_: " << apinfo.bdd_to_string(b_) << endl;
      cout << "a & b: " << (a & b).s(apinfo) << endl;
      cout << "(a & b).is_false(): " << (a & b).is_false() << endl;
      // cout << "a & b (as bdd): " << apinfo.bdd_to_string((a & b).to_bdd(apinfo)) << endl;
      cout << "a_ & b_: " << apinfo.bdd_to_string(a_ & b_) << endl;

      cout << " *** FAILED *** " << endl;
      exit(1);
    } else {
      // cout << "test passed" << endl;
    }
  };

  auto assert_single_disj_same = [&](const BSC & a, const BSC & b){
    bdd a_ = a.to_bdd(apinfo);
    bdd b_ = b.to_bdd(apinfo);
    eBSC aandb = a | b;
    bdd a_andb_ = a_ | b_;

    bdd aandb_bdd = aandb.to_bdd(apinfo);
    if (!eBSC_eq_BDD(aandb, a_andb_)) {
      cout << "a: " << a.s(apinfo) << endl;
      cout << "b: " << b.s(apinfo) << endl;
      cout << "a_: " << apinfo.bdd_to_string(a_) << endl;
      cout << "b_: " << apinfo.bdd_to_string(b_) << endl;
      cout << "a | b: " << (aandb).s(apinfo) << endl;
      cout << "a | b (as bdd): " << apinfo.bdd_to_string(aandb_bdd) << endl;
      cout << "a_ | b_: " << apinfo.bdd_to_string(a_andb_) << endl;
      // cout << "a_ zzz b_: " << apinfo.bdd_to_string(bdd_xor(a_, b_)) << endl;

      cout << " *** FAILED *** " << endl;
      exit(1);
    } else {
      // cout << "test passed" << endl;
    }
  };

  auto timers = StopwatchSet();
  auto cumul_total = timers.make_timer("Cumul BSC/BDD Total");
  cumul_total->flag_total();
  // auto convert_timer = timers.make_timer("BSC/BDD Conversion");
  auto bdd_timer = timers.make_timer("BDD Operations");
  auto bsc_timer = timers.make_timer("BSC Operations");
  auto bsdd_timer = timers.make_timer("BSDD Operations");
  auto bsc_taut_timer = timers.make_timer("BSC Truth Checks");
  page_start();
  page_heading("eBSC Tests Debug");

  auto cumulative_disj_test = [&](){
    // cout << " --- cumul --- " << endl;

    eBSC cumul  = eBSC::False();
    // convert_timer->start();
      auto cumul_ = cumul.to_bdd(apinfo);
    // convert_timer->stop();
    BSDD cumul_bsdd = BSDD(false);

    unsigned index = 0;
    while(true) {
      index++;

      BSC bsc = random_BSC();

      // convert_timer->start();
        
      // convert_timer->stop();

      auto prev_cumul = cumul;
      auto prev_cumul_ = cumul_;

      // cout << cumul.s() << " | " << bsc.s() << " = ..." << endl;
      // cout << "                  in bdd = " << apinfo.bdd_to_string(cumul_) << endl;

      bsc_timer->start();
        cumul  |= bsc;
      bsc_timer->stop();
      bsc_taut_timer->start();
        cumul.check_taut();
      bsc_taut_timer->stop();

      bdd_timer->start();
        auto bsc_ = bsc.to_bdd(apinfo);
        cumul_ |= bsc_;
      bdd_timer->stop();

      bsdd_timer->start();
        cumul_bsdd |= bsc;
      bsdd_timer->stop();

      // cout << endl << endl << endl << endl << endl;
      // cout << " ---- new iteration ----" << endl;
      // cout << "new cond:   " << bsc.s() << endl << endl;
      // cout << "bdd val: " << apinfo.bdd_to_string(cumul_) << endl << endl;
      // cout << "bsdd dump:" << endl;
      // cumul_bsdd.dump_debug();

      if (!assert_same_eBSC_bdd_BSDD(cumul, cumul_, cumul_bsdd)) {
        cout << "prev cumul: " << prev_cumul.s() << endl;
        cout << "new cond:   " << bsc.s() << endl;
        cout << "new cumul:  " << cumul.s() << endl;
        cout << endl;
        cout << "prev cumul bdd: " << apinfo.bdd_to_string(prev_cumul_) << endl;
        cout << "new cumul bdd:  " << apinfo.bdd_to_string(cumul_) << endl;
        cout << endl;
        cout << "new cumul bscc:  " << cumul_bsdd.to_string() << endl;
        cumul_bsdd.dump_debug();
        cout << endl;
        cout << "check_taut: " << cumul.check_taut() << endl;
        throw std::runtime_error("we failed :(");
      }

      if (cumul_ == bdd_true()) break;

    }
    
  };

  // std::srand(std::time(nullptr));
  // print_vector(apinfo.dict->var_map);

  std::srand(123);
  constexpr const unsigned num_tests = 50000;
  const unsigned progress_interval = 10000000;

  cout << "cumulative disj tests ..." << endl;
  cumul_total->start();
  for (unsigned i=1; i<=num_tests; ++i) {
    cumulative_disj_test();
    if (i % progress_interval == 0) cout << i << endl;
  }
  cumul_total->stop();
  timers.draw_page_donut("cumul");
  cout << "cumulative disj tests passed." << endl << endl;
  
  cout << "single conj tests ..." << endl;
  simple_conj_total->start();
  for (unsigned i=1; i<=num_tests; ++i) {
    assert_single_conj_same(random_BSC(), random_BSC());
    if (i % progress_interval == 0) cout << i << endl;
  }
  simple_conj_total->stop();
  simple_conj_timers.draw_page_donut("single conj");
  cout << "single conj tests passed." << endl << endl;

  cout << "single disj tests ..." << endl;
  for (unsigned i=1; i<=num_tests; ++i) {
    assert_single_disj_same(random_BSC(), random_BSC());
    if (i % progress_interval == 0) cout << i << endl;
  }
  cout << "single disj tests passed." << endl << endl;

  cout << "complex conj tests ..." << endl;
  conj_total->start();
  for (unsigned i=1; i<=num_tests; ++i) {
    assert_conj_same(random_eBSC(), vrandom_eBSC());
    if (i % progress_interval == 0) cout << i << endl;
  }
  conj_total->stop();
  conj_timers.draw_page_donut("compound conj");
  cout << "complex conj tests passed." << endl << endl;


  cout << "complex disj tests ..." << endl;
  for (unsigned i=1; i<=num_tests; ++i) {
    assert_disj_same(random_eBSC(), random_eBSC());
    if (i % progress_interval == 0) cout << i << endl;
  }
  cout << "complex disj tests passed." << endl << endl;
  
  cout << "passed " << num_tests << " tests." << endl;

  page_finish();
  
}