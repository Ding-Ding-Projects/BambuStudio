#!/usr/bin/env python3
"""Validate Cantonese translation drafts and merge the accepted ones into the catalogue.

    py -3 scripts/i18n/merge_cantonese_drafts.py --batches DIR --drafts DIR [--check-only] [--rejects FILE]

A batch file ``batch-NN.json`` comes from export_cantonese_gaps.py; its draft is
``batch-NN.out.json``, a JSON list of ``{"id", "msgstr", "category"}``. Every
draft is checked mechanically before it can enter the catalogue:

* the msgstr is non-empty and contains Chinese text (unless the English is only
  symbols, placeholders or product names, where an identical copy is allowed);
* placeholders match the English exactly, and the line-break count is the same;
* a menu mnemonic (``&F``) is kept as one mnemonic;
* the category is one of the six reviewed categories;
* no Simplified-only character, no disallowed term from glossary.json, and no
  "filament" or "AMS" left in the Cantonese (the product says 墨水 / 墨水機);
* no "to be translated" stub, and a trailing colon, ellipsis or question mark
  in the English survives the translation;
* the draft belongs to its message: when the zh_TW reference has enough
  content, the draft shares a real part of its Han characters with it, and a
  long English message does not come back as a few characters. A draft copied
  from a neighbouring message, or a stock word pasted over a sentence, fails
  here even though every mechanical check above passes.

Accepted entries are appended to BambuStudio_yue_HK.po with
``#. reviewed-category:`` and ``#. review-status: agent-drafted``, and
coverage.json is recounted. ``--check-only`` validates without writing, for a
translator to check its own output. Rejects are written with their reasons.

``--audit`` runs the same checks over the agent-drafted entries already in the
catalogue (with the zh_TW catalogue as their reference) and lists the failures;
``--purge`` also removes them, so they become gaps to translate again.
"""

from __future__ import annotations

import argparse
from collections import Counter
import json
from pathlib import Path
import re
import sys

REPO_ROOT = Path(__file__).resolve().parents[2]
YUE_DIR = REPO_ROOT / "bbl" / "i18n" / "yue_HK"
sys.path.insert(0, str(REPO_ROOT / "bbl" / "i18n"))

from po_catalog import entry_map, parse_po, placeholder_signature, sequential_placeholders  # noqa: E402

CATEGORIES = (
    "connection-offline",
    "destructive-dirty-error-security",
    "file-actions",
    "navigation",
    "preferences",
    "slicing-printing",
)

# Simplified-only forms of common characters. Each has a different Traditional
# form that Hong Kong writing uses; none of them is ordinary Traditional text.
SIMPLIFIED_ONLY = set(
    "这们说为发设实时请输选删应档网线机开关载显图页项录击双单复条务统计认证错误类数据库连断协议购买费账号码验扫览"
    "设备继续终换执运转进远达队阶际陆随隐难须顶预领频题风飞饭馆马驱骤鱼鸟鸡麦黄齿龙仅从众优会传伤伪体侧债倾偿储儿兑"
    "党兰兴养内册写军农冲决况净减凑击创别剂剑剧劝办务劳势匀区医华卖历压厅厉县参变叙叶叹吗启员响团园围圆圣场坏块"
    "坚坝坠垒垫扩扫扬扰抚护报担拟拥拦拨择挂挤挥损据摄摆敌敛数断旧显晓暂术杀杂权来极构枪柜标栏树样桥检楼欢毁毕气"
    "汇污沟没泪泼泽洁浅测浏浓涂润涌涨淀渐湿溃满滤灭灯灵灾炉点炼烂烟烦烧热爱牵独环现电画畅疗盐盖盘确碍矿祸离种积"
    "称穷竖竞笔笼筑签简篮类粮紧纠红纤约级纪纬纯纲纳纵纷纸纹纽练组细织终经绑结绕绘给络绝统继绩绪续绳维综绿缓编缘"
    "缩缴罚罢职联聪肃肤肿胀胁胜脉脏脑脚脱脸舰艰艺节苏荐荡荣药获营蓝虑虚虽补衬袭装见观规视觉触誉计订认讨让训议讯"
    "记讲许论设访证评识诉诊词译试话询该详语误说请读课谁调谈谋谓谢谨谱负贡财责败账货质购贯贴贵贷费资赋赏赔赖赛赞"
    "赢赶趋跃践踪车轨转轮软轴轻载较辅辆辉辑输辞边达迁过迈运还这进远违连迟迹适选递逻遗邮郑鉴钉针钟钢钥钩钱钻铁"
    "铃铜铺链销锁锅错锚键锯镜长门闪闭问闲间闸闻阀阅队阳阴阵阶际陆险随隐难静韩页顶项顺须顾顿颁预领颇频颗题颜额风"
    "飘饭饮饰馆驶驻驾验骑骗鲜鸣"
)

ENGLISH_BANNED = re.compile(r"(?i)\bfilaments?\b|\bAMS\b")
MNEMONIC = re.compile(r"&(?!&)(\w)")
CJK = re.compile(r"[\u3400-\u4dbf\u4e00-\u9fff\uf900-\ufaff]")
MEANINGFUL_WORD = re.compile(r"[A-Za-z]{3,}")
PRODUCT_WORDS = {
    "bambu", "studio", "lab", "cloud", "makerworld", "wi-fi", "wlan", "lan", "ftp", "http", "https", "g-code",
    "gcode", "3mf", "stl", "step", "obj", "usb", "sd", "ok", "id", "url", "pla", "petg", "abs", "tpu", "asa",
    "pc", "pa", "pet", "helio", "mqtt", "rfid", "ip", "api", "ams", "dpi", "rgb", "hex", "hsl", "hsv",
    # Keyboard and mouse names are shown as printed on the keys.
    "ctrl", "shift", "alt", "cmd", "esc", "tab", "enter", "del", "delete", "space", "home", "end", "pgup",
    "pgdn", "ins", "backspace", "win", "fn", "lmb", "rmb", "mmb",
    # Units, formats and protocol names that stay in Latin script.
    "mm", "sec", "min", "rpm", "mbps", "kbps", "gb", "mb", "kb", "gib", "mib", "kib", "tib", "fps", "png", "jpg", "jpeg", "svg", "bmp",
    "ssid", "wpa", "ssl", "tls", "json", "xml", "csv", "zip", "gltf", "fbx", "ply", "amf", "oltp", "sla",
}


# Register: the catalogue is written Hong Kong Cantonese, not formal written
# Chinese. In a sentence (six or more Chinese characters) these formal-only
# function words have a Cantonese form the curated entries use instead. The
# allowed compounds keep their standard spelling in Cantonese too.
FORMAL_MARKERS = (
    ("的", "嘅", ("目的", "的確", "的士", "的話")),
    ("這", "呢 / 咁", ()),
    ("沒有", "冇 / 未", ()),
    ("們", "哋 / 啲", ()),
    ("那", "嗰 / 咁", ("那麼",)),
    ("很", "好 / 非常", ()),
    ("些", "啲", ()),
    ("是", "係", ("是否",)),
    ("了", "咗", ("了解", "為了", "除了", "了結")),
    ("他", "佢", ("其他",)),
    ("她", "佢", ()),
    ("它", "佢", ("其它",)),
)
SENTENCE_MIN_CJK = 6

# Placeholder text a translator leaves when it skipped a message.
STUB_MARKERS = re.compile(r"待翻譯|待翻译|未翻譯|翻譯中|\bTODO\b|\bTBD\b|\bTRANSLATE\b", re.I)

# Alignment with the zh_TW reference. Both are Traditional Chinese renderings of
# the same English, so a correct draft shares a good part of its content
# characters with the reference even when the register differs. Product terms
# are mapped to one spelling on both sides first, and function words (the part
# that differs by register) are ignored.
ALIGNMENT_TERMS = (
    ("耗材絲", "墨水"), ("耗材", "墨水"), ("線材", "墨水"), ("列印", "打印"), ("印表機", "打印機"),
    ("熱床", "打印板"), ("列印板", "打印板"), ("專案", "項目"), ("軟體", "軟件"), ("網路", "網絡"),
    ("如何", "點樣"), ("當前", "目前"), ("檔案夾", "資料夾"), ("智慧", "智能"), ("預設集", "預設"),
)
FUNCTION_CHARACTERS = set("的嘅是係在喺這呢那嗰沒冇們哋了咗他她它佢很好些啲不唔與同及和就都會可以請你您我之將把被")
ALIGNMENT_MIN_SHARED = 0.25
ALIGNMENT_MIN_REFERENCE = 3
# A long English message rendered in a handful of characters has lost content.
SHORT_MIN_LETTERS = 40
SHORT_MIN_RATIO = 0.12


def content_characters(text: str) -> set:
    for term, canonical in ALIGNMENT_TERMS:
        text = text.replace(term, canonical)
    return {char for char in CJK.findall(text) if char not in FUNCTION_CHARACTERS}


def alignment_problems(entry: dict, msgstr: str) -> list:
    problems = []
    english = entry["msgid"].rstrip()
    translated = msgstr.rstrip()
    if english.endswith(":") and not translated.endswith((":", "：")):
        problems.append("the English ends with a colon; the translation lost it (truncated?)")
    if english.endswith(("...", "…")) and not translated.endswith(("...", "…")):
        problems.append("the English ends with an ellipsis; the translation lost it (truncated?)")
    if english.endswith("?") and not translated.endswith(("?", "？")):
        problems.append("the English is a question; the translation lost the question mark")
    # Names kept in Latin script (Marlin, RepRap, G-code) are not missing content.
    kept = set(re.findall(r"[A-Za-z]+", translated))
    letters = sum(len(word) for word in re.findall(r"[A-Za-z]+", english) if word not in kept)
    han = len(CJK.findall(translated))
    if letters >= SHORT_MIN_LETTERS and han and han / letters < SHORT_MIN_RATIO:
        problems.append(f"{han} Chinese characters for {letters} English letters: content is missing")
    reference = entry.get("zh_TW_reference") or ""
    ours, theirs = content_characters(translated), content_characters(reference)
    if len(theirs) >= ALIGNMENT_MIN_REFERENCE and len(ours) >= 2:
        shared = len(ours & theirs) / min(len(ours), len(theirs))
        if shared < ALIGNMENT_MIN_SHARED:
            problems.append(
                f"shares {shared:.0%} of its content characters with the zh_TW reference "
                f"({reference.strip()[:40]!r}); the draft probably belongs to another message"
            )
    return problems


def register_problems(msgstr: str) -> list:
    if len(CJK.findall(msgstr)) < SENTENCE_MIN_CJK:
        return []
    problems = []
    for formal, cantonese, allowed in FORMAL_MARKERS:
        stripped = msgstr
        for compound in allowed:
            stripped = stripped.replace(compound, "")
        if formal in stripped:
            problems.append(f"formal '{formal}' in a sentence; written Cantonese uses {cantonese}")
    return problems


def load_glossary() -> dict:
    return json.loads((YUE_DIR / "glossary.json").read_text(encoding="utf-8"))


def english_needs_translation(english: str) -> bool:
    # All-caps acronyms (VFA, PEI, HMS) stay in Latin script; forcing Chinese
    # onto them invites an invented gloss.
    words = [word for word in MEANINGFUL_WORD.findall(english) if not word.isupper()]
    return any(word.lower() not in PRODUCT_WORDS for word in words)


def validate(entry: dict, draft: dict, disallowed: dict) -> list:
    problems = []
    msgstr = draft.get("msgstr")
    if not entry["msgid"].strip():
        # Whitespace-only source (a spacer label): the translation is the same whitespace.
        return [] if msgstr == entry["msgid"] else ["whitespace-only English must be copied unchanged"]
    if not isinstance(msgstr, str) or not msgstr.strip():
        return ["empty msgstr"]
    reference = entry.get("msgid_plural") or entry["msgid"]
    if placeholder_signature(reference) != placeholder_signature(msgstr):
        problems.append(f"placeholders {dict(placeholder_signature(reference))} != {dict(placeholder_signature(msgstr))}")
    elif sequential_placeholders(reference) != sequential_placeholders(msgstr):
        # printf/boost-style placeholders without a position number consume the
        # arguments in order, so moving one would swap the facts at run time.
        problems.append("sequential placeholders (%s, %d, ...) must keep the English order; only %1$s / %1% may move")
    if reference.count("\n") != msgstr.count("\n"):
        problems.append(f"line breaks {reference.count(chr(10))} != {msgstr.count(chr(10))}")
    if len(MNEMONIC.findall(reference)) != len(MNEMONIC.findall(msgstr)):
        problems.append("menu mnemonic (&X) not kept exactly once")
    if draft.get("category") not in CATEGORIES:
        problems.append(f"unknown category {draft.get('category')!r}")
    if english_needs_translation(entry["english"]) and not CJK.search(msgstr):
        problems.append("no Chinese text in the translation")
    simplified = sorted({char for char in msgstr if char in SIMPLIFIED_ONLY})
    if simplified:
        problems.append("Simplified-only characters: " + "".join(simplified))
    for term, replacement in disallowed.items():
        if term in msgstr:
            problems.append(f"disallowed term {term} (use {replacement})")
    if ENGLISH_BANNED.search(msgstr):
        problems.append("English 'filament'/'AMS' left in the Cantonese (use 墨水 / 墨水機)")
    if STUB_MARKERS.search(msgstr) and not STUB_MARKERS.search(entry["msgid"]):
        problems.append("contains a 'to be translated' stub instead of a translation")
    problems.extend(register_problems(msgstr))
    problems.extend(alignment_problems(entry, msgstr))
    return problems


def po_quote(text: str) -> str:
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"').replace("\t", "\\t").replace("\n", "\\n") + '"'


def po_block(entry: dict, draft: dict) -> list:
    lines = [f"#. reviewed-category: {draft['category']}", "#. review-status: agent-drafted"]
    if entry.get("msgctxt") is not None:
        lines.append(f"msgctxt {po_quote(entry['msgctxt'])}")
    lines.append(f"msgid {po_quote(entry['msgid'])}")
    if entry.get("msgid_plural") is not None:
        lines.append(f"msgid_plural {po_quote(entry['msgid_plural'])}")
        lines.append(f"msgstr[0] {po_quote(draft['msgstr'])}")
    else:
        lines.append(f"msgstr {po_quote(draft['msgstr'])}")
    return lines


def recount_coverage() -> None:
    entries = [entry for entry in parse_po(YUE_DIR / "BambuStudio_yue_HK.po") if not entry.is_header]
    coverage_path = YUE_DIR / "coverage.json"
    coverage = json.loads(coverage_path.read_text(encoding="utf-8"))
    coverage["translated_messages"] = len(entries)
    coverage["categories"] = dict(sorted(Counter(entry.categories[0] for entry in entries if entry.categories).items()))
    coverage["agent_drafted_messages"] = sum(1 for entry in entries if entry.review_status == "agent-drafted")
    coverage_path.write_text(json.dumps(coverage, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def entry_block_lines(lines: list, entry) -> range:
    """Line indexes of one PO entry: its comments, fields and the blank line after it."""
    start = entry.line - 1
    while start > 0 and lines[start - 1].startswith("#"):
        start -= 1
    end, seen_msgstr = entry.line - 1, False
    while end < len(lines) and lines[end].strip():
        line = lines[end]
        if seen_msgstr and (line.startswith("#") or line.startswith("msgctxt") or re.match(r"msgid\s", line)):
            break  # the next entry starts without a blank separator
        seen_msgstr = seen_msgstr or line.startswith("msgstr")
        end += 1
    if end < len(lines) and not lines[end].strip():
        end += 1
    return range(start, end)


def audit_catalogue(purge: bool, report: Path | None) -> int:
    disallowed = load_glossary().get("disallowed_terms", {})
    po_path = YUE_DIR / "BambuStudio_yue_HK.po"
    zh_tw_path = REPO_ROOT / "bbl" / "i18n" / "zh_TW" / "BambuStudio_zh_TW.po"
    zh_tw = {}
    for reference in parse_po(zh_tw_path):
        if not reference.is_header:
            zh_tw[(reference.msgctxt, reference.msgid)] = reference.msgstr or reference.msgstr_plural.get(0, "")

    entries = parse_po(po_path)
    failures = []
    for entry in entries:
        if entry.is_header or entry.review_status != "agent-drafted":
            continue
        msgstr = entry.msgstr_plural.get(0, "") if entry.is_plural else entry.msgstr
        as_batch = {"msgid": entry.msgid, "msgctxt": entry.msgctxt, "msgid_plural": entry.msgid_plural,
                    "english": entry.msgid, "zh_TW_reference": zh_tw.get((entry.msgctxt, entry.msgid), "")}
        draft = {"msgstr": msgstr, "category": entry.categories[0] if entry.categories else None}
        problems = validate(as_batch, draft, disallowed)
        if problems:
            failures.append((entry, msgstr, problems))

    kinds = Counter(problem.split(" (")[0].split(";")[0][:48] for _, _, problems in failures for problem in problems)
    drafted = sum(1 for entry in entries if entry.review_status == "agent-drafted")
    print(f"audited {drafted} agent-drafted entries: {len(failures)} fail the current checks")
    for kind, count in kinds.most_common(12):
        print(f"  {count:5d}  {kind}")
    for entry, msgstr, problems in failures[:20]:
        print(f"  {entry.msgid[:60]!r} -> {msgstr[:30]!r}: {problems[0][:90]}")
    if report:
        report.write_text(json.dumps([
            {"msgctxt": entry.msgctxt, "msgid": entry.msgid, "msgstr": msgstr, "problems": problems}
            for entry, msgstr, problems in failures
        ], ensure_ascii=False, indent=1), encoding="utf-8")

    if purge and failures:
        raw = po_path.read_bytes()
        newline = "\r\n" if b"\r\n" in raw else "\n"
        lines = raw.decode("utf-8").splitlines()
        drop = set()
        for entry, _, _ in failures:
            drop.update(entry_block_lines(lines, entry))
        kept = [line for index, line in enumerate(lines) if index not in drop]
        po_path.write_bytes((newline.join(kept).rstrip("\r\n") + newline).encode("utf-8"))
        # The purge must remove exactly the failing entries and leave every other one untouched.
        before = {(entry.msgctxt, entry.msgid): (entry.msgstr, dict(entry.msgstr_plural)) for entry in entries if not entry.is_header}
        after = {(entry.msgctxt, entry.msgid): (entry.msgstr, dict(entry.msgstr_plural)) for entry in parse_po(po_path) if not entry.is_header}
        removed = {(entry.msgctxt, entry.msgid) for entry, _, _ in failures}
        expected = {key: value for key, value in before.items() if key not in removed}
        if after != expected:
            raise SystemExit("purge changed entries it should have kept; restore the catalogue from Git")
        recount_coverage()
        print(f"purged {len(removed)} entries from {po_path.relative_to(REPO_ROOT)} and recounted coverage.json")
        return 0
    return 1 if failures else 0


def main() -> int:
    # Windows consoles default to a legacy code page; the report prints Chinese.
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="replace")
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--batches", type=Path)
    parser.add_argument("--drafts", type=Path)
    parser.add_argument("--only", help="comma-separated batch numbers to process, e.g. 01,02")
    parser.add_argument("--check-only", action="store_true")
    parser.add_argument("--rejects", type=Path)
    parser.add_argument("--audit", action="store_true", help="check the agent-drafted entries already merged")
    parser.add_argument("--purge", action="store_true", help="like --audit, and remove the failing entries")
    parser.add_argument("--report", type=Path, help="with --audit/--purge: write the failures as JSON")
    args = parser.parse_args()
    if args.audit or args.purge:
        return audit_catalogue(args.purge, args.report)
    if args.batches is None or args.drafts is None:
        parser.error("--batches and --drafts are required unless --audit or --purge is given")

    glossary = load_glossary()
    disallowed = glossary.get("disallowed_terms", {})
    po_path = YUE_DIR / "BambuStudio_yue_HK.po"
    existing = entry_map(parse_po(po_path), po_path)
    wanted = set(args.only.split(",")) if args.only else None

    accepted_blocks, rejects, seen = [], [], set()
    for batch_path in sorted(args.batches.glob("batch-*.json")):
        number = batch_path.stem.split("-")[1]
        if wanted is not None and number not in wanted:
            continue
        draft_path = args.drafts / f"batch-{number}.out.json"
        if not draft_path.is_file():
            continue
        batch = {entry["id"]: entry for entry in json.loads(batch_path.read_text(encoding="utf-8"))}
        drafts = json.loads(draft_path.read_text(encoding="utf-8"))
        for draft in drafts:
            entry = batch.get(draft.get("id"))
            if entry is None:
                rejects.append({"batch": number, "id": draft.get("id"), "problems": ["unknown id"]})
                continue
            key = (entry.get("msgctxt"), entry["msgid"])
            if key in existing or key in seen:
                continue
            problems = validate(entry, draft, disallowed)
            if problems:
                rejects.append({"batch": number, "id": entry["id"], "english": entry["english"],
                                "msgstr": draft.get("msgstr"), "problems": problems})
                continue
            seen.add(key)
            accepted_blocks.append(po_block(entry, draft))
        missing_ids = sorted(set(batch) - {draft.get("id") for draft in drafts})
        for missing_id in missing_ids:
            rejects.append({"batch": number, "id": missing_id, "english": batch[missing_id]["english"],
                            "problems": ["no draft"]})

    if args.rejects:
        args.rejects.write_text(json.dumps(rejects, ensure_ascii=False, indent=1), encoding="utf-8")
    print(f"accepted {len(accepted_blocks)}, rejected {len(rejects)}")
    for reject in rejects[:15]:
        print(f"  batch {reject['batch']} {reject['id']}: {'; '.join(reject['problems'])}")

    if not args.check_only and accepted_blocks:
        raw = po_path.read_bytes()
        newline = "\r\n" if b"\r\n" in raw else "\n"
        text = raw.decode("utf-8").rstrip("\r\n") + newline
        text += newline + (newline + newline).join(newline.join(block) for block in accepted_blocks) + newline
        po_path.write_bytes(text.encode("utf-8"))
        recount_coverage()
        print(f"merged into {po_path.relative_to(REPO_ROOT)} and recounted coverage.json")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
