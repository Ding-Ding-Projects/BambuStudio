#!/usr/bin/env python3
"""Create one isolated BambuStudio data directory per capture tuple.

The layout-probe matrix and the screenshot inventory run the real built
executable once per (language, theme, density) tuple. Each run gets its own
--datadir so the user's profile is never read or written, and so the tuple is
fixed by the configuration file rather than by whatever the last run left
behind. Display scale is not a configuration value; it comes from the monitor
the hidden desktop reports, so scale tuples are separate runs on separate
displays and are recorded by the caller.

    py -3 scripts/md3/prepare-capture-datadirs.py <root> [--languages en,yue_HK,bilingual_en_yue_HK]
                                                           [--themes light,dark] [--densities comfortable,compact]

Prints one line per directory: "<tuple-id>\t<path>". Existing directories are
replaced so a rerun starts clean.

Captures are published, and the app shows folders on screen: Config profiles &
backup lists the data folder, Preferences the download folder. So the root must
lie outside the user profile (for example C:\\Users\\Public\\bbsdd), or a capture
shows the Windows account name; the script refuses a root inside the profile
unless --allow-profile-root is given for a run that stays private. Each profile
keeps its downloads in its own "downloads" folder.
"""
from __future__ import annotations

import argparse
import json
import os
import shutil
import sys

LANGUAGES = ["en", "yue_HK", "bilingual_en_yue_HK"]
THEMES = ["light", "dark"]
DENSITIES = ["comfortable", "compact"]


def config_for(language: str, theme: str, density: str, downloads: str) -> dict:
    # Keys read by GUI_App at startup: "language" (LanguageMode id),
    # "dark_color_mode" ("1" forces dark, "0" forces light), "ui_density".
    return {
        "app": {
            "language": language,
            # Otherwise the app fills in the account's own Downloads folder, and
            # Preferences shows it (and every layout dump records it).
            "download_path": downloads,
            "dark_color_mode": "1" if theme == "dark" else "0",
            "ui_density": density,
            # Keep first-run chrome out of the capture: no update prompt, no
            # network plugin download, no privacy banner.
            "check_update": "false",
            "installed_networking": "1",
            "privacy_version": "1",
            "show_hints": "false",
        }
    }


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("root")
    ap.add_argument("--languages", default=",".join(LANGUAGES))
    ap.add_argument("--themes", default=",".join(THEMES))
    ap.add_argument("--densities", default=",".join(DENSITIES))
    ap.add_argument("--allow-profile-root", action="store_true",
                    help="allow a root inside the user profile, for captures that are never published")
    args = ap.parse_args()
    root = os.path.abspath(args.root)
    profile = os.path.normcase(os.path.abspath(os.path.expanduser("~")))
    inside_profile = os.path.normcase(root) == profile or os.path.normcase(root).startswith(profile + os.sep)
    if inside_profile and not args.allow_profile_root:
        print("prepare-capture-datadirs: the root is inside the user profile, so captures would show the account "
              "name in the data and download folders; use a root such as C:\\Users\\Public\\bbsdd, or pass "
              "--allow-profile-root for a private run", file=sys.stderr)
        return 2
    os.makedirs(root, exist_ok=True)
    for language in args.languages.split(","):
        for theme in args.themes.split(","):
            for density in args.densities.split(","):
                tuple_id = f"{language}-{theme}-{density}"
                path = os.path.join(root, tuple_id)
                if os.path.isdir(path):
                    shutil.rmtree(path)
                os.makedirs(os.path.join(path, "log"))
                downloads = os.path.join(path, "downloads")
                os.makedirs(downloads)
                with open(os.path.join(path, "BambuStudio.conf"), "w", encoding="utf-8", newline="\n") as fh:
                    json.dump(config_for(language, theme, density, downloads), fh, indent=4)
                    fh.write("\n")
                print(f"{tuple_id}\t{path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
