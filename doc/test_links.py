# SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
# SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
"""Check the published-site boundary as well as local fragment resolution."""
from contextlib import redirect_stderr, redirect_stdout
from io import StringIO
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

from check_links import check


class LinkTests(unittest.TestCase):
    def result(self, href):
        with TemporaryDirectory() as directory:
            base = Path(directory)
            root = base / 'site'
            (root / 'shaders').mkdir(parents=True)
            (root / 'index.html').write_text(f'<a href="{href}">link</a>', encoding='utf-8')
            (root / 'shaders/index.html').write_text('<h1 id="shader">Shader</h1>', encoding='utf-8')
            (base / 'unpublished.md').write_text('Not in the uploaded artifact.', encoding='utf-8')
            with redirect_stdout(StringIO()), redirect_stderr(StringIO()):
                return check(root)

    def test_nested_site_anchor(self):
        self.assertFalse(self.result('shaders/index.html#shader'))

    def test_missing_anchor(self):
        self.assertTrue(self.result('shaders/index.html#missing'))

    def test_existing_file_outside_upload(self):
        self.assertTrue(self.result('../unpublished.md'))


if __name__ == '__main__':
    unittest.main()
