#!/usr/bin/env python3
"""Focused compile regression for the shipped HUD's Windows SDK macro collision.

Uses the real HUD, RPG, engine and Ship::GuiWindow/GuiElement headers.
ImGui and the CVar bridge are explicit declaration-only test doubles because the
complete third-party dependency set is unavailable. This is NOT an ImGui ABI,
Windows SDK/MSVC, complete-game link or rendering test. No binaries are shipped.
"""
from pathlib import Path
import argparse, json, re, shutil, subprocess, tempfile, zipfile

IMGUI = r'''#pragma once
// DECLARATION-ONLY TEST DOUBLE. Not a replacement for the real ImGui library.
#include <cstdint>
using ImU32 = uint32_t;
using ImGuiID = uint32_t;
using ImDrawFlags = int;
using ImGuiWindowFlags = int;
using ImGuiCond = int;
using ImGuiStyleVar = int;
struct ImVec2 { float x, y; constexpr ImVec2(float X=0,float Y=0):x(X),y(Y){} };
struct ImVec4 { float x,y,z,w; };
template<class T> struct ImVector { int Size,Capacity; T* Data; };
struct ImFont {
    ImVec2 CalcTextSizeA(float,float,float,const char*,const char* = nullptr,const char** = nullptr) const;
};
struct ImDrawList {
    void AddText(const ImFont*,float,const ImVec2&,ImU32,const char*,const char* = nullptr,float = 0.0f,const ImVec4* = nullptr);
    void AddRectFilled(const ImVec2&,const ImVec2&,ImU32,float = 0.0f,ImDrawFlags = 0);
    void AddRect(const ImVec2&,const ImVec2&,ImU32,float = 0.0f,ImDrawFlags = 0,float = 1.0f);
    void AddLine(const ImVec2&,const ImVec2&,ImU32,float = 1.0f);
    void PushClipRect(const ImVec2&,const ImVec2&,bool);
    void PopClipRect();
};
#define IM_COL32(R,G,B,A) (((ImU32)(A)<<24)|((ImU32)(B)<<16)|((ImU32)(G)<<8)|(ImU32)(R))
inline constexpr int ImGuiCond_Always=1;
inline constexpr int ImGuiStyleVar_WindowPadding=2, ImGuiStyleVar_WindowBorderSize=4;
inline constexpr int ImGuiWindowFlags_NoDecoration=1, ImGuiWindowFlags_NoInputs=2,
    ImGuiWindowFlags_NoNav=4, ImGuiWindowFlags_NoBackground=8,
    ImGuiWindowFlags_NoSavedSettings=16, ImGuiWindowFlags_NoDocking=32,
    ImGuiWindowFlags_NoFocusOnAppearing=64, ImGuiWindowFlags_NoBringToFrontOnFocus=128;
namespace ImGui {
    ImFont* GetFont();
    void SetNextWindowViewport(ImGuiID);
    void SetNextWindowPos(const ImVec2&,ImGuiCond=0,const ImVec2&=ImVec2());
    void SetNextWindowSize(const ImVec2&,ImGuiCond=0);
    void PushStyleVar(ImGuiStyleVar,const ImVec2&);
    void PushStyleVar(ImGuiStyleVar,float);
    bool Begin(const char*,bool* = nullptr,ImGuiWindowFlags=0);
    ImDrawList* GetWindowDrawList();
    void End();
    void PopStyleVar(int=1);
    void Text(const char*,...);
    void TextWrapped(const char*,...);
}
'''.replace('const ImVec2&=','const ImVec2& =')
INTERNAL = r'''#pragma once
#include "imgui.h"
// DECLARATION-ONLY TEST DOUBLE; field layout is NOT the real ImGui ABI.
struct ImRect { ImVec2 Min,Max; };
struct ImGuiWindow { bool Collapsed,Hidden; ImRect InnerRect; ImGuiID ViewportId; };
namespace ImGui { ImGuiWindow* FindWindowByName(const char*); }
'''
CVAR = r'''#pragma once
// DECLARATION-ONLY TEST DOUBLE for the one CVar API used by this source.
#include <cstdint>
extern "C" int32_t CVarGetInteger(const char*, int32_t);
'''

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--source-root',required=True,type=Path)
    ap.add_argument('--original-zip',required=True,type=Path)
    ap.add_argument('--output',required=True,type=Path)
    args=ap.parse_args()
    root=args.source_root.resolve(); out=args.output.resolve(); out.mkdir(parents=True,exist_ok=True)
    rel=Path('mm/2s2h/Enhancements/RpgFramework/RpgHud.cpp')
    fixed=root/rel
    with zipfile.ZipFile(args.original_zip) as z: original=z.read('MajoraWithBots/'+rel.as_posix())
    current=fixed.read_bytes()
    assert re.sub(rb'\bsmallFontSize\b',b'small',current)==original, 'Unexpected non-rename source changes'
    assert len(re.findall(rb'\bsmallFontSize\b',current))==9
    report=[]
    with tempfile.TemporaryDirectory(prefix='rpg-hud-macro-') as temp:
        temp=Path(temp); shim=temp/'declarations'; shim.mkdir()
        (shim/'imgui.h').write_text(IMGUI)
        (shim/'imgui_internal.h').write_text(INTERNAL)
        bridge=shim/'libultraship/bridge/consolevariablebridge.h'
        bridge.parent.mkdir(parents=True); bridge.write_text(CVAR)
        before=temp/'RpgHud.before.cpp'; before.write_bytes(original)
        paths=[shim, fixed.parent,root/'mm',root/'mm/include',root/'mm/2s2h',root/'mm/src',
               root/'mm/assets',root/'libultraship/include',root/'libultraship/include/libultraship',
               root/'mm/include/PR',root/'libultraship/src',root/'libultraship/extern/fast3d/include']
        for name in ['g++','clang++']:
            compiler=shutil.which(name)
            if not compiler:
                report.append({'test':name,'status':'NOT_RUN','reason':'Compiler unavailable'});continue
            version=subprocess.run([compiler,'--version'],capture_output=True,text=True,check=True).stdout.splitlines()[0]
            for mode,source,macro,expect in [('original_plain',before,False,0),('original_windows_macro',before,True,1),
                                           ('fixed_plain',fixed,False,0),('fixed_windows_macro',fixed,True,0)]:
                label=name.replace('+','p')+'_'+mode
                cmd=[compiler,'-std=c++20','-O0','-c','-DF3DEX_GBI_2','-DCONTROLLERBUTTONS_T=uint32_t']
                if macro:cmd+=['-Dsmall=char']
                cmd+=['-I'+str(p) for p in paths]+[str(source),'-o',str(temp/(label+'.o'))]
                r=subprocess.run(cmd,cwd=root,capture_output=True,text=True)
                log=r.stdout+r.stderr
                (out/(label+'.log')).write_text(log)
                ok=(r.returncode==0) if expect==0 else (r.returncode!=0 and 'char' in log and 'RpgHud.before.cpp:44' in log)
                report.append({'test':label,'status':'PASS' if ok else 'FAIL','compiler':version,
                               'returncode':r.returncode,'expected':'reject original macro-colliding code' if expect else 'compile object',
                               'command':cmd,'log':label+'.log','boundary':'real production source/engine/Ship headers; ImGui/CVar declaration doubles'})
                (out/'hud_compile_report.json').write_text(json.dumps(report,indent=2)+'\n')
                print(('PASS ' if ok else 'FAIL ')+label,flush=True)
                if not ok:
                    print(log[-5000:]);return 1
    print('All nine changes are identifier-only. The original fails with small=char; corrected source compiles with it.')
    print('No actual MSVC/Windows SDK, full ImGui, game linking or rendering test was performed.')
    return 0
if __name__=='__main__':raise SystemExit(main())
