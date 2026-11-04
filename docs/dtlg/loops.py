"""프레임 이벤트의 루프 호출 그래프에서 '지연 없는 재호출 순환'(무한 루프로 멈출 수 있는 곳)을 찾는다.
조건 (objectType -4, num -6)  = On loop <이름>
동작 (objectType -4, num 1)   = Start loop 이름 (파라미터: 시간(지연), 이름)
동작 (objectType -4, num 2)   = 지연/반복 시작 (파라미터: 지연, 반복 횟수식, 간격 또는 횟수, 이름)"""
import sys, json, collections
from tokens import *

ver = sys.argv[1]
frames = [int(x) for x in sys.argv[2].split(',')] if len(sys.argv) > 2 else range(31)


def const_name(p):
    if not isinstance(p, dict):
        return None
    toks = p.get('tokens', [])
    if len(toks) == 1 and toks[0].get('kind') == 'string':
        return toks[0]['value']
    return None


def delay_ms(p):
    if isinstance(p, dict) and p.get('kind') == 'time':
        return p.get('timer') or 0
    return None


def tarjan(graph):
    index = {}; low = {}; onstack = set(); stack = []; res = []; idx = [0]

    def sc(v):
        index[v] = low[v] = idx[0]; idx[0] += 1; stack.append(v); onstack.add(v)
        for w in graph.get(v, ()):
            if w not in index:
                sc(w); low[v] = min(low[v], low[w])
            elif w in onstack:
                low[v] = min(low[v], index[w])
        if low[v] == index[v]:
            comp = []
            while True:
                w = stack.pop(); onstack.discard(w); comp.append(w)
                if w == v:
                    break
            res.append(comp)
    sys.setrecursionlimit(10000)
    for v in list(graph):
        if v not in index:
            sc(v)
    return res


total = 0
for fr in frames:
    gs = frame_groups(ver, fr)
    graph = collections.defaultdict(set)
    edges = collections.defaultdict(list)
    for g in gs:
        on = [const_name(c['params'][0]) for c in g['conditions'] if c['objectType'] == -4 and c['num'] == -6 and c.get('params')]
        on = [n for n in on if n]
        if not on:
            continue
        others = [c for c in g['conditions'] if not (c['objectType'] == -4 and c['num'] == -6)]
        for a in g['actions']:
            if a['objectType'] != -4 or a['num'] not in (1, 2):
                continue
            ps = a.get('params', [])
            if a['num'] == 1:
                dly, name = delay_ms(ps[0]) if ps else None, const_name(ps[1]) if len(ps) > 1 else None
            else:
                dly, name = delay_ms(ps[0]) if ps else None, const_name(ps[3]) if len(ps) > 3 else None
            if name is None or dly is None:
                continue
            if dly > 0:
                continue
            for o in on:
                graph[o].add(name)
                edges[(o, name)].append((g['line'], g['uid'], len(others)))
    for comp in tarjan(graph):
        cyc = len(comp) > 1 or (comp[0] in graph.get(comp[0], ()))
        if cyc:
            total += 1
            print('frame', fr, 'zero-delay loop cycle:', comp[:8])
            for a in comp:
                for b in graph[a]:
                    if b in comp:
                        print('   ', a, '->', b, 'groups(line,uid,extraConditions):', edges[(a, b)][:4])
print('cycles', total)
