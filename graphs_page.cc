#pragma once

#include <spot/tl/parse.hh>
#include <spot/tl/print.hh>
#include <spot/twaalgos/translate.hh>
#include <spot/twaalgos/neverclaim.hh>
#include <spot/twaalgos/hoa.hh>
#include <spot/twaalgos/word.hh>
#include <spot/twaalgos/dot.hh>

#include "common.hh"
#include "json.hpp"

using namespace std;
using namespace spot;

string generate_graph_dot(twa_graph_ptr g) {
  stringstream out;
  print_dot(out, g, "arf(Lato)C(#ffffaa)b");
  return out.str();
}

ofstream page;

void write_page_header() {
  page << "<html><head>" << endl;
  page << "<link rel=\"stylesheet\" href=\"style.css\">" << endl;
  page << "<script src=\"include/viz-standalone.js\"></script>" << endl;
  page << "<link rel=\"preconnect\" href=\"https://fonts.googleapis.com\">" << endl;
  page << "<link rel=\"preconnect\" href=\"https://fonts.gstatic.com\" crossorigin>" << endl;
  page << "<link href=\"https://fonts.googleapis.com/css2?family=Lato:ital,wght@0,100;0,300;0,400;0,700;0,900;1,100;1,300;1,400;1,700;1,900&display=swap\" rel=\"stylesheet\">" << endl;
  page << "<script src=\"include/d3.v4.js\"></script>" << endl;
  page << "<script src=\"include/d3-scale-chromatic.v1.min.js\"></script>" << endl;
  page << "<script src=\"include/jquery.3.7.1.min.js\"></script>" << endl;
  page << "</head><body>" << endl;
  page << "<script src=\"render.js\"></script>";
}

void page_start(string heading = "") {
  const string full_report_path = "page/" + opt.report_filename;

  ifstream page_test;
  page_test.open(full_report_path);
  if (page_test.good() && opt.report_append) {
    // the file already exists and we're in append mode.
    cout << "appending to existing html file" << endl;
    page.open(full_report_path, std::ofstream::app);
  } else {
    // we are starting a new file, possibly overwriting what was already there.
    page.open(full_report_path);
    write_page_header();
  }
  
  if (heading.length() > 0)
    page << "<h1>" << heading << "</h1>" << endl;
}

void page_finish() {
  if (!opt.report_append)
    page << "</body></html>" << endl;
  page.close();
}

unsigned nameindex = 1;
void page_graph(twa_graph_ptr g, string description) {
  unsigned state_count = g->num_states();
  unsigned edge_count  = g->num_edges();
  string name = "graph_" + to_string(nameindex++);
  string dot = generate_graph_dot(g);
  
  page << "<div>";
  page << "<p><b>" << description << ":</b><br>";
  page << "<span class=\"graphviz-graph\" id=\"" << name << "\"></span>" << endl;
  page << "<script>" << endl;
  page << "maybe_render_graph('" << name << "', `" << dot << "`, " << state_count << ", " << edge_count << ");" << endl;
  page << "</script>" << endl;
  page << "</p>";
  page << "</div>" << endl;
}

typedef std::vector<tuple<string, long long, string>> pie_chart_data;
void page_donut(pie_chart_data values, string heading = "") {
  nlohmann::json j;
  for (const auto & [lbl, val, val_text] : values) {
    j.push_back({lbl, val, val_text});
  }

  string index_str = to_string(nameindex++);
  string name = "donut_" + index_str;
  // string dot = generate_graph_dot(g);
  page << "<div>";
  if (heading.length()>0) page << "<p><b>" << heading << ":</b><br>";
  page << "<span class=\"donut-thing\" id=\"" << name << "\"></span>" << endl;
  page << "<script>" << endl
     << "let values_" << index_str << " = " << j << ";"  << endl
     << "render_donut('" + name + "', values_" + index_str + ");"  << endl
     << "</script>";
  page << "</p>";
  page << "</div>" << endl;
}

void page_text(string text, string label="") {
  page << "<p>";
  if (label.length()>0)
    page << "<b>" << label << ": </b>";
  page << text;
  page << "</p>" << endl;
}

void page_heading(string text) {
  page << "<h2>" << text << "</h2>" << endl;
}

void page_text_bool(bool which, string text_true, string text_false, string label="") {
  if (which) {
    page_text(text_true, label);
  } else {
    page_text(text_false, label);
  }
}

void page_code(string description, string code) {
  page << "<div class='codeblock'>";
  page << "<p><b>" << description << "</b></p>";
  page << "<pre>" << "<code>";
  page << code;
  page << "</code>" << "</pre>";
  page << "</div>";
}