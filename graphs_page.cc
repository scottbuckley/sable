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




void generate_graph_image(twa_graph_ptr g, bool render_via_cmd, string name = "graph") {
    ofstream outdata;
    outdata.open(name + ".dot");
    print_dot(outdata, g, "arf(Lato)C(#ffffaa)b");
    outdata.close();
    if (render_via_cmd) {
        std::string command = "dot -Tpng " + name + ".dot -o " + name + ".png";
        system(command.c_str());
    }
}


ofstream page;

void page_start(string heading = "") {
    page.open("page/index.html");
    page << "<html><head>" << endl;
    page << "<link rel=\"stylesheet\" href=\"style.css\">" << endl;
    page << "</head><body>" << endl;
    if (heading.length() > 0)
        page << "<h1>" << heading << "</h1>" << endl;
}

void page_finish() {
    page << "</body></html>" << endl;
    page.close();
}

unsigned nameindex = 1;
void page_graph(twa_graph_ptr g, string description) {\
    string name = "graph_" + to_string(nameindex++);
    generate_graph_image(g, true, "page/"+name);
    page << "<div>";
    page << "<p><b>" << description << ":</b><br>";
    page << "<img src=\"" << name << ".png\">";
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