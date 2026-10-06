"""Compile repository feature articles into an immutable, offline native bundle."""
import argparse
import base64
import hashlib
import html
import json
import pathlib
import re
import urllib.parse

ROOT = pathlib.Path(__file__).resolve().parents[2]
MAX_ARTICLE = 1024 * 1024
MAX_TOTAL = 16 * 1024 * 1024

def validate_source(text):
    # Reject obvious credential and private-instruction material without logging it.
    forbidden = (r'codingmachineedge/private-vocabulary', r'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----',
                 r'\bgh[pousr]_[A-Za-z0-9]{30,}', r'\bgithub_pat_[A-Za-z0-9_]{30,}',
                 r'(?i)\b[A-Z]:[/\\]Users[/\\](?!<|\{|%)[A-Za-z0-9_.-]+[/\\]',
                 r'(?i)(?<![A-Za-z0-9])/(?:home|Users)/[A-Za-z0-9_.-]+/')
    if any(re.search(pattern, text) for pattern in forbidden):
        raise ValueError('Sensitive or private source material is not allowed in the bundle')

def digest(value):
    return hashlib.sha256(value).hexdigest()

def slug(text):
    return re.sub(r'[^\w\- ]', '', text.lower()).replace(' ', '-')

def headings(text):
    result, seen, fenced = [], {}, False
    for line in text.splitlines():
        if line.startswith('```') or line.startswith('~~~'):
            fenced = not fenced
        if fenced:
            continue
        match = re.match(r'^#{1,6}\s+(.+)', line)
        if match:
            base = slug(match[1])
            count = seen.get(base, 0)
            seen[base] = count + 1
            result.append(base + ('-' + str(count) if count else ''))
    return result

def resolve(source, target, available):
    """Only canonical, known bundle routes may become actionable links."""
    parsed = urllib.parse.urlsplit(target)
    if parsed.scheme or parsed.netloc or '\\' in target or '\x00' in target:
        return None
    path = urllib.parse.unquote(parsed.path)
    if path.startswith('/') or ':' in path or '\\' in path:
        return None
    parts = list(pathlib.PurePosixPath(source).parent.parts)
    for part in path.split('/') if path else []:
        if part in ('', '.'):
            continue
        if part == '..':
            if not parts:
                return None
            parts.pop()
        else:
            parts.append(part)
    route = '/'.join(parts) if path else source
    if route not in available:
        return None
    anchor = urllib.parse.unquote(parsed.fragment)
    if anchor and isinstance(available, dict) and anchor not in available[route]:
        return None
    return route + ('#' + anchor if anchor else '')

def render(source, text, available, rejected, images=None):
    anchors = []
    seen = {}
    def inline(value):
        output, end = [], 0
        for match in re.finditer(r'(!?)\[([^\]]*)\]\(([^\s)]+)(?:\s+"[^"]*")?\)', value):
            output.append(html.escape(value[end:match.start()]))
            image, label, target = match.groups()
            route = resolve(source, target, available)
            image_id = images.get((source, target)) if images else None
            if image and image_id:
                output.append('<img src="memory:documentation-' + image_id + '" width="320" alt="' + html.escape(label, quote=True) + '">')
            elif route and not image:
                output.append('<a href="doc:' + html.escape(route, quote=True) + '">' + html.escape(label) + '</a>')
            else:
                output.append(html.escape(label) + ' <small>[reference unavailable offline]</small>')
                rejected.add((source, target, 'image' if image else 'link'))
            end = match.end()
        output.append(html.escape(value[end:]))
        result = ''.join(output)
        result = re.sub(r'`([^`]+)`', r'<code>\1</code>', result)
        result = re.sub(r'\*\*([^*]+)\*\*', r'<b>\1</b>', result)
        return result
    out, code, listing, table = [], False, False, False
    for line in text.splitlines():
        if line.startswith('```') or line.startswith('~~~'):
            out.append('</pre>' if code else '<pre>')
            code = not code
            continue
        if code:
            out.append(html.escape(line) + '\n')
            continue
        heading = re.match(r'^(#{1,6})\s+(.+)', line)
        item = re.match(r'^\s*(?:[-*+] |\d+\. )(.+)', line)
        table_row = line.strip().startswith('|') and line.strip().endswith('|')
        if table and not table_row:
            out.append('</table>')
            table = False
        if listing and not item:
            out.append('</ul>')
            listing = False
        if table_row:
            cells = line.strip().strip('|').split('|')
            if all(re.fullmatch(r'\s*:?-+:?\s*', cell) for cell in cells):
                continue
            if not table:
                out.append('<table border="1" cellspacing="0" cellpadding="5">')
                table = True
            out.append('<tr>' + ''.join('<td>' + inline(cell.strip()) + '</td>' for cell in cells) + '</tr>')
        elif heading:
            level, title = len(heading[1]), heading[2]
            base = slug(title)
            count = seen.get(base, 0)
            seen[base] = count + 1
            anchor = base + ('-' + str(count) if count else '')
            anchors.append((title, anchor))
            out.append(f'<a name="{html.escape(anchor, quote=True)}"></a><h{level}>{inline(title)}</h{level}>')
        elif item:
            if not listing:
                out.append('<ul>')
                listing = True
            out.append('<li>' + inline(item[1]) + '</li>')
        elif line.strip():
            out.append('<p>' + inline(line) + '</p>')
    if listing:
        out.append('</ul>')
    if code:
        out.append('</pre>')
    if table:
        out.append('</table>')
    toc = '<p>' + ' | '.join('<a href="doc:' + html.escape(source + '#' + a, quote=True) + '">' + html.escape(t) + '</a>' for t, a in anchors) + '</p>'
    return toc + '\n'.join(out)

def literal(value):
    # Keep each MSVC literal comfortably below the per-literal size limit.
    chunks = ['u8' + json.dumps(value[i:i+4000], ensure_ascii=True) for i in range(0, len(value), 4000)]
    return 'std::string(' + chunks[0] + ')' + ''.join('\n + ' + chunk for chunk in chunks[1:]) if chunks else 'std::string()'

def build(root):
    directory = root / 'docs/features'
    paths = sorted(directory.rglob('*.md'))
    if not paths or len(paths) > 4096:
        raise ValueError('Article count outside supported bounds')
    records, total = [], 0
    for path in paths:
        if path.is_symlink() or not path.resolve().is_relative_to(directory.resolve()):
            raise ValueError('Article escapes documentation root')
        raw = path.read_bytes()
        total += len(raw)
        if len(raw) > MAX_ARTICLE or total > MAX_TOTAL:
            raise ValueError('Documentation bundle exceeds size limit')
        text = raw.decode('utf-8-sig').replace('\r\n', '\n').replace('\r', '\n')
        validate_source(text)
        title = re.search(r'^#\s+(.+)', text, re.M)
        if not title:
            raise ValueError('Article has no title: ' + path.name)
        records.append(dict(route=path.relative_to(directory).as_posix(), title=title[1], text=text, sha256=digest(text.encode('utf-8'))))
    routes = {r['route']: set(headings(r['text'])) for r in records}
    rejected = set()
    image_routes, image_records = {}, {}
    for record in records:
        for target in re.findall(r'!\[[^\]]*\]\(([^\s)]+)(?:\s+"[^"]*")?\)', record['text']):
            parsed = urllib.parse.urlsplit(target)
            if parsed.scheme or parsed.netloc or '\\' in target:
                continue
            decoded = urllib.parse.unquote(parsed.path)
            if ':' in decoded or '\\' in decoded or '\x00' in decoded:
                continue
            candidate = (directory / pathlib.PurePosixPath(record['route']).parent / decoded).resolve()
            if not candidate.is_relative_to((root / 'docs').resolve()) or not candidate.is_file() or candidate.is_symlink():
                continue
            raw = candidate.read_bytes()
            extension = candidate.suffix.lower()
            magic_valid = (extension == '.png' and raw.startswith(b'\x89PNG\r\n\x1a\n')) or (extension in ('.jpg', '.jpeg') and raw.startswith(b'\xff\xd8\xff')) or (extension == '.gif' and raw[:6] in (b'GIF87a', b'GIF89a'))
            if not magic_valid or len(raw) > 4 * 1024 * 1024:
                continue
            identity = digest(raw) + extension
            image_routes[(record['route'], target)] = identity
            image_records[identity] = raw
    if sum(map(len, image_records.values())) > 32 * 1024 * 1024:
        raise ValueError('Embedded images exceed bundle limit')
    fields = []
    for record in records:
        rendered = render(record['route'], record['text'], routes, rejected, image_routes)
        fields.append('{' + ',\n'.join(literal(record[k]) for k in ('route', 'title', 'text')) + ',\n' + literal(rendered) + '}')
    image_fields = ['{' + literal(identity) + ',' + literal(base64.b64encode(raw).decode()) + '}' for identity,raw in sorted(image_records.items())]
    header = '// Generated by scripts/documentation/build_bundle.py. Do not edit.\n#pragma once\n#include <string>\nnamespace Slic3r { namespace GUI { namespace Documentation {\nstruct Article { std::string route, title, text, html; };\ninline const Article articles[] = {\n' + ',\n'.join(fields) + '\n};\nstruct Image { std::string name, base64; };\ninline const Image images[] = {\n' + ',\n'.join(image_fields or ['{"", ""}']) + '\n};\n}}}\n'
    manifest = dict(schema=1, text_hash_encoding='UTF-8 without BOM; LF line endings', articles=[{k:v for k,v in r.items() if k != 'text'} for r in records], images=[dict(name=n, sha256=digest(b), bytes=len(b)) for n,b in sorted(image_records.items())], bundle_sha256=digest(header.encode()), unavailable_references=[dict(source=s, target=t, kind=k) for s,t,k in sorted(rejected)])
    return header, json.dumps(manifest, ensure_ascii=False, indent=2) + '\n'

def write_or_check(root, outputs, check):
    for relative, value in outputs.items():
        path = root / relative
        if check:
            if not path.exists() or path.read_text(encoding='utf-8') != value:
                raise ValueError('Stale or incomplete documentation bundle: ' + relative)
        else:
            path.write_bytes(value.encode())

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    header, manifest = build(ROOT)
    outputs = {'src/slic3r/GUI/Documentation/DocumentationBundle.hpp': header, 'src/slic3r/GUI/Documentation/bundle-manifest.json': manifest}
    write_or_check(ROOT, outputs, args.check)
    print('Documentation bundle: complete, deterministic and current')

if __name__ == '__main__':
    main()
