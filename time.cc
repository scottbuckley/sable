#pragma once

#include <chrono>
#include <iomanip>
#include "common.hh"

using namespace std::chrono;
using namespace std;

#define REPORT_PREFIX "* "

#define TIMER_GRAPH_COMPONENT 1
#define TIMER_GRAPH_TOTAL 2
#define TIMER_GRAPH_IGNORED 4

class Stopwatch {
private:
  bool is_running = false;
  steady_clock::time_point start_time = steady_clock::now();
  nanoseconds elapsed = nanoseconds::zero();
  string label = "";
  unsigned graph_flag = TIMER_GRAPH_COMPONENT;

  shared_ptr<Stopwatch> addition_parent = nullptr;
  shared_ptr<Stopwatch> subsumption_parent = nullptr;

  void addTime(nanoseconds dur) {
    elapsed += dur;
  }

public:
  Stopwatch(string label, shared_ptr<Stopwatch> parent = nullptr, unsigned flag = TIMER_GRAPH_COMPONENT)
    : label{label}, addition_parent{parent}, graph_flag{flag} {}
  
  void set_subsumption_parent(shared_ptr<Stopwatch> sp) {
    subsumption_parent = sp;
  }

  // set the parent
  Stopwatch * parent(shared_ptr<Stopwatch> p) {
    addition_parent = p;
    return this;
  }

  Stopwatch * report(string prefix = REPORT_PREFIX) {
    if (is_running) throw runtime_error("trying to report on a stopwatch that's still running");
    cout << prefix <<  "Stopwatch \"" << label << "\" reports " << ms_duration(elapsed) << endl;
    return this;
  }

  Stopwatch * start() {
    if (is_running) throw runtime_error("trying to start a stopwatch that's already running");
    is_running = true;
    start_time = steady_clock::now();
    if (subsumption_parent) subsumption_parent->stop();
    return this;
  }

  // void stop_and_report() {
  //   stop(); report();
  // }

  Stopwatch * stop() {
    steady_clock::time_point stop_time = steady_clock::now();
    if (!is_running) throw runtime_error("trying to stop a stopwatch that isn't running");
    is_running = false;
    nanoseconds sub_elapsed = stop_time - start_time;
    addTime(sub_elapsed);
    if (addition_parent) addition_parent->addTime(sub_elapsed);
    if (subsumption_parent) subsumption_parent->start();
    // return sub_elapsed;
    return this;
  }

  Stopwatch * reset() {
    elapsed = nanoseconds();
    is_running = false;
    return this;
  }

  void time_block(function <void()> f) {
    start(); f(); stop();
  }

  microseconds get_micros() {
    return duration_cast<microseconds>(elapsed);
  }

  long long get_nanoseconds() {
    return elapsed.count();
  }

  static string ms_duration(nanoseconds e) {
    stringstream out;
    auto us = (duration_cast<microseconds>(e)).count() / 1000.0;
    out << std::fixed << std::setprecision(2) << us;
    return out.str() + " ms";
  }

  static string ns_duration(nanoseconds e) {
    auto ns = (duration_cast<microseconds>(e)).count();
    return to_string(ns);
  }

  static string smart_duration(long long bigcount) {
    if (bigcount < 1000) return to_string(bigcount) + " ns";
    bigcount /= 1000;
    if (bigcount < 1000) return to_string(bigcount) + " us";
    bigcount /= 1000;
    if (bigcount < 1000) return to_string(bigcount) + " ms";
    bigcount /= 1000;
    return to_string(bigcount) + " sec";
  }

  static string smart_duration(nanoseconds e) {
    return smart_duration(e.count());
  }

  string smart_duration_string() {
    return smart_duration(elapsed);
  }

  string get_label() {
    return label;
  }

  void flag_total()   { graph_flag = TIMER_GRAPH_TOTAL; }
  void flag_ignored() { graph_flag = TIMER_GRAPH_IGNORED; }
  unsigned get_flag() { return graph_flag; }
};



class StopwatchSet {
private:
  std::unordered_map<string, shared_ptr<Stopwatch>> timer_map = std::unordered_map<string, shared_ptr<Stopwatch>>();
  std::unordered_map<string, bool> timer_flags = std::unordered_map<string, bool>();
public:
  void add_timer(shared_ptr<Stopwatch> timer) {
    timer_map.insert({timer->get_label(), timer});
  }

  // make a timer and add it to the set
  template<class ..._Args>
  shared_ptr<Stopwatch> make_timer(_Args&& ...__args) {
    shared_ptr<Stopwatch> new_timer = make_shared<Stopwatch>(__args...);
    timer_map.insert({new_timer->get_label(), new_timer});
    return new_timer;
  }

  shared_ptr<Stopwatch> getTimer(string label) const {
    return timer_map.at(label);
  }

  void report(string prefix = REPORT_PREFIX) const {
    for (const auto & [l, t] : timer_map) {
      t->report(prefix);
    }
  }

  void draw_page_donut(string heading = "") {
    auto data = pie_chart_data();
    long long total;
    long long time_measured = 0;
    for (const auto & [lbl, timer] : timer_map) {
      if (timer->get_flag() == TIMER_GRAPH_TOTAL) {
        total = timer->get_nanoseconds();
      } else if (timer->get_flag() == TIMER_GRAPH_COMPONENT) {
        const long long time = timer->get_nanoseconds();
        time_measured += time;
        data.push_back({lbl, time, timer->smart_duration_string()});
      }
    }
    if (time_measured <= total) {
      const auto unmeasured = total - time_measured;
      data.push_back({"[unmeasured]", unmeasured, Stopwatch::smart_duration(nanoseconds(unmeasured))});
    } else {
      cout << "UH-OH. Your 'total' time is less than the sum of the sub-times" << endl;
    }
    page_donut(data, heading);
  }

  // void draw_page_donut2(string label) {
  //   auto data = pie_chart_data();
  //   long long total;
  //   long long time_measured = 0;
  //   for (const auto & [l, t] : timer_map) {
  //     if (l == total_label) {
  //       total = t->get_nanoseconds();
  //     } else if (vec_contains(sub_labels, l)) {
  //       const long long time = t->get_nanoseconds();
  //       time_measured += time;
  //       data.push_back({l, time, t->smart_duration_string()});
  //     }
  //   }
  //   if (time_measured <= total) {
  //     const auto unmeasured = total - time_measured;
  //     data.push_back({"[unmeasured]", unmeasured, Stopwatch::smart_duration(nanoseconds(unmeasured))});
  //   } else {
  //     cout << "UH-OH. Your 'total' time is less than the sum of the sub-times" << endl;
  //   }
  //   page_donut(data);
  // }
  
};

struct TimeUntilReturn {
  Stopwatch * timer;
  TimeUntilReturn(Stopwatch * t, bool draw_graph = false) : timer{t} {
    timer->start();
  }

  ~TimeUntilReturn() {
    timer->stop();
  }
};

struct RunOnReturn {
  function<void()> f;
  RunOnReturn(function<void()> f) : f{f} {}
  ~RunOnReturn() { f(); }
};

struct ReportOnReturn {
  Stopwatch * timer;
  bool stop = false;
  ReportOnReturn(Stopwatch * t, bool stop_before_report = false) : timer{t}, stop{stop_before_report} {}

  ~ReportOnReturn() {
    if (stop) timer->stop();
    timer->report();
  }
};

struct StopOnReturn {
  Stopwatch * timer;
  StopOnReturn(Stopwatch * t) : timer{t} {}
  ~StopOnReturn() { timer->stop(); }
};