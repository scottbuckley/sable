#include <chrono>
#include <iomanip>
#include "common.hh"

using namespace std::chrono;
using namespace std;

#define REPORT_PREFIX "* "

class Stopwatch {
private:
    bool is_running = false;
    steady_clock::time_point start_time = high_resolution_clock::now();
    nanoseconds elapsed = nanoseconds::zero();
    string label = "";
    Stopwatch * parentStopwatch = nullptr;

    void addTime(nanoseconds dur) {
        elapsed += dur;
    }

public:
    Stopwatch(string label) : label{label} {}

    Stopwatch(string label, Stopwatch * parent) : label{label}, parentStopwatch{parent} {}

    // set the parent
    Stopwatch * parent(Stopwatch * p) {
        parentStopwatch = p;
        return this;
    }

    Stopwatch * report() {
        if (is_running) throw runtime_error("trying to report on a stopwatch that's still running");
        cout << REPORT_PREFIX <<  "Stopwatch \"" << label << "\" reports " << ms_duration(elapsed) << endl;
        return this;
    }

    Stopwatch * report(string prefix) {
        if (is_running) throw runtime_error("trying to report on a stopwatch that's still running");
        cout << prefix << ": Stopwatch \"" << label << "\" reports " << ms_duration(elapsed) << endl;
        return this;
    }

    Stopwatch * start() {
        if (is_running) throw runtime_error("trying to start a stopwatch that's already running");
        is_running = true;
        start_time = high_resolution_clock::now();
        return this;
    }

    // void stop_and_report() {
    //     stop(); report();
    // }

    Stopwatch * stop() {
        steady_clock::time_point stop_time = high_resolution_clock::now();
        if (!is_running) throw runtime_error("trying to stop a stopwatch that isn't running");
        is_running = false;
        nanoseconds sub_elapsed = stop_time - start_time;
        addTime(sub_elapsed);
        if (parentStopwatch) parentStopwatch->addTime(sub_elapsed);
        // return sub_elapsed;
        return this;
    }

    Stopwatch * reset() {
        elapsed = nanoseconds();
        is_running = false;
        return this;
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

    static string smart_duration(nanoseconds e) {
        // cout << e.count() << endl;
        long long bigcount = e.count();
        if (bigcount < 1000) return to_string(bigcount) + " ns";
        bigcount /= 1000;
        if (bigcount < 1000) return to_string(bigcount) + " us";
        bigcount /= 1000;
        if (bigcount < 1000) return to_string(bigcount) + " ms";
        bigcount /= 1000;
        return to_string(bigcount) + " sec";
    }

    string smart_duration_string() {
        return smart_duration(elapsed);
    }

    string getLabel() {
        return label;
    }
};

class StopwatchSet {
private:
    std::unordered_map<string, Stopwatch *> timer_map = std::unordered_map<string, Stopwatch *>();

public:
    void addTimer(Stopwatch * timer) {
        timer_map.insert({timer->getLabel(), timer});
    }

    Stopwatch * getTimer(string label) const {
        return timer_map.at(label);
    }

    void report() const {
        for (const auto & [l, t] : timer_map) {
            t->report();
        }
    }

    void draw_page_donut(string total_label = "", vector<string> sub_labels = vector<string>()) {
        auto data = pie_chart_data();
        long long total;
        long long time_measured = 0;
        for (const auto & [l, t] : timer_map) {
            if (l == total_label) {
                total = t->get_nanoseconds();
            } else if (vec_contains(sub_labels, l)) {
                const long long time = t->get_nanoseconds();
                time_measured += time;
                data.push_back({l, time, t->smart_duration_string()});
            }
        }
        if (time_measured <= total) {
            const auto unmeasured = total - time_measured;
            data.push_back({"[unmeasured]", unmeasured, Stopwatch::smart_duration(nanoseconds(unmeasured))});
        } else {
            cout << "UH-OH. Your 'total' time is less than the sum of the sub-times" << endl;
        }
        page_donut(data);
    }
    
};

struct ReportOnReturn {
    Stopwatch * timer;
    bool stop = false;
    ReportOnReturn(Stopwatch * t, bool stop_before_return = false) : timer{t}, stop{stop_before_return} {}

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