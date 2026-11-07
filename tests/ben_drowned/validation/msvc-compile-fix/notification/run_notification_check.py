from pathlib import Path
import hashlib
import subprocess

HERE = Path(__file__).resolve().parent
SOURCE = HERE.parents[2] / 'source/Majoras-Mask-NXT/mm/2s2h/BenGui/Notification.cpp'
text = SOURCE.read_text()

def block(start):
    opening = text.index('{', start)
    depth = 1
    ending = opening + 1
    while depth:
        depth += (text[ending] == '{') - (text[ending] == '}')
        ending += 1
    return text[start:ending]

first_switch_start = text.index('switch (position)')
first_switch = block(first_switch_start)
second_switch = block(text.index('switch (position)', first_switch_start + 1))
update = block(text.index('void Window::UpdateElement()'))
loop_start = text.index('for (std::size_t index')
loop_prefix = text[loop_start:text.index('ImGui::SetNextWindowViewport', loop_start)]
code = r'''
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
''' + first_switch + '\n' + loop_prefix + r'''
        (void)notification;
        ImVec2 notificationPos;
''' + second_switch + r'''
        positions.push_back(notificationPos);
    }
}
''' + update + r'''
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
'''
fixture = HERE / 'notification_extracted.cpp'
fixture.write_text(code)
binary = HERE / 'notification_extracted'
command = ['g++', '-std=c++20', '-Wall', '-Wextra', '-Wconversion', '-Wsign-conversion',
           '-Wsign-compare', '-Werror', '-pedantic', '-fsanitize=undefined',
           '-fno-sanitize-recover=all', str(fixture), '-o', str(binary)]
compiled = subprocess.run(command, text=True, capture_output=True)
report = 'Source SHA256: ' + hashlib.sha256(SOURCE.read_bytes()).hexdigest() + '\n'
report += 'Scope: extracted production notification loop prefix, both corner-position switches,\n'
report += 'and complete UpdateElement method. Explicit viewport/window/IO/Options doubles.\n'
report += 'This does not compile the complete Notification.cpp translation unit or Windows application.\n'
report += 'Checks: empty/single/four notification layout in all corners; hidden layout;\n'
report += 'empty/single/all/adjacent expiry while preserving remaining item order and decrement.\n'
report += 'Command: ' + ' '.join(command) + '\n'
report += 'Compiler exit: ' + str(compiled.returncode) + '\n' + compiled.stdout + compiled.stderr
if compiled.returncode == 0:
    tested = subprocess.run([str(binary)], text=True, capture_output=True)
    report += tested.stdout + tested.stderr + 'Test exit: ' + str(tested.returncode) + '\n'
else:
    tested = compiled
(HERE / 'result.txt').write_text(report)
print(report)
raise SystemExit(tested.returncode)
