"""한글판에서 '비교 조건에 쓰인 문자열'이 '그 문자열을 정하는 동작'에 하나도 없는 곳을 찾는다.
조건: (objectType -1, num -3) 값 비교의 두 번째 파라미터가 문자열 상수
동작: (objectType 3, num 88) 텍스트 설정의 문자열 상수"""
import sys,json,re,collections
from tokens import *
ver=sys.argv[1]
H=re.compile('[\uac00-\ud7a3]')
def strs_of(p):
    return [t['value'] for t in p.get('tokens',[]) if t.get('kind')=='string'] if isinstance(p,dict) else []
tot=0
for fr in range(31):
    gs=frame_groups(ver,fr)
    set_strings=collections.Counter()
    for g in gs:
        for a in g['actions']:
            if a['objectType']==3 and a['num']==88 or (a['objectType']==2 and a['num'] in (31,)):
                for p in a.get('params',[]):
                    for s in strs_of(p): set_strings[s]+=1
    for g in gs:
        for c in g['conditions']:
            if c['objectType']==-1 and c['num']==-3:
                for p in c.get('params',[])[1:]:
                    for s in strs_of(p):
                        if len(s)>3 and s not in set_strings:
                            tot+=1
                            print('frame',fr,'line',g['line'],'compare-string not set anywhere in frame:',repr(s[:70]))
print('total',tot)
