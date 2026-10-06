#include "../../src/slic3r/GUI/Documentation/DocumentationBundle.hpp"
#include <cassert>
#include <set>
#include <iostream>
int main() {
    using namespace Slic3r::GUI::Documentation;
    std::set<std::string> routes;
    for (const auto& article : articles) {
        assert(!article.title.empty());
        assert(routes.insert(article.route).second);
        assert(article.route.find("..") == std::string::npos);
        assert(article.html.find("<script") == std::string::npos);
        assert(article.html.find("src=\"https:") == std::string::npos);
    }
    for (const auto& image : images) {
        if (image.name.empty()) continue;
        assert(image.name.size() > 64);
        assert(!image.base64.empty());
    }
    assert(routes.count("windows/README.md"));
    std::cout << routes.size() << " compiled article records verified\n";
}
