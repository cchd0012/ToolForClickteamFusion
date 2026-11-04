"""번역 뒤에도 '접두 비교'(전체 대사의 앞부분만 비교하는 조건)가 맞는지 확인한다.
영어 1.1.0 에서 조건 문자열 S 가 어떤 설정 문자열 F 의 앞부분이면, 번역한 F 도 번역한 S 로 시작해야 한다."""
import json,sys,re
from tokens import *
ft=json.load(open(sys.argv[1]+'/text/frameEventText.json',encoding='utf-8'))['texts']
tr={e['original']:(e['translation'] or e['original']) for e in ft}
def toks(p):
    return [t['value'] for t in p.get('tokens',[]) if t.get('kind')=='string'] if isinstance(p,dict) else []
bad=0; total=0
for fr in range(31):
    gs=frame_groups('v110/dtlg110',fr)
    sets=set()
    for g in gs:
        for a in g['actions']:
            if a['objectType']==3 and a['num']==88:
                for p in a.get('params',[]): sets.update(toks(p))
    conds=set()
    for g in gs:
        for c in g['conditions']:
            if c['objectType']==-1 and c['num']==-3 and len(c.get('params',[]))>1:
                conds.update(toks(c['params'][1]))
    for s in conds:
        if s in sets or len(s)<4: continue
        fulls=[f for f in sets if f.startswith(s) and f!=s]
        for f in fulls:
            total+=1
            ts,tf=tr.get(s,s),tr.get(f,f)
            if not tf.startswith(ts):
                bad+=1; print('frame',fr,'접두 비교 어긋남: 조건',repr(ts[:40]),'| 설정',repr(tf[:60]))
print('prefix pairs',total,'violations',bad)
