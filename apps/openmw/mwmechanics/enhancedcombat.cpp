#include "enhancedcombat.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

#include <components/esm/attr.hpp>
#include <components/settings/values.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/inventorystore.hpp"

#include "actorutil.hpp"
#include "combat.hpp"
#include "creaturestats.hpp"
#include "movement.hpp"
#include "npcstats.hpp"
#include "stat.hpp"

namespace
{
    // Stagger accumulators per actor (stored by RefNum for persistence)
    std::unordered_map<unsigned int, MWMechanics::StaggerAccumulator> sStaggerAccumulators;

    unsigned int getActorKey(const MWWorld::Ptr& actor)
    {
        return actor.getCellRef().getRefNum().mIndex;
    }
}

namespace MWMechanics
{
    void StaggerAccumulator::addDamage(float damage)
    {
        accumulatedDamage += damage;
    }

    void StaggerAccumulator::update(float dt)
    {
        // Decay accumulated damage over time
        if (accumulatedDamage > 0.0f)
        {
            accumulatedDamage = std::max(0.0f, accumulatedDamage - staggerDecayRate * dt);
        }

        // Update stagger recovery timer
        if (staggerTimer > 0.0f)
        {
            staggerTimer = std::max(0.0f, staggerTimer - dt);
            if (staggerTimer <= 0.0f)
                currentState = StaggerState::None;
        }
    }

    void StaggerAccumulator::triggerStagger(StaggerState state)
    {
        currentState = state;
        accumulatedDamage = 0.0f; // Reset accumulator on stagger

        switch (state)
        {
            case StaggerState::Light:
                staggerTimer = 0.3f;
                break;
            case StaggerState::Medium:
                staggerTimer = 0.7f;
                break;
            case StaggerState::Heavy:
                staggerTimer = 1.2f;
                break;
            default:
                staggerTimer = 0.0f;
                break;
        }
    }

    float EnhancedCombatSystem::getEnhancedHitChance(
        const MWWorld::Ptr& attacker, const MWWorld::Ptr& victim, int skillValue)
    {
        if (Settings::game().mCombatAlwaysHits)
        {
            // Always hit - return 100% hit chance
            // Evasion/invisibility still provide damage reduction instead
            return 100.0f;
        }

        // Use standard hit chance calculation
        return getHitChance(attacker, victim, skillValue);
    }

    float EnhancedCombatSystem::getMomentumDamageMultiplier(const MWWorld::Ptr& attacker)
    {
        if (!Settings::game().mMomentumCombat)
            return 1.0f;

        // Get attacker's current velocity
        const MWWorld::Class& cls = attacker.getClass();
        if (!cls.isActor())
            return 1.0f;

        const auto& movement = cls.getMovementSettings(attacker);

        // Calculate movement magnitude (forward movement gives most bonus)
        float forwardSpeed = std::max(0.0f, movement.mPosition[1]); // Forward movement
        float lateralSpeed = std::abs(movement.mPosition[0]);       // Strafe
        float totalSpeed = std::sqrt(forwardSpeed * forwardSpeed + lateralSpeed * lateralSpeed * 0.5f);

        // Normalize to 0-1 range (1 = running)
        totalSpeed = std::min(1.0f, totalSpeed);

        // Calculate momentum bonus
        float multiplier = Settings::game().mMomentumDamageMultiplier;
        return 1.0f + totalSpeed * multiplier;
    }

    AttackDirection EnhancedCombatSystem::getAttackDirection(const MWWorld::Ptr& attacker)
    {
        if (!Settings::game().mDirectionalPowerAttacks)
            return AttackDirection::None;

        const MWWorld::Class& cls = attacker.getClass();
        if (!cls.isActor())
            return AttackDirection::None;

        const auto& movement = cls.getMovementSettings(attacker);

        float forward = movement.mPosition[1];
        float strafe = movement.mPosition[0];

        // Determine dominant direction
        if (std::abs(forward) > std::abs(strafe) && std::abs(forward) > 0.3f)
        {
            return forward > 0 ? AttackDirection::Forward : AttackDirection::Back;
        }
        else if (std::abs(strafe) > 0.3f)
        {
            return strafe > 0 ? AttackDirection::Right : AttackDirection::Left;
        }

        return AttackDirection::None;
    }

    float EnhancedCombatSystem::getDirectionalDamageModifier(AttackDirection direction, const MWWorld::Ptr& weapon)
    {
        switch (direction)
        {
            case AttackDirection::Forward:
                // Thrust - armor penetration (simulated as slight damage bonus)
                return 1.15f;
            case AttackDirection::Left:
            case AttackDirection::Right:
                // Swing attacks - stagger bonus (damage modifier)
                return 1.1f;
            case AttackDirection::Back:
                // Overhead/power attack - critical damage
                return 1.25f;
            default:
                return 1.0f;
        }
    }

    float EnhancedCombatSystem::getAttackStaminaCost(
        const MWWorld::Ptr& attacker, const MWWorld::Ptr& weapon, float attackStrength)
    {
        if (!Settings::game().mCombatStaminaSystem)
            return 0.0f;

        float baseCost = 5.0f;

        // Weapon weight affects stamina cost
        if (!weapon.isEmpty())
        {
            float weight = weapon.getClass().getWeight(weapon);
            baseCost += weight * 0.5f;
        }

        // Attack strength (charge time) affects cost
        baseCost *= (0.5f + attackStrength * 0.5f);

        // Endurance reduces cost
        const auto& stats = attacker.getClass().getCreatureStats(attacker);
        float endurance = stats.getAttribute(ESM::Attribute::Endurance).getModified();
        float enduranceModifier = 1.0f - (endurance / 200.0f); // Up to 50% reduction at 100 endurance
        enduranceModifier = std::max(0.5f, enduranceModifier);

        return baseCost * enduranceModifier;
    }

    bool EnhancedCombatSystem::hasStaminaForAttack(
        const MWWorld::Ptr& attacker, const MWWorld::Ptr& weapon, float attackStrength)
    {
        if (!Settings::game().mCombatStaminaSystem)
            return true;

        float cost = getAttackStaminaCost(attacker, weapon, attackStrength);
        const auto& stats = attacker.getClass().getCreatureStats(attacker);

        // Allow attack even at low stamina, but with penalties
        return stats.getFatigue().getCurrent() >= cost * 0.25f;
    }

    void EnhancedCombatSystem::applyAttackStaminaCost(
        const MWWorld::Ptr& attacker, const MWWorld::Ptr& weapon, float attackStrength)
    {
        if (!Settings::game().mCombatStaminaSystem)
            return;

        float cost = getAttackStaminaCost(attacker, weapon, attackStrength);
        auto& stats = attacker.getClass().getCreatureStats(attacker);

        MWMechanics::DynamicStat<float> fatigue = stats.getFatigue();
        fatigue.setCurrent(std::max(0.0f, fatigue.getCurrent() - cost));
        stats.setFatigue(fatigue);
    }

    float EnhancedCombatSystem::getStaminaDamageModifier(const MWWorld::Ptr& attacker)
    {
        if (!Settings::game().mCombatStaminaSystem)
            return 1.0f;

        const auto& stats = attacker.getClass().getCreatureStats(attacker);
        float fatigueRatio = stats.getFatigueTerm(); // 0 to 1 based on current/max fatigue

        // Low stamina reduces damage
        // At 50% fatigue: 90% damage
        // At 25% fatigue: 75% damage
        // At 0% fatigue: 50% damage
        return 0.5f + fatigueRatio * 0.5f;
    }

    bool EnhancedCombatSystem::applyStaggerDamage(const MWWorld::Ptr& victim, float damage)
    {
        if (!Settings::game().mStaggerSystem)
            return false;

        unsigned int key = getActorKey(victim);
        auto& accumulator = sStaggerAccumulators[key];

        accumulator.addDamage(damage);

        float threshold = Settings::game().mStaggerThreshold;

        // Check for stagger threshold
        if (accumulator.accumulatedDamage >= threshold * 2.0f)
        {
            accumulator.triggerStagger(StaggerState::Heavy);
            return true;
        }
        else if (accumulator.accumulatedDamage >= threshold * 1.5f)
        {
            accumulator.triggerStagger(StaggerState::Medium);
            return true;
        }
        else if (accumulator.accumulatedDamage >= threshold)
        {
            accumulator.triggerStagger(StaggerState::Light);
            return true;
        }

        return false;
    }

    StaggerState EnhancedCombatSystem::getStaggerState(const MWWorld::Ptr& actor)
    {
        unsigned int key = getActorKey(actor);
        auto it = sStaggerAccumulators.find(key);
        if (it != sStaggerAccumulators.end())
            return it->second.currentState;
        return StaggerState::None;
    }

    void EnhancedCombatSystem::updateStagger(const MWWorld::Ptr& actor, float dt)
    {
        if (!Settings::game().mStaggerSystem)
            return;

        unsigned int key = getActorKey(actor);
        auto it = sStaggerAccumulators.find(key);
        if (it != sStaggerAccumulators.end())
            it->second.update(dt);
    }

    bool EnhancedCombatSystem::isStaggered(const MWWorld::Ptr& actor)
    {
        StaggerState state = getStaggerState(actor);
        return state != StaggerState::None;
    }

    float EnhancedCombatSystem::applyEnhancedCombatDamage(const MWWorld::Ptr& attacker, const MWWorld::Ptr& victim,
        const MWWorld::Ptr& weapon, float baseDamage, float attackStrength)
    {
        float damage = baseDamage;

        // Apply momentum bonus
        damage *= getMomentumDamageMultiplier(attacker);

        // Apply directional attack bonus
        AttackDirection direction = getAttackDirection(attacker);
        damage *= getDirectionalDamageModifier(direction, weapon);

        // Apply stamina damage modifier
        damage *= getStaminaDamageModifier(attacker);

        // Apply stamina cost
        applyAttackStaminaCost(attacker, weapon, attackStrength);

        // If always-hit is enabled, evasion gives damage reduction instead of miss chance
        if (Settings::game().mCombatAlwaysHits && victim.getClass().isActor())
        {
            const auto& victimStats = victim.getClass().getCreatureStats(victim);
            float evasion = victimStats.getEvasion();
            // Convert evasion to damage reduction (max 40% reduction at 100 evasion)
            float evasionReduction = 1.0f - (evasion / 250.0f);
            evasionReduction = std::max(0.6f, evasionReduction);
            damage *= evasionReduction;
        }

        // Check for stagger
        applyStaggerDamage(victim, damage);

        // Staggered victims take bonus damage
        if (isStaggered(victim))
        {
            StaggerState state = getStaggerState(victim);
            switch (state)
            {
                case StaggerState::Light:
                    damage *= 1.1f;
                    break;
                case StaggerState::Medium:
                    damage *= 1.2f;
                    break;
                case StaggerState::Heavy:
                    damage *= 1.35f;
                    break;
                default:
                    break;
            }
        }

        return damage;
    }
}
