"""시험용 변형: 시작하자마자 지정한 장면으로 이동 + 플레이어 HP 가 줄지 않게 한다 (프레임 JSON 편집).
사용: python make_debug.py <추출 폴더> <이동 값 N> [--nodamage 프레임:uid,uid,...;프레임:uid,...]"""
import json, os, sys

root = sys.argv[1]
target = int(sys.argv[2])
nodmg = {}
if len(sys.argv) > 3 and sys.argv[3] == '--nodamage':
    for part in sys.argv[4].split(';'):
        fr, uids = part.split(':')
        nodmg[int(fr)] = [int(u) for u in uids.split(',')]


def groups(node, out):
    if isinstance(node, dict):
        if 'conditions' in node and 'actions' in node and 'uid' in node:
            out.append(node)
        for v in node.values():
            groups(v, out)
    elif isinstance(node, list):
        for v in node:
            groups(v, out)
    return out


def load(i):
    p = os.path.join(root, 'script', 'frame', '%06d.json' % i)
    return p, json.load(open(p, encoding='utf-8'))


def save(p, d):
    json.dump(d, open(p, 'w', encoding='utf-8'), ensure_ascii=False, indent=1)


# 프레임 0 의 시작 이동 값을 바꾼다 (uid 12)
p, d = load(0)
n = 0
for g in groups(d['events'], []):
    if g['uid'] == 12:
        for ev in g['actions']:
            if ev['objectType'] == -3 and ev['num'] == 2:
                ev['params'][0]['value'] = target
                n += 1
save(p, d)
print('frame 0 jump ->', target, 'changed', n)

for fr, uids in nodmg.items():
    p, d = load(fr)
    c = 0
    for g in groups(d['events'], []):
        if g['uid'] in uids:
            for ev in g['actions']:
                if ev['objectType'] == 7 and ev['num'] == 82:
                    for prm in ev['params']:
                        if prm.get('kind') == 'expression':
                            prm['tokens'] = [{'kind': 'long', 'num': 0, 'objectType': -1, 'value': 0}]
                            prm['text'] = '0'
                            prm.pop('tail', None)
                            c += 1
    save(p, d)
    print('frame', fr, 'no damage edits', c)
