import re, sys, pathlib
src = pathlib.Path(sys.argv[1]); out = pathlib.Path(sys.argv[2]); fonts = sys.argv[3]
face = f"""<style>
@font-face{{font-family:Roboto;font-weight:400;src:url(file://{fonts}/Roboto-Regular.ttf)}}
@font-face{{font-family:Roboto;font-weight:500;src:url(file://{fonts}/Roboto-Medium.ttf)}}
@font-face{{font-family:Roboto;font-weight:700;src:url(file://{fonts}/Roboto-Bold.ttf)}}
@font-face{{font-family:'Roboto Mono';font-weight:400;src:url(file://{fonts}/RobotoMono-Regular.ttf)}}
@font-face{{font-family:'Roboto Mono';font-weight:500;src:url(file://{fonts}/RobotoMono-Medium.ttf)}}
@font-face{{font-family:'Material Symbols Outlined';font-weight:400;src:url(file://{fonts}/MaterialSymbolsOutlined.ttf)}}
x-dc,helmet{{display:contents}}
</style>"""
for p in src.glob('*.dc.html'):
    t = p.read_text()
    t = t.replace('<script src="./support.js"></script>', face)
    t = re.sub(r'<script type="text/x-dc".*?</script>', '', t, flags=re.S)
    t = re.sub(r'<link href="https://fonts.googleapis.com[^>]*>', '', t)
    (out / p.name.replace('.dc.html', '.html')).write_text(t)
    print('prepared', p.name)
