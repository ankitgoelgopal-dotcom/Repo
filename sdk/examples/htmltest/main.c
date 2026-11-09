/* Lexbor (HTML5 parser, DOM, CSS parser, selectors, style cascade, WHATWG URL) on QAR-OS.
 * Expected strings follow the HTML Standard / CSS Syntax / URL Standard. The same file is also built on the
 * host (tests/lexbor_host.sh) so guest and host output can be compared. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lexbor/html/html.h>
#include <lexbor/css/css.h>
#include <lexbor/selectors/selectors.h>
#include <lexbor/style/style.h>
#include <lexbor/url/url.h>

static int pass, fail;
static void expect_str(const char *name, const char *got, const char *want) {
    if (!strcmp(got, want)) pass++; else { fail++; printf("HTML FAIL %s\n  got : %s\n  want: %s\n", name, got, want); }
}
static void expect_int(const char *name, long got, long want) {
    if (got == want) pass++; else { fail++; printf("HTML FAIL %s: got %ld want %ld\n", name, got, want); }
}

struct buf { char s[8192]; size_t n; };
static lxb_status_t cb(const lxb_char_t *d, size_t n, void *ctx) {
    struct buf *b = ctx; if (b->n + n >= sizeof b->s) n = sizeof b->s - 1 - b->n;
    memcpy(b->s + b->n, d, n); b->n += n; b->s[b->n] = 0; return LXB_STATUS_OK;
}
static lxb_html_document_t *parse(const char *html) {
    lxb_html_document_t *doc = lxb_html_document_create();
    if (!doc || lxb_html_document_parse(doc, (const lxb_char_t *)html, strlen(html)) != LXB_STATUS_OK) return NULL;
    return doc;
}
static lxb_status_t count_cb(lxb_dom_node_t *n, lxb_css_selector_specificity_t s, void *ctx) { (void)n; (void)s; (*(long *)ctx)++; return LXB_STATUS_OK; }
static long count_matches(lxb_html_document_t *doc, const char *sel) {
    long count = 0; lxb_css_parser_t *p = lxb_css_parser_create(); lxb_css_parser_init(p, NULL);
    lxb_selectors_t *s = lxb_selectors_create(); lxb_selectors_init(s);
    lxb_css_selector_list_t *list = lxb_css_selectors_parse(p, (const lxb_char_t *)sel, strlen(sel));
    if (p->status != LXB_STATUS_OK) count = -1;
    else lxb_selectors_find(s, lxb_dom_interface_node(doc), list, count_cb, &count);
    lxb_selectors_destroy(s, true); lxb_css_parser_destroy(p, true);
    return count;
}
static void serialize_doc(lxb_html_document_t *doc, struct buf *b) { b->n = 0; b->s[0] = 0; lxb_html_serialize_tree_cb(lxb_dom_interface_node(doc), cb, b); }

int main(void) {
    struct buf b;
    /* 1. HTML5 tree construction repairs broken markup exactly as the spec says */
    lxb_html_document_t *d = parse("<p>one<p>two<table><td>x");
    if (!d) { puts("HTML FAIL parse"); return 1; }
    serialize_doc(d, &b);
    /* no doctype = quirks mode: a <table> does not close an open <p> (HTML Standard, "in body" insertion mode) */
    expect_str("implied tags + tbody/tr (quirks mode)", b.s, "<html><head></head><body><p>one</p><p>two<table><tbody><tr><td>x</td></tr></tbody></table></p></body></html>");
    lxb_html_document_destroy(d);
    d = parse("<!DOCTYPE html><p>one<p>two<table><td>x");
    serialize_doc(d, &b);
    expect_str("implied tags + tbody/tr (standards mode)", b.s, "<!DOCTYPE html><html><head></head><body><p>one</p><p>two</p><table><tbody><tr><td>x</td></tr></tbody></table></body></html>");
    lxb_html_document_destroy(d);

    d = parse("<b><i>x</b>y</i><ul><li>a<li>b</ul>");
    serialize_doc(d, &b);
    expect_str("adoption agency algorithm", b.s, "<html><head></head><body><b><i>x</i></b><i>y</i><ul><li>a</li><li>b</li></ul></body></html>");
    lxb_html_document_destroy(d);

    /* 2. character references, title whitespace, scripts are kept as text */
    d = parse("<title>  Hello \n  World </title><p>a &amp; b &copy; &#x1F600; &notanentity; &zzzz; &lt;tag&gt;</p>");
    size_t tl = 0; const lxb_char_t *title = lxb_html_document_title(d, &tl);
    expect_str("title whitespace collapsed", title ? (const char *)title : "(null)", "Hello World");
    size_t len = 0; lxb_char_t *txt = lxb_dom_node_text_content(lxb_dom_interface_node(lxb_html_document_body_element(d)), &len);
    expect_str("entities decoded", txt ? (const char *)txt : "(null)", "a & b \xC2\xA9 \xF0\x9F\x98\x80 \xC2\xAC" "anentity; &zzzz; <tag>");
    lxb_dom_document_destroy_text(lxb_dom_interface_document(d), txt);
    lxb_html_document_destroy(d);

    /* 3. CSS selectors (level 4): combinators, attribute matching, :nth-child, :not, :has, :is */
    d = parse("<div id=a class='box big'><p class=x>1</p><p id=y>2</p><ul><li>a</li><li class=on>b</li><li>c</li><li>d</li><li>e</li></ul>"
              "<a href='https://example.com/x.pdf' rel=nofollow>l</a><a href='/rel'>m</a><span lang='en-GB'>s</span></div>");
    expect_int("selector .x", count_matches(d, ".x"), 1);
    expect_int("selector div > p", count_matches(d, "div > p"), 2);
    expect_int("selector li:nth-child(2n+1)", count_matches(d, "li:nth-child(2n+1)"), 3);
    expect_int("selector li:not(.on)", count_matches(d, "li:not(.on)"), 4);
    expect_int("selector li.on + li", count_matches(d, "li.on + li"), 1);
    expect_int("selector li.on ~ li", count_matches(d, "li.on ~ li"), 3);
    expect_int("selector a[href$='.pdf']", count_matches(d, "a[href$='.pdf']"), 1);
    expect_int("selector a[href^='/']", count_matches(d, "a[href^='/']"), 1);
    expect_int("selector a[rel~=nofollow]", count_matches(d, "a[rel~=nofollow]"), 1);
    expect_int("selector [lang|=en]", count_matches(d, "[lang|=en]"), 1);
    expect_int("selector div:has(> ul)", count_matches(d, "div:has(> ul)"), 1);
    expect_int("selector :is(p, li)", count_matches(d, ":is(p, li)"), 7);
    expect_int("selector #y, .big", count_matches(d, "#y, .big"), 2);
    expect_int("selector invalid is rejected", count_matches(d, "div >> p"), -1);
    lxb_html_document_destroy(d);

    /* 4. style cascade: author rules + inline style, !important, specificity */
    d = lxb_html_document_create();
    lxb_html_document_css_init(d, true);
    lxb_css_parser_t *cp = lxb_css_parser_create(); lxb_css_parser_init(cp, NULL);
    lxb_css_stylesheet_t *sst = lxb_css_stylesheet_create(NULL);
    static const char css[] = "div {width: 5pt !important; height: 200px !important} p {color: red}";
    expect_int("stylesheet parse", lxb_css_stylesheet_parse(sst, cp, (const lxb_char_t *)css, sizeof css - 1), LXB_STATUS_OK);
    static const char html4[] = "<div style='width: 10px; height: 20pt !important'></div><p>t</p>";
    lxb_html_document_parse(d, (const lxb_char_t *)html4, sizeof html4 - 1);
    lxb_html_document_stylesheet_attach(d, sst);
    lxb_dom_collection_t col; memset(&col, 0, sizeof col);
    lxb_dom_node_by_tag_name(lxb_dom_interface_node(d), &col, (const lxb_char_t *)"div", 3);
    lxb_dom_element_t *div = lxb_dom_collection_element(&col, 0);
    const lxb_css_rule_declaration_t *w = lxb_html_element_style_by_name(lxb_html_interface_element(div), (const lxb_char_t *)"width", 5);
    const lxb_css_rule_declaration_t *h = lxb_html_element_style_by_id(lxb_html_interface_element(div), LXB_CSS_PROPERTY_HEIGHT);
    b.n = 0; b.s[0] = 0; if (w) lxb_css_rule_declaration_serialize(w, cb, &b);
    expect_str("cascade: author !important beats inline normal", b.s, "width: 5pt !important");
    b.n = 0; b.s[0] = 0; if (h) lxb_css_rule_declaration_serialize(h, cb, &b);
    expect_str("cascade: inline !important beats author !important", b.s, "height: 20pt !important");
    lxb_dom_collection_clean(&col); lxb_html_document_destroy(d); lxb_css_parser_destroy(cp, true);

    /* 5. CSS parser round trip: values are normalised (case, colours, spacing). NOTE: Lexbor 2.7 parses @media but does not
     *    expose or serialize its contents, so responsive rules are not testable yet (see docs/BROWSER-ENGINES.md). */
    static const struct { const char *in, *out; } css_cases[] = {
        { "a{color:RED;margin:0 auto}", "a {color: red; margin: 0 auto}" },
        { "@import url(x.css);a{b:c}", "@import url(x.css)\na {b: c}" },
        { "@font-face{font-family:F;src:url(f.woff2)} a{color:#FFF;width:calc(1px + 2em)}", "@font-face {font-family: F; src: url(f.woff2)}\na {color: #fff; width: calc(1px + 2em)}" },
    };
    for (unsigned i = 0; i < sizeof css_cases / sizeof *css_cases; i++) {
        cp = lxb_css_parser_create(); lxb_css_parser_init(cp, NULL);
        sst = lxb_css_stylesheet_create(NULL);
        expect_int("stylesheet parse", lxb_css_stylesheet_parse(sst, cp, (const lxb_char_t *)css_cases[i].in, strlen(css_cases[i].in)), LXB_STATUS_OK);
        b.n = 0; b.s[0] = 0; lxb_css_rule_serialize(sst->root, cb, &b);
        expect_str("stylesheet serializes", b.s, css_cases[i].out);
        lxb_css_stylesheet_destroy(sst, true); lxb_css_parser_destroy(cp, true);
    }

    /* 6. WHATWG URL parsing and relative resolution */
    lxb_url_parser_t up; lxb_url_parser_init(&up, NULL);
    static const char u1[] = "HTTPS://user:pw@Example.COM:8443/a/../b/./c?q=1#frag";
    lxb_url_t *url = lxb_url_parse(&up, NULL, (const lxb_char_t *)u1, sizeof u1 - 1);
    b.n = 0; b.s[0] = 0; if (url) lxb_url_serialize(url, cb, &b, false);
    expect_str("url normalised", b.s, "https://user:pw@example.com:8443/b/c?q=1#frag");
    static const char u2[] = "../img/logo.png?v=2";
    lxb_url_t *rel = lxb_url_parse(&up, url, (const lxb_char_t *)u2, sizeof u2 - 1);
    b.n = 0; b.s[0] = 0; if (rel) lxb_url_serialize(rel, cb, &b, false);
    expect_str("url relative resolution", b.s, "https://user:pw@example.com:8443/img/logo.png?v=2");
    static const char u3[] = "http://example.com:80/\xC3\xA9 x";
    lxb_url_t *u3p = lxb_url_parse(&up, NULL, (const lxb_char_t *)u3, sizeof u3 - 1);
    b.n = 0; b.s[0] = 0; if (u3p) lxb_url_serialize(u3p, cb, &b, false);
    expect_str("url default port dropped, path percent-encoded", b.s, "http://example.com/%C3%A9%20x");
    static const char u4[] = "http://exa mple.com/";
    expect_int("url with space in host is rejected", lxb_url_parse(&up, NULL, (const lxb_char_t *)u4, sizeof u4 - 1) == NULL, 1);
    lxb_url_parser_destroy(&up, false);

    /* 7. robustness: very deep nesting, and garbage input must not crash or hang */
    enum { DEPTH = 3000 }; char *deep = malloc(DEPTH * 5 + 1); deep[0] = 0;
    for (int i = 0; i < DEPTH; i++) strcat(deep, "<div>");
    d = parse(deep);
    expect_int("deep nesting parsed (no stack overflow)", d && count_matches(d, "div") > 100, 1);
    if (d) lxb_html_document_destroy(d);
    free(deep);
    char junk[2000]; unsigned x = 12345; for (size_t i = 0; i < sizeof junk; i++) { x = x * 1103515245 + 12345; junk[i] = (char)(x >> 16) | 1; junk[sizeof junk - 1] = 0; }
    d = parse(junk); expect_int("garbage input does not crash", d != NULL, 1); if (d) lxb_html_document_destroy(d);

    printf("HTMLTEST DONE pass=%d fail=%d\n", pass, fail);
    return fail ? 1 : 0;
}
