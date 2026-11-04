"""1.0.5 한글판의 문자열을 1.1.0 영어 문자열에 짝지어 번역표를 만든다 (프레임별 순서 정렬)"""
import difflib, json, os, sys, collections
from tokens import *

NEW = 'v110/dtlg110'
OLD = 'v105k/dtlg105k'

pairs = collections.defaultdict(collections.Counter)    # 새 영어 -> {한글판 문자열: 횟수}
unresolved = []                                           # (프레임, 설명)
frame_stats = []

for i in range(31):
    a = [t[4] for g in frame_groups(OLD, i) for t in string_tokens(g)]
    b = [t[4] for g in frame_groups(NEW, i) for t in string_tokens(g)]
    sm = difflib.SequenceMatcher(None, a, b, autojunk=False)
    paired = same = 0
    for tag, a0, a1, b0, b1 in sm.get_opcodes():
        if tag == 'equal':
            for k in range(a1 - a0):
                pairs[b[b0 + k]][a[a0 + k]] += 1
            same += a1 - a0
        elif tag == 'replace':
            if a1 - a0 == b1 - b0:
                for k in range(a1 - a0):
                    pairs[b[b0 + k]][a[a0 + k]] += 1
                paired += a1 - a0
            else:
                unresolved.append((i, 'replace', a[a0:a1], b[b0:b1]))
        elif tag == 'delete':
            unresolved.append((i, 'old-only', a[a0:a1], []))
        elif tag == 'insert':
            unresolved.append((i, 'new-only', [], b[b0:b1]))
    frame_stats.append((i, len(a), len(b), same, paired))

json.dump({'pairs': {k: dict(v) for k, v in pairs.items()}, 'unresolved': unresolved, 'stats': frame_stats},
          open('align_text.json', 'w', encoding='utf-8'), ensure_ascii=False, indent=1)

changed = {k: v for k, v in pairs.items() if any(x != k for x in v)}
conflict = {k: v for k, v in pairs.items() if len(v) > 1}
print('새 문자열(고유)', len(pairs), '한글판과 다른 것', len(changed), '충돌(여러 번역)', len(conflict), '미해결 블록', len(unresolved))
for s in frame_stats:
    print(s)
