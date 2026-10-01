#!/usr/bin/env python3
import argparse,csv,json,os,pathlib,platform,subprocess,time
p=argparse.ArgumentParser();p.add_argument('--sizes',default='100000,1000000,10000000');p.add_argument('--repeats',type=int,default=3);p.add_argument('--q',type=int,default=20000);p.add_argument('--engines',default='stable,compact,atomic_map,robin,unordered,fixed1,fixed2,fixed3,adaptive,adaptive64,adaptive1024');p.add_argument('--workloads',default='uniform,prefix,zipf,rho,rho_phi,fp8,poor');p.add_argument('--out',default='results');a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[2];here=pathlib.Path(__file__).resolve().parent;out=here/a.out;out.mkdir(exist_ok=True)
cmd=['g++','-O3','-std=c++17','-pthread','-Iexperimental/api_v86/include',str(here/'bench.cpp'),'experimental/api_v86/src/resolutive_index.cpp','-o',str(out/'bench')]
subprocess.run(cmd,cwd=root,check=True)
meta={'head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),'platform':platform.platform(),'compiler':subprocess.check_output(['g++','--version'],text=True),'cpu':subprocess.check_output(['lscpu'],text=True),'build':cmd,'args':vars(a),'memory_max':pathlib.Path('/sys/fs/cgroup/memory.max').read_text(),'cpu_max':pathlib.Path('/sys/fs/cgroup/cpu.max').read_text(),'clock':time.get_clock_info('perf_counter').__dict__}
(out/'environment.json').write_text(json.dumps(meta,indent=2))
header='N,workload,engine,seed,Q,put_ops_s,get_ops_s,p50_ns,p95_ns,p99_ns,preaddress_ns,depth,probes,comparisons,fp_false_matches,query_leaf_population,leaves,max_leaf_population,put_collision_events,structural_bytes_record,rss_bytes,rss_delta_bytes,peak_rss_bytes,cpu_s,fidelity,miss_ops_s,address_ns,route_ns'
with (out/'raw.csv').open('w') as f,(out/'runs.jsonl').open('w') as log:
 f.write(header+'\n');f.flush()
 for n in map(int,a.sizes.split(',')):
  for w in a.workloads.split(','):
   for eng in a.engines.split(','):
    if eng in ['stable','compact','atomic_map'] and w not in ['uniform','prefix','zipf']:continue
    if w in ['fp8','poor'] and n>100000:continue
    for rep in range(a.repeats):
     command=[str(out/'bench'),str(n),w,eng,str(42+rep),str(a.q)];t=time.monotonic()
     try:r=subprocess.run(command,cwd=root,text=True,capture_output=True,timeout=180)
     except subprocess.TimeoutExpired:log.write(json.dumps({'command':command,'timeout':180})+'\n');log.flush();continue
     log.write(json.dumps({'command':command,'returncode':r.returncode,'elapsed_s':time.monotonic()-t,'stderr':r.stderr})+'\n');log.flush()
     if r.returncode:raise RuntimeError(command,r.stderr)
     f.write(r.stdout);f.flush();print(n,w,eng,rep,flush=True)
