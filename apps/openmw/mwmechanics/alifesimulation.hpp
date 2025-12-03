#ifndef OPENMW_MWMECHANICS_ALIFESIMULATION_H
#define OPENMW_MWMECHANICS_ALIFESIMULATION_H

#include "../mwworld/ptr.hpp"

#include <components/esm/refid.hpp>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace MWMechanics
{
    /// NPC schedule entry
    struct ScheduleEntry
    {
        float startHour;        // Game hour this activity starts
        float endHour;          // Game hour this activity ends
        ESM::RefId location;    // Cell or location to be at
        std::string activity;   // What to do (work, sleep, eat, patrol, etc.)
        osg::Vec3f position;    // Exact position if needed
        bool mandatory;         // If true, NPC must follow schedule
    };

    /// NPC needs/desires for A-Life behavior
    struct NPCNeeds
    {
        float hunger = 0.0f;       // 0-100, increases over time
        float fatigue = 0.0f;      // 0-100, increases with activity
        float social = 50.0f;      // 0-100, social satisfaction
        float safety = 100.0f;     // 0-100, perceived safety
        float wealth = 50.0f;      // 0-100, satisfaction with possessions

        void update(float gameHours);
        float getUrgentNeed() const;      // Returns most urgent need value
        std::string getMostUrgentNeed() const;
    };

    /// NPC relationship with another NPC
    struct NPCRelationship
    {
        ESM::RefId targetNPC;
        float disposition = 50.0f;   // 0-100
        float trust = 50.0f;         // 0-100
        int interactionCount = 0;
        float lastInteractionTime = 0.0f;
    };

    /// World economic data for a location
    struct LocationEconomy
    {
        std::map<ESM::RefId, float> itemPriceModifiers;  // Item -> price modifier
        std::map<ESM::RefId, int> itemSupply;            // Item -> supply level
        std::map<ESM::RefId, int> itemDemand;            // Item -> demand level
        float wealthLevel = 1.0f;                         // Overall economic health

        void update(float gameHours);
        float getPriceModifier(const ESM::RefId& item) const;
    };

    /// Faction territory data
    struct FactionTerritory
    {
        ESM::RefId faction;
        std::set<ESM::RefId> controlledCells;
        float influence = 1.0f;     // 0-1, strength in area
        float stability = 1.0f;     // 0-1, how secure the control is
        
        void update(float gameHours);
    };

    /// World event types
    enum class WorldEventType
    {
        None,
        BanditRaid,           // Bandits attack location
        MerchantCaravan,      // Trade opportunity
        DiseaseOutbreak,      // Health crisis
        FactionConflict,      // Two factions clash
        WildlifeAttack,       // Creatures threaten area
        Festival,             // Celebration event
        FamineStart,          // Resource shortage
        WealthDiscovery       // Treasure/mine found
    };

    /// Active world event
    struct WorldEvent
    {
        WorldEventType type;
        ESM::RefId location;
        float startTime;
        float duration;
        float intensity;      // How severe/significant
        bool playerAware;     // Has player been notified
        std::string description;

        bool isActive(float currentGameTime) const;
    };

    /// Simulated NPC state for background simulation
    struct SimulatedNPCState
    {
        ESM::RefId npcId;
        ESM::RefId currentCell;
        osg::Vec3f position;
        NPCNeeds needs;
        std::string currentActivity;
        float lastUpdateTime = 0.0f;

        // Relationships
        std::map<ESM::RefId, NPCRelationship> relationships;
        
        // Schedule
        std::vector<ScheduleEntry> schedule;
        int currentScheduleIndex = 0;

        void updateSimulated(float gameHours, float dt);
        ScheduleEntry* getCurrentScheduleEntry(float gameHour);
    };

    /// A-Life World Simulation System
    /// Simulates NPC behavior, economy, and world events in the background
    class ALifeSimulation
    {
    public:
        ALifeSimulation();
        ~ALifeSimulation();

        /// Initialize the A-Life system
        void initialize();

        /// Update simulation (called periodically)
        void update(float gameHours, float dt);

        /// Register an NPC for simulation
        void registerNPC(const MWWorld::Ptr& npc);

        /// Unregister an NPC
        void unregisterNPC(const ESM::RefId& npcId);

        /// Get simulated state for an NPC
        const SimulatedNPCState* getNPCState(const ESM::RefId& npcId) const;

        /// Apply simulated state when NPC becomes active
        void applySimulatedState(const MWWorld::Ptr& npc);

        /// Get current world events for a location
        std::vector<WorldEvent> getEventsAtLocation(const ESM::RefId& cell) const;

        /// Get economic modifier for an item at a location
        float getItemPriceModifier(const ESM::RefId& item, const ESM::RefId& cell) const;

        /// Get faction influence at a location
        float getFactionInfluence(const ESM::RefId& faction, const ESM::RefId& cell) const;

        /// Check if a world event should trigger
        void checkForWorldEvents(float gameHours);

        /// Enable/disable A-Life systems
        void setEnabled(bool enabled) { mEnabled = enabled; }
        void setSchedulesEnabled(bool enabled) { mSchedulesEnabled = enabled; }
        void setNeedsEnabled(bool enabled) { mNeedsEnabled = enabled; }
        void setEconomyEnabled(bool enabled) { mEconomyEnabled = enabled; }
        void setEventsEnabled(bool enabled) { mEventsEnabled = enabled; }

        /// Get all active events
        const std::vector<WorldEvent>& getActiveEvents() const { return mActiveEvents; }

        /// Notify player of relevant event
        void notifyPlayerOfEvent(const WorldEvent& event);

    private:
        void updateNPCSchedule(SimulatedNPCState& state, float gameHour);
        void updateNPCNeeds(SimulatedNPCState& state, float gameHours, float dt);
        void updateEconomy(float gameHours, float dt);
        void updateFactionTerritories(float gameHours, float dt);
        void generateWorldEvent(float gameHours);
        void processActiveEvents(float gameHours, float dt);

        std::map<ESM::RefId, SimulatedNPCState> mSimulatedNPCs;
        std::map<ESM::RefId, LocationEconomy> mLocationEconomies;
        std::map<ESM::RefId, FactionTerritory> mFactionTerritories;
        std::vector<WorldEvent> mActiveEvents;

        float mLastUpdateTime = 0.0f;
        float mUpdateInterval = 5.0f; // Game minutes between updates

        bool mEnabled = true;
        bool mSchedulesEnabled = true;
        bool mNeedsEnabled = true;
        bool mEconomyEnabled = true;
        bool mEventsEnabled = true;
        bool mFactionTerritoriesEnabled = true;
    };

    /// Singleton accessor
    ALifeSimulation& getALifeSimulation();
}

#endif
