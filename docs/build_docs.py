"""build_docs.py : docs/ 의 마크다운 문서를 보기 편한 단일 HTML 로 바꾼다 (원본은 .md, .html 은 이 스크립트가 만든다)

사용: python docs/build_docs.py            docs/ 안의 모든 .md 를 같은 이름의 .html 로
      python docs/build_docs.py 파일.md ... 지정한 문서만

외부 패키지 없이 이 문서들이 쓰는 문법(제목, 문단, 표, 목록, 코드 블록, 인용, 링크, **굵게**, `코드`)만 처리한다.
HTML 은 파일 하나로 열린다 (스타일과 스크립트가 안에 들어 있다). 어두운 화면 설정을 따르고, 목차와 코드 복사 버튼이 있다."""
import html
import os
import re
import sys

DOCS = os.path.dirname(os.path.abspath(__file__))


# ---------------------------------------------------------------- 인라인

def inline(text, titles):
    codes = []

    def stash(m):
        codes.append(m.group(1))
        return '\x00%d\x00' % (len(codes) - 1)

    text = re.sub(r'`([^`]+)`', stash, text)
    text = html.escape(text, quote=False)
    text = re.sub(r'\*\*(.+?)\*\*', r'<strong>\1</strong>', text)

    def link(m):
        label, url = m.group(1), m.group(2)
        url = html.unescape(url)
        if url.endswith('.md'):
            target = url[:-3]
            url = target + '.html'
            # 파일 이름 그대로인 링크 글자는 대상 문서의 제목으로 바꾼다
            if label.strip().endswith('.md') and target in titles:
                label = html.escape(titles[target], quote=False)
        external = url.startswith('http')
        attrs = ' target="_blank" rel="noopener"' if external else ''
        return '<a href="%s"%s>%s</a>' % (html.escape(url, quote=True), attrs, label)

    text = re.sub(r'\[([^\]]+)\]\(([^)\s]+)\)', link, text)
    text = re.sub(r'\x00(\d+)\x00', lambda m: '<code>%s</code>' % html.escape(codes[int(m.group(1))], quote=False), text)
    return text


def slugify(text, used):
    slug = re.sub(r'<[^>]+>', '', text)
    slug = re.sub(r'[`*]', '', slug).strip().lower()
    slug = re.sub(r'[^\w\-가-힣 ]', '', slug)
    slug = re.sub(r'\s+', '-', slug).strip('-') or 'section'
    base, n = slug, 2
    while slug in used:
        slug = '%s-%d' % (base, n)
        n += 1
    used.add(slug)
    return slug


# ---------------------------------------------------------------- 블록

def convert(md_text, titles):
    lines = md_text.replace('\r\n', '\n').replace('﻿', '').split('\n')
    out = []
    toc = []
    used = set()
    title = ''
    i = 0
    n = len(lines)

    def is_table_sep(s):
        return bool(re.match(r'^\s*\|?\s*:?-{2,}:?\s*(\|\s*:?-{2,}:?\s*)*\|?\s*$', s)) and '|' in s

    def split_row(s):
        s = s.strip()
        if s.startswith('|'):
            s = s[1:]
        if s.endswith('|'):
            s = s[:-1]
        cells, cur, code = [], '', False
        for ch in s:
            if ch == '`':
                code = not code
            if ch == '|' and not code:
                cells.append(cur.strip())
                cur = ''
            else:
                cur += ch
        cells.append(cur.strip())
        return cells

    while i < n:
        line = lines[i]
        if not line.strip():
            i += 1
            continue

        # 코드 블록
        m = re.match(r'^```(\w*)\s*$', line)
        if m:
            lang = m.group(1)
            buf = []
            i += 1
            while i < n and not lines[i].startswith('```'):
                buf.append(lines[i])
                i += 1
            i += 1
            label = lang or '코드'
            out.append('<div class="code"><div class="code-bar"><span>%s</span><button type="button" class="copy">복사</button></div><pre><code>%s</code></pre></div>'
                       % (html.escape(label), html.escape('\n'.join(buf))))
            continue

        # 제목
        m = re.match(r'^(#{1,4})\s+(.+?)\s*$', line)
        if m:
            level = len(m.group(1))
            content = inline(m.group(2), titles)
            slug = slugify(content, used)
            if level == 1 and not title:
                title = re.sub(r'<[^>]+>', '', content)
            if level in (2, 3):
                toc.append((level, slug, re.sub(r'<[^>]+>', '', content)))
            out.append('<h%d id="%s">%s</h%d>' % (level, slug, content, level))
            i += 1
            continue

        # 구분선
        if re.match(r'^\s*-{3,}\s*$', line):
            out.append('<hr>')
            i += 1
            continue

        # 표
        if '|' in line and i + 1 < n and is_table_sep(lines[i + 1]):
            head = split_row(line)
            i += 2
            rows = []
            while i < n and lines[i].strip() and '|' in lines[i]:
                rows.append(split_row(lines[i]))
                i += 1
            t = ['<div class="table-wrap"><table><thead><tr>']
            t += ['<th>%s</th>' % inline(c, titles) for c in head]
            t.append('</tr></thead><tbody>')
            for r in rows:
                r = (r + [''] * len(head))[:len(head)]
                t.append('<tr>' + ''.join('<td>%s</td>' % inline(c, titles) for c in r) + '</tr>')
            t.append('</tbody></table></div>')
            out.append(''.join(t))
            continue

        # 인용
        if line.startswith('>'):
            buf = []
            while i < n and lines[i].startswith('>'):
                buf.append(lines[i][1:].strip())
                i += 1
            out.append('<blockquote><p>%s</p></blockquote>' % inline(' '.join(buf), titles))
            continue

        # 목록
        if re.match(r'^\s*([-*]|\d+\.)\s+', line):
            ordered = bool(re.match(r'^\s*\d+\.\s+', line))
            items = []
            while i < n:
                cur = lines[i]
                mm = re.match(r'^(\s*)([-*]|\d+\.)\s+(.*)$', cur)
                if mm:
                    items.append([mm.group(3)])
                    i += 1
                elif cur.strip() and re.match(r'^\s{2,}\S', cur) and items:
                    items[-1].append(cur.strip())
                    i += 1
                else:
                    break
            tag = 'ol' if ordered else 'ul'
            out.append('<%s>%s</%s>' % (tag, ''.join('<li>%s</li>' % inline(' '.join(it), titles) for it in items), tag))
            continue

        # 문단
        buf = []
        while i < n and lines[i].strip() and not re.match(r'^(#{1,4}\s|```|>|\s*([-*]|\d+\.)\s+|\s*-{3,}\s*$)', lines[i]) \
                and not ('|' in lines[i] and i + 1 < n and is_table_sep(lines[i + 1])):
            buf.append(lines[i].strip())
            i += 1
        if buf:
            out.append('<p>%s</p>' % inline(' '.join(buf), titles))
        else:
            i += 1

    return title, '\n'.join(out), toc


# ---------------------------------------------------------------- 템플릿

CSS = r'''
:root {
  --bg: #f7f7f4; --panel: #ffffff; --ink: #1f2328; --muted: #5d6670; --line: #d9dde1;
  --accent: #1f6feb; --accent-soft: #e3eefc; --code-bg: #f0f2f4; --pre-bg: #14181d; --pre-ink: #e6e9ed;
  --head: #111418; --ok: #1a7f37; --warn-bg: #fff4d6; --warn-line: #e2b93d;
}
@media (prefers-color-scheme: dark) {
  :root:not([data-theme="light"]) {
    --bg: #14171a; --panel: #1b1f23; --ink: #dde1e5; --muted: #98a2ad; --line: #2e353c;
    --accent: #6ea8ff; --accent-soft: #1d2b40; --code-bg: #262c32; --pre-bg: #0d1013; --pre-ink: #dde3e9;
    --head: #f2f4f6; --ok: #56d364; --warn-bg: #3a3118; --warn-line: #a8862a;
  }
}
:root[data-theme="dark"] {
  --bg: #14171a; --panel: #1b1f23; --ink: #dde1e5; --muted: #98a2ad; --line: #2e353c;
  --accent: #6ea8ff; --accent-soft: #1d2b40; --code-bg: #262c32; --pre-bg: #0d1013; --pre-ink: #dde3e9;
  --head: #f2f4f6; --ok: #56d364; --warn-bg: #3a3118; --warn-line: #a8862a;
}
* { box-sizing: border-box; }
html { scroll-behavior: smooth; scroll-padding-top: 72px; }
body {
  margin: 0; background: var(--bg); color: var(--ink);
  font: 16px/1.75 "Pretendard", "Noto Sans KR", "Malgun Gothic", "Apple SD Gothic Neo", system-ui, -apple-system, "Segoe UI", sans-serif;
  -webkit-font-smoothing: antialiased;
}
.topbar {
  position: sticky; top: 0; z-index: 10; background: color-mix(in srgb, var(--panel) 92%, transparent);
  backdrop-filter: blur(8px); border-bottom: 1px solid var(--line);
}
.topbar-inner { max-width: 1180px; margin: 0 auto; padding: 0 20px; height: 52px; display: flex; align-items: center; gap: 18px; }
.brand { font-weight: 700; color: var(--head); white-space: nowrap; }
.topnav { display: flex; gap: 4px; flex: 1; overflow-x: auto; }
.topnav a {
  padding: 6px 12px; border-radius: 8px; color: var(--muted); text-decoration: none; white-space: nowrap; font-size: 14px;
}
.topnav a:hover { background: var(--accent-soft); color: var(--accent); }
.topnav a.current { background: var(--accent-soft); color: var(--accent); font-weight: 600; }
.theme { border: 1px solid var(--line); background: var(--panel); color: var(--muted); border-radius: 8px; padding: 5px 10px; cursor: pointer; font: inherit; font-size: 13px; }
.theme:hover { color: var(--accent); border-color: var(--accent); }
.layout { max-width: 1180px; margin: 0 auto; padding: 28px 20px 80px; display: grid; grid-template-columns: 250px minmax(0, 1fr); gap: 36px; }
.toc { position: sticky; top: 76px; align-self: start; max-height: calc(100vh - 100px); overflow-y: auto; font-size: 14px; padding-right: 6px; }
.toc-title { font-size: 12px; letter-spacing: .06em; color: var(--muted); margin: 0 0 8px; font-weight: 700; }
.toc ul { list-style: none; margin: 0; padding: 0; }
.toc li a { display: block; padding: 5px 10px; border-left: 2px solid var(--line); color: var(--muted); text-decoration: none; line-height: 1.45; }
.toc li.sub a { padding-left: 24px; font-size: 13px; }
.toc li a:hover { color: var(--accent); }
.toc li a.active { color: var(--accent); border-left-color: var(--accent); background: var(--accent-soft); font-weight: 600; }
.toc-mobile { display: none; }
article {
  background: var(--panel); border: 1px solid var(--line); border-radius: 14px; padding: 36px 44px 44px; min-width: 0;
}
article h1 { font-size: 30px; line-height: 1.3; margin: 0 0 18px; color: var(--head); letter-spacing: -0.01em; }
article h2 { font-size: 23px; margin: 48px 0 14px; padding-top: 22px; border-top: 1px solid var(--line); color: var(--head); letter-spacing: -0.01em; }
article h2:first-of-type { margin-top: 30px; }
article h3 { font-size: 18px; margin: 32px 0 10px; color: var(--head); }
article h4 { font-size: 16px; margin: 22px 0 8px; color: var(--head); }
article p { margin: 12px 0; }
article ul, article ol { margin: 12px 0; padding-left: 24px; }
article li { margin: 6px 0; }
article li::marker { color: var(--accent); }
article a { color: var(--accent); text-decoration: none; border-bottom: 1px solid color-mix(in srgb, var(--accent) 35%, transparent); }
article a:hover { border-bottom-color: var(--accent); }
article hr { border: 0; border-top: 1px solid var(--line); margin: 30px 0; }
code { font: 0.88em/1.4 "Cascadia Mono", "D2Coding", Consolas, "Courier New", monospace; background: var(--code-bg); padding: 2px 6px; border-radius: 5px; word-break: break-word; }
.code { margin: 16px 0; border-radius: 10px; overflow: hidden; background: var(--pre-bg); }
.code-bar { display: flex; justify-content: space-between; align-items: center; padding: 6px 12px; font-size: 12px; color: #9aa4af; background: rgba(255,255,255,.05); }
.copy { border: 1px solid rgba(255,255,255,.2); background: transparent; color: #c9d1d9; border-radius: 6px; padding: 2px 10px; cursor: pointer; font: inherit; font-size: 12px; }
.copy:hover { background: rgba(255,255,255,.1); }
pre { margin: 0; padding: 14px 16px; overflow-x: auto; color: var(--pre-ink); }
pre code { background: none; padding: 0; font-size: 13.5px; line-height: 1.65; word-break: normal; white-space: pre; }
.table-wrap { overflow-x: auto; margin: 16px 0; border: 1px solid var(--line); border-radius: 10px; }
table { border-collapse: collapse; width: 100%; font-size: 14.5px; line-height: 1.55; }
th, td { padding: 9px 14px; text-align: left; vertical-align: top; border-bottom: 1px solid var(--line); }
th { background: var(--accent-soft); color: var(--head); font-weight: 600; white-space: nowrap; }
tbody tr:last-child td { border-bottom: 0; }
tbody tr:nth-child(even) td { background: color-mix(in srgb, var(--code-bg) 55%, transparent); }
td code, th code { white-space: nowrap; }
blockquote { margin: 16px 0; padding: 4px 18px; border-left: 4px solid var(--accent); background: var(--accent-soft); border-radius: 0 10px 10px 0; }
footer { max-width: 1180px; margin: 0 auto; padding: 0 20px 40px; color: var(--muted); font-size: 13px; }
@media (max-width: 960px) {
  .layout { grid-template-columns: minmax(0, 1fr); padding-top: 16px; }
  .toc { display: none; }
  .toc-mobile { display: block; margin-bottom: 14px; background: var(--panel); border: 1px solid var(--line); border-radius: 10px; padding: 10px 14px; font-size: 14px; }
  .toc-mobile summary { cursor: pointer; font-weight: 600; }
  .toc-mobile a { display: block; padding: 5px 0; color: var(--accent); text-decoration: none; }
  article { padding: 22px 18px 30px; border-radius: 10px; }
  article h1 { font-size: 24px; }
  article h2 { font-size: 20px; }
}
@media print {
  .topbar, .toc, .toc-mobile, .copy, .theme { display: none !important; }
  .layout { display: block; padding: 0; }
  article { border: 0; padding: 0; }
  body { background: #fff; color: #000; }
  .code { background: #f3f3f3; } pre { color: #000; } .code-bar { display: none; }
}
'''

JS = r'''
(function () {
  var root = document.documentElement;
  try { var saved = localStorage.getItem('docs-theme'); if (saved) root.setAttribute('data-theme', saved); } catch (e) {}
  var btn = document.querySelector('.theme');
  function current() {
    var t = root.getAttribute('data-theme');
    if (t) return t;
    return window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light';
  }
  function label() { btn.textContent = current() === 'dark' ? '밝게' : '어둡게'; }
  if (btn) {
    label();
    btn.addEventListener('click', function () {
      var next = current() === 'dark' ? 'light' : 'dark';
      root.setAttribute('data-theme', next);
      try { localStorage.setItem('docs-theme', next); } catch (e) {}
      label();
    });
  }
  document.querySelectorAll('.copy').forEach(function (b) {
    b.addEventListener('click', function () {
      var code = b.closest('.code').querySelector('code').innerText;
      var done = function () { b.textContent = '복사됨'; setTimeout(function () { b.textContent = '복사'; }, 1400); };
      if (navigator.clipboard && navigator.clipboard.writeText) { navigator.clipboard.writeText(code).then(done, done); }
      else { var t = document.createElement('textarea'); t.value = code; document.body.appendChild(t); t.select(); try { document.execCommand('copy'); } catch (e) {} document.body.removeChild(t); done(); }
    });
  });
  var links = document.querySelectorAll('.toc a');
  var map = {};
  links.forEach(function (a) { map[a.getAttribute('href').slice(1)] = a; });
  var heads = Array.prototype.slice.call(document.querySelectorAll('article h2, article h3')).filter(function (h) { return map[h.id]; });
  function onScroll() {
    var y = window.scrollY + 110, cur = null;
    heads.forEach(function (h) { if (h.offsetTop <= y) cur = h; });
    links.forEach(function (a) { a.classList.remove('active'); });
    if (cur && map[cur.id]) { map[cur.id].classList.add('active'); }
  }
  window.addEventListener('scroll', onScroll, { passive: true });
  onScroll();
})();
'''


def page(title, body, toc, name, docs):
    nav = ''.join('<a href="%s.html"%s>%s</a>' % (n, ' class="current"' if n == name else '', label) for n, label in docs)
    toc_items = ''.join('<li class="%s"><a href="#%s">%s</a></li>' % ('sub' if lvl == 3 else '', slug, html.escape(text)) for lvl, slug, text in toc)
    mobile = ''.join('<a href="#%s">%s</a>' % (slug, html.escape(text)) for lvl, slug, text in toc if lvl == 2)
    return '''<!doctype html>
<html lang="ko">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>%(title)s</title>
<style>%(css)s</style>
</head>
<body>
<header class="topbar"><div class="topbar-inner">
  <span class="brand">ToolForClickteamFusion 문서</span>
  <nav class="topnav">%(nav)s</nav>
  <button type="button" class="theme">어둡게</button>
</div></header>
<div class="layout">
  <aside class="toc"><p class="toc-title">목차</p><ul>%(toc)s</ul></aside>
  <main>
    <details class="toc-mobile"><summary>목차</summary>%(mobile)s</details>
    <article>
%(body)s
    </article>
  </main>
</div>
<footer>이 문서는 <code>docs/%(name)s.md</code> 에서 <code>docs/build_docs.py</code> 로 만든 것입니다. 고칠 때는 .md 를 고치고 다시 만드세요.</footer>
<script>%(js)s</script>
</body>
</html>
''' % {'title': html.escape(title), 'css': CSS, 'nav': nav, 'toc': toc_items, 'mobile': mobile, 'body': body, 'name': name, 'js': JS}


def main():
    targets = [os.path.abspath(a) for a in sys.argv[1:]]
    if not targets:
        targets = [os.path.join(DOCS, f) for f in sorted(os.listdir(DOCS)) if f.endswith('.md')]
    # 위쪽 메뉴에 나올 문서 목록 (제목은 각 문서의 첫 # 제목)
    docs = []
    for path in sorted(os.path.join(DOCS, f) for f in os.listdir(DOCS) if f.endswith('.md')):
        text = open(path, encoding='utf-8-sig').read()
        m = re.search(r'^#\s+(.+)$', text, re.M)
        docs.append((os.path.splitext(os.path.basename(path))[0], m.group(1).strip() if m else os.path.basename(path)))
    # 작업 흐름 문서를 먼저
    docs.sort(key=lambda d: (0 if '작업흐름' in d[0] else 1, d[0]))
    titles = dict(docs)
    for path in targets:
        name = os.path.splitext(os.path.basename(path))[0]
        text = open(path, encoding='utf-8-sig').read()
        title, body, toc = convert(text, titles)
        out = os.path.join(os.path.dirname(path), name + '.html')
        with open(out, 'w', encoding='utf-8', newline='\n') as f:
            f.write(page(title or name, body, toc, name, docs))
        print('만듦:', out)


if __name__ == '__main__':
    main()
