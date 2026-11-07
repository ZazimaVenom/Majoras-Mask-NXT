#include "BenHud.h"
#include "BenDrowned.h"

#include <libultraship/bridge/consolevariablebridge.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>

namespace {
const ImVec4 Parchment(0.88f, 0.85f, 0.74f, 1.0f);
const ImVec4 Teal(0.40f, 0.72f, 0.68f, 1.0f);
const ImVec4 Ash(0.59f, 0.64f, 0.63f, 1.0f);

float Unit(float value) {
    return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.0f;
}

ImU32 Color(int r, int g, int b, float alpha = 1.0f) {
    return IM_COL32(r, g, b, static_cast<int>(Unit(alpha) * 255.0f));
}

void DrawText(ImDrawList* draw, ImVec2 pos, float size, ImU32 color, const char* value,
              float wrapWidth = 0.0f, float alpha = 1.0f) {
    draw->AddText(ImGui::GetFont(), size, ImVec2(pos.x + 1.0f, pos.y + 1.0f), Color(0, 0, 0, alpha * 0.65f),
                  value, nullptr, wrapWidth);
    draw->AddText(ImGui::GetFont(), size, pos, color, value, nullptr, wrapWidth);
}

void CenterText(ImDrawList* draw, float center, float y, float size, ImU32 color, const char* value) {
    const float width = ImGui::GetFont()->CalcTextSizeA(size, 100000.0f, 0.0f, value).x;
    DrawText(draw, ImVec2(center - width * 0.5f, y), size, color, value);
}

void Diamond(ImDrawList* draw, ImVec2 center, float radius, ImU32 color) {
    draw->AddQuad(ImVec2(center.x, center.y - radius), ImVec2(center.x + radius, center.y),
                  ImVec2(center.x, center.y + radius), ImVec2(center.x - radius, center.y), color);
}

void Panel(ImDrawList* draw, ImVec2 min, ImVec2 max, float scale, float alpha) {
    draw->AddRectFilled(min, max, Color(7, 13, 15, alpha * 0.92f), 3.0f * scale);
    draw->AddRect(min, max, Color(100, 135, 127, alpha * 0.48f), 3.0f * scale);
    draw->AddLine(ImVec2(min.x, min.y + 1.0f), ImVec2(min.x + 54.0f * scale, min.y + 1.0f),
                  Color(140, 65, 61, alpha), 2.0f * scale);
}

void Vignette(ImDrawList* draw, ImVec2 min, ImVec2 max, float amount, bool reduced) {
    // Gradients are fixed to the viewport edges. No flashes, shake, moving scan
    // lines or full-screen inversion, including at the highest intensity.
    const float depth = std::min(max.x - min.x, max.y - min.y) * (reduced ? 0.055f : 0.13f);
    const ImU32 edge = Color(2, 10, 11, amount * (reduced ? 0.13f : 0.35f));
    const ImU32 clear = Color(2, 10, 11, 0.0f);
    draw->AddRectFilledMultiColor(min, ImVec2(min.x + depth, max.y), edge, clear, clear, edge);
    draw->AddRectFilledMultiColor(ImVec2(max.x - depth, min.y), max, clear, edge, edge, clear);
    draw->AddRectFilledMultiColor(min, ImVec2(max.x, min.y + depth), edge, edge, clear, clear);
    draw->AddRectFilledMultiColor(ImVec2(min.x, max.y - depth), max, clear, clear, edge, edge);
}

void DrawSession(const BenHudSnapshot& h) {
    ImGui::TextColored(Teal, "BEN DROWNED");
    ImGui::TextWrapped("A presence with a memory. Every return leaves a trace.");
    ImGui::Spacing();
    if (!h.loaded) {
        ImGui::TextColored(Ash, "Waiting at the title screen");
        ImGui::TextWrapped("Enable this mode before loading a save. Its encounters begin only after entering playable Termina.");
    } else if (!h.active) {
        ImGui::TextColored(Ash, "This session is unhaunted");
        ImGui::TextWrapped("Enable the mode, then return to the title screen and load your save to begin.");
    } else {
        ImGui::TextColored(h.suspended ? Ash : Parchment, "%s", h.suspended ? "The signal is suspended" : h.phase);
        ImGui::Text("Return %u  /  Manifestations %u", h.loop + 1U, h.encounters);
        if (h.presence[0]) ImGui::TextWrapped("%s", h.presence);
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, h.suspended ? ImVec4(0.12f, 0.30f, 0.27f, 1.0f)
                                                        : ImVec4(0.40f, 0.14f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, h.suspended ? ImVec4(0.18f, 0.40f, 0.36f, 1.0f)
                                                               : ImVec4(0.53f, 0.20f, 0.20f, 1.0f));
        if (ImGui::Button(h.suspended ? "Resume the signal" : "Suspend haunting now", ImVec2(-1.0f, 0.0f))) {
            Ben_QueueRequest(h.suspended ? BEN_REQUEST_RESUME : BEN_REQUEST_SUSPEND);
        }
        ImGui::PopStyleColor(2);
        ImGui::TextColored(Ash, "Suspend is always available here.");
    }
    if (h.pendingEnable) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.90f, 0.70f, 0.38f, 1.0f), "Mode change waiting");
        ImGui::TextWrapped("Return to the title screen and load a save to apply the enable setting. Use Suspend for the current session.");
    }
}

void DrawResponses(const BenHudSnapshot& h) {
    // Cosmetic acknowledgement only. The simulation owns validation, decision
    // availability and the actual response; ImGui can only enqueue an enum.
    static uint32_t sentMessage = UINT32_MAX;
    static double sentTime = -10.0;
    const bool waiting = sentMessage == h.messageId && ImGui::GetTime() - sentTime < 2.0;
    if (!h.loaded || !h.active) {
        ImGui::TextWrapped("When the presence asks something of you, answer here. Silence is also an answer.");
        return;
    }
    if (h.objective[0]) {
        ImGui::TextColored(Teal, "CURRENT THREAD");
        ImGui::TextWrapped("%s", h.objective);
        if (h.challengeSeconds > 0.0f) {
            ImGui::TextColored(Ash, "Time remaining: %.0f seconds", std::ceil(h.challengeSeconds));
            ImGui::ProgressBar(Unit(h.challengeProgress), ImVec2(-1.0f, 5.0f), "");
        }
        ImGui::Spacing();
    }
    if (!h.choiceAvailable) {
        ImGui::TextWrapped("No answer is being asked for. Keep exploring; the casebook will remember what you witness.");
        return;
    }
    ImGui::TextColored(Parchment, "AN ANSWER IS WAITING");
    ImGui::TextWrapped("%s", h.choicePrompt[0] ? h.choicePrompt : "How will you answer the presence?");
    ImGui::Spacing();
    ImGui::BeginDisabled(h.suspended || waiting);
    struct Response { const char* label; const char* note; BenRequest request; };
    const Response responses[] = {
        { "Offer kindness", "Reach toward the voice without a bargain.", BEN_REQUEST_KINDNESS },
        { "Defy the presence", "Refuse the part it has written for you.", BEN_REQUEST_DEFY },
        { "Listen in silence", "Let the signal finish its thought.", BEN_REQUEST_LISTEN },
        { "Offer mercy", "Show compassion without demanding a reward.", BEN_REQUEST_MERCY },
    };
    for (const auto& response : responses) {
        if (ImGui::Button(response.label, ImVec2(-1.0f, 0.0f))) {
            Ben_QueueRequest(response.request);
            sentMessage = h.messageId;
            sentTime = ImGui::GetTime();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", response.note);
    }
    ImGui::EndDisabled();
    if (waiting) ImGui::TextColored(Teal, "Your answer has been sent.");
    else if (h.suspended) ImGui::TextColored(Ash, "Resume the signal to answer.");
    else ImGui::TextColored(Ash, "One response per invitation.");
}

void DrawJournal(const BenHudSnapshot& h, float height) {
    static ImGuiTextFilter filter;
    ImGui::TextColored(Teal, "THE WATER REMEMBERS");
    ImGui::TextWrapped("A record of what happened, even when the presence tells you otherwise.");
    filter.Draw("Search records", -1.0f);
    const unsigned count = std::min<unsigned>(h.journalCount, 32U);
    if (!count) {
        ImGui::Spacing();
        ImGui::TextColored(Ash, "The first page is still dry.");
        return;
    }
    ImGui::BeginChild("##BenCasebookRecords", ImVec2(0.0f, height), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    unsigned matches = 0;
    for (unsigned i = 0; i < count; ++i) {
        const BenJournalEntry& entry = h.journal[i];
        char searchable[512]{};
        std::snprintf(searchable, sizeof(searchable), "%s %s", entry.title, entry.text);
        if (!filter.PassFilter(searchable)) continue;
        ++matches;
        ImGui::PushID(static_cast<int>(i));
        ImGui::TextColored(Ash, "RETURN %u  /  RECORD %02u", entry.loop + 1U, count - i);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(Parchment, "%s", entry.title);
        ImGui::TextUnformatted(entry.text);
        ImGui::PopTextWrapPos();
        ImGui::Spacing();
        if (i + 1U < count) ImGui::Separator();
        ImGui::Spacing();
        ImGui::PopID();
    }
    if (!matches) ImGui::TextColored(Ash, "No record matches this search.");
    ImGui::EndChild();
    ImGui::TextColored(Ash, "Recent records: %u / 32", count);
}
} // namespace

void BenDrownedHud::Draw() {
    BenHudSnapshot h{};
    Ben_GetHudSnapshot(&h);
    if (!h.active || !h.visible || h.suspended) return;
    ImGuiWindow* game = ImGui::FindWindowByName("Main Game");
    if (!game || game->Collapsed || game->Hidden) return;
    const ImVec2 origin = game->InnerRect.Min;
    const ImVec2 bounds = game->InnerRect.Max;
    const float width = bounds.x - origin.x;
    const float height = bounds.y - origin.y;
    if (width < 240.0f || height < 180.0f) return;
    const float scale = std::clamp(height / 800.0f, 0.62f, 1.80f);
    const float center = (origin.x + bounds.x) * 0.5f;
    // Keep labels readable at ordinary window sizes. Only very small preview
    // viewports use proportional scaling below these minimums.
    const float font = height >= 360.0f ? std::max(13.0f, 17.0f * scale) : 17.0f * scale;
    const float labelFontSize = height >= 360.0f ? std::max(11.0f, 13.0f * scale) : 13.0f * scale;

    ImGui::SetNextWindowViewport(game->ViewportId);
    ImGui::SetNextWindowPos(origin, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    if (ImGui::Begin("##BenDrownedGameHud", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav |
                         ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                         ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus)) {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->PushClipRect(origin, bounds, true);
        Vignette(draw, origin, bounds, Unit(h.vignette), h.reducedEffects);

        char status[128]{};
        std::snprintf(status, sizeof(status), "%s   /   RETURN %u", h.phase, h.loop + 1U);
        const float statusFont = std::min(labelFontSize, width * 0.58f /
            std::max(1.0f, ImGui::GetFont()->CalcTextSizeA(labelFontSize, 100000.0f, 0.0f, status).x) * labelFontSize);
        const float statusWidth = ImGui::GetFont()->CalcTextSizeA(statusFont, 100000.0f, 0.0f, status).x;
        const float statusY = origin.y + 13.0f * scale;
        draw->AddRectFilled(ImVec2(center - statusWidth * 0.5f - 21.0f * scale, statusY - 5.0f * scale),
                            ImVec2(center + statusWidth * 0.5f + 21.0f * scale, statusY + 18.0f * scale),
                            Color(8, 14, 16, 0.74f), 2.0f * scale);
        CenterText(draw, center, statusY, statusFont, Color(160, 194, 180, 0.88f), status);
        Diamond(draw, ImVec2(center - statusWidth * 0.5f - 12.0f * scale, statusY + 5.0f * scale),
                3.0f * scale, Color(139, 73, 66, 0.85f));

        static uint32_t previousMessage = UINT32_MAX;
        static double messageStart = 0.0;
        if (h.messageId != previousMessage) {
            previousMessage = h.messageId;
            messageStart = ImGui::GetTime();
        }
        const bool subtitle = h.subtitleSeconds > 0.0f && h.subtitle[0] &&
                              CVarGetInteger("gEnhancements.BenDrowned.Subtitles", 1);
        const bool objective = h.objective[0] != '\0';
        if (subtitle || objective || h.choiceAvailable) {
            const float panelWidth = std::min(width * 0.76f, 710.0f * scale);
            const float textWidth = panelWidth - 38.0f * scale;
            const float textHeight = subtitle ? ImGui::GetFont()->CalcTextSizeA(font, textWidth, textWidth, h.subtitle).y : 0.0f;
            const float objectiveHeight = objective ? ImGui::GetFont()->CalcTextSizeA(labelFontSize + scale, textWidth, textWidth, h.objective).y : 0.0f;
            const float hintHeight = h.choiceAvailable ? 18.0f * scale : 0.0f;
            const float panelHeight = 37.0f * scale + textHeight + objectiveHeight + hintHeight +
                                      (subtitle && objective ? 14.0f * scale : 0.0f) +
                                      (h.challengeSeconds > 0.0f ? 15.0f * scale : 0.0f);
            const float x = center - panelWidth * 0.5f;
            // Keep the clock, XP bar, and minimap below the narrative surface.
            const float bottom = bounds.y - std::min(height * 0.24f, 155.0f * scale);
            const float y = std::max(origin.y + height * 0.32f, bottom - panelHeight);
            const float fadeIn = Unit(static_cast<float>((ImGui::GetTime() - messageStart) / 0.45));
            const float alpha = subtitle ? std::min(fadeIn, Unit(h.subtitleSeconds / 0.8f)) : 0.92f;
            const float panelAlpha = (objective || h.choiceAvailable) ? 0.92f : alpha;
            Panel(draw, ImVec2(x, y), ImVec2(x + panelWidth, y + panelHeight), scale, panelAlpha);
            DrawText(draw, ImVec2(x + 19.0f * scale, y + 10.0f * scale), labelFontSize,
                     Color(117, 182, 169, panelAlpha), "THE DROWNED SIGNAL", 0.0f, panelAlpha);
            float textY = y + 28.0f * scale;
            if (subtitle) {
                DrawText(draw, ImVec2(x + 19.0f * scale, textY), font, Color(229, 223, 202, alpha),
                         h.subtitle, textWidth, alpha);
                textY += textHeight + (objective ? 14.0f * scale : 0.0f);
            }
            if (objective) {
                DrawText(draw, ImVec2(x + 19.0f * scale, textY), labelFontSize + scale, Color(158, 198, 184, panelAlpha),
                         h.objective, textWidth, panelAlpha);
                textY += objectiveHeight;
            }
            if (h.challengeSeconds > 0.0f) {
                const float barY = textY + 7.0f * scale;
                draw->AddRectFilled(ImVec2(x + 19.0f * scale, barY),
                                    ImVec2(x + panelWidth - 19.0f * scale, barY + 3.0f * scale), Color(55, 64, 61, panelAlpha));
                if (Unit(h.challengeProgress) > 0.0f) {
                    draw->AddRectFilled(ImVec2(x + 19.0f * scale, barY),
                                        ImVec2(x + 19.0f * scale + textWidth * Unit(h.challengeProgress), barY + 3.0f * scale),
                                        Color(122, 174, 156, panelAlpha));
                }
                textY += 15.0f * scale;
            }
            if (h.choiceAvailable) {
                DrawText(draw, ImVec2(x + 19.0f * scale, textY + 6.0f * scale), labelFontSize,
                         Color(191, 162, 123, panelAlpha), "An answer is waiting: Enhancements > BEN DROWNED", textWidth, panelAlpha);
            }
        }
        draw->PopClipRect();
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

void Ben_DrawSessionPanel() {
    BenHudSnapshot h{};
    Ben_GetHudSnapshot(&h);
    ImGui::PushID("BenSessionPanel");
    DrawSession(h);
    ImGui::PopID();
}

void Ben_DrawResponsePanel() {
    BenHudSnapshot h{};
    Ben_GetHudSnapshot(&h);
    ImGui::PushID("BenResponsePanel");
    DrawResponses(h);
    ImGui::PopID();
}

void Ben_DrawJournalPanel() {
    BenHudSnapshot h{};
    Ben_GetHudSnapshot(&h);
    ImGui::PushID("BenJournalPanel");
    DrawJournal(h, 330.0f);
    ImGui::PopID();
}

void BenDrownedJournal::DrawElement() {
    BenHudSnapshot h{};
    Ben_GetHudSnapshot(&h);
    ImGui::PushStyleColor(ImGuiCol_Text, Parchment);
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.24f, 0.36f, 0.32f, 0.65f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.24f, 0.22f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.35f, 0.31f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.24f, 0.41f, 0.36f, 1.0f));
    DrawSession(h);
    ImGui::Spacing();
    ImGui::Separator();
    if (ImGui::BeginTabBar("##BenCasebookTabs")) {
        if (ImGui::BeginTabItem("The signal")) {
            ImGui::Spacing();
            DrawResponses(h);
            if (h.subtitle[0]) {
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextColored(Ash, "LAST TRANSMISSION");
                ImGui::TextWrapped("%s", h.subtitle);
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Casebook")) {
            ImGui::Spacing();
            DrawJournal(h, std::max(140.0f, ImGui::GetContentRegionAvail().y - 98.0f));
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::PopStyleColor(5);
}
