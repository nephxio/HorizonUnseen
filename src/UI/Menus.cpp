#include "UI/Menus.h"

#include "UI/UiTheme.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace hu {
namespace {

constexpr float ButtonHeight = 42.0f;

// A wide button that spans the panel; returns true when clicked. `enabled`
// false renders it greyed and swallows the click.
bool menuButton(const char* label, bool enabled = true, const char* tooltip = nullptr) {
    if (!enabled) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::Locked);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.10f, 0.12f, 0.70f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.10f, 0.10f, 0.12f, 0.70f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.10f, 0.10f, 0.12f, 0.70f));
    }

    const bool clicked = ImGui::Button(label, ImVec2(-1.0f, ButtonHeight));

    if (tooltip && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }

    if (!enabled) {
        ImGui::PopStyleColor(4);
        return false;
    }
    return clicked;
}

void centeredText(const char* text, const ImVec4& color) {
    const float width = ImGui::GetContentRegionAvail().x;
    const ImVec2 size = ImGui::CalcTextSize(text);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (width - size.x) * 0.5f));
    ImGui::TextColored(color, "%s", text);
}

} // namespace

void Menus::beginCenteredPanel(const char* id, float viewportWidth, float viewportHeight,
                               float panelWidth, float panelHeight) {
    ImGui::SetNextWindowPos(ImVec2((viewportWidth - panelWidth) * 0.5f,
                                   (viewportHeight - panelHeight) * 0.5f));
    ImGui::SetNextWindowSize(ImVec2(panelWidth, panelHeight));
    ImGui::Begin(id, nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoSavedSettings);
}

void Menus::endCenteredPanel() {
    ImGui::End();
}

void Menus::drawTitle(const char* title, const char* subtitle, float viewportWidth, float y) {
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    // Scale the title up by drawing it at a larger font size.
    const float titleScale = 3.2f;
    const float fontSize = ImGui::GetFontSize() * titleScale;
    const ImVec2 size = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, title);
    const ImVec2 pos((viewportWidth - size.x) * 0.5f, y);

    // Soft glow behind the title: the same text drawn a few times at low alpha.
    const float pulse = 0.5f + 0.5f * std::sin(m_titlePulse * 1.6f);
    for (int i = 3; i >= 1; --i) {
        const float offset = static_cast<float>(i);
        dl->AddText(ImGui::GetFont(), fontSize,
                    ImVec2(pos.x - offset, pos.y - offset),
                    ImGui::GetColorU32(theme::withAlpha(theme::Accent, 0.06f + 0.03f * pulse)),
                    title);
        dl->AddText(ImGui::GetFont(), fontSize,
                    ImVec2(pos.x + offset, pos.y + offset),
                    ImGui::GetColorU32(theme::withAlpha(theme::Accent, 0.06f + 0.03f * pulse)),
                    title);
    }
    dl->AddText(ImGui::GetFont(), fontSize, pos,
                ImGui::GetColorU32(ImVec4(0.92f, 0.96f, 1.0f, 1.0f)), title);

    if (subtitle) {
        const ImVec2 subSize = ImGui::CalcTextSize(subtitle);
        dl->AddText(ImVec2((viewportWidth - subSize.x) * 0.5f, pos.y + size.y + 6.0f),
                    ImGui::GetColorU32(theme::TextDim), subtitle);
    }
}

MenuAction Menus::drawMainMenu(const ProgressModel& progress, float viewportWidth,
                               float viewportHeight, float deltaTime) {
    m_titlePulse += deltaTime;

    drawTitle("HORIZON UNSEEN", "a side-scrolling shooter", viewportWidth, viewportHeight * 0.16f);

    MenuAction action = MenuAction::None;

    const float panelWidth = 380.0f;
    const float panelHeight = 330.0f;
    ImGui::SetNextWindowPos(ImVec2((viewportWidth - panelWidth) * 0.5f, viewportHeight * 0.40f));
    ImGui::SetNextWindowSize(ImVec2(panelWidth, panelHeight));
    ImGui::Begin("##mainmenu", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoSavedSettings);

    if (menuButton("START GAME")) {
        action = MenuAction::StartNormal;
    }

    // Bullet hell is the reward for finding every secret in every level.
    const bool unlocked = progress.bulletHellUnlocked;
    char bulletHellLabel[96];
    if (unlocked) {
        std::snprintf(bulletHellLabel, sizeof(bulletHellLabel), "BULLET HELL MODE");
    } else {
        std::snprintf(bulletHellLabel, sizeof(bulletHellLabel), "BULLET HELL  [LOCKED %d/%d]",
                      progress.totalFound, progress.totalSecrets);
    }

    if (menuButton(bulletHellLabel, unlocked,
                   unlocked ? "Every secret found. Good luck."
                            : "Find every secret in every level to unlock.")) {
        action = MenuAction::StartBulletHell;
    }

    if (menuButton("SECRETS")) {
        action = MenuAction::OpenSecrets;
    }
    if (menuButton("OPTIONS")) {
        action = MenuAction::OpenOptions;
    }
    if (menuButton("QUIT")) {
        action = MenuAction::QuitGame;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(theme::TextDim, "WASD/Arrows move  -  Space fire  -  Shift super");
    ImGui::TextColored(theme::TextDim, "[ and ] swap weapons  -  ` debug console");

    ImGui::End();
    return action;
}

MenuAction Menus::drawPauseMenu(float viewportWidth, float viewportHeight) {
    MenuAction action = MenuAction::None;

    // Dim the game behind the panel.
    ImGui::GetBackgroundDrawList()->AddRectFilled(
        ImVec2(0, 0), ImVec2(viewportWidth, viewportHeight),
        ImGui::GetColorU32(ImVec4(0, 0, 0, 0.55f)));

    beginCenteredPanel("##pause", viewportWidth, viewportHeight, 340.0f, 250.0f);

    centeredText("PAUSED", theme::Accent);
    ImGui::Separator();
    ImGui::Spacing();

    if (menuButton("RESUME"))        { action = MenuAction::Resume; }
    if (menuButton("RESTART LEVEL")) { action = MenuAction::RestartLevel; }
    if (menuButton("QUIT TO MENU"))  { action = MenuAction::QuitToMenu; }

    endCenteredPanel();
    return action;
}

MenuAction Menus::drawGameOver(const HudModel& model, float viewportWidth, float viewportHeight) {
    MenuAction action = MenuAction::None;

    ImGui::GetBackgroundDrawList()->AddRectFilled(
        ImVec2(0, 0), ImVec2(viewportWidth, viewportHeight),
        ImGui::GetColorU32(ImVec4(0.12f, 0.0f, 0.0f, 0.62f)));

    beginCenteredPanel("##gameover", viewportWidth, viewportHeight, 380.0f, 280.0f);

    centeredText("ALL CELLS BROKEN", theme::Danger);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Score");
    ImGui::SameLine();
    ImGui::TextColored(theme::Accent, "%lld", model.score);

    ImGui::Text("Secrets found");
    ImGui::SameLine();
    ImGui::TextColored(theme::Secret, "%d / %d", model.secretsFound, model.secretsTotal);

    ImGui::Spacing();
    if (menuButton("RETRY"))        { action = MenuAction::RestartLevel; }
    if (menuButton("QUIT TO MENU")) { action = MenuAction::QuitToMenu; }

    endCenteredPanel();
    return action;
}

MenuAction Menus::drawLevelComplete(const HudModel& model, float viewportWidth, float viewportHeight) {
    MenuAction action = MenuAction::None;

    ImGui::GetBackgroundDrawList()->AddRectFilled(
        ImVec2(0, 0), ImVec2(viewportWidth, viewportHeight),
        ImGui::GetColorU32(ImVec4(0.0f, 0.06f, 0.10f, 0.62f)));

    beginCenteredPanel("##levelcomplete", viewportWidth, viewportHeight, 400.0f, 290.0f);

    centeredText("SECTOR CLEARED", theme::Health);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Level");
    ImGui::SameLine();
    ImGui::TextColored(theme::Accent, "%s", model.levelName.c_str());

    ImGui::Text("Score");
    ImGui::SameLine();
    ImGui::TextColored(theme::Accent, "%lld", model.score);

    ImGui::Text("Secrets");
    ImGui::SameLine();
    ImGui::TextColored(theme::Secret, "%d / %d", model.secretsFound, model.secretsTotal);

    if (model.secretsFound < model.secretsTotal) {
        ImGui::TextColored(theme::TextDim, "Replay to find the rest.");
    }

    ImGui::Spacing();
    if (menuButton("PLAY AGAIN"))   { action = MenuAction::RestartLevel; }
    if (menuButton("QUIT TO MENU")) { action = MenuAction::QuitToMenu; }

    endCenteredPanel();
    return action;
}

MenuAction Menus::drawSecretsScreen(const ProgressModel& progress, float viewportWidth,
                                    float viewportHeight) {
    MenuAction action = MenuAction::None;

    const float panelWidth = std::min(760.0f, viewportWidth - 80.0f);
    const float panelHeight = std::min(520.0f, viewportHeight - 80.0f);
    beginCenteredPanel("##secrets", viewportWidth, viewportHeight, panelWidth, panelHeight);

    centeredText("SECRETS", theme::Secret);

    // Overall progress toward the bullet hell unlock.
    const float pct = progress.totalSecrets > 0
                    ? static_cast<float>(progress.totalFound) / static_cast<float>(progress.totalSecrets)
                    : 0.0f;
    char overlay[64];
    std::snprintf(overlay, sizeof(overlay), "%d / %d found", progress.totalFound, progress.totalSecrets);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, theme::Secret);
    ImGui::ProgressBar(pct, ImVec2(-1.0f, 18.0f), overlay);
    ImGui::PopStyleColor();

    if (progress.bulletHellUnlocked) {
        ImGui::TextColored(theme::Danger, "BULLET HELL MODE UNLOCKED");
    } else {
        ImGui::TextColored(theme::TextDim, "Find every secret in every level to unlock Bullet Hell mode.");
    }

    ImGui::Separator();

    ImGui::BeginChild("##secretlist", ImVec2(0.0f, panelHeight - 190.0f));
    for (const LevelProgressView& level : progress.levels) {
        char header[192];
        std::snprintf(header, sizeof(header), "%s   (%d/%d)",
                      level.displayName.c_str(), level.secretsFound, level.secretsTotal);

        if (ImGui::CollapsingHeader(header, ImGuiTreeNodeFlags_DefaultOpen)) {
            if (!level.played) {
                // Nothing is revealed about a level the player has not tried.
                ImGui::TextColored(theme::TextDim, "  Play this level to reveal its secret hints.");
                continue;
            }

            for (std::size_t i = 0; i < level.secretNames.size(); ++i) {
                const bool found = i < level.secretUnlocked.size() && level.secretUnlocked[i];
                ImGui::PushID(static_cast<int>(i));

                ImGui::TextColored(found ? theme::Secret : theme::Locked,
                                   "  %s  %s",
                                   found ? "[FOUND]" : "[ ---- ]",
                                   found ? level.secretNames[i].c_str() : "???");

                // The hint is the help; the name is the reward.
                if (i < level.secretHints.size() && !found) {
                    ImGui::TextColored(theme::TextDim, "          hint: %s",
                                       level.secretHints[i].c_str());
                }
                ImGui::PopID();
            }
        }
    }
    ImGui::EndChild();

    ImGui::Separator();
    if (menuButton("BACK")) {
        action = MenuAction::BackToMainMenu;
    }

    endCenteredPanel();
    return action;
}

MenuAction Menus::drawOptions(float viewportWidth, float viewportHeight) {
    MenuAction action = MenuAction::None;

    beginCenteredPanel("##options", viewportWidth, viewportHeight, 420.0f, 320.0f);

    centeredText("OPTIONS", theme::Accent);
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextColored(theme::TextDim, "CONTROLS");
    ImGui::BulletText("Move          WASD or Arrow keys");
    ImGui::BulletText("Fire          Space");
    ImGui::BulletText("Superweapon   Left or Right Shift");
    ImGui::BulletText("Swap weapon   [ and ]");
    ImGui::BulletText("Pause         Escape");
    ImGui::BulletText("Debug console `  (backtick)");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(theme::TextDim,
                       "Live tuning lives in the debug console; the editor\n"
                       "application exposes the full config.");

    ImGui::Spacing();
    if (menuButton("BACK")) {
        action = MenuAction::BackToMainMenu;
    }

    endCenteredPanel();
    return action;
}

} // namespace hu
