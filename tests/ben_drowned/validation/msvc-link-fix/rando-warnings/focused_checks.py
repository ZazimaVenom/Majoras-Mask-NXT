from pathlib import Path
import re,subprocess,json,difflib
root=Path('/workspace/scratch/cda0e8a05ecb/source/Majoras-Mask-NXT');out=Path(__file__).resolve().parent
results=[]
def run(name,cmd,text=None):
 p=subprocess.run(cmd,input=text,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
 (out/(name+'.log')).write_text(p.stdout)
 results.append({'name':name,'command':cmd,'returncode':p.returncode,'log':str(out/(name+'.log'))})
 return p
s=(root/'mm/2s2h/Rando/Menu.cpp').read_text()
decl='\n'.join(re.findall(r'^static (?:const float POOL_BALANCE_(?:UNLIKELY|LIMIT)_RATIO|std::size_t (?:checksInPool|itemsInPool|junkInPool)|int balanceStatus) = [^;]*;',s,re.M))
assert len(decl.splitlines())==6
balance=s[s.index('    if (static_cast<float>(checksInPool)'):s.index('    checkPoolGeneration++;')]
fmt=re.search(r'ImGui::TextColored\(DIM_TEXT_COLOR, ("%zu checks[^\n]+)\n\s*(checksInPool,[^;]+);',s)
assert fmt
cpp='''#include <cstddef>
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include <climits>
'''+decl+'''
void refresh(std::size_t checks, std::size_t items, std::size_t junk) {
checksInPool = checks;
itemsInPool = items;
junkInPool = junk;
'''+balance+'''}
int main() {
unsigned cases=0;
for (std::size_t checks=0;checks<=200;checks++) {
  for (std::size_t important=0;important<=200;important++) {
    for (std::size_t junk : {std::size_t(0),std::size_t(7)}) {
      refresh(checks,important+junk,junk);
      int expected = static_cast<float>(checks) * 0.9f < static_cast<float>(important) ? 2 :
                     static_cast<float>(checks) * 0.85f < static_cast<float>(important) ? 1 : 0;
      assert(balanceStatus == expected); ++cases;
    }
  }
}
refresh(static_cast<std::size_t>(INT_MAX)+4,static_cast<std::size_t>(INT_MAX)+9,3);
const std::size_t importantItems = itemsInPool-junkInPool;
std::vector<int> checkExclusionList(12);
char buffer[256];
std::snprintf(buffer,sizeof(buffer),'''+fmt.group(1)+'\n'+fmt.group(2)+''';
const std::string expected = std::to_string(checksInPool) + " checks  |  " + std::to_string(importantItems) + " important items  |  3 junk fill  |  12 excluded";
assert(buffer == expected); ++cases;
std::printf("PASS: %u source-extracted pool-metric/format cases including counts above INT_MAX.\\n",cases);
}
'''
(out/'pool_metrics.cpp').write_text(cpp)
p=run('pool_metrics_build',['g++','-std=c++20','-Wall','-Wextra','-Werror','-Wconversion','-Wformat=2','-fsanitize=undefined',str(out/'pool_metrics.cpp'),'-o',str(out/'pool_metrics')])
assert not p.returncode,p.stdout
p=run('pool_metrics_run',[str(out/'pool_metrics')]);assert not p.returncode,p.stdout;print(p.stdout.strip())
for file in ['mm/2s2h/mixer.c','mm/src/overlays/kaleido_scope/ovl_kaleido_scope/z_kaleido_scope_NES.c']:
 # Preprocess the source's exact conditional directives without unavailable dependency headers.
 text=re.sub(r'^\s*#include[^\n]*\n','\n',(root/file).read_text(),flags=re.M)
 p=run(Path(file).stem+'_msvc_guard',['gcc','-E','-P','-x','c','-undef','-D_MSC_VER=1944','-'],text)
 assert not p.returncode,p.stdout
 assert not re.search(r'^#pragma (GCC|increment_block_number)',p.stdout,re.M),file
 print('PASS: MSVC macro-environment source preprocessing hides compiler-specific pragmas:',file)
# All changed files preserve original CRLF encoding and have no bare LF.
patch=[]
for prior in (out/'before').rglob('*'):
 if not prior.is_file():continue
 rel=prior.relative_to(out/'before');target=root/rel;b=target.read_bytes();assert b.count(b'\n')==b.count(b'\r\n')
 patch.extend(difflib.unified_diff(prior.read_text().splitlines(True),target.read_text().splitlines(True),fromfile='a/'+str(rel),tofile='b/'+str(rel)))
(out/'changes.patch').write_text(''.join(patch))
(out/'focused-report.json').write_text(json.dumps(results,indent=2))
