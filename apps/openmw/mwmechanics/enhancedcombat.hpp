#ifndef OPENMW_MWMECHANICS_ENHANCEDCOMBAT_H
#define OPENMW_MWMECHANICS_ENHANCEDCOMBAT_H

#include "../mwworld/ptr.hpp"

#include <osg/Vec3f>

namespace MWMechanics
{
    /// Attack direction for directional power attacks
    enum class AttackDirection
    {
        None,
        Forward,  // Thrust attack - bonus to armor penetration
        Left,     // Left swing - bonus to stagger
        Right,    // Right swing - bonus to stagger
        Back      // Overhead/back - critical damage bonus
    };

    /// Stagger state for combat
    enum class StaggerState
    {
        None,
        Light,  // Brief flinch, can still act
        Medium, // Stumble, brief action interrupt
        Heavy   // Knocked back, longer recovery
    };

    /// Enhanced combat system with momentum-based damage and stagger mechanics
    class EnhancedCombatSystem
    {
    public:
        /// Calculate hit chance with enhanced combat option
        /// If "combat always hits" is enabled, returns 100 (guaranteed hit)
        /// Otherwise uses standard hit calculation
        static float getEnhancedHitChance(const MWWorld::Ptr& attacker, const MWWorld::Ptr& victim, int skillValue);

        /// Calculate momentum damage modifier based on attacker's movement
        /// Returns multiplier (1.0 = no bonus, up to 2.0 with full momentum)
        static float getMomentumDamageMultiplier(const MWWorld::Ptr& attacker);

        /// Get directional attack bonus based on movement direction during attack
        static AttackDirection getAttackDirection(const MWWorld::Ptr& attacker);

        /// Apply directional attack damage modifier
        static float getDirectionalDamageModifier(AttackDirection direction, const MWWorld::Ptr& weapon);

        /// Calculate stamina cost for attack
        static float getAttackStaminaCost(const MWWorld::Ptr& attacker, const MWWorld::Ptr& weapon,
            float attackStrength);

        /// Check if attacker has enough stamina for attack
        static bool hasStaminaForAttack(const MWWorld::Ptr& attacker, const MWWorld::Ptr& weapon,
            float attackStrength);

        /// Apply stamina cost for attack
        static void applyAttackStaminaCost(const MWWorld::Ptr& attacker, const MWWorld::Ptr& weapon,
            float attackStrength);

        /// Get damage reduction from low stamina
        static float getStaminaDamageModifier(const MWWorld::Ptr& attacker);

        /// Apply stagger to target if damage threshold exceeded
        /// Returns true if stagger was applied
        static bool applyStaggerDamage(const MWWorld::Ptr& victim, float damage);

        /// Get current stagger state of actor
        static StaggerState getStaggerState(const MWWorld::Ptr& actor);

        /// Update stagger recovery (called each frame)
        static void updateStagger(const MWWorld::Ptr& actor, float dt);

        /// Check if actor is currently staggered (cannot act freely)
        static bool isStaggered(const MWWorld::Ptr& actor);

        /// Apply full enhanced combat modifications to damage
        /// Combines momentum, directional attacks, stamina, and stagger
        static float applyEnhancedCombatDamage(const MWWorld::Ptr& attacker, const MWWorld::Ptr& victim,
            const MWWorld::Ptr& weapon, float baseDamage, float attackStrength);
    };

    /// Accumulated stagger damage tracking per actor
    struct StaggerAccumulator
    {
        float accumulatedDamage = 0.0f;
        StaggerState currentState = StaggerState::None;
        float staggerTimer = 0.0f;
        float staggerDecayRate = 10.0f; // Damage decay per second

        void addDamage(float damage);
        void update(float dt);
        void triggerStagger(StaggerState state);
        bool isRecovering() const { return staggerTimer > 0.0f; }
    };
}

#endif
