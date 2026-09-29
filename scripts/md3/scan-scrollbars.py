#!/usr/bin/env python3
"""List the scrollbars a release shows, from layout-probe dumps.

Every window record in a layout-probe dump carries `scrollbars`
(docs/features/design-system/layout-probe.md): `native_v`/`native_h` for a bar
Windows draws itself, `kit_v`/`kit_h` for the kit scrollbar of an
MD3ScrolledWindow, a kit ListBox, an MD3 table or a TextAreaEditor. This
reads every `*.jsonl` dump under the given folders (the page captures and the
dialog sweep write them) and reports each shown window with a Windows bar, and
how many kit bars were seen.

    py -3 scripts/md3/scan-scrollbars.py <folder> [<folder> ...] [--json <report.json>]

Exits 1 when a shown window has a Windows scrollbar, 2 when no dump carries
the `scrollbars` field at all (a build from before the field existed), and 0
otherwise.
"""
import argparse
import json
import pathlib
import sys


def records(path):
    with open(path, encoding='utf-8') as fh:
        for line in fh:
            line = line.strip()
            if not line:
                continue
            try:
                yield json.loads(line)
            except json.JSONDecodeError:
                continue


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('folders', nargs='+')
    parser.add_argument('--json', dest='report')
    args = parser.parse_args()

    dumps = sorted({p for folder in args.folders for p in pathlib.Path(folder).rglob('*.jsonl')})
    native, kit, with_field = [], 0, 0
    for dump in dumps:
        for record in records(dump):
            if record.get('kind') != 'window' or 'scrollbars' not in record:
                continue
            with_field += 1
            if not (record.get('shown') and record.get('on_screen', True)):
                continue
            bars = record['scrollbars']
            kit += int(bool(bars.get('kit_v'))) + int(bool(bars.get('kit_h')))
            if bars.get('native_v') or bars.get('native_h'):
                native.append({
                    'dump': dump.name,
                    'class': record.get('class'),
                    'type': record.get('type'),
                    'name': record.get('name'),
                    'label': record.get('label'),
                    'bars': 'vertical and horizontal' if bars.get('native_v') and bars.get('native_h')
                            else 'vertical' if bars.get('native_v') else 'horizontal',
                    'rect': record.get('screen'),
                })

    report = {'dumps': len(dumps), 'windows_with_field': with_field, 'kit_bars_shown': kit, 'native': native}
    if args.report:
        with open(args.report, 'w', encoding='utf-8') as fh:
            json.dump(report, fh, ensure_ascii=False, indent=1)
    sys.stdout.reconfigure(encoding='utf-8')
    print(f'{len(dumps)} dumps, {with_field} window records with scrollbars, {kit} kit bars shown, '
          f'{len(native)} shown windows with a Windows bar')
    for n in native:
        print(f"  {n['dump']}: {n['type'] or n['class']} {n['name']!r} ({n['bars']})")
    if with_field == 0:
        return 2
    return 1 if native else 0


if __name__ == '__main__':
    sys.exit(main())
