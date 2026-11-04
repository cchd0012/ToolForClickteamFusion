import json, re, glob, os, sys

BASE = os.path.dirname(os.path.abspath(__file__))
HANGUL = re.compile('[가-힣ㄱ-ㆎ]')


def load_frame(version_dir, idx):
    path = os.path.join(BASE, version_dir, 'script', 'frame', '%06d.json' % idx)
    with open(path, encoding='utf-8') as f:
        return json.load(f)


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


def string_tokens(group):
    """그룹 안의 문자열 토큰을 순서대로 (조건/동작, 이벤트 위치, 파라미터 위치, 토큰 위치, 값) 로"""
    res = []
    for kind in ('conditions', 'actions'):
        for ei, ev in enumerate(group[kind]):
            for pi, p in enumerate(ev.get('params', [])):
                for ti, t in enumerate(p.get('tokens', []) if isinstance(p, dict) else []):
                    if t.get('kind') == 'string':
                        res.append((kind, ei, pi, ti, t['value']))
    return res


def frame_groups(version_dir, idx):
    d = load_frame(version_dir, idx)
    return groups_of(d['events'], [])
