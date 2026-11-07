"""1.1.0 추출 폴더에 1.0.5 한글판의 번역/폰트/이미지를 옮긴다.
사용: python build_ko.py <새 작업 폴더>   (v110/dtlg110 를 복사해서 만든다)"""
import json, os, re, shutil, sys, hashlib, glob, collections

BASE = os.path.dirname(os.path.abspath(__file__))
SRC_NEW = os.path.join(BASE, 'v110', 'dtlg110')
SRC_OLD = os.path.join(BASE, 'v105k', 'dtlg105k')
EX = os.environ.get('DTLG_EX_FONT', r'ex_font')  # 이전 한글판 작업의 ex_font 폴더 (번역된 TTF 와 NNNNNN_old.ttf 원본)
HANGUL = re.compile('[\uac00-\ud7a3\u3131-\u318e]')

out = sys.argv[1]

# ---- 안전장치: 작업 폴더는 아래에서 지우고 다시 만든다. 입력 폴더가 그 안에 있거나, 비어 있지 않은데 이 스크립트가 만든 폴더가 아니면 지우지 않고 멈춘다
MARKER = '.build_ko_work'


def _norm(p):
    return os.path.normcase(os.path.abspath(p))


_out = _norm(out)
for _p in (SRC_NEW, SRC_OLD, EX, BASE):
    _n = _norm(_p)
    if _n == _out or _n.startswith(_out + os.sep):
        sys.exit('오류: 작업 폴더 안에 입력 폴더가 있어서 중단합니다. 새로운 빈 폴더를 지정하세요.\n  작업 폴더: %s\n  입력: %s' % (out, _p))
if os.path.exists(out) and os.listdir(out) and not os.path.exists(os.path.join(out, MARKER)):
    sys.exit('오류: 작업 폴더가 비어 있지 않고 이 스크립트가 만든 폴더도 아니라서 지우지 않고 중단합니다. 새로운 빈 폴더를 지정하세요.\n  작업 폴더: %s' % out)

if os.path.exists(out):
    shutil.rmtree(out)
shutil.copytree(SRC_NEW, out)
open(os.path.join(out, MARKER), 'w', encoding='utf-8').write('build_ko 가 만든 작업 폴더. 다음 실행 때 지우고 다시 만든다\n')
log = []


def md5(p):
    return hashlib.md5(open(p, 'rb').read()).hexdigest()


# ---------------------------------------------------------------- 1. 문구 (이벤트)
align = json.load(open(os.path.join(BASE, 'align_text.json'), encoding='utf-8'))
pairs = align['pairs']            # 새 영어 -> {한글판 문자열: 횟수}
manual = {
    # 위치로 짝지어지지 않은 곳 (프레임 5, 6, 10/13, 27~29)
    'omg getr is that you': '오마이갓 getr 너야?',
    '* Spare': '* 자비',
    'see you in hell, kid.': '지옥에서 보자, 꼬맹아.',
    "is that all you've got?": '그게 네 전부야?',
    'some people never learn...': '어떤 사람들은 절대로 배우지 않지...',
    'some things never change...': '어떤 것들은 절대로 바뀌지 않지...',
    "that's what you deserve.": '넌 마땅히 치러야 할 대가를 받은 거야.',
    "their deaths won't be in vain.": '그들의 죽음은 헛되지 않을 거야.',
    'i know you will return.': '어차피 다시 돌아올 거 알아.',
    'HE HE HE HAW': '녜 헤 헤 헤',
    'do NOT come back.': '다시는 돌아오지 마.',
    # 한글판 안에서 서로 다르게 번역돼 있던 곳은 하나로 맞춘다
    'whatever limit you\r\nwant to break so\r\nbadly, kiddo...': '네가 그토록\r\n간절히 깨고 싶은\r\n한계가 있다면...',
    '* Sans - ??? ATK ??? DEF': '* 샌즈 - ??? ATK ??? DEF',
    '* Sans - ??? ATK ??? DEF\r\n* His time is up.': '* 샌즈 - ??? ATK ??? DEF\r\n* 샌즈의 시간은 끝났다.',
    # 1.1.0 에서 새로 생겼거나 한글판이 빠뜨린 표시 문구 (초안: 번역자 확인 필요)
    '* Nice moves, partner...': '* 좋은 움직임이야, 파트너...',
    '* But we have some work to do.': '* 하지만 우린 아직 할 일이 남아 있어.',
    'no hit': '노히트',
    'no heal': '노힐',
    'no item': '노아이템',
    'no hit: on': '노히트: ON',
    'no hit: off': '노히트: OFF',
    ' left': ' 남음',
}
DRAFT = ['* Nice moves, partner...', '* But we have some work to do.', 'no hit', 'no heal', 'no item', 'no hit: on', 'no hit: off', ' left']
keep_english = {'...'}            # '...' 는 번역하지 않는다 (한글판의 '널 사랑해 <3' 는 위치 하나만의 변경이라 따로 다룬다)
ascii_override = {}

trans = {}
for en, v in pairs.items():
    if en in keep_english:
        continue
    kos = [k for k in v if HANGUL.search(k)]
    if kos:
        trans[en] = max(kos, key=lambda k: v[k])
for en, ko in manual.items():
    trans[en] = ko
for en, ko in ascii_override.items():
    trans[en] = ko

ft_path = os.path.join(out, 'text', 'frameEventText.json')
ft = json.load(open(ft_path, encoding='utf-8'))
applied = 0
for e in ft['texts']:
    ko = trans.get(e['original'])
    if ko is not None and ko != e['original']:
        e['translation'] = ko
        applied += 1
json.dump(ft, open(ft_path, 'w', encoding='utf-8'), ensure_ascii=False, indent=2)
log.append('frameEventText: %d 개 번역 적용 (표 크기 %d)' % (applied, len(trans)))
missing = [en for en in trans if en not in {e['original'] for e in ft['texts']}]
log.append('초안 번역(확인 필요): %s' % [d for d in DRAFT])
log.append('번역표에는 있으나 1.1.0 에 없는 문자열: %d' % len(missing))


# ---------------------------------------------------------------- 1-2. 위치별 보정 (같은 문자열이라도 한글판이 곳마다 다르게 둔 것)
def groups_of(node, out):
    if isinstance(node, dict):
        if 'conditions' in node and 'actions' in node and 'uid' in node:
            out.append(node)
        for v in node.values():
            groups_of(v, out)
    elif isinstance(node, list):
        for v in node:
            groups_of(v, out)
    return out


def patch_strings(frame, match, old, new):
    """프레임의 '텍스트 설정' 동작 중 문자열이 old 인 것을, match(group) 가 참인 그룹에서 new 로 바꾼다"""
    p = os.path.join(out, 'script', 'frame', '%06d.json' % frame)
    d = json.load(open(p, encoding='utf-8'))
    n = 0
    for g in groups_of(d['events'], []):
        if not match(g):
            continue
        for ev in g['actions']:
            if ev['objectType'] != 3 or ev['num'] != 88:
                continue
            for prm in ev.get('params', []):
                for t in prm.get('tokens', []):
                    if t.get('kind') == 'string' and t['value'] == old:
                        t['value'] = new
                        prm['text'] = prm.get('text', '').replace('"' + old + '"', '"' + new + '"')
                        n += 1
    json.dump(d, open(p, 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
    return n


def cond_text(g, idx=0):
    params = g['conditions'][idx].get('params', []) if len(g['conditions']) > idx else []
    return params[0].get('text') if params else None


# 크레딧: 한글판은 앞의 6곳을 '| ' 로, 마지막(조건 220)만 'by ' 로 두었다
n1 = patch_strings(25, lambda g: cond_text(g) in ('156', '160', '163', '168', '204'), 'by ', '| ')
# 이름 SOUP 반응: 한글판의 농담 문구를 유지한다
n2 = patch_strings(5, lambda g: g['conditions'] and any(
    c.get('params') and len(c['params']) > 1 and c['params'][1].get('text') == '"SOUP"' for c in g['conditions']), '...', '널 사랑해 <3')
log.append('위치별 보정: 크레딧 %d, SOUP %d' % (n1, n2))

# ---------------------------------------------------------------- 2. 문구 (오브젝트)
import difflib
a = json.load(open(os.path.join(SRC_OLD, 'text', 'objectText.json'), encoding='utf-8'))['objects']
ot_path = os.path.join(out, 'text', 'objectText.json')
ot = json.load(open(ot_path, encoding='utf-8'))
b = ot['objects']
sm = difflib.SequenceMatcher(None, [o['name'] for o in a], [o['name'] for o in b], autojunk=False)
n_obj = 0
for tag, a0, a1, b0, b1 in sm.get_opcodes():
    if tag != 'equal':
        continue
    for k in range(a1 - a0):
        oa, ob = a[a0 + k], b[b0 + k]
        for pb in ob['paragraphs']:
            pa = next((q for q in oa['paragraphs'] if q['index'] == pb['index']), None)
            if pa and HANGUL.search(pa['original']):
                pb['translation'] = pa['original']
                n_obj += 1
# 1.1.0 에서 이벤트 문자열이 오브젝트 문단으로 옮겨간 것 (한글판 이벤트 문구를 그대로 쓴다)
CRLF = chr(13) + chr(10)
MOVED = {'HARDMODE will be available' + CRLF + 'in future updates!': '하드모드는 추후 업데이트에서' + CRLF + '플레이할 수 있습니다!'}
for ob in ot['objects']:
    for pb in ob['paragraphs']:
        if pb['original'] in MOVED and not pb.get('translation'):
            pb['translation'] = MOVED[pb['original']]
            n_obj += 1
json.dump(ot, open(ot_path, 'w', encoding='utf-8'), ensure_ascii=False, indent=2)
log.append('objectText: %d 개 문단 번역 적용' % n_obj)

# ---------------------------------------------------------------- 3. 폰트 (font/ttf)
old_hash = {}
for f in os.listdir(EX):
    if f.endswith('_old.ttf'):
        old_hash[f[:6]] = md5(os.path.join(EX, f))
replaced = []
for p in sorted(glob.glob(glob.escape(os.path.join(out, 'font', 'ttf')) + '/*.ttf')):
    h = md5(p)
    src = [i for i, v in old_hash.items() if v == h]
    if src:
        shutil.copyfile(os.path.join(EX, src[0] + '.ttf'), p)
        replaced.append(os.path.basename(p) + '<-' + src[0])
log.append('font/ttf 교체: ' + ', '.join(replaced))
for rel in ('font/ttf/info.json', 'font/logFontInfo.json'):
    p = os.path.join(out, rel)
    data = json.load(open(p, encoding='utf-8-sig'))
    for e in data:
        e['charset'] = 129
    json.dump(data, open(p, 'w', encoding='utf-8'), ensure_ascii=False, indent='\t')
log.append('LogFont charset -> 129')

# ---------------------------------------------------------------- 4. 이미지
A = json.load(open(os.path.join(SRC_OLD, 'image', 'imageList.json'), encoding='utf-8'))
B = json.load(open(os.path.join(out, 'image', 'imageList.json'), encoding='utf-8'))
ia = {i['handle']: i for i in A['images']}
ib = {i['handle']: i for i in B['images']}
# 한글판 이미지 핸들 -> 1.1.0 핸들 (같은 핸들 + 핸들이 바뀐 것을 눈으로 확인해 짝지은 것)
IMG_MAP = {h: h for h in (227, 228, 243, 261, 244, 32, 45, 593, 894, 923, 969, 970, 925, 972, 1080, 927, 775, 151, 1187, 1399)}
IMG_MAP.update({33: 806, 35: 828, 1299: 1526, 1301: 1527, 1309: 1528, 1400: 1184, 1488: 1516})
done = []
for old_h, new_h in IMG_MAP.items():
    a_, b_ = ia.get(old_h), ib.get(new_h)
    if not a_ or not b_:
        log.append('이미지 핸들 %d -> %d 없음' % (old_h, new_h))
        continue
    shutil.copyfile(os.path.join(SRC_OLD, 'image', a_['file']), os.path.join(out, 'image', b_['file']))
    done.append('%d->%d' % (old_h, new_h))
log.append('이미지 교체 핸들: %s' % done)

print('\n'.join(log))
