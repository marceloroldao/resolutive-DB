#!/usr/bin/env python3
"""Aggregate independent runs; retain raw CSV untouched."""
import csv,collections,pathlib,statistics,json
here=pathlib.Path(__file__).resolve().parent
rows=[]
for directory in ['results','supplemental','recheck']:
 p=here/directory/'raw.csv'
 if p.exists():
  for r in csv.DictReader(p.open()):r['suite']=directory;rows.append(r)
groups=collections.defaultdict(list)
for r in rows:groups[(r['suite'],int(r['N']),r['workload'],r['engine'])].append(r)
fields=list(rows[0]) if rows else []
with (here/'summary.csv').open('w') as f:
 w=csv.DictWriter(f,fieldnames=['runs']+fields,lineterminator='\n');w.writeheader()
 for (suite,n,workload,engine),rs in sorted(groups.items()):
  out={'runs':len(rs),'N':n,'workload':workload,'engine':engine,'suite':suite}
  for k in fields:
   if k in out:continue
   vals=[r[k] for r in rs]
   out[k]='NA' if 'NA' in vals else statistics.median(float(v) for v in vals)
  w.writerow(out)
# Route-depth/probe/latency are three separate plots. No O(1) inference from a line.
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
fig,axes=plt.subplots(1,3,figsize=(13,4))
for eng in ['robin','fixed1','fixed2','adaptive']:
 points=[]
 for n in [100000,1000000,10000000]:
  rs=groups.get(('results',n,'uniform',eng),[])
  if rs:points.append((n,*[statistics.median(float(r[k]) for r in rs) for k in ['depth','probes','p50_ns']]))
 for ax,col in zip(axes,[1,2,3]):ax.plot([p[0] for p in points],[p[col] for p in points],'-o',label=eng);ax.set_xscale('log');ax.set_xlabel('Records N');ax.grid(alpha=.25)
for ax,title in zip(axes,['Routing decisions','Local probes / hit','GET p50 (ns)']):ax.set_title(title)
axes[0].legend(fontsize=8);fig.suptitle('Same encoder and exact-ID Robin Hood leaves; uniform dispersed keys')
fig.tight_layout();fig.savefig(here/'scaling.png',dpi=160);plt.close(fig)
# Machine-readable coverage and strict integrity checks.
coverage={'rows':len(rows),'groups':len(groups),'all_sampled_fidelity_pass':all(r['fidelity']=='1' for r in rows),'seeds':sorted(set(r['seed'] for r in rows))}
(here/'coverage.json').write_text(json.dumps(coverage,indent=2))
print(json.dumps(coverage))
