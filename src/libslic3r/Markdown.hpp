#ifndef slic3r_Markdown_hpp_
#define slic3r_Markdown_hpp_

#include <functional>
#include <string>

// Small, deterministic, dependency-free Markdown -> HTML renderer used by the
// in-app documentation browser (src/slic3r/GUI/DocsBrowserDialog) and by the
// docs bundle tests. It is the ONE renderer every Markdown surface in the
// native app goes through; add features here rather than growing a second
// renderer beside a widget.
//
// Supported (CommonMark subset, GitHub-flavoured where noted):
//   * ATX headings `#`..`######` (optional closing hashes), each with a
//     stable slug `id` so `#fragment` links resolve;
//   * paragraphs, hard line breaks (two trailing spaces or a trailing `\`);
//   * thematic breaks (`---`, `***`, `___`);
//   * fenced code blocks (``` or ~~~, optional info string -> `class="language-x"`);
//   * indented code blocks (4 spaces / tab) outside lists;
//   * unordered (`-`, `*`, `+`) and ordered (`1.`, `1)`) lists, nested by
//     indentation, with GitHub task-list checkboxes (`- [ ]`, `- [x]`);
//   * blockquotes (`>`), nestable;
//   * GitHub pipe tables with an alignment row (`:--`, `:-:`, `--:`);
//   * inline: code spans, `*em*`/`_em_`, `**strong**`/`__strong__`,
//     `~~strikethrough~~`, links `[text](url "title")`, images
//     `![alt](src "title")`, autolinks `<https://...>`, backslash escapes;
//   * raw HTML is NOT passed through: every `<`, `>`, `&`, `"` in source text
//     is escaped, which makes the output safe to hand to a WebView.
//
// Documented as unsupported (rendered as literal text, never as markup):
//   * setext (underlined) headings, reference-style links and images,
//     footnotes, definition lists, HTML blocks and inline HTML, entity
//     references (`&amp;` is shown as the literal text `&amp;`), link
//     reference definitions, lazy continuation lines inside blockquotes,
//     and CommonMark's exact emphasis delimiter-run rules (a simple
//     left/right-flanking check is used instead).
//
// Output is deterministic: identical input and options produce identical
// bytes, so snapshot tests are stable.
namespace Slic3r::Markdown {

struct RenderOptions
{
    // Optional rewrite hooks. Each receives the raw destination as written in
    // the source (already unescaped) and returns the attribute value to emit.
    // A hook returning an empty string keeps the original destination.
    std::function<std::string(const std::string &)> resolve_link;
    std::function<std::string(const std::string &)> resolve_image;
};

// Escape `&`, `<`, `>`, `"` and `'` for safe insertion into HTML text or
// attribute values.
std::string escape_html(const std::string &text);

// Render the Markdown document to an HTML fragment (no <html>/<body>).
std::string render_to_html(const std::string &markdown, const RenderOptions &options = {});

// Title of a document: the text of its first `# ` heading, or "" when none.
std::string first_heading(const std::string &markdown);

// Heading text -> anchor slug, GitHub style (lower-case, punctuation removed,
// spaces to hyphens). Exposed so tests and link builders agree on anchors.
std::string heading_slug(const std::string &heading_text);

// Plain text of the document with markup stripped (for search indexing).
std::string plain_text(const std::string &markdown);

} // namespace Slic3r::Markdown

#endif // slic3r_Markdown_hpp_
