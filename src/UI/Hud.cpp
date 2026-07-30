#include "UI/Hud.h"

#include "UI/UiTheme.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace hu {
namespace {

// ---- Layout constants -----------------------------------------------------
constexpr float Margin        = 18.0f;
constexpr float CellWidth     = 78.0f;
constexpr float CellHeight    = 44.0f;
constexpr float CellGap       = 8.0f;
constexpr float ToastWidth    = 340.0f;
constexpr float ToastHeight   = 54.0f;
constexpr float ToastFadeTime = 0.45f;
constexpr float MaxToasts     = 4.0f;

ImU32 toU32(const ImVec4& c) { return ImGui::GetColorU32(c); }

} // namespace

void Hud::pushToast(const Toast& toast) {
    m_toasts.push_back(toast);
    // Cap the stack so a burst of pickups cannot bury the screen.
    while (m_toasts.size() > static_cast<std::size_t>(MaxToasts)) {
        m_toasts.pop_front();
    }
}

void Hud::clearToasts() {
    m_toasts.clear();
}

void Hud::updateToasts(float deltaTime) {
    for (auto& toast : m_toasts) {
        toast.age += deltaTime;
    }
    while (!m_toasts.empty() && m_toasts.front().age >= m_toasts.front().lifetime) {
        m_toasts.pop_front();
    }
}

void Hud::draw(const HudModel& model, float viewportWidth, float viewportHeight, float deltaTime) {
    m_pulseTime += deltaTime;
    updateToasts(deltaTime);

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(viewportWidth, viewportHeight));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoFocusOnAppearing;

    if (ImGui::Begin("##hud", nullptr, flags)) {
        const float cellsWidth = 5.0f * CellWidth + 4.0f * CellGap;

        drawEnergyCells(model, Margin, Margin, cellsWidth);
        drawDamagePressure(model, Margin, Margin + CellHeight + 26.0f, cellsWidth);
        drawWeaponBar(model, Margin, Margin + CellHeight + 58.0f);
        drawSuperweaponReadout(model, Margin + cellsWidth + 28.0f, Margin);
        drawLevelProgress(model, viewportWidth);
        drawPowerupLabels(model);
        drawBossBar(model, viewportWidth, viewportHeight);
        drawToasts(viewportWidth, viewportHeight);
    }
    ImGui::End();

    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// Each cell is drawn as a box with two independent fills: hit points as a
// bottom-up bar behind the box, and charge as a left-to-right overlay. That way
// the player can read "how hurt" and "how charged" at a glance, which is the
// whole point of the mechanic.
void Hud::drawEnergyCells(const HudModel& model, float x, float y, float width) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    (void)width;

    dl->AddText(ImVec2(x, y - 15.0f), toU32(theme::TextDim), "ENERGY CELLS");

    for (std::size_t i = 0; i < model.cells.size(); ++i) {
        const CellView& cell = model.cells[i];
        const float cx = x + static_cast<float>(i) * (CellWidth + CellGap);
        const ImVec2 topLeft(cx, y);
        const ImVec2 bottomRight(cx + CellWidth, y + CellHeight);

        // Backing plate.
        dl->AddRectFilled(topLeft, bottomRight, toU32(theme::Panel), 3.0f);

        if (cell.broken) {
            // A broken cell is unmistakable: dark red fill and a cross.
            dl->AddRectFilled(topLeft, bottomRight, toU32(theme::Broken), 3.0f);
            dl->AddLine(topLeft, bottomRight, toU32(theme::Danger), 2.0f);
            dl->AddLine(ImVec2(topLeft.x, bottomRight.y), ImVec2(bottomRight.x, topLeft.y),
                        toU32(theme::Danger), 2.0f);
        } else {
            // Hit points: vertical fill from the bottom.
            const float hp01 = cell.maxHealth > 0.0f
                             ? std::clamp(cell.health / cell.maxHealth, 0.0f, 1.0f) : 0.0f;
            const float hpTop = bottomRight.y - CellHeight * hp01;
            const ImVec4 hpColor = theme::mix(theme::Danger, theme::Health, hp01);
            dl->AddRectFilled(ImVec2(topLeft.x, hpTop), bottomRight,
                              toU32(theme::withAlpha(hpColor, 0.45f)), 3.0f);

            // Charge: horizontal overlay bar across the lower third.
            const float charge01 = cell.maxCharge > 0.0f
                                 ? std::clamp(cell.charge / cell.maxCharge, 0.0f, 1.0f) : 0.0f;
            const float barTop = bottomRight.y - 11.0f;
            dl->AddRectFilled(ImVec2(topLeft.x + 3.0f, barTop),
                              ImVec2(bottomRight.x - 3.0f, bottomRight.y - 3.0f),
                              toU32(ImVec4(0, 0, 0, 0.55f)), 2.0f);
            if (charge01 > 0.0f) {
                // A fully charged cell pulses so "you can fire a superweapon"
                // is readable in peripheral vision.
                ImVec4 chargeColor = theme::Charge;
                if (cell.charged) {
                    const float pulse = 0.5f + 0.5f * std::sin(m_pulseTime * 6.0f);
                    chargeColor = theme::mix(theme::Charge, theme::ChargeFull, pulse);
                }
                dl->AddRectFilled(
                    ImVec2(topLeft.x + 3.0f, barTop),
                    ImVec2(topLeft.x + 3.0f + (CellWidth - 6.0f) * charge01, bottomRight.y - 3.0f),
                    toU32(chargeColor), 2.0f);
            }
        }

        // Border, brightened when the cell is charged and ready.
        const ImVec4 border = cell.broken   ? theme::Danger
                            : cell.charged  ? theme::ChargeFull
                                            : theme::AccentDim;
        dl->AddRect(topLeft, bottomRight, toU32(border), 3.0f, 0, cell.charged ? 2.0f : 1.0f);

        char label[8];
        std::snprintf(label, sizeof(label), "%d", static_cast<int>(i) + 1);
        dl->AddText(ImVec2(cx + 6.0f, y + 4.0f), toU32(theme::TextDim), label);
    }
}

// Shows the rolling 5-second damage rate against the active cell's threshold.
// Under the line, incoming fire charges the cells; over it, cells lose HP.
void Hud::drawDamagePressure(const HudModel& model, float x, float y, float width) {
    ImDrawList* dl = ImGui::GetWindowDrawList();

    const float barHeight = 9.0f;
    const ImVec2 topLeft(x, y);
    const ImVec2 bottomRight(x + width, y + barHeight);

    dl->AddRectFilled(topLeft, bottomRight, toU32(ImVec4(0, 0, 0, 0.55f)), 2.0f);

    // Scale the bar to twice the threshold so the threshold sits mid-bar and
    // the player can see how far over/under they are.
    const float scaleMax = std::max(model.damageThreshold * 2.0f, 1.0f);
    const float rate01 = std::clamp(model.damageRate / scaleMax, 0.0f, 1.0f);
    const bool overThreshold = model.damageRate > model.damageThreshold;

    dl->AddRectFilled(topLeft, ImVec2(x + width * rate01, bottomRight.y),
                      toU32(overThreshold ? theme::Danger : theme::Accent), 2.0f);

    // Threshold tick.
    const float tickX = x + width * std::clamp(model.damageThreshold / scaleMax, 0.0f, 1.0f);
    dl->AddLine(ImVec2(tickX, y - 2.0f), ImVec2(tickX, bottomRight.y + 2.0f),
                toU32(theme::ChargeFull), 2.0f);

    char text[96];
    std::snprintf(text, sizeof(text), "DAMAGE %.0f/s  %s",
                  model.damageRate,
                  overThreshold ? "BREAKING CELLS" : "CHARGING");
    dl->AddText(ImVec2(x, y + barHeight + 3.0f),
                toU32(overThreshold ? theme::Danger : theme::TextDim), text);
}

void Hud::drawWeaponBar(const HudModel& model, float x, float y) {
    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddText(ImVec2(x, y), toU32(theme::TextDim), "WEAPONS  [ / ] to swap");

    float cursorX = x;
    const float rowY = y + 18.0f;
    for (std::size_t i = 0; i < model.weapons.size(); ++i) {
        const WeaponView& weapon = model.weapons[i];
        const bool isCurrent = weapon.type == model.currentWeapon;
        const ImVec4 color = weapon.unlocked ? theme::weaponColor(static_cast<int>(i))
                                             : theme::Locked;

        char label[64];
        if (weapon.unlocked) {
            std::snprintf(label, sizeof(label), "%s L%d", weaponName(weapon.type), weapon.level);
        } else {
            std::snprintf(label, sizeof(label), "%s --", weaponName(weapon.type));
        }

        const ImVec2 textSize = ImGui::CalcTextSize(label);
        const ImVec2 topLeft(cursorX, rowY);
        const ImVec2 bottomRight(cursorX + textSize.x + 16.0f, rowY + textSize.y + 8.0f);

        if (isCurrent) {
            dl->AddRectFilled(topLeft, bottomRight, toU32(theme::withAlpha(color, 0.22f)), 3.0f);
            dl->AddRect(topLeft, bottomRight, toU32(color), 3.0f, 0, 1.5f);
        }
        dl->AddText(ImVec2(topLeft.x + 8.0f, topLeft.y + 4.0f), toU32(color), label);

        // Level pips make "how upgraded am I" readable without counting text.
        if (weapon.unlocked) {
            for (int pip = 0; pip < MaxWeaponLevel; ++pip) {
                const float px = topLeft.x + 8.0f + static_cast<float>(pip) * 6.0f;
                const float py = bottomRight.y + 4.0f;
                dl->AddRectFilled(ImVec2(px, py), ImVec2(px + 4.0f, py + 3.0f),
                                  toU32(pip < weapon.level ? color
                                                           : theme::withAlpha(color, 0.22f)));
            }
        }

        cursorX = bottomRight.x + 10.0f;
    }
}

void Hud::drawSuperweaponReadout(const HudModel& model, float x, float y) {
    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddText(ImVec2(x, y - 15.0f), toU32(theme::TextDim), "SUPERWEAPON  (Shift)");

    const bool ready = model.pendingSuperweapon != SuperweaponType::None;
    const float pulse = 0.5f + 0.5f * std::sin(m_pulseTime * 5.0f);
    const ImVec4 color = ready ? theme::mix(theme::Charge, theme::ChargeFull, pulse)
                               : theme::Locked;

    const ImVec2 topLeft(x, y);
    const ImVec2 bottomRight(x + 210.0f, y + CellHeight);
    dl->AddRectFilled(topLeft, bottomRight, toU32(theme::Panel), 3.0f);
    dl->AddRect(topLeft, bottomRight, toU32(color), 3.0f, 0, ready ? 2.0f : 1.0f);

    dl->AddText(ImVec2(x + 10.0f, y + 8.0f), toU32(color),
                ready ? superweaponName(model.pendingSuperweapon) : "No charge");

    char sub[64];
    std::snprintf(sub, sizeof(sub), "%d cell%s charged",
                  model.chargedCells, model.chargedCells == 1 ? "" : "s");
    dl->AddText(ImVec2(x + 10.0f, y + 25.0f), toU32(theme::TextDim), sub);
}

void Hud::drawLevelProgress(const HudModel& model, float viewportWidth) {
    ImDrawList* dl = ImGui::GetWindowDrawList();

    const float width = 260.0f;
    const float x = viewportWidth - width - Margin;
    const float y = Margin;

    char header[160];
    std::snprintf(header, sizeof(header), "%s%s",
                  model.levelName.c_str(),
                  model.difficulty == DifficultyMode::BulletHell ? "  [BULLET HELL]" : "");
    const ImVec2 headerSize = ImGui::CalcTextSize(header);
    dl->AddText(ImVec2(x + width - headerSize.x, y),
                toU32(model.difficulty == DifficultyMode::BulletHell ? theme::Danger
                                                                    : theme::Accent),
                header);

    // Progress through the level.
    const float barY = y + 20.0f;
    dl->AddRectFilled(ImVec2(x, barY), ImVec2(x + width, barY + 7.0f),
                      toU32(ImVec4(0, 0, 0, 0.55f)), 2.0f);
    dl->AddRectFilled(ImVec2(x, barY),
                      ImVec2(x + width * std::clamp(model.levelProgress01, 0.0f, 1.0f), barY + 7.0f),
                      toU32(theme::Accent), 2.0f);

    char stats[160];
    std::snprintf(stats, sizeof(stats), "SCORE %lld    GRAZE %lld    SECRETS %d/%d",
                  model.score, model.grazeCount, model.secretsFound, model.secretsTotal);
    const ImVec2 statsSize = ImGui::CalcTextSize(stats);
    dl->AddText(ImVec2(x + width - statsSize.x, barY + 12.0f), toU32(theme::TextDim), stats);
}

// Names each power-up on the field. World pixels map 1:1 to screen pixels in
// this projection, so the pickup's world position is its screen position.
void Hud::drawPowerupLabels(const HudModel& model) {
    if (model.powerupLabels.empty()) {
        return;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();

    for (const PowerupLabel& label : model.powerupLabels) {
        if (label.text.empty() || label.alpha <= 0.01f) {
            continue;
        }

        ImVec4 color;
        switch (label.type) {
            case PowerupType::WeaponSpread:  color = theme::weaponColor(1); break;
            case PowerupType::WeaponMissile: color = theme::weaponColor(2); break;
            case PowerupType::WeaponLaser:   color = theme::weaponColor(3); break;
            case PowerupType::BulletUpgrade: color = theme::weaponColor(0); break;
            case PowerupType::CellRepair:    color = theme::Health;         break;
            case PowerupType::EnergyCharge:  color = theme::Charge;         break;
            default:                         color = theme::TextDim;        break;
        }

        const ImVec2 size = ImGui::CalcTextSize(label.text.c_str());
        // Centred just below the pickup so the icon itself stays unobscured.
        const ImVec2 pos(label.position.x - size.x * 0.5f, label.position.y + 20.0f);

        // Dark plate behind the text: the field can be bright with additive
        // bullets, and unbacked text disappears into it.
        dl->AddRectFilled(ImVec2(pos.x - 4.0f, pos.y - 2.0f),
                          ImVec2(pos.x + size.x + 4.0f, pos.y + size.y + 2.0f),
                          ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.55f * label.alpha)),
                          3.0f);
        dl->AddText(pos, ImGui::GetColorU32(theme::withAlpha(color, label.alpha)),
                    label.text.c_str());
    }
}

void Hud::drawBossBar(const HudModel& model, float viewportWidth, float viewportHeight) {
    if (!model.bossActive) {
        return;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();

    const float width = viewportWidth * 0.55f;
    const float x = (viewportWidth - width) * 0.5f;
    const float y = viewportHeight - 52.0f;

    const char* name = model.bossName.empty() ? "WARDEN" : model.bossName.c_str();
    const ImVec2 nameSize = ImGui::CalcTextSize(name);
    dl->AddText(ImVec2((viewportWidth - nameSize.x) * 0.5f, y - 18.0f), toU32(theme::Danger), name);

    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + width, y + 14.0f),
                      toU32(ImVec4(0, 0, 0, 0.65f)), 3.0f);
    dl->AddRectFilled(ImVec2(x, y),
                      ImVec2(x + width * std::clamp(model.bossHealth01, 0.0f, 1.0f), y + 14.0f),
                      toU32(theme::Danger), 3.0f);
    dl->AddRect(ImVec2(x, y), ImVec2(x + width, y + 14.0f), toU32(theme::Danger), 3.0f, 0, 1.5f);
}

void Hud::drawToasts(float viewportWidth, float viewportHeight) {
    ImDrawList* dl = ImGui::GetWindowDrawList();

    float y = viewportHeight * 0.30f;
    for (const Toast& toast : m_toasts) {
        // Fade in at the start and out at the end.
        float alpha = 1.0f;
        if (toast.age < ToastFadeTime) {
            alpha = toast.age / ToastFadeTime;
        } else if (toast.age > toast.lifetime - ToastFadeTime) {
            alpha = std::max(0.0f, (toast.lifetime - toast.age) / ToastFadeTime);
        }

        ImVec4 accent;
        switch (toast.kind) {
            case ToastKind::Secret:  accent = theme::Secret; break;
            case ToastKind::Powerup: accent = theme::Health; break;
            case ToastKind::Warning: accent = theme::Danger; break;
            default:                 accent = theme::Accent; break;
        }

        const float x = (viewportWidth - ToastWidth) * 0.5f;
        const ImVec2 topLeft(x, y);
        const ImVec2 bottomRight(x + ToastWidth, y + ToastHeight);

        dl->AddRectFilled(topLeft, bottomRight, toU32(theme::withAlpha(theme::Panel, 0.88f * alpha)), 4.0f);
        dl->AddRect(topLeft, bottomRight, toU32(theme::withAlpha(accent, alpha)), 4.0f, 0, 1.5f);
        // Accent stripe down the left edge.
        dl->AddRectFilled(topLeft, ImVec2(x + 4.0f, bottomRight.y),
                          toU32(theme::withAlpha(accent, alpha)), 4.0f);

        dl->AddText(ImVec2(x + 14.0f, y + 8.0f),
                    toU32(theme::withAlpha(accent, alpha)), toast.title.c_str());
        if (!toast.subtitle.empty()) {
            dl->AddText(ImVec2(x + 14.0f, y + 28.0f),
                        toU32(theme::withAlpha(theme::TextDim, alpha)), toast.subtitle.c_str());
        }

        y += ToastHeight + 8.0f;
    }
}

} // namespace hu
