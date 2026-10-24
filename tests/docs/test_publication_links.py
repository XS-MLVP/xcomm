"""Catch links that work in a checkout but break on PyPI or GitHub Wiki."""
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch


SPEC = importlib.util.spec_from_file_location(
    "docs", Path(__file__).resolve().parents[2] / "scripts/docs.py"
)
docs = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(docs)


class PublicationLinks(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name).resolve()
        self.repo = "https://github.com/XS-MLVP/xcomm"
        config = {
            "site_name": "xcomm", "repo_url": self.repo,
            "nav": [{"Home": "index.md"}, {"Guide": "guide.md"}],
            "plugins": [{"i18n": {"languages": [{}, {}]}}],
        }
        self.enterContext(patch.multiple(docs, ROOT=self.root, DOCS=self.root / "docs", CONFIG=config))
        self.enterContext(contextlib.redirect_stdout(io.StringIO()))
        (self.root / "LICENSE").write_text("License\n")
        (self.root / "example image.png").write_bytes(b"image")
        for language in ("en", "zh"):
            folder = self.root / "docs" / language
            folder.mkdir(parents=True)
            (folder / "index.md").write_text(
                "# Home\n\n[Guide](guide.md#install)\n\n"
                "[License](../../LICENSE)\n\n"
                "![Image](../../example%20image.png)\n\n"
                "```text\n[Literal example](missing.md)\n```\n"
            )
            (folder / "guide.md").write_text(
                "# Guide\n\n## Install\n\nSteps.\n\n[Home](index.md)\n\nDone.\n"
            )

    def test_readme_rejects_existing_relative_file(self):
        (self.root / "README.en.md").write_text("[License][license]\n\n[license]: LICENSE\n")
        with self.assertRaisesRegex(SystemExit, "use an absolute URL for PyPI: LICENSE"):
            docs.check()

    def test_readme_accepts_absolute_links_and_local_anchors(self):
        (self.root / "README.en.md").write_text(
            f"# xcomm\n\n[Docs]({self.repo}/wiki/en-Home)\n\n[Top](#xcomm)\n"
        )
        docs.check()

    def test_readme_rejects_unknown_wiki_page(self):
        (self.root / "README.en.md").write_text(f"[Docs]({self.repo}/wiki/missing-page)\n")
        with self.assertRaisesRegex(SystemExit, "unknown Wiki page"):
            docs.check()

    def test_wiki_exports_links_for_github_and_preserves_code(self):
        output = self.root / "export"
        docs.export_wiki(SimpleNamespace(output=output))
        home = (output / "en-Home.md").read_text()
        self.assertIn(f"[Guide]({self.repo}/wiki/en-guide#install)", home)
        self.assertIn(f"[中文]({self.repo}/wiki/zh-Home)", home)
        self.assertIn(f"[License]({self.repo}/blob/master/LICENSE)", home)
        self.assertIn(f"![Image]({self.repo}/raw/refs/heads/master/example%20image.png)", home)
        self.assertIn("```text\n[Literal example](missing.md)\n```", home)
        self.assertIn(f"[English]({self.repo}/wiki/en-Home)", (output / "Home.md").read_text())
        sidebar = (output / "_Sidebar.md").read_text()
        self.assertIn(f"[Guide]({self.repo}/wiki/en-guide)", sidebar)
        self.assertIn(f"[Guide]({self.repo}/wiki/zh-guide)", sidebar)
        self.assertEqual(len(json.loads((output / "manifest.json").read_text())), 7)


if __name__ == "__main__":
    unittest.main()
