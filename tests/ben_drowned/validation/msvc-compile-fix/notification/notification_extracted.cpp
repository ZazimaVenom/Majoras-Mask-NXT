
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <vector>
struct ImVec2 { float x{}, y{}; ImVec2() = default; ImVec2(float a, float b): x(a), y(b) {} };
struct Viewport { ImVec2 Pos{100.0f,50.0f}, Size{1280.0f,720.0f}; };
struct Io { float DeltaTime = 1.0f; };
namespace ImGui {
Viewport viewport;
Io io;
ImVec2 GetWindowSize() { return {220.0f,40.0f}; }
Io& GetIO() { return io; }
}
struct Options { int id{}; float remainingTime{}; };
std::vector<Options> notifications;
std::vector<ImVec2> positions;
struct Window { void UpdateElement(); };
void DrawPositions(int position) {
    auto* vp = &ImGui::viewport;
    const float margin = 30.0f;
    const float padding = 10.0f;
    ImVec2 basePosition;
switch (position) {
        case 0: // Top Left
            basePosition = ImVec2(vp->Pos.x + margin, vp->Pos.y + margin);
            break;
        case 1: // Top Right
            basePosition = ImVec2(vp->Pos.x + vp->Size.x - margin, vp->Pos.y + margin);
            break;
        case 2: // Bottom Left
            basePosition = ImVec2(vp->Pos.x + margin, vp->Pos.y + vp->Size.y - margin);
            break;
        case 3: // Bottom Right
            basePosition = ImVec2(vp->Pos.x + vp->Size.x - margin, vp->Pos.y + vp->Size.y - margin);
            break;
        case 4: // Hidden
            return;
    }
for (std::size_t index = 0; index < notifications.size(); ++index) {
        auto& notification = notifications[index];
        const float inverseIndex = static_cast<float>(notifications.size() - 1 - index);

        
        (void)notification;
        ImVec2 notificationPos;
switch (position) {
            case 0: // Top Left
                notificationPos =
                    ImVec2(basePosition.x, basePosition.y + ((ImGui::GetWindowSize().y + padding) * inverseIndex));
                break;
            case 1: // Top Right
                notificationPos = ImVec2(basePosition.x - ImGui::GetWindowSize().x,
                                         basePosition.y + ((ImGui::GetWindowSize().y + padding) * inverseIndex));
                break;
            case 2: // Bottom Left
                notificationPos = ImVec2(basePosition.x,
                                         basePosition.y - ((ImGui::GetWindowSize().y + padding) * (inverseIndex + 1)));
                break;
            case 3: // Bottom Right
                notificationPos = ImVec2(basePosition.x - ImGui::GetWindowSize().x,
                                         basePosition.y - ((ImGui::GetWindowSize().y + padding) * (inverseIndex + 1)));
                break;
        }
        positions.push_back(notificationPos);
    }
}
void Window::UpdateElement() {
    for (auto notification = notifications.begin(); notification != notifications.end();) {
        // decrement remainingTime
        notification->remainingTime -= ImGui::GetIO().DeltaTime;

        // remove notification if it has expired
        if (notification->remainingTime <= 0) {
            notification = notifications.erase(notification);
        } else {
            ++notification;
        }
    }
}
int checks = 0;
void Check(bool value) {
    ++checks;
    if (!value) { std::cerr << "check failed: " << checks << '\n'; std::exit(1); }
}
int main() {
    const float topY[] = {230.0f,180.0f,130.0f,80.0f};
    const float bottomY[] = {540.0f,590.0f,640.0f,690.0f};
    for (int corner = 0; corner != 4; ++corner) {
        notifications.clear(); positions.clear(); DrawPositions(corner);
        Check(positions.empty());
        notifications.resize(1); positions.clear(); DrawPositions(corner);
        Check(positions.size() == 1);
        Check(positions[0].x == (corner % 2 == 0 ? 130.0f : 1130.0f));
        Check(positions[0].y == (corner < 2 ? 80.0f : 690.0f));
        notifications.resize(4); positions.clear(); DrawPositions(corner);
        Check(positions.size() == 4);
        for (std::size_t i = 0; i < positions.size(); ++i) {
            Check(positions[i].x == (corner % 2 == 0 ? 130.0f : 1130.0f));
            Check(positions[i].y == (corner < 2 ? topY[i] : bottomY[i]));
            Check(positions[i].y >= 50.0f && positions[i].y + 40.0f <= 770.0f);
        }
    }
    positions.clear(); DrawPositions(4); Check(positions.empty());
    Window window;
    notifications.clear(); window.UpdateElement(); Check(notifications.empty());
    notifications = {{1,0.1f},{2,0.2f},{3,5.0f},{4,0.3f},{5,0.4f},{6,6.0f}};
    window.UpdateElement();
    Check(notifications.size() == 2);
    Check(notifications[0].id == 3 && notifications[0].remainingTime == 4.0f);
    Check(notifications[1].id == 6 && notifications[1].remainingTime == 5.0f);
    ImGui::io.DeltaTime = 10.0f; window.UpdateElement(); Check(notifications.empty());
    notifications = {{7,10.0f}}; ImGui::io.DeltaTime = 1.0f; window.UpdateElement();
    Check(notifications.size() == 1 && notifications[0].remainingTime == 9.0f);
    std::cout << "PASS: " << checks << " extracted notification checks\n";
}
