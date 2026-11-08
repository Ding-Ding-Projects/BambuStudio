#!/usr/bin/env python3
"""Checks recapture.py's blank web page guard.

Home and Ink are WebView2 pages, and WebView2 does not paint into a
PrintWindow capture of a hidden desktop, so their body can come out as one
flat colour. The guard must call such a capture blank, must not call a real
page blank, and the page recipes must judge before anything is replaced.

    py -3 scripts/md3/test-recapture-blank-body.py
"""
from __future__ import annotations

import importlib.util
import os
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..'))

try:
    from PIL import Image, ImageDraw
except ImportError:  # the README jobs install Pillow; elsewhere say so plainly
    print('Pillow is required: pip install Pillow')
    sys.exit(2)


def load_recapture():
    spec = importlib.util.spec_from_file_location('recapture', os.path.join(HERE, 'recapture.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


recapture = load_recapture()


def frame(body_colour=(255, 255, 255)):
    """A 1200x800 frame: caption, project tabs and rail painted, body flat."""
    im = Image.new('RGB', (1200, 800), body_colour)
    d = ImageDraw.Draw(im)
    d.rectangle((0, 0, 1199, 44), fill=(238, 243, 239))         # caption
    d.rectangle((0, 45, 1199, 104), fill=(245, 248, 245))       # project tabs
    d.rounded_rectangle((12, 52, 118, 94), 12, fill=(166, 244, 184))
    d.rectangle((0, 105, 100, 799), fill=(245, 248, 245))       # rail
    for y in range(120, 560, 64):
        d.rectangle((40, y, 60, y + 20), outline=(64, 72, 66), width=2)
    return im


class BlankBodyTest(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()

    def tearDown(self):
        self.dir.cleanup()

    def save(self, im, name):
        path = os.path.join(self.dir.name, name)
        im.save(path)
        return path

    def test_a_flat_body_is_blank_whatever_the_chrome(self):
        self.assertTrue(recapture.body_is_blank(self.save(frame(), 'blank.png')))
        self.assertTrue(recapture.body_is_blank(self.save(frame((250, 250, 252)), 'tinted.png')))

    def test_a_page_with_content_is_not_blank(self):
        im = frame()
        d = ImageDraw.Draw(im)
        d.rounded_rectangle((266, 176, 1146, 252), 26, fill=(178, 240, 196))   # banner
        d.rectangle((110, 110, 330, 790), fill=(245, 246, 250))                # left menu
        self.assertFalse(recapture.body_is_blank(self.save(im, 'page.png')))

    def test_a_little_content_is_enough(self):
        # About 0.6 % of the body: one heading-sized block of text.
        im = frame()
        ImageDraw.Draw(im).rectangle((300, 300, 380, 352), fill=(20, 24, 22))
        self.assertFalse(recapture.body_is_blank(self.save(im, 'heading.png')))

    def test_the_committed_home_capture_is_not_blank(self):
        path = os.path.join(REPO, 'docs', 'readme-assets', 'shot-home.png')
        self.assertTrue(os.path.isfile(path))
        self.assertFalse(recapture.body_is_blank(path))

    def test_an_image_smaller_than_the_body_box_is_judged_on_what_exists(self):
        small = self.save(Image.new('RGB', (100, 100), (255, 255, 255)), 'small.png')
        self.assertFalse(recapture.body_is_blank(small))  # no body at all: nothing to call blank
        mid = self.save(Image.new('RGB', (400, 300), (255, 255, 255)), 'mid.png')
        self.assertTrue(recapture.body_is_blank(mid))

    def test_only_the_web_pages_are_judged(self):
        self.assertEqual(recapture.WEBVIEW_SURFACES, {'home', 'ink'})
        self.assertTrue(recapture.WEBVIEW_SURFACES <= recapture.MAIN_SURFACES)

    def test_the_page_recipes_judge_before_replacing(self):
        with open(os.path.join(HERE, 'recapture.py'), encoding='utf-8') as f:
            source = f.read()
        run = source[source.index('    def run(self, recipe, out_path):'):]
        run = run[:run.index('\n    def ', 10)] if '\n    def ' in run[10:] else run
        page = run[run.index("if recipe['kind'] == 'page':"):run.index("if recipe['kind'] == 'crop-probe':")]
        # The web page is shot beside the target, judged, and only then moved over it.
        self.assertLess(page.index("self.app.shot(target, page)"), page.index('body_is_blank(page)'))
        self.assertLess(page.index('body_is_blank(page)'), page.index('os.replace(page, out_path)'))
        self.assertIn('raise RuntimeError(WEBVIEW_BLANK)', page)
        crop = run[run.index("if recipe['kind'] == 'crop-rect':"):run.index("if recipe['kind'] == 'crop-gl':")]
        self.assertLess(crop.index('body_is_blank(page)'), crop.index("cheap('crop_image'"))
        # The failure is reported as failed, not blocked, so a run says it plainly.
        self.assertFalse(recapture.WEBVIEW_BLANK.startswith('blocked'))


if __name__ == '__main__':
    unittest.main()
