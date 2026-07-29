#include "UI/DebugOverlay.h"

#include "UI/UiTheme.h"

#include <cstring>

namespace hu {
namespace {

ImVec4 levelColor(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return ImVec4(0.50f, 0.50f, 0.55f, 1.0f);
        case LogLevel::Debug: return ImVec4(0.62f, 0.70f, 0.78f, 1.0f);
        case LogLevel::Info:  return ImVec4(0.85f, 0.90f, 0.95f, 1.0f);
        case LogLevel::Warn:  return ImVec4(1.00f, 0.78f, 0.30f, 1.0f);
        case LogLevel::Error: return ImVec4(1.00f, 0.40f, 0.40f, 1.0f);
        default:              return ImVec4(1, 1, 1, 1);
    }
}

// Case-insensitive substring test, so the filter box is forgiving.
bool containsInsensitive(const std::string& haystack, const char* needle) {
    if (!needle || needle[0] == '\0') {
        return true;
    }
    const std::size_t needleLen = std::strlen(needle);
    if (needleLen > haystack.size()) {
        return false;
    }
    for (std::size_t i = 0; i + needleLen <= haystack.size(); ++i) {
        std::size_t j = 0;
        while (j < needleLen &&
               std::tolower(static_cast<unsigned char>(haystack[i + j])) ==
               std::tolower(static_cast<unsigned char>(needle[j]))) {
            ++j;
        }
        if (j == needleLen) {
            return true;
        }
    }
    return false;
}

} // namespace

void DebugOverlay::draw(const DebugStats& stats, const HudModel& hud, bool& open,
                        DebugRequest& request) {
    if (!open) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(760.0f, 460.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(40.0f, 40.0f), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Debug Console", &open)) {
        if (ImGui::BeginTabBar("##debugtabs")) {
            if (ImGui::BeginTabItem("Log")) {
                drawLogPanel();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Stats")) {
                drawStatsPanel(stats, hud);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Cheats")) {
                drawCheatsPanel(stats, request);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

void DebugOverlay::drawCheatsPanel(const DebugStats& stats, DebugRequest& request) {
    ImGui::TextColored(theme::TextDim,
                       "Playtest shortcuts. These change live game state and\n"
                       "progression -- they are not part of normal play.");
    ImGui::Separator();

    ImGui::TextColored(theme::Accent, "MODE");
    // Bullet hell is gated behind finding every secret, which makes the mode
    // impossible to iterate on. Starting it from here bypasses the menu lock
    // without touching the saved progression.
    if (ImGui::Button("Restart in BULLET HELL", ImVec2(220.0f, 0.0f))) {
        request.startBulletHell = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Restart in Normal", ImVec2(180.0f, 0.0f))) {
        request.startNormal = true;
    }
    ImGui::TextColored(theme::TextDim, "hitbox %.1f px, graze band to %.1f px, %lld grazes",
                       stats.hitboxRadius, stats.grazeRadius, stats.grazeCount);

    ImGui::Separator();
    ImGui::TextColored(theme::Accent, "PROGRESSION");
    if (ImGui::Button("Unlock all secrets", ImVec2(220.0f, 0.0f))) {
        request.unlockAllSecrets = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset progress", ImVec2(180.0f, 0.0f))) {
        request.resetProgress = true;
    }
    ImGui::TextColored(theme::TextDim,
                       "Unlocking writes to the save file and enables the\n"
                       "Bullet Hell button on the main menu.");

    ImGui::Separator();
    ImGui::TextColored(theme::Accent, "ENERGY CELLS");
    if (ImGui::Button("Fill all charge", ImVec2(150.0f, 0.0f))) {
        request.fillCharge = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Repair all cells", ImVec2(150.0f, 0.0f))) {
        request.repairAllCells = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Break a cell", ImVec2(130.0f, 0.0f))) {
        request.breakOneCell = true;
    }
    ImGui::TextColored(theme::TextDim,
                       "Fill charge to test every superweapon tier immediately.");

    ImGui::Separator();
    ImGui::TextColored(theme::Accent, "LOADOUT");
    if (ImGui::Button("Grant all weapons at L5", ImVec2(220.0f, 0.0f))) {
        request.grantAllWeapons = true;
    }

    ImGui::Separator();
    ImGui::TextColored(theme::Accent, "WORLD");
    if (ImGui::Button("Kill all enemies", ImVec2(150.0f, 0.0f))) {
        request.killAllEnemies = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Skip to boss", ImVec2(150.0f, 0.0f))) {
        request.skipToBoss = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Toggle invulnerable", ImVec2(180.0f, 0.0f))) {
        request.toggleInvulnerable = true;
    }
}

void DebugOverlay::drawStatsPanel(const DebugStats& stats, const HudModel& hud) {
    ImGui::TextColored(theme::Accent, "PERFORMANCE");
    ImGui::Text("FPS %.1f    frame %.2f ms", stats.fps, stats.frameTimeMs);
    ImGui::Text("draw instances %d in %d batches", stats.drawInstances, stats.drawBatches);

    ImGui::Separator();
    ImGui::TextColored(theme::Accent, "ENTITIES");
    ImGui::Text("enemies            %d", stats.enemies);
    ImGui::Text("player projectiles %d", stats.playerProjectiles);
    ImGui::Text("enemy projectiles  %d", stats.enemyProjectiles);
    ImGui::Text("powerups           %d", stats.powerups);
    ImGui::Text("particles          %d / %d", stats.particles, stats.particleCapacity);

    ImGui::Separator();
    ImGui::TextColored(theme::Accent, "LEVEL");
    ImGui::Text("name        %s", hud.levelName.c_str());
    ImGui::Text("time        %.2f s", stats.levelTime);
    ImGui::Text("progress    %.1f %%", hud.levelProgress01 * 100.0f);
    ImGui::Text("scroll      %.1f px/s", stats.scrollSpeed);
    ImGui::Text("difficulty  %s",
                hud.difficulty == DifficultyMode::BulletHell ? "BULLET HELL" : "Normal");
    ImGui::Text("player      (%.1f, %.1f)", stats.playerPosition.x, stats.playerPosition.y);

    ImGui::Separator();
    ImGui::TextColored(theme::Accent, "ENERGY CELLS");
    // The damage-routing decision is the single most confusing part of the
    // game's design, so show the exact numbers behind it.
    ImGui::Text("5s damage rate %.2f/s   threshold %.2f/s  ->  %s",
                stats.damageWindowRate, stats.activeThreshold,
                stats.damageWindowRate > stats.activeThreshold ? "BREAKING HP" : "CHARGING");
    ImGui::Text("hitbox %.1f px   graze band %.1f-%.1f px   grazes %lld",
                stats.hitboxRadius, stats.hitboxRadius, stats.grazeRadius, stats.grazeCount);

    if (ImGui::BeginTable("##cells", 6,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Cell");
        ImGui::TableSetupColumn("HP");
        ImGui::TableSetupColumn("Max HP");
        ImGui::TableSetupColumn("Charge");
        ImGui::TableSetupColumn("Max Chg");
        ImGui::TableSetupColumn("State");
        ImGui::TableHeadersRow();

        for (std::size_t i = 0; i < hud.cells.size(); ++i) {
            const CellView& cell = hud.cells[i];
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("%d", static_cast<int>(i) + 1);
            ImGui::TableNextColumn(); ImGui::Text("%.1f", cell.health);
            ImGui::TableNextColumn(); ImGui::Text("%.1f", cell.maxHealth);
            ImGui::TableNextColumn(); ImGui::Text("%.1f", cell.charge);
            ImGui::TableNextColumn(); ImGui::Text("%.1f", cell.maxCharge);
            ImGui::TableNextColumn();
            if (cell.broken) {
                ImGui::TextColored(theme::Danger, "BROKEN");
            } else if (cell.charged) {
                ImGui::TextColored(theme::ChargeFull, "CHARGED");
            } else {
                ImGui::TextColored(theme::TextDim, "ok");
            }
        }
        ImGui::EndTable();
    }

    ImGui::Separator();
    ImGui::TextColored(theme::Accent, "WEAPONS");
    for (const WeaponView& weapon : hud.weapons) {
        ImGui::Text("%-8s %s  level %d",
                    weaponName(weapon.type),
                    weapon.unlocked ? "unlocked" : "locked  ",
                    weapon.level);
    }
    ImGui::Text("pending superweapon: %s (%d cells)",
                superweaponName(hud.pendingSuperweapon), hud.chargedCells);
}

void DebugOverlay::drawLogPanel() {
    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputTextWithHint("##filter", "filter text or category", m_filter, sizeof(m_filter));
    ImGui::SameLine();

    ImGui::SetNextItemWidth(110.0f);
    const char* levelNames[] = { "TRACE", "DEBUG", "INFO", "WARN", "ERROR" };
    ImGui::Combo("##minlevel", &m_minLevel, levelNames, IM_ARRAYSIZE(levelNames));
    ImGui::SameLine();

    // Pausing freezes a snapshot so a fast-scrolling log can actually be read.
    if (ImGui::Checkbox("Pause", &m_paused) && m_paused) {
        m_frozen = Log::snapshot();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &m_autoScroll);
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        Log::clearHistory();
        m_frozen.clear();
    }

    ImGui::Separator();

    // While paused we read the frozen snapshot; otherwise pull a fresh one.
    std::deque<LogEntry> live;
    if (!m_paused) {
        live = Log::snapshot();
    }
    const std::deque<LogEntry>& entries = m_paused ? m_frozen : live;

    ImGui::BeginChild("##logscroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    ImGuiListClipper clipper;
    std::vector<const LogEntry*> visible;
    visible.reserve(entries.size());
    for (const LogEntry& entry : entries) {
        if (static_cast<int>(entry.level) < m_minLevel) {
            continue;
        }
        if (!containsInsensitive(entry.message, m_filter) &&
            !containsInsensitive(entry.category, m_filter)) {
            continue;
        }
        visible.push_back(&entry);
    }

    clipper.Begin(static_cast<int>(visible.size()));
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            const LogEntry& entry = *visible[static_cast<std::size_t>(i)];
            ImGui::TextColored(levelColor(entry.level), "[%7.2f][f%06llu][%-5s][%-10s] %s",
                               entry.timeSeconds,
                               entry.frame,
                               Log::levelName(entry.level),
                               entry.category.c_str(),
                               entry.message.c_str());
        }
    }
    clipper.End();

    if (m_autoScroll && !m_paused && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
}

} // namespace hu
