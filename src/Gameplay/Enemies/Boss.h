#pragma once

// The level boss.
//
// Unlike the fodder archetypes the boss does not use a pluggable behaviour: its
// phases change movement *and* weapon together, and each phase needs its own
// timers, so it is simpler and clearer as a subclass with an explicit state
// machine.
//
// Phases are keyed to remaining health:
//   Phase 1  100% - 66%   Sentinel : slow vertical hover, aimed volleys.
//   Phase 2   66% - 33%   Weaver   : figure-eight path, sweeping spiral.
//   Phase 3   33% -  0%   Bulwark  : full-height sweeps, bullet wall with a gap.
// Every transition telegraphs with an effect, screen shake and a short
// invulnerability window, and is logged under the "Boss" category.

#include "Core/DrawList.h"
#include "Core/GameTypes.h"
#include "Game/Entity.h"
#include "Gameplay/Enemies/EnemyBase.h"

namespace hu {

class IGameWorld;

enum class BossPhase : std::uint8_t {
    Entering = 0,   // Flies in from off the right edge, invulnerable.
    Sentinel,       // Phase 1.
    Weaver,         // Phase 2.
    Bulwark,        // Phase 3.
    Dying           // Death throes before the death event fires.
};

const char* bossPhaseName(BossPhase phase);

class Boss final : public EnemyBase {
public:
    explicit Boss(const EnemySpawnParams& params);

    void update(float dt, IGameWorld& world) override;
    void appendDraw(DrawList& out) const override;

    bool isBoss() const override { return true; }

    BossPhase phase() const { return m_phase; }
    int phaseIndex() const;
    // 0..1 across the whole fight, for a boss health bar.
    float healthBar01() const { return healthFraction(); }

private:
    void enterPhase(BossPhase next, IGameWorld& world);
    void checkPhaseTransition(IGameWorld& world);

    void updateEntering(float dt, IGameWorld& world);
    void updateSentinel(float dt, IGameWorld& world);
    void updateWeaver(float dt, IGameWorld& world);
    void updateBulwark(float dt, IGameWorld& world);

    BossPhase m_phase = BossPhase::Entering;
    float m_phaseTime = 0.0f;
    float m_transitionTimer = 0.0f;   // > 0 while telegraphing a phase change.

    Vector2 m_home{ 0.0f, 0.0f };     // Anchor the movement patterns orbit.
    float m_pathPhase = 0.0f;

    // Weapon timers, reused across phases.
    float m_fireTimer = 0.0f;
    float m_burstTimer = 0.0f;
    int m_burstLeft = 0;
    float m_spiralAngle = 0.0f;
    // Bullet hell only: a second spiral wound the other way, at a rate that is
    // not a mirror of the first so the two precess against each other.
    float m_counterSpiralAngle = 0.0f;
    float m_wallGap = 0.5f;           // Normalised y of the gap in the wall.
    float m_wallGapDrift = 1.0f;
    int m_sweepDirection = 1;

    // Cached each update so the const appendDraw() can lay out screen-relative
    // telegraphs without a world reference.
    float m_screenHeight = 720.0f;
};

} // namespace hu
