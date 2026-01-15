#include "infrastructuregenerator.hpp"

#include <algorithm>
#include <cmath>
#include <queue>
#include <set>

#include <apps/opencs/model/doc/document.hpp>
#include <apps/opencs/model/world/data.hpp>

#include <components/esm3/loadland.hpp>

#include "proceduralgenerator.hpp"

namespace CSMProcs
{
    InfrastructureGenerator::InfrastructureGenerator(CSMDoc::Document& document, RandomGenerator& rng)
        : mDocument(document)
        , mData(document.getData())
        , mRng(rng)
    {
    }

    void InfrastructureGenerator::setWorldBounds(int originX, int originY, int sizeX, int sizeY)
    {
        mOriginX = originX;
        mOriginY = originY;
        mSizeX = sizeX;
        mSizeY = sizeY;
    }

    void InfrastructureGenerator::setSettlements(const std::vector<SettlementLocation>& settlements)
    {
        mSettlements = settlements;
    }

    bool InfrastructureGenerator::generate()
    {
        mPlaced.clear();
        mRoads.clear();

        int totalSteps = 8;
        int currentStep = 0;

        if (mConfig.generateRoads)
        {
            reportProgress(currentStep++, totalSteps, "Generating road network...");
            if (!generateRoadNetwork())
                return false;
        }

        if (mConfig.generateWaypoints)
        {
            reportProgress(currentStep++, totalSteps, "Generating waypoints...");
            if (!generateWaypoints())
                return false;
        }

        if (mConfig.generateWatchtowers)
        {
            reportProgress(currentStep++, totalSteps, "Generating defense structures...");
            if (!generateDefenseStructures())
                return false;
        }

        if (mConfig.generateFarms)
        {
            reportProgress(currentStep++, totalSteps, "Generating rural structures...");
            if (!generateRuralStructures())
                return false;
        }

        if (mConfig.generateMines || mConfig.generateLumberCamps)
        {
            reportProgress(currentStep++, totalSteps, "Generating industry...");
            if (!generateIndustry())
                return false;
        }

        if (mConfig.generateRuins)
        {
            reportProgress(currentStep++, totalSteps, "Generating ruins...");
            if (!generateRuins())
                return false;
        }

        if (mConfig.generateCamps)
        {
            reportProgress(currentStep++, totalSteps, "Generating camps...");
            if (!generateCamps())
                return false;
        }

        if (mConfig.generateDocks || mConfig.generateLighthouses)
        {
            reportProgress(currentStep++, totalSteps, "Generating water structures...");
            if (!generateWaterStructures())
                return false;
        }

        reportProgress(totalSteps, totalSteps, "Infrastructure generation complete.");
        return true;
    }

    bool InfrastructureGenerator::generateRoadNetwork()
    {
        if (!mConfig.connectSettlements || mSettlements.size() < 2)
            return true;

        // Connect settlements using minimum spanning tree approach
        // This creates realistic road networks where settlements are connected efficiently

        // Calculate all possible connections
        struct Connection
        {
            size_t from, to;
            float distance;
            float cost; // Considers terrain difficulty
        };

        std::vector<Connection> connections;
        for (size_t i = 0; i < mSettlements.size(); ++i)
        {
            for (size_t j = i + 1; j < mSettlements.size(); ++j)
            {
                float dx = mSettlements[j].centerX - mSettlements[i].centerX;
                float dy = mSettlements[j].centerY - mSettlements[i].centerY;
                float dist = std::sqrt(dx * dx + dy * dy);

                // Calculate path cost considering terrain
                float cost = dist;

                // Sample terrain along path
                int samples = static_cast<int>(dist / 200.0f);
                samples = std::max(5, samples);

                for (int s = 1; s < samples; ++s)
                {
                    float t = static_cast<float>(s) / static_cast<float>(samples);
                    float x = mSettlements[i].centerX + dx * t;
                    float y = mSettlements[i].centerY + dy * t;

                    // Penalize steep terrain
                    if (mGetSlope)
                    {
                        float slope = mGetSlope(x, y);
                        cost += slope * 500.0f; // Steep terrain is expensive
                    }

                    // Penalize water crossings
                    if (isInWater(x, y))
                    {
                        cost += 1000.0f; // Water is very expensive
                    }
                }

                connections.push_back({i, j, dist, cost});
            }
        }

        // Sort by cost
        std::sort(connections.begin(), connections.end(),
            [](const Connection& a, const Connection& b) { return a.cost < b.cost; });

        // Build MST using Kruskal's algorithm
        std::vector<size_t> parent(mSettlements.size());
        for (size_t i = 0; i < parent.size(); ++i)
            parent[i] = i;

        std::function<size_t(size_t)> find = [&](size_t x) -> size_t {
            if (parent[x] != x)
                parent[x] = find(parent[x]);
            return parent[x];
        };

        std::vector<Connection> selectedRoads;
        for (const auto& conn : connections)
        {
            size_t rootFrom = find(conn.from);
            size_t rootTo = find(conn.to);

            if (rootFrom != rootTo)
            {
                selectedRoads.push_back(conn);
                parent[rootFrom] = rootTo;
            }
        }

        // Add some extra roads for redundancy (important settlements should have multiple connections)
        for (const auto& conn : connections)
        {
            bool alreadySelected =
                std::find_if(selectedRoads.begin(), selectedRoads.end(), [&](const Connection& c) {
                    return (c.from == conn.from && c.to == conn.to) || (c.from == conn.to && c.to == conn.from);
                }) != selectedRoads.end();

            if (!alreadySelected)
            {
                // Add extra roads with probability based on settlement importance
                bool importantFrom = (mSettlements[conn.from].type >= SettlementType::Town);
                bool importantTo = (mSettlements[conn.to].type >= SettlementType::Town);

                if ((importantFrom || importantTo) && mRng.nextBool(0.3f))
                {
                    selectedRoads.push_back(conn);
                }
            }
        }

        // Create actual roads from selected connections
        for (const auto& conn : selectedRoads)
        {
            WorldRoad road;
            road.startLocation = mSettlements[conn.from].name;
            road.endLocation = mSettlements[conn.to].name;

            // Determine road importance
            bool importantFrom = (mSettlements[conn.from].type >= SettlementType::Town);
            bool importantTo = (mSettlements[conn.to].type >= SettlementType::Town);
            road.isMainRoad = importantFrom && importantTo;
            road.width = road.isMainRoad ? 120.0f : 80.0f;

            // Find path between settlements
            road.waypoints =
                findPath(mSettlements[conn.from].centerX, mSettlements[conn.from].centerY,
                    mSettlements[conn.to].centerX, mSettlements[conn.to].centerY);

            // Smooth the path
            smoothPath(road.waypoints);

            mRoads.push_back(road);

            // Place road objects
            placeRoadSegments(road);
        }

        return true;
    }

    std::vector<std::pair<float, float>> InfrastructureGenerator::findPath(float startX, float startY, float endX,
        float endY)
    {
        std::vector<std::pair<float, float>> path;
        path.push_back({startX, startY});

        float dx = endX - startX;
        float dy = endY - startY;
        float totalDist = std::sqrt(dx * dx + dy * dy);

        if (totalDist < 100.0f)
        {
            path.push_back({endX, endY});
            return path;
        }

        // Use A*-like pathfinding with terrain consideration
        float stepSize = 200.0f;
        int maxSteps = static_cast<int>(totalDist / stepSize) * 3;

        float currentX = startX;
        float currentY = startY;

        for (int step = 0; step < maxSteps; ++step)
        {
            dx = endX - currentX;
            dy = endY - currentY;
            float remainingDist = std::sqrt(dx * dx + dy * dy);

            if (remainingDist < stepSize)
            {
                path.push_back({endX, endY});
                break;
            }

            // Normalize direction to target
            dx /= remainingDist;
            dy /= remainingDist;

            // Try different directions and pick the best
            float bestScore = -1e10f;
            float bestX = currentX + dx * stepSize;
            float bestY = currentY + dy * stepSize;

            for (int angle = -3; angle <= 3; ++angle)
            {
                float testAngle = std::atan2(dy, dx) + angle * 0.3f;
                float testX = currentX + std::cos(testAngle) * stepSize;
                float testY = currentY + std::sin(testAngle) * stepSize;

                // Score this direction
                float score = 0.0f;

                // Prefer directions toward goal
                float toGoalX = endX - testX;
                float toGoalY = endY - testY;
                float toGoalDist = std::sqrt(toGoalX * toGoalX + toGoalY * toGoalY);
                score -= toGoalDist * 0.1f;

                // Penalize steep terrain
                if (mGetSlope)
                {
                    float slope = mGetSlope(testX, testY);
                    if (slope > mConfig.maxRoadSlope)
                    {
                        score -= 10000.0f; // Very high penalty
                    }
                    else
                    {
                        score -= slope * 100.0f;
                    }
                }

                // Penalize water
                if (isInWater(testX, testY))
                {
                    score -= 5000.0f;
                }

                // Prefer staying near current direction (smooth path)
                if (!path.empty())
                {
                    float prevDx = currentX - path.back().first;
                    float prevDy = currentY - path.back().second;
                    float prevAngle = std::atan2(prevDy, prevDx);
                    float angleDiff = std::abs(testAngle - prevAngle);
                    while (angleDiff > 3.14159f)
                        angleDiff -= 6.28318f;
                    score -= std::abs(angleDiff) * 10.0f;
                }

                if (score > bestScore)
                {
                    bestScore = score;
                    bestX = testX;
                    bestY = testY;
                }
            }

            currentX = bestX;
            currentY = bestY;
            path.push_back({currentX, currentY});
        }

        return path;
    }

    void InfrastructureGenerator::smoothPath(std::vector<std::pair<float, float>>& path) const
    {
        if (path.size() < 3)
            return;

        // Apply Chaikin's corner cutting algorithm for smoother curves
        for (int iteration = 0; iteration < 2; ++iteration)
        {
            std::vector<std::pair<float, float>> smoothed;
            smoothed.push_back(path.front());

            for (size_t i = 0; i < path.size() - 1; ++i)
            {
                float x1 = path[i].first;
                float y1 = path[i].second;
                float x2 = path[i + 1].first;
                float y2 = path[i + 1].second;

                // Add two new points at 1/4 and 3/4 along the segment
                smoothed.push_back({x1 * 0.75f + x2 * 0.25f, y1 * 0.75f + y2 * 0.25f});
                smoothed.push_back({x1 * 0.25f + x2 * 0.75f, y1 * 0.25f + y2 * 0.75f});
            }

            smoothed.push_back(path.back());
            path = smoothed;
        }
    }

    void InfrastructureGenerator::placeRoadSegments(const WorldRoad& road)
    {
        if (road.waypoints.size() < 2)
            return;

        std::vector<std::string> roadIds;
        if (mAssetLibrary)
        {
            if (road.isMainRoad)
            {
                roadIds = mAssetLibrary->getAssetIds(AssetCategory::CobblestoneRoad);
            }
            if (roadIds.empty())
            {
                roadIds = mAssetLibrary->getAssetIds(AssetCategory::Road);
            }
        }

        if (roadIds.empty())
            return; // No road objects

        for (size_t i = 0; i < road.waypoints.size() - 1; ++i)
        {
            float x1 = road.waypoints[i].first;
            float y1 = road.waypoints[i].second;
            float x2 = road.waypoints[i + 1].first;
            float y2 = road.waypoints[i + 1].second;

            float dx = x2 - x1;
            float dy = y2 - y1;
            float length = std::sqrt(dx * dx + dy * dy);

            if (length < 10.0f)
                continue;

            float angle = std::atan2(dy, dx);
            float spacing = 40.0f;
            int segmentCount = static_cast<int>(length / spacing);

            for (int j = 0; j < segmentCount; ++j)
            {
                float t = static_cast<float>(j) / static_cast<float>(segmentCount);
                float x = x1 + dx * t;
                float y = y1 + dy * t;
                float z = mGetHeight ? mGetHeight(x, y) : 0.0f;

                std::string cellId = getCellId(x, y);
                std::string objectId = roadIds[mRng.nextInt(static_cast<uint32_t>(roadIds.size()))];
                mCreateReference(objectId, cellId, x, y, z, angle, 1.0f);
            }
        }
    }

    bool InfrastructureGenerator::generateWaypoints()
    {
        // Place signposts at road intersections and crossroads
        for (size_t i = 0; i < mRoads.size(); ++i)
        {
            for (size_t j = i + 1; j < mRoads.size(); ++j)
            {
                // Check if roads intersect
                // Simplified: check if any waypoints are close together
                for (const auto& wp1 : mRoads[i].waypoints)
                {
                    for (const auto& wp2 : mRoads[j].waypoints)
                    {
                        float dx = wp1.first - wp2.first;
                        float dy = wp1.second - wp2.second;
                        float dist = std::sqrt(dx * dx + dy * dy);

                        if (dist < 100.0f && mRng.nextBool(mConfig.signpostFrequency))
                        {
                            float x = (wp1.first + wp2.first) * 0.5f;
                            float y = (wp1.second + wp2.second) * 0.5f;
                            float z = mGetHeight ? mGetHeight(x, y) : 0.0f;

                            placeInfrastructure(InfrastructureType::Signpost, x, y, z, mRng.nextFloatRange(0.0f, 6.28f));
                        }
                    }
                }
            }
        }

        // Place shrines and rest stops along roads
        for (const auto& road : mRoads)
        {
            if (road.waypoints.size() < 2)
                continue;

            float totalLength = 0.0f;
            for (size_t i = 0; i < road.waypoints.size() - 1; ++i)
            {
                float dx = road.waypoints[i + 1].first - road.waypoints[i].first;
                float dy = road.waypoints[i + 1].second - road.waypoints[i].second;
                totalLength += std::sqrt(dx * dx + dy * dy);
            }

            // Place shrines
            if (totalLength > 1000.0f && mRng.nextBool(mConfig.shrineFrequency))
            {
                float t = mRng.nextFloatRange(0.3f, 0.7f);
                size_t idx = static_cast<size_t>(t * static_cast<float>(road.waypoints.size() - 1));
                idx = std::min(idx, road.waypoints.size() - 1);

                float x = road.waypoints[idx].first;
                float y = road.waypoints[idx].second;

                // Offset from road
                x += mRng.nextFloatRange(-50.0f, 50.0f);
                y += mRng.nextFloatRange(-50.0f, 50.0f);

                float z = mGetHeight ? mGetHeight(x, y) : 0.0f;
                placeInfrastructure(InfrastructureType::Shrine, x, y, z, mRng.nextFloatRange(0.0f, 6.28f));
            }

            // Place rest stops at intervals
            float restInterval = mConfig.restStopInterval;
            int restCount = static_cast<int>(totalLength / restInterval);

            for (int r = 1; r < restCount; ++r)
            {
                if (!mRng.nextBool(mConfig.restStopFrequency))
                    continue;

                float targetDist = static_cast<float>(r) * restInterval;
                float currentDist = 0.0f;

                for (size_t i = 0; i < road.waypoints.size() - 1; ++i)
                {
                    float dx = road.waypoints[i + 1].first - road.waypoints[i].first;
                    float dy = road.waypoints[i + 1].second - road.waypoints[i].second;
                    float segLen = std::sqrt(dx * dx + dy * dy);

                    if (currentDist + segLen >= targetDist)
                    {
                        float t = (targetDist - currentDist) / segLen;
                        float x = road.waypoints[i].first + dx * t;
                        float y = road.waypoints[i].second + dy * t;

                        // Offset from road
                        x += mRng.nextFloatRange(30.0f, 80.0f) * (mRng.nextBool(0.5f) ? 1.0f : -1.0f);
                        y += mRng.nextFloatRange(30.0f, 80.0f) * (mRng.nextBool(0.5f) ? 1.0f : -1.0f);

                        float z = mGetHeight ? mGetHeight(x, y) : 0.0f;
                        placeInfrastructure(InfrastructureType::RestStop, x, y, z, mRng.nextFloatRange(0.0f, 6.28f));
                        break;
                    }

                    currentDist += segLen;
                }
            }
        }

        return true;
    }

    bool InfrastructureGenerator::generateDefenseStructures()
    {
        // Place watchtowers on hills overlooking roads and settlements
        int watchtowerCount = static_cast<int>(
            static_cast<float>(mSizeX * mSizeY) * mConfig.watchtowerDensity * 0.1f);

        for (int i = 0; i < watchtowerCount; ++i)
        {
            float x, y, z;
            if (findValidLocation(InfrastructureType::Watchtower, x, y, z))
            {
                placeInfrastructure(InfrastructureType::Watchtower, x, y, z, mRng.nextFloatRange(0.0f, 6.28f));
            }
        }

        // Place guard posts along roads
        if (mConfig.generateGuardPosts)
        {
            for (const auto& road : mRoads)
            {
                if (!road.isMainRoad)
                    continue;

                // Place guard posts at regular intervals on main roads
                float totalLength = 0.0f;
                for (size_t i = 0; i < road.waypoints.size() - 1; ++i)
                {
                    float dx = road.waypoints[i + 1].first - road.waypoints[i].first;
                    float dy = road.waypoints[i + 1].second - road.waypoints[i].second;
                    totalLength += std::sqrt(dx * dx + dy * dy);
                }

                float guardInterval = 3000.0f;
                int guardCount = static_cast<int>(totalLength / guardInterval);

                for (int g = 1; g < guardCount; ++g)
                {
                    float targetDist = static_cast<float>(g) * guardInterval;
                    float currentDist = 0.0f;

                    for (size_t i = 0; i < road.waypoints.size() - 1; ++i)
                    {
                        float dx = road.waypoints[i + 1].first - road.waypoints[i].first;
                        float dy = road.waypoints[i + 1].second - road.waypoints[i].second;
                        float segLen = std::sqrt(dx * dx + dy * dy);

                        if (currentDist + segLen >= targetDist)
                        {
                            float t = (targetDist - currentDist) / segLen;
                            float x = road.waypoints[i].first + dx * t;
                            float y = road.waypoints[i].second + dy * t;
                            float z = mGetHeight ? mGetHeight(x, y) : 0.0f;

                            placeInfrastructure(InfrastructureType::GuardPost, x, y, z,
                                std::atan2(dy, dx) + 1.5708f);
                            break;
                        }
                        currentDist += segLen;
                    }
                }
            }
        }

        return true;
    }

    bool InfrastructureGenerator::generateRuralStructures()
    {
        // Place farms near settlements on flat fertile land
        int farmCount = static_cast<int>(
            static_cast<float>(mSizeX * mSizeY) * mConfig.farmDensity * 0.2f);

        for (int i = 0; i < farmCount; ++i)
        {
            float x, y, z;
            if (findValidLocation(InfrastructureType::Farm, x, y, z))
            {
                placeFarm(x, y, z);
            }
        }

        // Place mills near settlements
        if (mConfig.generateMills)
        {
            for (const auto& settlement : mSettlements)
            {
                if (settlement.type < SettlementType::Village)
                    continue;

                if (mRng.nextBool(0.7f))
                {
                    // Place windmill or watermill
                    float angle = mRng.nextFloatRange(0.0f, 6.28f);
                    float dist = settlement.radius * mRng.nextFloatRange(1.2f, 2.0f);

                    float x = settlement.centerX + std::cos(angle) * dist;
                    float y = settlement.centerY + std::sin(angle) * dist;

                    // Check if near water for watermill
                    InfrastructureType millType =
                        isNearWater(x, y, 200.0f) ? InfrastructureType::WaterMill : InfrastructureType::Windmill;

                    if (!isInWater(x, y))
                    {
                        float z = mGetHeight ? mGetHeight(x, y) : 0.0f;
                        placeInfrastructure(millType, x, y, z, mRng.nextFloatRange(0.0f, 6.28f));
                    }
                }
            }
        }

        return true;
    }

    bool InfrastructureGenerator::generateIndustry()
    {
        int industryCount = static_cast<int>(
            static_cast<float>(mSizeX * mSizeY) * mConfig.industryDensity * 0.1f);

        // Place mines in mountainous areas
        if (mConfig.generateMines)
        {
            int mineCount = industryCount / 2;
            for (int i = 0; i < mineCount; ++i)
            {
                float x, y, z;
                if (findValidLocation(InfrastructureType::Mine, x, y, z))
                {
                    placeMine(x, y, z);
                }
            }
        }

        // Place lumber camps in forested areas (we approximate forest by distance from settlements)
        if (mConfig.generateLumberCamps)
        {
            int campCount = industryCount / 2;
            for (int i = 0; i < campCount; ++i)
            {
                float x, y, z;
                if (findValidLocation(InfrastructureType::LumberCamp, x, y, z))
                {
                    placeInfrastructure(InfrastructureType::LumberCamp, x, y, z, mRng.nextFloatRange(0.0f, 6.28f));
                }
            }
        }

        return true;
    }

    bool InfrastructureGenerator::generateRuins()
    {
        int ruinCount = static_cast<int>(
            static_cast<float>(mSizeX * mSizeY) * mConfig.ruinDensity * 0.15f);

        for (int i = 0; i < ruinCount; ++i)
        {
            float x, y, z;
            if (findValidLocation(InfrastructureType::Ruins, x, y, z))
            {
                placeRuins(x, y, z);
            }
        }

        // Place ancient structures on prominent terrain features
        int ancientCount = ruinCount / 3;
        for (int i = 0; i < ancientCount; ++i)
        {
            float x, y, z;

            // For standing stones and burial mounds, prefer slightly elevated areas
            InfrastructureType type =
                mRng.nextBool(0.5f) ? InfrastructureType::StandingStone : InfrastructureType::BurialMound;

            if (findValidLocation(type, x, y, z))
            {
                placeInfrastructure(type, x, y, z, mRng.nextFloatRange(0.0f, 6.28f));
            }
        }

        return true;
    }

    bool InfrastructureGenerator::generateCamps()
    {
        // Bandit camps: away from settlements, near roads
        if (mConfig.banditCampDensity > 0.0f)
        {
            int banditCount = static_cast<int>(
                static_cast<float>(mSizeX * mSizeY) * mConfig.banditCampDensity * 0.1f);

            for (int i = 0; i < banditCount; ++i)
            {
                float x, y, z;
                if (findValidLocation(InfrastructureType::BanditCamp, x, y, z))
                {
                    placeCamp(InfrastructureType::BanditCamp, x, y, z);
                }
            }
        }

        // Traveler camps: along roads
        if (mConfig.travelerCampDensity > 0.0f)
        {
            for (const auto& road : mRoads)
            {
                if (road.waypoints.size() < 3)
                    continue;

                if (mRng.nextBool(mConfig.travelerCampDensity))
                {
                    size_t idx = mRng.nextInt(static_cast<uint32_t>(road.waypoints.size() - 2)) + 1;
                    float x = road.waypoints[idx].first + mRng.nextFloatRange(-100.0f, 100.0f);
                    float y = road.waypoints[idx].second + mRng.nextFloatRange(-100.0f, 100.0f);

                    if (!isInWater(x, y))
                    {
                        float z = mGetHeight ? mGetHeight(x, y) : 0.0f;
                        placeCamp(InfrastructureType::TravelerCamp, x, y, z);
                    }
                }
            }
        }

        return true;
    }

    bool InfrastructureGenerator::generateWaterStructures()
    {
        // Place docks at settlements near water
        if (mConfig.generateDocks)
        {
            for (const auto& settlement : mSettlements)
            {
                if (settlement.type < SettlementType::Village)
                    continue;

                // Check if settlement is near water
                bool nearWater = false;
                float waterX = 0.0f, waterY = 0.0f;

                for (int angle = 0; angle < 8; ++angle)
                {
                    float a = static_cast<float>(angle) * 0.785f; // 45 degree increments
                    float x = settlement.centerX + std::cos(a) * settlement.radius * 1.5f;
                    float y = settlement.centerY + std::sin(a) * settlement.radius * 1.5f;

                    if (isInWater(x, y))
                    {
                        nearWater = true;
                        // Find water edge
                        for (float dist = settlement.radius * 1.5f; dist > 0; dist -= 20.0f)
                        {
                            x = settlement.centerX + std::cos(a) * dist;
                            y = settlement.centerY + std::sin(a) * dist;
                            if (!isInWater(x, y))
                            {
                                waterX = x;
                                waterY = y;
                                break;
                            }
                        }
                        break;
                    }
                }

                if (nearWater)
                {
                    float z = mGetHeight ? mGetHeight(waterX, waterY) : 0.0f;
                    InfrastructureType dockType =
                        (settlement.type >= SettlementType::Town) ? InfrastructureType::Dock : InfrastructureType::Pier;
                    placeInfrastructure(dockType, waterX, waterY, z,
                        std::atan2(waterY - settlement.centerY, waterX - settlement.centerX));
                }
            }
        }

        // Place lighthouses on coastal high points
        if (mConfig.generateLighthouses)
        {
            // Find coastal high points
            float cellSize = ESM::Land::REAL_SIZE;
            for (int cy = 0; cy < mSizeY; ++cy)
            {
                for (int cx = 0; cx < mSizeX; ++cx)
                {
                    float worldX = static_cast<float>(mOriginX + cx) * cellSize + cellSize * 0.5f;
                    float worldY = static_cast<float>(mOriginY + cy) * cellSize + cellSize * 0.5f;

                    // Check if this is a coastal high point
                    if (!isInWater(worldX, worldY) && isNearWater(worldX, worldY, 500.0f) && isOnHill(worldX, worldY))
                    {
                        if (mRng.nextBool(0.2f))
                        {
                            float z = mGetHeight ? mGetHeight(worldX, worldY) : 0.0f;
                            placeInfrastructure(InfrastructureType::Lighthouse, worldX, worldY, z,
                                mRng.nextFloatRange(0.0f, 6.28f));
                        }
                    }
                }
            }
        }

        // Place fishing huts along coastlines
        if (mConfig.generateFishingHuts)
        {
            float cellSize = ESM::Land::REAL_SIZE;
            int hutCount = static_cast<int>(static_cast<float>(mSizeX + mSizeY) * 0.5f);

            for (int i = 0; i < hutCount; ++i)
            {
                float x, y, z;
                if (findValidLocation(InfrastructureType::FishingHut, x, y, z))
                {
                    placeInfrastructure(InfrastructureType::FishingHut, x, y, z, mRng.nextFloatRange(0.0f, 6.28f));
                }
            }
        }

        return true;
    }

    PlacementContext InfrastructureGenerator::getContextForType(InfrastructureType type) const
    {
        PlacementContext ctx;

        switch (type)
        {
            case InfrastructureType::Watchtower:
                ctx.onHill = true;
                ctx.maxSlope = 0.3f;
                ctx.avoidWater = true;
                ctx.minSpacingFromSame = 2000.0f;
                break;

            case InfrastructureType::Farm:
                ctx.maxSlope = 0.15f;
                ctx.avoidWater = true;
                ctx.nearSettlement = true;
                ctx.settlementDistance = 1500.0f;
                ctx.minSpacingFromSame = 500.0f;
                break;

            case InfrastructureType::Mine:
                ctx.onHill = true;
                ctx.maxSlope = 0.6f; // Mines can be on steep terrain
                ctx.avoidWater = true;
                ctx.awayFromSettlement = true;
                ctx.minSettlementDistance = 1000.0f;
                ctx.minSpacingFromSame = 1500.0f;
                break;

            case InfrastructureType::LumberCamp:
                ctx.maxSlope = 0.3f;
                ctx.avoidWater = true;
                ctx.awayFromSettlement = true;
                ctx.minSettlementDistance = 800.0f;
                ctx.minSpacingFromSame = 1000.0f;
                break;

            case InfrastructureType::Ruins:
            case InfrastructureType::AncientTower:
                ctx.maxSlope = 0.4f;
                ctx.avoidWater = true;
                ctx.awayFromSettlement = true;
                ctx.minSettlementDistance = 1500.0f;
                ctx.minSpacingFromSame = 2000.0f;
                break;

            case InfrastructureType::BanditCamp:
                ctx.maxSlope = 0.25f;
                ctx.avoidWater = true;
                ctx.nearRoad = true;
                ctx.roadDistance = 500.0f;
                ctx.awayFromSettlement = true;
                ctx.minSettlementDistance = 1000.0f;
                ctx.minSpacingFromSame = 2000.0f;
                break;

            case InfrastructureType::FishingHut:
                ctx.nearWater = true;
                ctx.avoidWater = true; // On shore, not in water
                ctx.maxSlope = 0.2f;
                ctx.minSpacingFromSame = 800.0f;
                break;

            case InfrastructureType::StandingStone:
            case InfrastructureType::BurialMound:
                ctx.maxSlope = 0.2f;
                ctx.avoidWater = true;
                ctx.awayFromSettlement = true;
                ctx.minSettlementDistance = 500.0f;
                ctx.minSpacingFromSame = 1500.0f;
                break;

            default:
                ctx.maxSlope = 0.3f;
                ctx.avoidWater = true;
                ctx.minSpacingFromSame = 500.0f;
                break;
        }

        return ctx;
    }

    bool InfrastructureGenerator::findValidLocation(InfrastructureType type, float& outX, float& outY,
        float& outZ)
    {
        PlacementContext ctx = getContextForType(type);
        float cellSize = ESM::Land::REAL_SIZE;

        int maxAttempts = 100;
        for (int attempt = 0; attempt < maxAttempts; ++attempt)
        {
            // Random position in world
            float x = (static_cast<float>(mOriginX) + mRng.nextFloatRange(0.0f, static_cast<float>(mSizeX))) * cellSize;
            float y = (static_cast<float>(mOriginY) + mRng.nextFloatRange(0.0f, static_cast<float>(mSizeY))) * cellSize;

            if (isValidLocation(x, y, ctx))
            {
                outX = x;
                outY = y;
                outZ = mGetHeight ? mGetHeight(x, y) : 0.0f;
                return true;
            }
        }

        return false;
    }

    bool InfrastructureGenerator::isValidLocation(float x, float y, const PlacementContext& ctx) const
    {
        // Check height bounds
        float height = mGetHeight ? mGetHeight(x, y) : 0.0f;
        if (height < ctx.minHeight || height > ctx.maxHeight)
            return false;

        // Check slope
        if (mGetSlope)
        {
            float slope = mGetSlope(x, y);
            if (slope > ctx.maxSlope)
                return false;
        }

        // Check water
        bool inWater = isInWater(x, y);
        if (ctx.avoidWater && inWater)
            return false;
        if (ctx.onWater && !inWater)
            return false;
        if (ctx.nearWater && !isNearWater(x, y, 200.0f))
            return false;

        // Check hill requirement
        if (ctx.onHill && !isOnHill(x, y))
            return false;

        // Check settlement distance
        float settlementDist = distanceToNearestSettlement(x, y);
        if (ctx.nearSettlement && settlementDist > ctx.settlementDistance)
            return false;
        if (ctx.awayFromSettlement && settlementDist < ctx.minSettlementDistance)
            return false;

        // Check road distance
        if (ctx.nearRoad)
        {
            float roadDist = distanceToNearestRoad(x, y);
            if (roadDist > ctx.roadDistance)
                return false;
        }

        // Check spacing from same type
        for (const auto& placed : mPlaced)
        {
            float dx = x - placed.worldX;
            float dy = y - placed.worldY;
            float dist = std::sqrt(dx * dx + dy * dy);

            if (dist < ctx.minSpacingFromAny)
                return false;
        }

        return true;
    }

    float InfrastructureGenerator::distanceToNearestSettlement(float x, float y) const
    {
        float minDist = 1e10f;
        for (const auto& settlement : mSettlements)
        {
            float dx = x - settlement.centerX;
            float dy = y - settlement.centerY;
            float dist = std::sqrt(dx * dx + dy * dy) - settlement.radius;
            minDist = std::min(minDist, dist);
        }
        return minDist;
    }

    float InfrastructureGenerator::distanceToNearestRoad(float x, float y) const
    {
        float minDist = 1e10f;

        for (const auto& road : mRoads)
        {
            for (size_t i = 0; i < road.waypoints.size() - 1; ++i)
            {
                // Distance to line segment
                float x1 = road.waypoints[i].first;
                float y1 = road.waypoints[i].second;
                float x2 = road.waypoints[i + 1].first;
                float y2 = road.waypoints[i + 1].second;

                float dx = x2 - x1;
                float dy = y2 - y1;
                float lengthSq = dx * dx + dy * dy;
                
                // Skip degenerate segments
                if (lengthSq < 0.001f)
                    continue;
                
                float t = std::max(0.0f, std::min(1.0f, ((x - x1) * dx + (y - y1) * dy) / lengthSq));

                float nearX = x1 + t * dx;
                float nearY = y1 + t * dy;

                float dist = std::sqrt((x - nearX) * (x - nearX) + (y - nearY) * (y - nearY));
                minDist = std::min(minDist, dist);
            }
        }

        return minDist;
    }

    bool InfrastructureGenerator::isNearWater(float x, float y, float radius) const
    {
        // Check several points around the position
        for (int angle = 0; angle < 8; ++angle)
        {
            float a = static_cast<float>(angle) * 0.785f;
            float checkX = x + std::cos(a) * radius;
            float checkY = y + std::sin(a) * radius;

            if (isInWater(checkX, checkY))
                return true;
        }
        return false;
    }

    bool InfrastructureGenerator::isInWater(float x, float y) const
    {
        if (!mGetHeight || !mGetWaterLevel)
            return false;

        float height = mGetHeight(x, y);
        float waterLevel = mGetWaterLevel();

        return height < waterLevel;
    }

    bool InfrastructureGenerator::isOnHill(float x, float y) const
    {
        if (!mGetHeight)
            return false;

        float centerHeight = mGetHeight(x, y);
        float avgSurrounding = 0.0f;
        int count = 0;

        // Check surrounding area
        float checkRadius = 300.0f;
        for (int angle = 0; angle < 8; ++angle)
        {
            float a = static_cast<float>(angle) * 0.785f;
            float checkX = x + std::cos(a) * checkRadius;
            float checkY = y + std::sin(a) * checkRadius;

            avgSurrounding += mGetHeight(checkX, checkY);
            ++count;
        }

        avgSurrounding /= static_cast<float>(count);

        // Position is on a hill if it's significantly higher than surroundings
        return centerHeight > avgSurrounding + 100.0f;
    }

    PlacedInfrastructure InfrastructureGenerator::placeInfrastructure(InfrastructureType type, float x, float y,
        float z, float rotation)
    {
        std::string objectId = selectObjectForType(type);
        std::string cellId = getCellId(x, y);

        std::string refId = mCreateReference(objectId, cellId, x, y, z, rotation, 1.0f);

        PlacedInfrastructure placed;
        placed.type = type;
        placed.refId = refId;
        placed.objectId = objectId;
        placed.cellX = static_cast<int>(x / ESM::Land::REAL_SIZE);
        placed.cellY = static_cast<int>(y / ESM::Land::REAL_SIZE);
        placed.worldX = x;
        placed.worldY = y;
        placed.worldZ = z;
        placed.rotation = rotation;
        placed.scale = 1.0f;

        mPlaced.push_back(placed);
        return placed;
    }

    std::string InfrastructureGenerator::selectObjectForType(InfrastructureType type) const
    {
        if (!mAssetLibrary)
            return "furn_marker_arrow"; // Default fallback

        std::vector<std::string> candidates;

        switch (type)
        {
            case InfrastructureType::Watchtower:
            case InfrastructureType::GuardPost:
                candidates = mAssetLibrary->getAssetIds(AssetCategory::WallTower);
                if (candidates.empty())
                    candidates = mAssetLibrary->getAssetIds(AssetCategory::Building);
                break;

            case InfrastructureType::Signpost:
                candidates = mAssetLibrary->getAssetIds(AssetCategory::CityProp);
                break;

            case InfrastructureType::Shrine:
            case InfrastructureType::Wayshrine:
                candidates = mAssetLibrary->getAssetIds(AssetCategory::CityProp);
                break;

            case InfrastructureType::Farm:
            case InfrastructureType::Barn:
                candidates = mAssetLibrary->getAssetIds(AssetCategory::Farm);
                if (candidates.empty())
                    candidates = mAssetLibrary->getAssetIds(AssetCategory::Building);
                break;

            case InfrastructureType::Mine:
                candidates = mAssetLibrary->getAssetIds(AssetCategory::CaveEntrance);
                break;

            case InfrastructureType::Ruins:
            case InfrastructureType::AncientTower:
                candidates = mAssetLibrary->getAssetIds(AssetCategory::Building);
                break;

            case InfrastructureType::Dock:
            case InfrastructureType::Pier:
                candidates = mAssetLibrary->getAssetIds(AssetCategory::Dock);
                break;

            case InfrastructureType::Lighthouse:
                candidates = mAssetLibrary->getAssetIds(AssetCategory::Building);
                break;

            default:
                candidates = mAssetLibrary->getAssetIds(AssetCategory::Building);
                break;
        }

        if (candidates.empty())
            return "furn_marker_arrow";

        return candidates[mRng.nextInt(static_cast<uint32_t>(candidates.size()))];
    }

    void InfrastructureGenerator::placeFarm(float x, float y, float z)
    {
        // Place main farmhouse
        auto farm = placeInfrastructure(InfrastructureType::Farm, x, y, z, mRng.nextFloatRange(0.0f, 6.28f));

        // Place barn nearby
        float barnAngle = mRng.nextFloatRange(0.0f, 6.28f);
        float barnDist = mRng.nextFloatRange(100.0f, 200.0f);
        float barnX = x + std::cos(barnAngle) * barnDist;
        float barnY = y + std::sin(barnAngle) * barnDist;
        float barnZ = mGetHeight ? mGetHeight(barnX, barnY) : z;

        if (!isInWater(barnX, barnY))
        {
            placeInfrastructure(InfrastructureType::Barn, barnX, barnY, barnZ, barnAngle + 3.14159f);
        }

        // Add well
        if (mRng.nextBool(0.7f))
        {
            float wellAngle = mRng.nextFloatRange(0.0f, 6.28f);
            float wellDist = mRng.nextFloatRange(50.0f, 100.0f);
            float wellX = x + std::cos(wellAngle) * wellDist;
            float wellY = y + std::sin(wellAngle) * wellDist;
            float wellZ = mGetHeight ? mGetHeight(wellX, wellY) : z;

            if (!isInWater(wellX, wellY))
            {
                placeInfrastructure(InfrastructureType::Well, wellX, wellY, wellZ, 0.0f);
            }
        }
    }

    void InfrastructureGenerator::placeMine(float x, float y, float z)
    {
        placeInfrastructure(InfrastructureType::Mine, x, y, z, mRng.nextFloatRange(0.0f, 6.28f));

        // Add some support structures
        for (int i = 0; i < mRng.nextIntRange(1, 3); ++i)
        {
            float angle = mRng.nextFloatRange(0.0f, 6.28f);
            float dist = mRng.nextFloatRange(50.0f, 150.0f);
            float structX = x + std::cos(angle) * dist;
            float structY = y + std::sin(angle) * dist;
            float structZ = mGetHeight ? mGetHeight(structX, structY) : z;

            if (!isInWater(structX, structY))
            {
                // Place a storage building or similar
                std::string cellId = getCellId(structX, structY);
                std::vector<std::string> containerIds;
                if (mAssetLibrary)
                {
                    containerIds = mAssetLibrary->getAssetIds(AssetCategory::Container);
                }
                if (!containerIds.empty())
                {
                    mCreateReference(containerIds[mRng.nextInt(static_cast<uint32_t>(containerIds.size()))],
                        cellId, structX, structY, structZ, mRng.nextFloatRange(0.0f, 6.28f), 1.0f);
                }
            }
        }
    }

    void InfrastructureGenerator::placeRuins(float x, float y, float z)
    {
        // Place main ruin structure
        placeInfrastructure(InfrastructureType::Ruins, x, y, z, mRng.nextFloatRange(0.0f, 6.28f));

        // Add scattered debris/rocks
        int debrisCount = mRng.nextIntRange(3, 8);
        for (int i = 0; i < debrisCount; ++i)
        {
            float angle = mRng.nextFloatRange(0.0f, 6.28f);
            float dist = mRng.nextFloatRange(30.0f, 150.0f);
            float debrisX = x + std::cos(angle) * dist;
            float debrisY = y + std::sin(angle) * dist;
            float debrisZ = mGetHeight ? mGetHeight(debrisX, debrisY) : z;

            if (!isInWater(debrisX, debrisY))
            {
                std::string cellId = getCellId(debrisX, debrisY);
                std::vector<std::string> rockIds;
                if (mAssetLibrary)
                {
                    rockIds = mAssetLibrary->getAssetIds(AssetCategory::Rock);
                }
                if (!rockIds.empty())
                {
                    mCreateReference(rockIds[mRng.nextInt(static_cast<uint32_t>(rockIds.size()))],
                        cellId, debrisX, debrisY, debrisZ, mRng.nextFloatRange(0.0f, 6.28f),
                        mRng.nextFloatRange(0.5f, 1.5f));
                }
            }
        }
    }

    void InfrastructureGenerator::placeCamp(InfrastructureType type, float x, float y, float z)
    {
        std::string cellId = getCellId(x, y);

        // Place tents or bedrolls based on camp type
        int tentCount = (type == InfrastructureType::BanditCamp) ? mRng.nextIntRange(2, 5) : mRng.nextIntRange(1, 3);

        for (int i = 0; i < tentCount; ++i)
        {
            float angle = mRng.nextFloatRange(0.0f, 6.28f);
            float dist = mRng.nextFloatRange(20.0f, 80.0f);
            float tentX = x + std::cos(angle) * dist;
            float tentY = y + std::sin(angle) * dist;
            float tentZ = mGetHeight ? mGetHeight(tentX, tentY) : z;

            if (!isInWater(tentX, tentY))
            {
                // Use furniture for tent/bedroll
                std::vector<std::string> furnitureIds;
                if (mAssetLibrary)
                {
                    furnitureIds = mAssetLibrary->getAssetIds(AssetCategory::Furniture);
                }
                if (!furnitureIds.empty())
                {
                    mCreateReference(furnitureIds[mRng.nextInt(static_cast<uint32_t>(furnitureIds.size()))],
                        cellId, tentX, tentY, tentZ, angle + 3.14159f, 1.0f);
                }
            }
        }

        // Add campfire (light)
        std::vector<std::string> lightIds;
        if (mAssetLibrary)
        {
            lightIds = mAssetLibrary->getAssetIds(AssetCategory::Light);
        }
        if (!lightIds.empty())
        {
            mCreateReference(lightIds[mRng.nextInt(static_cast<uint32_t>(lightIds.size()))],
                cellId, x, y, z, 0.0f, 1.0f);
        }

        // Add some clutter
        int clutterCount = mRng.nextIntRange(2, 6);
        for (int i = 0; i < clutterCount; ++i)
        {
            float angle = mRng.nextFloatRange(0.0f, 6.28f);
            float dist = mRng.nextFloatRange(10.0f, 50.0f);
            float clutterX = x + std::cos(angle) * dist;
            float clutterY = y + std::sin(angle) * dist;
            float clutterZ = mGetHeight ? mGetHeight(clutterX, clutterY) : z;

            if (!isInWater(clutterX, clutterY))
            {
                std::vector<std::string> clutterIds;
                if (mAssetLibrary)
                {
                    clutterIds = mAssetLibrary->getAssetIds(AssetCategory::Clutter);
                    if (clutterIds.empty())
                        clutterIds = mAssetLibrary->getAssetIds(AssetCategory::Container);
                }
                if (!clutterIds.empty())
                {
                    mCreateReference(clutterIds[mRng.nextInt(static_cast<uint32_t>(clutterIds.size()))],
                        cellId, clutterX, clutterY, clutterZ, mRng.nextFloatRange(0.0f, 6.28f), 1.0f);
                }
            }
        }

        // Record as placed
        PlacedInfrastructure placed;
        placed.type = type;
        placed.worldX = x;
        placed.worldY = y;
        placed.worldZ = z;
        mPlaced.push_back(placed);
    }

    std::string InfrastructureGenerator::getCellId(float x, float y) const
    {
        int cellX = static_cast<int>(std::floor(x / ESM::Land::REAL_SIZE));
        int cellY = static_cast<int>(std::floor(y / ESM::Land::REAL_SIZE));
        return "#" + std::to_string(cellX) + ", " + std::to_string(cellY);
    }

    void InfrastructureGenerator::reportProgress(int current, int total, const std::string& message)
    {
        if (mProgressCallback)
        {
            mProgressCallback(current, total, message);
        }
    }

    std::string InfrastructureGenerator::getTypeName(InfrastructureType type)
    {
        switch (type)
        {
            case InfrastructureType::MainRoad:
                return "Main Road";
            case InfrastructureType::Watchtower:
                return "Watchtower";
            case InfrastructureType::Farm:
                return "Farm";
            case InfrastructureType::Mine:
                return "Mine";
            case InfrastructureType::Ruins:
                return "Ruins";
            case InfrastructureType::BanditCamp:
                return "Bandit Camp";
            case InfrastructureType::Dock:
                return "Dock";
            default:
                return "Structure";
        }
    }
}
