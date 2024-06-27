#pragma once

#include <boost/program_options.hpp>
#include "common.hh"

namespace po = boost::program_options;
using namespace std;

// global options stored in here
struct global_options {
  string tlsf_path;

  unsigned report = 0;
  unsigned report_time = 0;
  string report_filename = "index.html";
  bool report_append = false;
} opt;

#define IF_PAGE_BASIC   if (opt.report > 0)
#define IF_PAGE_DETAILS if (opt.report > 1)
#define IF_PAGE_TIMER   if (opt.report_time > 0)
#define IF_K_TIMERS     if (opt.report_time > 0)
#define IF_PAGE_ANYTHING if (opt.report > 0 || opt.report_time > 0)

int setup_program_options(int ac, char* av[]) {
  // Declare the supported options.
  po::options_description desc("Allowed options");
  desc.add_options()
      ("help", "produce help message")
      ("input-tlsf,i",     po::value<string>()->required(), "set the path to the tlsf input file")
      ("report,r",         po::value<unsigned>()->default_value(0)->implicit_value(2), "set whether to report process details to a HTML file. 0=nothing, 1=basics, 2=details. Default=0, Implicit=2.")
      ("report-time,rt",   po::value<unsigned>()->default_value(0)->implicit_value(2), "set whether to report timing details to a HTML file. 0=nothing, 1=basics, 2=details. Default=0, Implicit=2.")
      ("report-append,ra", po::value<bool>()->default_value(false)->implicit_value(true), "append the report to an existing report file, instead of creating a new one. Default=False")
  ;


  // parse the inputs
  po::variables_map vars;
  po::store(po::parse_command_line(ac, av, desc), vars);

  // process various specific options
  if (vars.count("help")) {
      cout << desc << "\n";
      return 1;
  }

  try {
    po::notify(vars);
  } catch(exception& e) {
    cerr << "error: " << e.what() << "\n";
    exit(1);
  } catch(...) {
    cerr << "Exception of unknown type!\n";
    exit(1);
  }

  if (vars.count("input-tlsf")) {
      // cout << "Compression level was set to " << vars["compression"].as<int>() << ".\n";
      opt.tlsf_path = vars["input-tlsf"].as<string>();
  } else {
    // we shouldn't get here, but it's here just in case.
    cout << "Must provide an input file. Run with `help` for details";
    exit(1);
  }

  if (vars.count("report")) {
    opt.report = vars["report"].as<unsigned>();
  }

  if (vars.count("report-time")) {
    opt.report_time = vars["report-time"].as<unsigned>();
  }

  if (vars.count("report-append")) {
    opt.report_append = vars["report-append"].as<bool>();
  }

  return 0;
}