import importlib.util
import pathlib
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('bundle', ROOT / 'scripts/documentation/build_bundle.py')
bundle = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bundle)

class BundleTests(unittest.TestCase):
    def test_known_sibling_and_anchor(self):
        self.assertEqual(bundle.resolve('windows/index.md', '../prepare/README.md#start', {'prepare/README.md'}), 'prepare/README.md#start')
        self.assertEqual(bundle.resolve('windows/index.md', '#start', {'windows/index.md'}), 'windows/index.md#start')

    def test_untrusted_routes_are_not_actionable(self):
        for target in ('../../secret.md', '%2e%2e/%2e%2e/secret.md', 'file:///etc/passwd', 'https://example.com', '//example.com/x', 'C:/secret.md', '%5csecret.md', 'missing.md'):
            with self.subTest(target=target):
                self.assertIsNone(bundle.resolve('windows/index.md', target, {'secret.md'}))

    def test_raw_html_is_inert_and_code_is_escaped(self):
        rendered = bundle.render('a.md', '# Title\n<script>alert(1)</script>\n```html\n<img src="https://example.com">\n```', {'a.md'}, set())
        self.assertNotIn('<script>', rendered)
        self.assertNotIn('<img ', rendered)
        self.assertIn('&lt;script&gt;', rendered)
        self.assertIn('<pre>', rendered)

    def test_duplicate_anchors_and_internal_links(self):
        rendered = bundle.render('a.md', '# Same\n## Same\n[next](b.md#b)', {'a.md', 'b.md'}, set())
        self.assertIn('name="same-1"', rendered)
        self.assertIn('href="doc:b.md#b"', rendered)

    def test_table_is_rendered(self):
        rendered = bundle.render('a.md', '# Table\n| A | B |\n| --- | --- |\n| one | two |', {'a.md'}, set())
        self.assertIn('<table', rendered)
        self.assertIn('<td>two</td>', rendered)
        self.assertNotIn('<td>---</td>', rendered)

    def test_every_article_changes_bundle_and_manifest(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            docs = root / 'docs/features/category'
            docs.mkdir(parents=True)
            (docs / 'first.md').write_text('# First\nHello', encoding='utf8')
            initial = bundle.build(root)
            (docs / 'second.md').write_text('# Second\nWorld', encoding='utf8')
            added = bundle.build(root)
            self.assertNotEqual(initial, added)
            self.assertIn('category/second.md', added[0])
            (docs / 'first.md').write_text('# First\nChanged', encoding='utf8')
            self.assertNotEqual(added, bundle.build(root))

    def test_missing_title_fails_closed(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            docs = root / 'docs/features'
            docs.mkdir(parents=True)
            (docs / 'broken.md').write_text('No title', encoding='utf8')
            with self.assertRaisesRegex(ValueError, 'no title'):
                bundle.build(root)

    def test_bounded_article_rejects_large_input(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            docs = root / 'docs/features'
            docs.mkdir(parents=True)
            (docs / 'large.md').write_text('# Large\n' + 'x' * bundle.MAX_ARTICLE, encoding='utf8')
            with self.assertRaisesRegex(ValueError, 'size limit'):
                bundle.build(root)

    def test_stale_and_missing_output_fail_then_restore(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            expected = {'bundle.hpp': 'complete bundle'}
            bundle.write_or_check(root, expected, False)
            bundle.write_or_check(root, expected, True)
            (root / 'bundle.hpp').write_text('dropped article', encoding='utf8')
            with self.assertRaisesRegex(ValueError, 'Stale or incomplete'):
                bundle.write_or_check(root, expected, True)
            bundle.write_or_check(root, expected, False)
            bundle.write_or_check(root, expected, True)
            (root / 'bundle.hpp').unlink()
            with self.assertRaisesRegex(ValueError, 'Stale or incomplete'):
                bundle.write_or_check(root, expected, True)

    def test_sensitive_sources_rejected_without_echo(self):
        bundle.validate_source('Up/Down/Home/End/PageUp/PageDown')
        for source in ('C:/Users/example/private.txt', '-----BEGIN PRIVATE KEY-----', 'github_pat_' + 'a' * 32):
            with self.assertRaises(ValueError) as context:
                bundle.validate_source(source)
            self.assertNotIn(source, str(context.exception))

    def test_only_known_images_render(self):
        source = '![Figure](figure.png) ![Unknown](https://example.com/a.png)'
        rejected = set()
        rendered = bundle.render('a.md', source, {'a.md'}, rejected, {('a.md', 'figure.png'): 'a' * 64 + '.png'})
        self.assertIn('src="memory:documentation-', rendered)
        self.assertNotIn('src="https:', rendered)
        self.assertIn(('a.md', 'https://example.com/a.png', 'image'), rejected)

if __name__ == '__main__':
    unittest.main()
