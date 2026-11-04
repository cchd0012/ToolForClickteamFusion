import sys,json,collections
from tokens import *
def chain(ver,fr):
    d=collections.defaultdict(list)
    for g in frame_groups(ver,fr):
        for c in g['conditions']:
            if c['objectType']==-1 and c['num']==-3 and c.get('params') and len(c['params'])>1:
                left=c['params'][0].get('text','')
                for t in c['params'][1].get('tokens',[]):
                    if t.get('kind')=='string':
                        d[(left.split('.exp')[0],t['value'])].append(g['line'])
    return d
tot=0
for fr in range(31):
    de=chain('v110/dtlg110',fr); dk=chain('v110k/dtlg110k',fr); d5=chain('v105k/dtlg105k',fr)
    ee={k for k,v in de.items() if len(v)>1}
    kk={k for k,v in dk.items() if len(v)>1}
    for k in sorted(kk):
        if len(k[1])>3:
            tot+=1
            # 영어에서도 중복이었나? (영어 키로는 못 찾으므로 줄 번호로 비교)
            print('frame',fr,'새 한글판 중복 조건',k[0],repr(k[1][:40]),dk[k][:4])
print('total',tot)

print('---- 번역 때문에 생긴 중복만')
n=0
for fr in range(31):
    de=chain('v110/dtlg110',fr); dk=chain('v110k/dtlg110k',fr)
    en_lines=[set(v) for k,v in de.items() if len(v)>1]
    for k,v in dk.items():
        if len(v)>1 and len(k[1])>3:
            if not any(set(v)<=s for s in en_lines):
                n+=1; print('frame',fr,k[0],repr(k[1][:40]),v)
print('translation-introduced duplicates',n)
