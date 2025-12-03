#include "alifesimulation.hpp"

#include <algorithm>
#include <cmath>
#include <random>

#include <components/misc/rng.hpp>
#include <components/settings/values.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"
#include "../mwbase/windowmanager.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"

namespace
{
    std::unique_ptr<MWMechanics::ALifeSimulation> sALifeSimulation;
}

namespace MWMechanics
{
    // ======================== NPCNeeds ========================

    void NPCNeeds::update(float gameHours)
    {
        // Hunger increases over time (full hunger in ~12 game hours)
        hunger = std::min(100.0f, hunger + gameHours * 8.33f);

        // Fatigue increases with activity (full fatigue in ~16 hours)
        fatigue = std::min(100.0f, fatigue + gameHours * 6.25f);

        // Social need fluctuates
        if (social > 50.0f)
            social = std::max(30.0f, social - gameHours * 2.0f);

        // Safety slowly recovers when not in danger
        if (safety < 100.0f)
            safety = std::min(100.0f, safety + gameHours * 5.0f);
    }

    float NPCNeeds::getUrgentNeed() const
    {
        return std::max({ hunger, fatigue, 100.0f - social, 100.0f - safety });
    }

    std::string NPCNeeds::getMostUrgentNeed() const
    {
        if (hunger >= fatigue && hunger >= (100.0f - social) && hunger >= (100.0f - safety))
            return "hunger";
        if (fatigue >= hunger && fatigue >= (100.0f - social) && fatigue >= (100.0f - safety))
            return "fatigue";
        if ((100.0f - social) >= hunger && (100.0f - social) >= fatigue)
            return "social";
        return "safety";
    }

    // ======================== LocationEconomy ========================

    void LocationEconomy::update(float gameHours)
    {
        // Slowly normalize prices toward 1.0
        for (auto& [item, modifier] : itemPriceModifiers)
        {
            if (modifier > 1.0f)
                modifier = std::max(1.0f, modifier - gameHours * 0.01f);
            else if (modifier < 1.0f)
                modifier = std::min(1.0f, modifier + gameHours * 0.01f);
        }

        // Supply naturally recovers, demand fluctuates
        for (auto& [item, supply] : itemSupply)
        {
            supply = std::min(100, supply + static_cast<int>(gameHours));
        }
    }

    float LocationEconomy::getPriceModifier(const ESM::RefId& item) const
    {
        auto it = itemPriceModifiers.find(item);
        if (it != itemPriceModifiers.end())
            return it->second;
        return 1.0f;
    }

    // ======================== FactionTerritory ========================

    void FactionTerritory::update(float gameHours)
    {
        // Stability slowly increases when influence is high
        if (influence > 0.5f)
            stability = std::min(1.0f, stability + gameHours * 0.01f);
        else
            stability = std::max(0.0f, stability - gameHours * 0.02f);
    }

    // ======================== WorldEvent ========================

    bool WorldEvent::isActive(float currentGameTime) const
    {
        return currentGameTime >= startTime && currentGameTime <= (startTime + duration);
    }

    // ======================== SimulatedNPCState ========================

    void SimulatedNPCState::updateSimulated(float gameHours, float dt)
    {
        needs.update(dt / 3600.0f); // Convert to game hours
        lastUpdateTime = gameHours;
    }

    ScheduleEntry* SimulatedNPCState::getCurrentScheduleEntry(float gameHour)
    {
        for (auto& entry : schedule)
        {
            if (gameHour >= entry.startHour && gameHour < entry.endHour)
                return &entry;
        }
        return nullptr;
    }

    // ======================== ALifeSimulation ========================

    ALifeSimulation::ALifeSimulation()
    {
        mEnabled = Settings::game().mALifeSimulation;
        mSchedulesEnabled = Settings::game().mNPCSchedules;
        mNeedsEnabled = Settings::game().mNPCNeeds;
        mEconomyEnabled = Settings::game().mDynamicEconomy;
        mEventsEnabled = Settings::game().mWorldEvents;
        mFactionTerritoriesEnabled = Settings::game().mFactionTerritories;
        mUpdateInterval = Settings::game().mALifeUpdateInterval;
    }

    ALifeSimulation::~ALifeSimulation() = default;

    void ALifeSimulation::initialize()
    {
        if (!mEnabled)
            return;

        // Initialize default schedules for known NPC types
        // This would normally load from game data
    }

    void ALifeSimulation::update(float gameHours, float dt)
    {
        if (!mEnabled)
            return;

        // Check if enough time has passed for update
        float gameMinutesPassed = (gameHours - mLastUpdateTime) * 60.0f;
        if (gameMinutesPassed < mUpdateInterval)
            return;

        mLastUpdateTime = gameHours;

        // Update simulated NPCs
        int maxNPCs = Settings::game().mALifeMaxSimulatedNPCs;
        int updatedCount = 0;

        for (auto& [npcId, state] : mSimulatedNPCs)
        {
            if (updatedCount >= maxNPCs)
                break;

            if (mSchedulesEnabled)
                updateNPCSchedule(state, gameHours);

            if (mNeedsEnabled)
                updateNPCNeeds(state, gameHours, dt);

            state.updateSimulated(gameHours, dt);
            ++updatedCount;
        }

        // Update world systems
        if (mEconomyEnabled)
            updateEconomy(gameHours, dt);

        if (mFactionTerritoriesEnabled)
            updateFactionTerritories(gameHours, dt);

        if (mEventsEnabled)
        {
            checkForWorldEvents(gameHours);
            processActiveEvents(gameHours, dt);
        }
    }

    void ALifeSimulation::registerNPC(const MWWorld::Ptr& npc)
    {
        if (!mEnabled || npc.isEmpty())
            return;

        ESM::RefId npcId = npc.getCellRef().getRefId();
        if (mSimulatedNPCs.find(npcId) != mSimulatedNPCs.end())
            return;

        SimulatedNPCState state;
        state.npcId = npcId;
        state.position = npc.getRefData().getPosition().asVec3();

        if (npc.getCell())
            state.currentCell = npc.getCell()->getCell()->getId();

        // Create default schedule based on NPC class
        ScheduleEntry sleep;
        sleep.startHour = 22.0f;
        sleep.endHour = 6.0f;
        sleep.activity = "sleep";
        sleep.mandatory = true;
        state.schedule.push_back(sleep);

        ScheduleEntry work;
        work.startHour = 8.0f;
        work.endHour = 18.0f;
        work.activity = "work";
        work.mandatory = false;
        state.schedule.push_back(work);

        ScheduleEntry eat;
        eat.startHour = 12.0f;
        eat.endHour = 13.0f;
        eat.activity = "eat";
        eat.mandatory = false;
        state.schedule.push_back(eat);

        mSimulatedNPCs[npcId] = state;
    }

    void ALifeSimulation::unregisterNPC(const ESM::RefId& npcId)
    {
        mSimulatedNPCs.erase(npcId);
    }

    const SimulatedNPCState* ALifeSimulation::getNPCState(const ESM::RefId& npcId) const
    {
        auto it = mSimulatedNPCs.find(npcId);
        if (it != mSimulatedNPCs.end())
            return &it->second;
        return nullptr;
    }

    void ALifeSimulation::applySimulatedState(const MWWorld::Ptr& npc)
    {
        if (!mEnabled || npc.isEmpty())
            return;

        ESM::RefId npcId = npc.getCellRef().getRefId();
        const SimulatedNPCState* state = getNPCState(npcId);
        if (!state)
            return;

        // Apply simulated position if NPC was simulated in background
        // In a full implementation, this would teleport NPC to correct location
        // and set appropriate AI state

        // For now, just update their needs-based behavior
        if (mNeedsEnabled && state->needs.getUrgentNeed() > 80.0f)
        {
            // NPC has urgent needs - could trigger AI package change
            std::string urgentNeed = state->needs.getMostUrgentNeed();
            // Log or apply behavior based on need
        }
    }

    std::vector<WorldEvent> ALifeSimulation::getEventsAtLocation(const ESM::RefId& cell) const
    {
        std::vector<WorldEvent> result;
        for (const auto& event : mActiveEvents)
        {
            if (event.location == cell)
                result.push_back(event);
        }
        return result;
    }

    float ALifeSimulation::getItemPriceModifier(const ESM::RefId& item, const ESM::RefId& cell) const
    {
        if (!mEconomyEnabled)
            return 1.0f;

        auto it = mLocationEconomies.find(cell);
        if (it != mLocationEconomies.end())
            return it->second.getPriceModifier(item);
        return 1.0f;
    }

    float ALifeSimulation::getFactionInfluence(const ESM::RefId& faction, const ESM::RefId& cell) const
    {
        if (!mFactionTerritoriesEnabled)
            return 0.0f;

        auto it = mFactionTerritories.find(faction);
        if (it != mFactionTerritories.end())
        {
            if (it->second.controlledCells.count(cell) > 0)
                return it->second.influence;
        }
        return 0.0f;
    }

    void ALifeSimulation::checkForWorldEvents(float gameHours)
    {
        if (!mEventsEnabled)
            return;

        // Random chance for world event each update cycle
        auto& prng = MWBase::Environment::get().getWorld()->getPrng();
        float eventChance = 0.01f; // 1% chance per update

        if (Misc::Rng::rollProbability(prng) < eventChance)
        {
            generateWorldEvent(gameHours);
        }
    }

    void ALifeSimulation::updateNPCSchedule(SimulatedNPCState& state, float gameHour)
    {
        ScheduleEntry* currentEntry = state.getCurrentScheduleEntry(gameHour);
        if (currentEntry)
        {
            state.currentActivity = currentEntry->activity;

            // In a full implementation, would update NPC position toward schedule location
        }
    }

    void ALifeSimulation::updateNPCNeeds(SimulatedNPCState& state, float gameHours, float dt)
    {
        state.needs.update(dt / 3600.0f);

        // NPCs act on urgent needs
        if (state.needs.hunger > 80.0f && state.currentActivity != "eat")
        {
            // Would trigger eating behavior
        }
        if (state.needs.fatigue > 80.0f && state.currentActivity != "sleep")
        {
            // Would trigger rest behavior
        }
    }

    void ALifeSimulation::updateEconomy(float gameHours, float dt)
    {
        for (auto& [location, economy] : mLocationEconomies)
        {
            economy.update(dt / 3600.0f);
        }
    }

    void ALifeSimulation::updateFactionTerritories(float gameHours, float dt)
    {
        for (auto& [faction, territory] : mFactionTerritories)
        {
            territory.update(dt / 3600.0f);
        }
    }

    void ALifeSimulation::generateWorldEvent(float gameHours)
    {
        auto& prng = MWBase::Environment::get().getWorld()->getPrng();

        WorldEvent event;
        int eventType = Misc::Rng::rollDice(8, prng);

        switch (eventType)
        {
            case 0:
                event.type = WorldEventType::BanditRaid;
                event.description = "Bandits have been spotted in the area!";
                event.duration = 24.0f;
                event.intensity = 0.5f + Misc::Rng::rollProbability(prng) * 0.5f;
                break;
            case 1:
                event.type = WorldEventType::MerchantCaravan;
                event.description = "A merchant caravan has arrived with exotic goods.";
                event.duration = 48.0f;
                event.intensity = 0.3f + Misc::Rng::rollProbability(prng) * 0.4f;
                break;
            case 2:
                event.type = WorldEventType::Festival;
                event.description = "The locals are celebrating a festival!";
                event.duration = 24.0f;
                event.intensity = 0.7f;
                break;
            case 3:
                event.type = WorldEventType::WildlifeAttack;
                event.description = "Wild creatures have been attacking travelers.";
                event.duration = 12.0f;
                event.intensity = 0.4f + Misc::Rng::rollProbability(prng) * 0.4f;
                break;
            case 4:
                event.type = WorldEventType::FactionConflict;
                event.description = "Tensions are rising between local factions.";
                event.duration = 72.0f;
                event.intensity = 0.6f;
                break;
            case 5:
                event.type = WorldEventType::DiseaseOutbreak;
                event.description = "A mysterious illness is spreading.";
                event.duration = 96.0f;
                event.intensity = 0.5f;
                break;
            case 6:
                event.type = WorldEventType::FamineStart;
                event.description = "Food supplies are running low.";
                event.duration = 168.0f;
                event.intensity = 0.4f;
                break;
            default:
                event.type = WorldEventType::WealthDiscovery;
                event.description = "Rumors of treasure have spread.";
                event.duration = 48.0f;
                event.intensity = 0.3f;
                break;
        }

        event.startTime = gameHours;
        event.playerAware = false;

        // Would select a random appropriate location
        // For now, leave location empty

        mActiveEvents.push_back(event);
    }

    void ALifeSimulation::processActiveEvents(float gameHours, float dt)
    {
        // Remove expired events
        mActiveEvents.erase(
            std::remove_if(mActiveEvents.begin(), mActiveEvents.end(),
                [gameHours](const WorldEvent& e) { return !e.isActive(gameHours); }),
            mActiveEvents.end());

        // Process ongoing effects of events
        for (auto& event : mActiveEvents)
        {
            switch (event.type)
            {
                case WorldEventType::BanditRaid:
                    // Could spawn bandits, affect NPC safety
                    for (auto& [npcId, state] : mSimulatedNPCs)
                    {
                        if (state.currentCell == event.location)
                            state.needs.safety -= event.intensity * 10.0f * (dt / 3600.0f);
                    }
                    break;

                case WorldEventType::MerchantCaravan:
                    // Affects local economy - increases supply, lowers prices
                    if (mLocationEconomies.count(event.location) > 0)
                    {
                        auto& economy = mLocationEconomies[event.location];
                        for (auto& [item, price] : economy.itemPriceModifiers)
                        {
                            price = std::max(0.8f, price - 0.01f * (dt / 3600.0f));
                        }
                    }
                    break;

                case WorldEventType::Festival:
                    // Increases NPC social satisfaction
                    for (auto& [npcId, state] : mSimulatedNPCs)
                    {
                        if (state.currentCell == event.location)
                            state.needs.social = std::min(100.0f, state.needs.social + 5.0f * (dt / 3600.0f));
                    }
                    break;

                default:
                    break;
            }
        }
    }

    void ALifeSimulation::notifyPlayerOfEvent(const WorldEvent& event)
    {
        if (event.playerAware)
            return;

        // Would show message to player via WindowManager
        // MWBase::Environment::get().getWindowManager()->messageBox(event.description);
    }

    ALifeSimulation& getALifeSimulation()
    {
        if (!sALifeSimulation)
            sALifeSimulation = std::make_unique<ALifeSimulation>();
        return *sALifeSimulation;
    }
}
