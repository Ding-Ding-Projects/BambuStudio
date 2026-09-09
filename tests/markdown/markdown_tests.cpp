#include <catch_main.hpp>

#include "libslic3r/Markdown.hpp"

#include <string>

using namespace Slic3r::Markdown;

// The renderer is the one shared Markdown path for the native app: every case
// here pins output bytes so the docs browser, its tests and the bundle guard
// agree on what an article looks like.

TEST_CASE("HTML escaping covers every markup character", "[Markdown][escape]")
{
    REQUIRE(escape_html("a < b && c > \"d\" 'e'") == "a &lt; b &amp;&amp; c &gt; &quot;d&quot; &#39;e&#39;");
    // Raw HTML in the source never passes through as markup.
    REQUIRE(render_to_html("<script>alert(1)</script>") == "<p>&lt;script&gt;alert(1)&lt;/script&gt;</p>\n");
    REQUIRE(render_to_html("`<b>`") == "<p><code>&lt;b&gt;</code></p>\n");
    REQUIRE(render_to_html("[x](javascript:alert(1)\"onmouseover=\"y)") ==
            "<p><a href=\"javascript:alert(1)&quot;onmouseover=&quot;y\">x</a></p>\n");
    // Entities are documented as unsupported and stay literal.
    REQUIRE(render_to_html("&amp;") == "<p>&amp;amp;</p>\n");
}

TEST_CASE("ATX headings carry stable slug ids", "[Markdown][headings]")
{
    REQUIRE(render_to_html("# Title") == "<h1 id=\"title\">Title</h1>\n");
    REQUIRE(render_to_html("### Sub *em* ###") == "<h3 id=\"sub-em\">Sub <em>em</em></h3>\n");
    REQUIRE(render_to_html("## Command palette (Ctrl+Shift+F)") ==
            "<h2 id=\"command-palette-ctrlshiftf\">Command palette (Ctrl+Shift+F)</h2>\n");
    // Duplicate headings get -1, -2 like GitHub.
    REQUIRE(render_to_html("## A\n\n## A\n") == "<h2 id=\"a\">A</h2>\n<h2 id=\"a-1\">A</h2>\n");
    REQUIRE(render_to_html("####### not a heading") == "<p>####### not a heading</p>\n");
    REQUIRE(render_to_html("#no space") == "<p>#no space</p>\n");
    REQUIRE(heading_slug("Ink terminology (filament \xE2\x86\x92 ink)") == "ink-terminology-filament-\xE2\x86\x92-ink");
}

TEST_CASE("first_heading returns the H1 text", "[Markdown][headings]")
{
    REQUIRE(first_heading("intro\n\n# Real title\n\n## Not this") == "Real title");
    REQUIRE(first_heading("## Only h2").empty());
    REQUIRE(first_heading("# CRLF title\r\n") == "CRLF title");
}

TEST_CASE("Paragraphs, soft and hard breaks", "[Markdown][paragraph]")
{
    REQUIRE(render_to_html("one\ntwo") == "<p>one\ntwo</p>\n");
    REQUIRE(render_to_html("one  \ntwo") == "<p>one<br>\ntwo</p>\n");
    REQUIRE(render_to_html("one\\\ntwo") == "<p>one<br>\ntwo</p>\n");
    REQUIRE(render_to_html("a\n\nb") == "<p>a</p>\n<p>b</p>\n");
    REQUIRE(render_to_html("---") == "<hr>\n");
    REQUIRE(render_to_html("* * *") == "<hr>\n");
}

TEST_CASE("Inline emphasis, code and strikethrough", "[Markdown][inline]")
{
    REQUIRE(render_to_html("*em* and _em_") == "<p><em>em</em> and <em>em</em></p>\n");
    REQUIRE(render_to_html("**strong** and __strong__") == "<p><strong>strong</strong> and <strong>strong</strong></p>\n");
    REQUIRE(render_to_html("***both***") == "<p><em><strong>both</strong></em></p>\n");
    REQUIRE(render_to_html("~~gone~~") == "<p><del>gone</del></p>\n");
    REQUIRE(render_to_html("snake_case_name stays") == "<p>snake_case_name stays</p>\n");
    REQUIRE(render_to_html("2 * 3 * 4") == "<p>2 * 3 * 4</p>\n");
    REQUIRE(render_to_html("`a *b* c`") == "<p><code>a *b* c</code></p>\n");
    REQUIRE(render_to_html("`` a ` b ``") == "<p><code>a ` b</code></p>\n");
    REQUIRE(render_to_html("unclosed `tick") == "<p>unclosed `tick</p>\n");
    REQUIRE(render_to_html("\\*literal\\*") == "<p>*literal*</p>\n");
}

TEST_CASE("Links, images and autolinks", "[Markdown][links]")
{
    REQUIRE(render_to_html("[text](https://x.y/z \"Title\")") ==
            "<p><a href=\"https://x.y/z\" title=\"Title\">text</a></p>\n");
    REQUIRE(render_to_html("[`code` link](../a/b.md)") ==
            "<p><a href=\"../a/b.md\"><code>code</code> link</a></p>\n");
    REQUIRE(render_to_html("![alt *text*](img.png)") == "<p><img src=\"img.png\" alt=\"alt text\"></p>\n");
    REQUIRE(render_to_html("<https://example.com/a?b=1&c=2>") ==
            "<p><a href=\"https://example.com/a?b=1&amp;c=2\">https://example.com/a?b=1&amp;c=2</a></p>\n");
    REQUIRE(render_to_html("<me@example.com>") == "<p><a href=\"mailto:me@example.com\">me@example.com</a></p>\n");
    REQUIRE(render_to_html("[not a link]") == "<p>[not a link]</p>\n");
    REQUIRE(render_to_html("[ref][1]") == "<p>[ref][1]</p>\n");
    REQUIRE(render_to_html("a <b> c") == "<p>a &lt;b&gt; c</p>\n");
    REQUIRE(render_to_html("[nested [brackets]](u)") == "<p><a href=\"u\">nested [brackets]</a></p>\n");
}

TEST_CASE("Link and image hooks rewrite destinations", "[Markdown][links]")
{
    RenderOptions opts;
    opts.resolve_link  = [](const std::string &d) { return d.rfind(".md") != std::string::npos ? "app://" + d : std::string(); };
    opts.resolve_image = [](const std::string &d) { return "file:///res/" + d; };
    REQUIRE(render_to_html("[a](x.md) [b](https://e.com) ![i](p.png)", opts) ==
            "<p><a href=\"app://x.md\">a</a> <a href=\"https://e.com\">b</a> <img src=\"file:///res/p.png\" alt=\"i\"></p>\n");
}

TEST_CASE("Fenced and indented code blocks", "[Markdown][code]")
{
    REQUIRE(render_to_html("```cpp\nint a = 1 < 2;\n```\n") ==
            "<pre><code class=\"language-cpp\">int a = 1 &lt; 2;\n</code></pre>\n");
    REQUIRE(render_to_html("~~~\n# not a heading\n~~~") == "<pre><code># not a heading\n</code></pre>\n");
    REQUIRE(render_to_html("```\nunterminated\n") == "<pre><code>unterminated\n</code></pre>\n");
    REQUIRE(render_to_html("    indented\n    code\n") == "<pre><code>indented\ncode\n</code></pre>\n");
    REQUIRE(render_to_html("```text tail\nx\n```") == "<pre><code class=\"language-text\">x\n</code></pre>\n");
}

TEST_CASE("Lists: unordered, ordered, nested, task", "[Markdown][lists]")
{
    REQUIRE(render_to_html("- a\n- b\n") == "<ul>\n<li>a</li>\n<li>b</li>\n</ul>\n");
    REQUIRE(render_to_html("1. one\n2. two\n") == "<ol>\n<li>one</li>\n<li>two</li>\n</ol>\n");
    REQUIRE(render_to_html("3. three\n4. four\n") == "<ol start=\"3\">\n<li>three</li>\n<li>four</li>\n</ol>\n");
    REQUIRE(render_to_html("- a\n  - b\n  - c\n- d\n") ==
            "<ul>\n<li>a\n<ul>\n<li>b</li>\n<li>c</li>\n</ul></li>\n<li>d</li>\n</ul>\n");
    REQUIRE(render_to_html("- [ ] todo\n- [x] done\n") ==
            "<ul>\n<li><input type=\"checkbox\" disabled> todo</li>\n<li><input type=\"checkbox\" disabled checked> done</li>\n</ul>\n");
    // Lazy continuation and a following paragraph.
    REQUIRE(render_to_html("- a\ncontinued\n\npara") == "<ul>\n<li>a\ncontinued</li>\n</ul>\n<p>para</p>\n");
    // A blank line between items keeps one list.
    REQUIRE(render_to_html("- a\n\n- b\n") == "<ul>\n<li>a</li>\n<li>b</li>\n</ul>\n");
    // Different bullets start a new list.
    REQUIRE(render_to_html("- a\n* b\n") == "<ul>\n<li>a</li>\n</ul>\n<ul>\n<li>b</li>\n</ul>\n");
}

TEST_CASE("Blockquotes", "[Markdown][blockquote]")
{
    REQUIRE(render_to_html("> quoted\n> more") == "<blockquote>\n<p>quoted\nmore</p>\n</blockquote>\n");
    REQUIRE(render_to_html("> # h\n> - i") == "<blockquote>\n<h1 id=\"h\">h</h1>\n<ul>\n<li>i</li>\n</ul>\n</blockquote>\n");
    REQUIRE(render_to_html("> outer\n> > inner") == "<blockquote>\n<p>outer</p>\n<blockquote>\n<p>inner</p>\n</blockquote>\n</blockquote>\n");
}

TEST_CASE("Pipe tables with alignment, escaped pipes and code", "[Markdown][tables]")
{
    const std::string src = "| Name | Value | Note |\n|:-----|------:|:----:|\n| a | 1 | `x|y` |\n| b \\| c | 2 | **z** |\n";
    REQUIRE(render_to_html(src) ==
            "<table>\n<thead>\n<tr>\n<th align=\"left\">Name</th>\n<th align=\"right\">Value</th>\n<th align=\"center\">Note</th>\n</tr>\n</thead>\n"
            "<tbody>\n<tr>\n<td align=\"left\">a</td>\n<td align=\"right\">1</td>\n<td align=\"center\"><code>x|y</code></td>\n</tr>\n"
            "<tr>\n<td align=\"left\">b | c</td>\n<td align=\"right\">2</td>\n<td align=\"center\"><strong>z</strong></td>\n</tr>\n</tbody>\n</table>\n");
    // Short rows are padded, a header without a delimiter row is a paragraph.
    REQUIRE(render_to_html("a | b\n--|--\n1\n") ==
            "<table>\n<thead>\n<tr>\n<th>a</th>\n<th>b</th>\n</tr>\n</thead>\n<tbody>\n<tr>\n<td>1</td>\n<td></td>\n</tr>\n</tbody>\n</table>\n");
    REQUIRE(render_to_html("a | b\nc | d\n") == "<p>a | b\nc | d</p>\n");
}

TEST_CASE("Rendering is deterministic and CRLF-insensitive", "[Markdown][determinism]")
{
    const std::string doc = "# T\r\n\r\nPara *x*\r\n\r\n- a\r\n- b\r\n";
    REQUIRE(render_to_html(doc) == render_to_html(doc));
    REQUIRE(render_to_html(doc) == render_to_html("# T\n\nPara *x*\n\n- a\n- b\n"));
}

TEST_CASE("plain_text strips markup for search indexing", "[Markdown][plain]")
{
    REQUIRE(plain_text("# Title\n\nSome **bold** and `code` with [a link](x.md).") ==
            "Title Some bold and code with a link .");
    REQUIRE(plain_text("a &lt; b") == "a &lt; b"); // literal entity text preserved
}
