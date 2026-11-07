#include "RpgHud.h"
#include "RpgFramework.h"
#include "RpgCore.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
float Fraction(float value, float total) {
    return std::isfinite(value) && total > 0 ? std::clamp(value/total, 0.0f, 1.0f) : 0.0f;
}
ImU32 Color(int r, int g, int b, float alpha = 1.0f) {
    return IM_COL32(r, g, b, static_cast<int>(std::clamp(alpha,0.0f,1.0f)*255));
}
void Text(ImDrawList* list, ImVec2 pos, float size, ImU32 color, const char* value) {
    list->AddText(ImGui::GetFont(), size, ImVec2(pos.x+1,pos.y+1), Color(0,0,0,0.85f), value);
    list->AddText(ImGui::GetFont(), size, pos, color, value);
}
void Panel(ImDrawList* list, float x, float y, float w, float h, float scale) {
    list->AddRectFilled(ImVec2(x,y), ImVec2(x+w,y+h), Color(10,14,22,0.88f), 5*scale);
    list->AddRect(ImVec2(x,y), ImVec2(x+w,y+h), Color(137,152,179,0.5f), 5*scale);
}
void Bar(ImDrawList* list, float x,float y,float w,float h,float fraction,ImU32 color,float scale) {
    list->AddRectFilled(ImVec2(x,y), ImVec2(x+w,y+h),Color(3,7,15,0.95f),2*scale);
    if (fraction > 0) list->AddRectFilled(ImVec2(x,y),ImVec2(x+w*std::clamp(fraction,0.0f,1.0f),y+h),color,2*scale);
}
void CenterText(ImDrawList* list, float center, float y,float size,ImU32 color,const char* text) {
    const float width=ImGui::GetFont()->CalcTextSizeA(size,1.0e6f,0,text).x;
    Text(list,ImVec2(center-width/2,y),size,color,text);
}
}
void RpgHud::Draw() {
    RpgHudSnapshot h{}; Rpg_GetHudSnapshot(&h);
    if (!h.visible || !CVarGetInteger("gEnhancements.Rpg.Hud",1)) return;
    ImGuiWindow* game = ImGui::FindWindowByName("Main Game");
    if (!game || game->Collapsed || game->Hidden) return;
    const ImVec2 origin=game->InnerRect.Min;
    const ImVec2 bounds=game->InnerRect.Max;
    const float width=bounds.x-origin.x, height=bounds.y-origin.y;
    if (width < 240 || height < 180) return;
    const float userScale=std::clamp(CVarGetInteger("gEnhancements.Rpg.HudScale",100),75,150)*0.01f;
    const float scale=std::clamp(height/900.0f,0.70f,1.8f)*userScale;
    const float font=16*scale, smallFontSize=13*scale;
    const float center=(origin.x+bounds.x)*0.5f;
    ImGui::SetNextWindowViewport(game->ViewportId);
    ImGui::SetNextWindowPos(origin,ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width,height),ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0.0f);
    const bool visible=ImGui::Begin("##RpgGameHud",nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus);
    if (visible) {
        ImDrawList* draw=ImGui::GetWindowDrawList();
        draw->PushClipRect(origin,bounds,true);
        char line[192]{};
        // A thin lower-edge bar stays below the native clock, not on top of the
        // hearts, equipment buttons, rupee count or minimap. Never scales in 3D.
        const float xpWidth=std::min(width*0.62f,720*scale), xpX=center-xpWidth/2;
        const float xpY=bounds.y-38*scale;
        Panel(draw,xpX-10*scale,xpY-23*scale,xpWidth+20*scale,45*scale,scale);
        std::snprintf(line,sizeof(line),"LV %d   |   HP %.0f / %.0f",h.level,h.hp,h.maxHp);
        Text(draw,ImVec2(xpX,xpY-19*scale),smallFontSize,Color(235,242,250),line);
        if (h.nextXp) std::snprintf(line,sizeof(line),"%llu / %llu XP",
            static_cast<unsigned long long>(h.xp),static_cast<unsigned long long>(h.nextXp));
        else std::snprintf(line,sizeof(line),"MAX LEVEL");
        const float textWidth=ImGui::GetFont()->CalcTextSizeA(smallFontSize,1.0e6f,0,line).x;
        Text(draw,ImVec2(xpX+xpWidth-textWidth,xpY-19*scale),smallFontSize,Color(157,209,250),line);
        Bar(draw,xpX,xpY,xpWidth,8*scale,h.nextXp ? static_cast<float>(h.xp)/h.nextXp : 1.0f,Color(70,164,230),scale);
        for (int i=1;i<10;++i) draw->AddLine(ImVec2(xpX+xpWidth*i/10,xpY),
            ImVec2(xpX+xpWidth*i/10,xpY+8*scale),Color(0,0,0,0.5f));
        if (h.hp < h.maxHp && CVarGetInteger("gEnhancements.Rpg.Regeneration",1)) {
            if (h.damageFreeSeconds < h.regenDelay)
                std::snprintf(line,sizeof(line),"Recovery in %.0fs",std::ceil(h.regenDelay-h.damageFreeSeconds));
            else std::snprintf(line,sizeof(line),"Recovering health");
            Text(draw,ImVec2(xpX,xpY+11*scale),10*scale,Color(153,220,175),line);
        }
        if (h.toastSeconds > 0 && h.toast[0])
            CenterText(draw,center,xpY-59*scale,font,Color(255,222,130,std::min(h.toastSeconds,1.0f)),h.toast);
        const float bossY=origin.y+height*std::clamp(CVarGetInteger("gEnhancements.Rpg.BossHudY",18),8,40)/100.0f;
        if (h.hasBoss) {
            const float w=std::min(width*0.66f,740*scale), x=center-w/2;
            const float actionFont=std::min(smallFontSize,(w-24*scale)/std::max(1.0f,
                ImGui::GetFont()->CalcTextSizeA(smallFontSize,1.0e6f,0,h.bossAction).x)*smallFontSize);
            Panel(draw,x-10*scale,bossY-9*scale,w+20*scale,95*scale,scale);
            std::snprintf(line,sizeof(line),"%s  |  LV %d%s",h.boss.name,h.boss.level,h.bossEnraged ? "  ENRAGED" : "");
            CenterText(draw,center,bossY,font,Color(255,214,143),line);
            Bar(draw,x,bossY+24*scale,w,14*scale,Fraction(h.boss.hp,h.boss.maxHp),
                h.bossEnraged ? Color(234,67,52) : Color(177,59,62),scale);
            std::snprintf(line,sizeof(line),"%.0f / %.0f HP  |  Raid %d / %d",h.boss.hp,h.boss.maxHp,h.raidAlive,h.raidTarget);
            CenterText(draw,center,bossY+25*scale,smallFontSize*0.86f,Color(255,247,239),line);
            CenterText(draw,center,bossY+46*scale,actionFont,Color(247,217,178),h.bossAction);
            if (h.bossCastDuration > 0)
                Bar(draw,x,bossY+67*scale,w,6*scale,Fraction(h.bossCast,h.bossCastDuration),Color(242,173,64),scale);
        } else if (h.hasTarget && CVarGetInteger("gEnhancements.Rpg.TargetHud",1)) {
            const float w=std::min(width*0.46f,410*scale), x=center-w/2;
            Panel(draw,x-10*scale,bossY-8*scale,w+20*scale,57*scale,scale);
            std::snprintf(line,sizeof(line),"%s  |  LV %d",h.target.name,h.target.level);
            CenterText(draw,center,bossY,font,h.target.berserk ? Color(255,115,87) : Color(238,235,222),line);
            Bar(draw,x,bossY+25*scale,w,12*scale,Fraction(h.target.hp,h.target.maxHp),
                h.target.berserk ? Color(218,67,36) : Color(158,63,76),scale);
            std::snprintf(line,sizeof(line),"%.0f / %.0f HP",h.target.hp,h.target.maxHp);
            CenterText(draw,center,bossY+24*scale,smallFontSize,Color(255,245,232),line);
        }
        draw->PopClipRect();
    }
    ImGui::End(); ImGui::PopStyleVar(2);
}
void Rpg_DrawCharacterSheet() {
    RpgHudSnapshot h{}; Rpg_GetHudSnapshot(&h);
    if (h.level < 1) { ImGui::TextWrapped("Load a save to view your character's RPG stats."); return; }
    ImGui::Text("Level %d / %d",h.level,Rpg::MaxLevel);
    ImGui::Text("Health: %.0f / %.0f",h.hp,h.maxHp);
    ImGui::Text("Attack: %.0f   Defense: %.0f",h.attack,h.defense);
    ImGui::Text("Magic: %.0f   Resistance: %.0f",h.magic,h.resistance);
    ImGui::Text("Agility: %.0f   Critical: %.1f%%",h.agility,h.critical);
    ImGui::Text("Kills: %u   Berserk: %u   World bosses: %u",h.creditedKills,h.berserkKills,h.worldBossKills);
    ImGui::TextWrapped("Physical attacks use Attack/Defense; elemental arrows and Zora barrier use Magic/Resistance. "
                       "Agility advantage can soften a hit. Native armor, elemental weaknesses and heart containers still matter.");
}
