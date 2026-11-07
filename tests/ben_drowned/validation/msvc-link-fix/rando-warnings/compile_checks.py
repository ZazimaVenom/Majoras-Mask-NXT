from pathlib import Path
import json,subprocess,concurrent.futures
root=Path('/workspace/scratch/cda0e8a05ecb/source/Majoras-Mask-NXT')
out=Path(__file__).resolve().parent
inc=['mm','mm/2s2h','mm/include','mm/src','mm/assets','libultraship/include','libultraship/include/libultraship','mm/include/PR','libultraship/src','libultraship/extern/fast3d/include']
common=['-DF3DEX_GBI_2','-DCONTROLLERBUTTONS_T=uint32_t']+['-I'+str(root/p) for p in inc]+['-I/workspace/scratch/cda0e8a05ecb/validation/dependencies/'+p for p in ['nlohmann_json/include','imgui','spdlog/include']]
(out/'headers.cpp').write_text('#include "Rando/CheckTracker/CheckTracker.h"\n#include "Rando/Logic/Logic.h"\nstatic_assert(SCENE_LOAD == 0);\nstatic_assert(SCENE_UPDATE == 1);\n')
jobs=[('headers',['g++','-std=c++20','-Wall','-Wextra','-Werror=sign-compare','-Werror=ignored-qualifiers',*common,'-c',str(out/'headers.cpp'),'-o',str(out/'headers.o')]),('menu',['g++','-std=c++20','-Wall','-Wextra','-Werror=format',*common,'-c','mm/2s2h/Rando/Menu.cpp','-o',str(out/'Menu.o')]),('mixer',['gcc','-std=gnu11','-Werror=unknown-pragmas',*common,'-c','mm/2s2h/mixer.c','-o',str(out/'mixer.o')]),('kaleido',['gcc','-std=gnu11','-Werror=unknown-pragmas',*common,'-c','mm/src/overlays/kaleido_scope/ovl_kaleido_scope/z_kaleido_scope_NES.c','-o',str(out/'kaleido.o')])]
def run(job):
 name,cmd=job;r=subprocess.run(cmd,cwd=root,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(out/(name+'.log')).write_text(r.stdout);return {'name':name,'command':cmd,'returncode':r.returncode,'log':str(out/(name+'.log')),'tail':r.stdout[-1800:]}
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as e: results=list(e.map(run,jobs))
(out/'compile-report.json').write_text(json.dumps(results,indent=2))
for r in results:print(r['name'],r['returncode'],r['tail'])
