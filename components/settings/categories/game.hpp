#ifndef OPENMW_COMPONENTS_SETTINGS_CATEGORIES_GAME_H
#define OPENMW_COMPONENTS_SETTINGS_CATEGORIES_GAME_H

#include <components/detournavigator/collisionshapetype.hpp>
#include <components/settings/sanitizerimpl.hpp>
#include <components/settings/settingvalue.hpp>

#include <osg/Math>
#include <osg/Vec2f>
#include <osg/Vec3f>

#include <cstdint>
#include <string>
#include <string_view>

namespace Settings
{
    struct GameCategory : WithIndex
    {
        using WithIndex::WithIndex;

        SettingValue<int> mShowOwned{ mIndex, "Game", "show owned", makeEnumSanitizerInt({ 0, 1, 2, 3 }) };
        SettingValue<bool> mShowProjectileDamage{ mIndex, "Game", "show projectile damage" };
        SettingValue<bool> mShowMeleeInfo{ mIndex, "Game", "show melee info" };
        SettingValue<bool> mShowEnchantChance{ mIndex, "Game", "show enchant chance" };
        SettingValue<bool> mBestAttack{ mIndex, "Game", "best attack" };
        SettingValue<int> mDifficulty{ mIndex, "Game", "difficulty", makeClampSanitizerInt(-500, 500) };
        // We have to cap it since using high values (larger than 7168) will make some quests harder or impossible to
        // complete (bug #1876)
        SettingValue<int> mActorsProcessingRange{ mIndex, "Game", "actors processing range",
            makeClampSanitizerInt(3584, 7168) };
        SettingValue<bool> mClassicReflectedAbsorbSpellsBehavior{ mIndex, "Game",
            "classic reflected absorb spells behavior" };
        SettingValue<bool> mClassicCalmSpellsBehavior{ mIndex, "Game", "classic calm spells behavior" };
        SettingValue<bool> mShowEffectDuration{ mIndex, "Game", "show effect duration" };
        SettingValue<bool> mPreventMerchantEquipping{ mIndex, "Game", "prevent merchant equipping" };
        SettingValue<bool> mEnchantedWeaponsAreMagical{ mIndex, "Game", "enchanted weapons are magical" };
        SettingValue<bool> mFollowersAttackOnSight{ mIndex, "Game", "followers attack on sight" };
        SettingValue<bool> mCanLootDuringDeathAnimation{ mIndex, "Game", "can loot during death animation" };
        SettingValue<bool> mRebalanceSoulGemValues{ mIndex, "Game", "rebalance soul gem values" };
        SettingValue<bool> mUseAdditionalAnimSources{ mIndex, "Game", "use additional anim sources" };
        SettingValue<bool> mSmoothAnimTransitions{ mIndex, "Game", "smooth animation transitions" };
        SettingValue<bool> mBarterDispositionChangeIsPermanent{ mIndex, "Game",
            "barter disposition change is permanent" };
        SettingValue<int> mStrengthInfluencesHandToHand{ mIndex, "Game", "strength influences hand to hand",
            makeEnumSanitizerInt({ 0, 1, 2 }) };
        SettingValue<bool> mWeaponSheathing{ mIndex, "Game", "weapon sheathing" };
        SettingValue<bool> mShieldSheathing{ mIndex, "Game", "shield sheathing" };
        SettingValue<bool> mOnlyAppropriateAmmunitionBypassesResistance{ mIndex, "Game",
            "only appropriate ammunition bypasses resistance" };
        SettingValue<bool> mUseMagicItemAnimations{ mIndex, "Game", "use magic item animations" };
        SettingValue<bool> mNormaliseRaceSpeed{ mIndex, "Game", "normalise race speed" };
        SettingValue<float> mProjectilesEnchantMultiplier{ mIndex, "Game", "projectiles enchant multiplier",
            makeClampSanitizerFloat(0, 1) };
        SettingValue<bool> mUncappedDamageFatigue{ mIndex, "Game", "uncapped damage fatigue" };
        SettingValue<bool> mTurnToMovementDirection{ mIndex, "Game", "turn to movement direction" };
        SettingValue<bool> mSmoothMovement{ mIndex, "Game", "smooth movement" };
        SettingValue<float> mSmoothMovementPlayerTurningDelay{ mIndex, "Game", "smooth movement player turning delay",
            makeMaxSanitizerFloat(0.01f) };
        SettingValue<bool> mNPCsAvoidCollisions{ mIndex, "Game", "NPCs avoid collisions" };
        SettingValue<bool> mNPCsGiveWay{ mIndex, "Game", "NPCs give way" };
        SettingValue<bool> mSwimUpwardCorrection{ mIndex, "Game", "swim upward correction" };
        SettingValue<float> mSwimUpwardCoef{ mIndex, "Game", "swim upward coef", makeClampSanitizerFloat(-1, 1) };
        SettingValue<bool> mTrainersTrainingSkillsBasedOnBaseSkill{ mIndex, "Game",
            "trainers training skills based on base skill" };
        SettingValue<bool> mAlwaysAllowStealingFromKnockedOutActors{ mIndex, "Game",
            "always allow stealing from knocked out actors" };
        SettingValue<bool> mGraphicHerbalism{ mIndex, "Game", "graphic herbalism" };
        SettingValue<bool> mAllowActorsToFollowOverWaterSurface{ mIndex, "Game",
            "allow actors to follow over water surface" };
        SettingValue<osg::Vec3f> mDefaultActorPathfindHalfExtents{ mIndex, "Game",
            "default actor pathfind half extents", makeMaxStrictSanitizerVec3f(osg::Vec3f(0, 0, 0)) };
        SettingValue<bool> mDayNightSwitches{ mIndex, "Game", "day night switches" };
        SettingValue<DetourNavigator::CollisionShapeType> mActorCollisionShapeType{ mIndex, "Game",
            "actor collision shape type" };
        SettingValue<bool> mPlayerMovementIgnoresAnimation{ mIndex, "Game", "player movement ignores animation" };
        
        // ==================== Enhanced Combat System ====================
        // Combat always hits - removes dice-roll misses for more action-oriented combat
        SettingValue<bool> mCombatAlwaysHits{ mIndex, "Game", "combat always hits" };
        // Momentum combat - damage scales with movement speed and attack timing
        SettingValue<bool> mMomentumCombat{ mIndex, "Game", "momentum combat" };
        // Momentum damage multiplier (how much movement affects damage)
        SettingValue<float> mMomentumDamageMultiplier{ mIndex, "Game", "momentum damage multiplier",
            makeClampSanitizerFloat(0.0f, 2.0f) };
        // Directional power attacks - different attack directions deal different damage types
        SettingValue<bool> mDirectionalPowerAttacks{ mIndex, "Game", "directional power attacks" };
        // Combat stamina system - attacks cost stamina, exhaustion reduces damage
        SettingValue<bool> mCombatStaminaSystem{ mIndex, "Game", "combat stamina system" };
        // Stagger system - consecutive hits can stagger opponents
        SettingValue<bool> mStaggerSystem{ mIndex, "Game", "stagger system" };
        // Stagger threshold (damage needed to trigger stagger)
        SettingValue<float> mStaggerThreshold{ mIndex, "Game", "stagger threshold",
            makeClampSanitizerFloat(10.0f, 100.0f) };

        // ==================== Interactive Grass System ====================
        // Enable grass interaction with player and NPCs
        SettingValue<bool> mGrassInteraction{ mIndex, "Game", "grass interaction" };
        // Grass interaction radius
        SettingValue<float> mGrassInteractionRadius{ mIndex, "Game", "grass interaction radius",
            makeClampSanitizerFloat(50.0f, 300.0f) };
        // Grass bend intensity
        SettingValue<float> mGrassBendIntensity{ mIndex, "Game", "grass bend intensity",
            makeClampSanitizerFloat(0.0f, 2.0f) };
        // Grass recovery speed (how fast grass returns to normal)
        SettingValue<float> mGrassRecoverySpeed{ mIndex, "Game", "grass recovery speed",
            makeClampSanitizerFloat(0.5f, 5.0f) };

        // ==================== Full Body IK & Physics Animation ====================
        // Enable full body IK system
        SettingValue<bool> mFullBodyIK{ mIndex, "Game", "full body ik" };
        // Spring-based body animation (Euphoria-like physics responses)
        SettingValue<bool> mSpringBodyAnimation{ mIndex, "Game", "spring body animation" };
        // Spring stiffness for procedural animation
        SettingValue<float> mSpringStiffness{ mIndex, "Game", "spring stiffness",
            makeClampSanitizerFloat(0.1f, 10.0f) };
        // Spring damping (reduces oscillation)
        SettingValue<float> mSpringDamping{ mIndex, "Game", "spring damping",
            makeClampSanitizerFloat(0.1f, 2.0f) };
        // Active ragdoll system - blends animation with physics
        SettingValue<bool> mActiveRagdoll{ mIndex, "Game", "active ragdoll" };
        // Ragdoll blend factor (0 = full animation, 1 = full physics)
        SettingValue<float> mRagdollBlendFactor{ mIndex, "Game", "ragdoll blend factor",
            makeClampSanitizerFloat(0.0f, 1.0f) };
        // Impact impulse threshold for triggering physics response
        SettingValue<float> mImpactImpulseThreshold{ mIndex, "Game", "impact impulse threshold",
            makeClampSanitizerFloat(50.0f, 500.0f) };

        // ==================== A-Life World Simulation ====================
        // Enable A-Life world simulation system
        SettingValue<bool> mALifeSimulation{ mIndex, "Game", "alife simulation" };
        // NPC daily schedule system
        SettingValue<bool> mNPCSchedules{ mIndex, "Game", "npc schedules" };
        // NPC needs simulation (hunger, fatigue, social)
        SettingValue<bool> mNPCNeeds{ mIndex, "Game", "npc needs" };
        // Dynamic economy - prices and availability change based on supply/demand
        SettingValue<bool> mDynamicEconomy{ mIndex, "Game", "dynamic economy" };
        // Faction territory control
        SettingValue<bool> mFactionTerritories{ mIndex, "Game", "faction territories" };
        // World events - random events occur in the world
        SettingValue<bool> mWorldEvents{ mIndex, "Game", "world events" };
        // A-Life simulation update interval (in game minutes)
        SettingValue<float> mALifeUpdateInterval{ mIndex, "Game", "alife update interval",
            makeClampSanitizerFloat(1.0f, 60.0f) };
        // Maximum NPCs to simulate in background
        SettingValue<int> mALifeMaxSimulatedNPCs{ mIndex, "Game", "alife max simulated npcs",
            makeClampSanitizerInt(50, 500) };
    };
}

#endif
