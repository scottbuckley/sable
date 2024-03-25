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



string generate_graph_dot(twa_graph_ptr g) {
    stringstream out;
    print_dot(out, g, "arf(Lato)C(#ffffaa)b");
    return out.str();
}

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
    page << "<script src=\"viz-standalone.js\"></script>" << endl;
    page << "<link rel=\"preconnect\" href=\"https://fonts.googleapis.com\">"
            "<link rel=\"preconnect\" href=\"https://fonts.gstatic.com\" crossorigin>"
            "<link href=\"https://fonts.googleapis.com/css2?family=Lato:ital,wght@0,100;0,300;0,400;0,700;0,900;1,100;1,300;1,400;1,700;1,900&display=swap\" rel=\"stylesheet\">" << endl;
    page << "</head><body>" << endl;

    page << "<script>"
        "var vizInst = Viz.instance();"
        "function render(id, dot) {"
            "vizInst.then(function(viz){"
                "document.getElementById(id).appendChild(viz.renderSVGElement(dot));"
            "});"
        "};"
    "</script>";

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
    string dot = generate_graph_dot(g);
    page << "<div>";
    page << "<p><b>" << description << ":</b><br>";
    page << "<span class=\"graphviz-graph\" id=\"" << name << "\"></span>" << endl;
    page << "<script>"
            "render('" + name + "', `" + dot + "`);"
            "</script>";
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