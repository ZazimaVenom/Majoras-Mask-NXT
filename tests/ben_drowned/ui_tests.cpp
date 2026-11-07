// Headless host for real BenHud.cpp + upstream Dear ImGui 1.91.9b.
// The game and Ship lifecycle are intentionally not present; this verifies the
// presentation layer, draw bounds and queued UI requests, not native gameplay.
#include "2s2h/Enhancements/BenDrowned/BenHud.h"
#include "2s2h/Enhancements/BenDrowned/BenDrowned.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>

namespace Ship {
GuiElement::GuiElement() : mIsVisible(false), mIsInitialized(false) {}
GuiElement::GuiElement(bool v) : mIsVisible(v), mIsInitialized(false) {}
GuiElement::~GuiElement() = default;
void GuiElement::SetVisibility(bool v) { mIsVisible = v; }
void GuiWindow::SetVisibility(bool v) { mIsVisible = v; }
void GuiWindow::Draw() { DrawElement(); }
}
static BenHudSnapshot sample;
static int subtitles=1;
static std::vector<BenRequest> requests;
extern "C" void Ben_GetHudSnapshot(BenHudSnapshot* out) { *out=sample; }
extern "C" void Ben_QueueRequest(BenRequest request) { requests.push_back(request); }
extern "C" int32_t CVarGetInteger(const char* key,int32_t value) {
    return !std::strcmp(key,"gEnhancements.BenDrowned.Subtitles") ? subtitles : value;
}
static std::vector<unsigned char> atlas;
static int atlasW,atlasH;
static void SaveFrame(const char* path) {
    ImDrawData* data=ImGui::GetDrawData();
    const int w=static_cast<int>(data->DisplaySize.x),h=static_cast<int>(data->DisplaySize.y);
    std::vector<unsigned char> pixels(w*h*3);
    for(int y=0;y<h;++y)for(int x=0;x<w;++x){ auto k=(y*w+x)*3; pixels[k]=24;pixels[k+1]=33;pixels[k+2]=35; }
    auto orient=[](ImVec2 a,ImVec2 b,ImVec2 p){return (b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x);};
    for(int list=0;list<data->CmdListsCount;++list){ const auto* l=data->CmdLists[list];
      for(const auto& cmd:l->CmdBuffer){ if(cmd.UserCallback)continue;
        for(unsigned i=cmd.IdxOffset;i+2<cmd.IdxOffset+cmd.ElemCount;i+=3){
          const ImDrawVert& a=l->VtxBuffer[l->IdxBuffer[i]+cmd.VtxOffset];
          const ImDrawVert& b=l->VtxBuffer[l->IdxBuffer[i+1]+cmd.VtxOffset];
          const ImDrawVert& c=l->VtxBuffer[l->IdxBuffer[i+2]+cmd.VtxOffset];
          const float area=orient(a.pos,b.pos,c.pos);if(std::abs(area)<0.00001f)continue;
          int x0=std::max(0,static_cast<int>(std::floor(std::max(cmd.ClipRect.x,std::min({a.pos.x,b.pos.x,c.pos.x})))));
          int x1=std::min(w,static_cast<int>(std::ceil(std::min(cmd.ClipRect.z,std::max({a.pos.x,b.pos.x,c.pos.x})))));
          int y0=std::max(0,static_cast<int>(std::floor(std::max(cmd.ClipRect.y,std::min({a.pos.y,b.pos.y,c.pos.y})))));
          int y1=std::min(h,static_cast<int>(std::ceil(std::min(cmd.ClipRect.w,std::max({a.pos.y,b.pos.y,c.pos.y})))));
          for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x){ImVec2 p(x+.5f,y+.5f);
            float wa=orient(b.pos,c.pos,p)/area,wb=orient(c.pos,a.pos,p)/area,wc=1-wa-wb;
            if(wa<0||wb<0||wc<0)continue;
            int tx=std::clamp(static_cast<int>((a.uv.x*wa+b.uv.x*wb+c.uv.x*wc)*atlasW),0,atlasW-1);
            int ty=std::clamp(static_cast<int>((a.uv.y*wa+b.uv.y*wb+c.uv.y*wc)*atlasH),0,atlasH-1);
            auto color=[&](int shift){return ((a.col>>shift)&255)*wa+((b.col>>shift)&255)*wb+((c.col>>shift)&255)*wc;};
            const float alpha=color(IM_COL32_A_SHIFT)/255.0f*atlas[ty*atlasW+tx]/255.0f;
            for(int channel=0;channel<3;++channel){int shift=channel==0?IM_COL32_R_SHIFT:channel==1?IM_COL32_G_SHIFT:IM_COL32_B_SHIFT;
              auto k=(y*w+x)*3+channel;pixels[k]=static_cast<unsigned char>(std::clamp(color(shift)*alpha+pixels[k]*(1-alpha),0.0f,255.0f));}
          }
        }
      }
    }
    std::ofstream out(path,std::ios::binary);out<<"P6\n"<<w<<" "<<h<<"\n255\n";out.write(reinterpret_cast<const char*>(pixels.data()),pixels.size());
}
static int Frame(BenDrownedHud& hud,BenDrownedJournal& journal,int w,int h,bool casebook=false) {
    auto& io=ImGui::GetIO();io.DisplaySize=ImVec2(static_cast<float>(w),static_cast<float>(h));io.DeltaTime=1.0f/60.0f;
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0,0));ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("Main Game",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoInputs);
    ImGui::End();
    hud.Draw();
    if(casebook){
      ImGui::SetNextWindowPos(ImVec2(40,40));ImGui::SetNextWindowSize(ImVec2(std::min(w-80,540),std::min(h-80,680)));
      ImGui::Begin("BEN DROWNED Casebook");journal.DrawElement();ImGui::End();
    }
    ImGui::Render();
    int vertices=0;
    for(auto* list:ImGui::GetDrawData()->CmdLists){
      for(const auto& v:list->VtxBuffer)assert(std::isfinite(v.pos.x)&&std::isfinite(v.pos.y));
      for(const auto& cmd:list->CmdBuffer)assert(cmd.ClipRect.x<=cmd.ClipRect.z&&cmd.ClipRect.y<=cmd.ClipRect.w);
      vertices+=list->VtxBuffer.Size;
    }
    return vertices;
}
int main(){
    IMGUI_CHECKVERSION();ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;
    unsigned char* pixels=nullptr;io.Fonts->GetTexDataAsAlpha8(&pixels,&atlasW,&atlasH);atlas.assign(pixels,pixels+atlasW*atlasH);
    io.Fonts->SetTexID((ImTextureID)1);ImGui::StyleColorsDark();
    BenDrownedHud hud;BenDrownedJournal journal;
    Frame(hud,journal,1280,800);Frame(hud,journal,1280,800);
    const int dormant=Frame(hud,journal,1280,800);assert(dormant==0);
    sample.loaded=sample.active=sample.visible=sample.reducedEffects=true;sample.vignette=.8f;sample.loop=2;
    sample.messageId=10;sample.subtitleSeconds=8;sample.choiceAvailable=true;sample.challengeSeconds=36;sample.challengeProgress=.4f;
    std::snprintf(sample.phase,sizeof(sample.phase),"THE FALSE DAWN");
    std::snprintf(sample.presence,sizeof(sample.presence),"Something is waiting on the other side of the water.");
    std::snprintf(sample.subtitle,sizeof(sample.subtitle),"You came back before the water settled. I left a place beside the clock for someone who could remember my name.");
    std::snprintf(sample.objective,sizeof(sample.objective),"Follow the pale markers. Keep the statue in sight until the last bell.");
    std::snprintf(sample.choicePrompt,sizeof(sample.choicePrompt),"If I stop following you, will you still come back?");
    sample.journalCount=3;
    for(unsigned i=0;i<3;++i){sample.journal[i].loop=2;std::snprintf(sample.journal[i].title,64,"Witness record %u: The water beneath the clock",i+1);std::snprintf(sample.journal[i].text,384,"The streets were reflected, but the footprints still belonged to you. A figure waited beside the door. When you turned back, there were two shadows and only one body.");}
    for(int i=0;i<60;++i)Frame(hud,journal,1280,800);
    assert(Frame(hud,journal,1280,800)>100);SaveFrame("ben_hud_1280.ppm");
    for(int i=0;i<3;++i) { Frame(hud,journal,640,480); }
    SaveFrame("ben_hud_640.ppm");
    Frame(hud,journal,3840,2160);Frame(hud,journal,240,180);
    sample.suspended=true;assert(Frame(hud,journal,1280,800)==0);sample.suspended=false;
    sample.visible=false;assert(Frame(hud,journal,1280,800)==0);sample.visible=true;
    subtitles=0;Frame(hud,journal,1280,800);subtitles=1;
    for(int i=0;i<3;++i)Frame(hud,journal,1280,800,true);
    SaveFrame("ben_casebook.ppm");
    assert(requests.empty());
    auto click=[&](float x,float y){
        io.AddMousePosEvent(x,y);Frame(hud,journal,1280,800,true);
        io.AddMouseButtonEvent(0,true);Frame(hud,journal,1280,800,true);
        io.AddMouseButtonEvent(0,false);Frame(hud,journal,1280,800,true);
    };
    const BenHudSnapshot beforeInput=sample;
    click(310,169);
    assert(requests.size()==1 && requests.back()==BEN_REQUEST_SUSPEND);
    assert(std::memcmp(&beforeInput,&sample,sizeof(sample))==0);
    sample.suspended=true;Frame(hud,journal,1280,800,true);click(310,169);
    assert(requests.size()==2 && requests.back()==BEN_REQUEST_RESUME);
    sample.suspended=false;Frame(hud,journal,1280,800,true);click(310,347);
    assert(requests.size()==3 && requests.back()==BEN_REQUEST_KINDNESS);
    click(310,347);assert(requests.size()==3); // UI acknowledgement prevents double submission.
    io.AddMousePosEvent(-100,-100);
    // Overlarge count must never walk beyond the copied 32-entry POD journal.
    sample.journalCount=UINT32_MAX;
    ImGui::NewFrame();ImGui::Begin("JournalStress");Ben_DrawJournalPanel();ImGui::End();ImGui::Render();
    ImGui::DestroyContext();
    std::puts("PASS: actual Dear ImGui HUD rendered at 240x180, 640x480, 1280x800, 3840x2160; inactive/suspended/hidden emit no overlay; finite geometry; journal count clamped; no unsolicited requests; Suspend/Resume/Kindness mouse interactions enqueue exact requests without mutating snapshot; double submission suppressed.");
}
