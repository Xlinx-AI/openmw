#include "settlementgenerator.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <queue>

#include <apps/opencs/model/doc/document.hpp>
#include <apps/opencs/model/world/data.hpp>

#include <components/esm3/loadland.hpp>

#include "proceduralgenerator.hpp"

namespace CSMProcs
{
    SettlementGenerator::SettlementGenerator(CSMDoc::Document& document, RandomGenerator& rng)
        : mDocument(document)
        , mData(document.getData())
        , mRng(rng)
    {
    }

    void SettlementGenerator::generateSettlement(SettlementLocation& location, const SettlementParams& params)
    {
        // Clear previous data
        mDistricts.clear();
        mRoads.clear();
        mLots.clear();
        mBuildings.clear();

        reportProgress(0, 7, "Planning districts...");

        // Step 1: Plan district layout based on settlement type
        planDistricts(location, location.type);

        reportProgress(1, 7, "Generating road network...");

        // Step 2: Generate road network
        generateRoadNetwork(location);

        reportProgress(2, 7, "Creating building lots...");

        // Step 3: Create building lots along roads
        createBuildingLots(location);

        reportProgress(3, 7, "Assigning building roles...");

        // Step 4: Assign building roles based on district and importance
        assignBuildingRoles(location.type);

        reportProgress(4, 7, "Placing buildings...");

        // Step 5: Place actual buildings
        placeBuildings(location);

        reportProgress(5, 7, "Adding street details...");

        // Step 6: Add street furniture and details
        addStreetDetails(location);

        // Step 7: Place roads
        placeRoads(location);

        reportProgress(6, 7, "Generating walls...");

        // Step 8: Place walls if applicable
        bool shouldHaveWalls = params.userOverrideWalls ? params.generateWalls
                                                        : settlementDefaultWalls(location.type);
        if (shouldHaveWalls)
        {
            placeWalls(location, params);
        }

        reportProgress(7, 7, "Generating interiors...");

        // Step 9: Generate interiors
        generateInteriors(location);

        // Store building IDs in location
        for (const auto& building : mBuildings)
        {
            location.buildingIds.push_back(building.refId);
            if (!building.interiorCellId.empty())
            {
                location.interiorIds.push_back(building.interiorCellId);
            }
        }
    }

    void SettlementGenerator::planDistricts(const SettlementLocation& location, SettlementType type)
    {
        if (!mConfig.enableDistricts)
        {
            // Single district for entire settlement
            District d;
            d.type = DistrictType::Residential;
            d.centerX = location.centerX;
            d.centerY = location.centerY;
            d.radius = location.radius;
            d.importance = 0.5f;
            mDistricts.push_back(d);
            return;
        }

        // District layout depends on settlement type
        switch (type)
        {
            case SettlementType::Farm:
            {
                District d;
                d.type = DistrictType::Residential;
                d.centerX = location.centerX;
                d.centerY = location.centerY;
                d.radius = location.radius;
                d.importance = 0.3f;
                mDistricts.push_back(d);
                break;
            }

            case SettlementType::Hamlet:
            case SettlementType::Village:
            {
                // Center district
                District center;
                center.type = DistrictType::Center;
                center.centerX = location.centerX;
                center.centerY = location.centerY;
                center.radius = location.radius * 0.3f;
                center.importance = 0.8f;
                mDistricts.push_back(center);

                // Residential around it
                District res;
                res.type = DistrictType::Residential;
                res.centerX = location.centerX;
                res.centerY = location.centerY;
                res.radius = location.radius;
                res.importance = 0.4f;
                mDistricts.push_back(res);
                break;
            }

            case SettlementType::Town:
            {
                // Center with market
                District center;
                center.type = DistrictType::Center;
                center.centerX = location.centerX;
                center.centerY = location.centerY;
                center.radius = location.radius * 0.2f;
                center.importance = 0.9f;
                mDistricts.push_back(center);

                // Market district
                float marketAngle = mRng.nextFloatRange(0.0f, 6.28f);
                District market;
                market.type = DistrictType::Market;
                market.centerX = location.centerX + std::cos(marketAngle) * location.radius * 0.35f;
                market.centerY = location.centerY + std::sin(marketAngle) * location.radius * 0.35f;
                market.radius = location.radius * 0.25f;
                market.importance = 0.7f;
                mDistricts.push_back(market);

                // Temple district opposite to market
                District temple;
                temple.type = DistrictType::Temple;
                temple.centerX = location.centerX + std::cos(marketAngle + 3.14f) * location.radius * 0.3f;
                temple.centerY = location.centerY + std::sin(marketAngle + 3.14f) * location.radius * 0.3f;
                temple.radius = location.radius * 0.2f;
                temple.importance = 0.6f;
                mDistricts.push_back(temple);

                // Residential areas
                for (int i = 0; i < 3; ++i)
                {
                    float angle = marketAngle + 1.5f + static_cast<float>(i) * 1.5f;
                    District res;
                    res.type = DistrictType::Residential;
                    res.centerX = location.centerX + std::cos(angle) * location.radius * 0.6f;
                    res.centerY = location.centerY + std::sin(angle) * location.radius * 0.6f;
                    res.radius = location.radius * 0.3f;
                    res.importance = 0.4f - static_cast<float>(i) * 0.1f;
                    mDistricts.push_back(res);
                }
                break;
            }

            case SettlementType::City:
            case SettlementType::Metropolis:
            {
                // Rich central area
                District center;
                center.type = DistrictType::Center;
                center.centerX = location.centerX;
                center.centerY = location.centerY;
                center.radius = location.radius * 0.15f;
                center.importance = 1.0f;
                mDistricts.push_back(center);

                // Noble district near center
                float nobleAngle = mRng.nextFloatRange(0.0f, 6.28f);
                District noble;
                noble.type = DistrictType::Noble;
                noble.centerX = location.centerX + std::cos(nobleAngle) * location.radius * 0.25f;
                noble.centerY = location.centerY + std::sin(nobleAngle) * location.radius * 0.25f;
                noble.radius = location.radius * 0.2f;
                noble.importance = 0.9f;
                mDistricts.push_back(noble);

                // Market district
                float marketAngle = nobleAngle + 1.5f;
                District market;
                market.type = DistrictType::Market;
                market.centerX = location.centerX + std::cos(marketAngle) * location.radius * 0.3f;
                market.centerY = location.centerY + std::sin(marketAngle) * location.radius * 0.3f;
                market.radius = location.radius * 0.2f;
                market.importance = 0.75f;
                mDistricts.push_back(market);

                // Temple district
                float templeAngle = nobleAngle + 3.0f;
                District temple;
                temple.type = DistrictType::Temple;
                temple.centerX = location.centerX + std::cos(templeAngle) * location.radius * 0.25f;
                temple.centerY = location.centerY + std::sin(templeAngle) * location.radius * 0.25f;
                temple.radius = location.radius * 0.15f;
                temple.importance = 0.7f;
                mDistricts.push_back(temple);

                // Industrial district on outskirts
                float industrialAngle = nobleAngle + 4.5f;
                District industrial;
                industrial.type = DistrictType::Industrial;
                industrial.centerX = location.centerX + std::cos(industrialAngle) * location.radius * 0.7f;
                industrial.centerY = location.centerY + std::sin(industrialAngle) * location.radius * 0.7f;
                industrial.radius = location.radius * 0.2f;
                industrial.importance = 0.3f;
                mDistricts.push_back(industrial);

                // Middle-class residential
                for (int i = 0; i < 4; ++i)
                {
                    float angle = static_cast<float>(i) * 1.57f;
                    District res;
                    res.type = DistrictType::Residential;
                    res.centerX = location.centerX + std::cos(angle) * location.radius * 0.5f;
                    res.centerY = location.centerY + std::sin(angle) * location.radius * 0.5f;
                    res.radius = location.radius * 0.2f;
                    res.importance = 0.5f;
                    mDistricts.push_back(res);
                }

                // Slums on far outskirts (opposite to noble)
                if (type == SettlementType::Metropolis)
                {
                    District slums;
                    slums.type = DistrictType::Slums;
                    slums.centerX = location.centerX + std::cos(nobleAngle + 3.14f) * location.radius * 0.8f;
                    slums.centerY = location.centerY + std::sin(nobleAngle + 3.14f) * location.radius * 0.8f;
                    slums.radius = location.radius * 0.25f;
                    slums.importance = 0.1f;
                    mDistricts.push_back(slums);
                }
                break;
            }

            case SettlementType::Fortress:
            {
                // Military center
                District military;
                military.type = DistrictType::Military;
                military.centerX = location.centerX;
                military.centerY = location.centerY;
                military.radius = location.radius * 0.6f;
                military.importance = 0.9f;
                mDistricts.push_back(military);

                // Support area
                District support;
                support.type = DistrictType::Industrial;
                support.centerX = location.centerX;
                support.centerY = location.centerY;
                support.radius = location.radius;
                support.importance = 0.4f;
                mDistricts.push_back(support);
                break;
            }

            case SettlementType::Castle:
            {
                // Castle grounds
                District castle;
                castle.type = DistrictType::Castle;
                castle.centerX = location.centerX;
                castle.centerY = location.centerY;
                castle.radius = location.radius * 0.5f;
                castle.importance = 1.0f;
                mDistricts.push_back(castle);

                // Gardens
                float gardenAngle = mRng.nextFloatRange(0.0f, 6.28f);
                District garden;
                garden.type = DistrictType::Garden;
                garden.centerX = location.centerX + std::cos(gardenAngle) * location.radius * 0.5f;
                garden.centerY = location.centerY + std::sin(gardenAngle) * location.radius * 0.5f;
                garden.radius = location.radius * 0.3f;
                garden.importance = 0.6f;
                mDistricts.push_back(garden);
                break;
            }

            default:
                break;
        }
    }

    void SettlementGenerator::generateRoadNetwork(const SettlementLocation& location)
    {
        if (mConfig.organicLayout)
        {
            generateOrganicRoads(location);
        }
        else
        {
            generateGridRoads(location);
        }
    }

    void SettlementGenerator::generateGridRoads(const SettlementLocation& location)
    {
        float gridSpacing = 200.0f + mRng.nextFloatRange(-20.0f, 20.0f);
        int gridCount = static_cast<int>(location.radius * 2.0f / gridSpacing);

        // Horizontal streets
        for (int i = 0; i < gridCount; ++i)
        {
            float y = location.centerY - location.radius + (static_cast<float>(i) + 0.5f) * gridSpacing;
            float offsetY = mConfig.organicFactor * mRng.nextFloatRange(-20.0f, 20.0f);

            RoadSegment road;
            road.startX = location.centerX - location.radius;
            road.startY = y + offsetY;
            road.endX = location.centerX + location.radius;
            road.endY = y + offsetY;
            road.width = (i == gridCount / 2) ? 150.0f : 80.0f;
            road.isMainRoad = (i == gridCount / 2);
            mRoads.push_back(road);
        }

        // Vertical streets
        for (int i = 0; i < gridCount; ++i)
        {
            float x = location.centerX - location.radius + (static_cast<float>(i) + 0.5f) * gridSpacing;
            float offsetX = mConfig.organicFactor * mRng.nextFloatRange(-20.0f, 20.0f);

            RoadSegment road;
            road.startX = x + offsetX;
            road.startY = location.centerY - location.radius;
            road.endX = x + offsetX;
            road.endY = location.centerY + location.radius;
            road.width = (i == gridCount / 2) ? 150.0f : 80.0f;
            road.isMainRoad = (i == gridCount / 2);
            mRoads.push_back(road);
        }
    }

    void SettlementGenerator::generateOrganicRoads(const SettlementLocation& location)
    {
        // Main radial roads from center
        int radialCount = 4 + static_cast<int>(location.radius / 500.0f);
        float angleOffset = mRng.nextFloatRange(0.0f, 6.28f / static_cast<float>(radialCount));

        for (int i = 0; i < radialCount; ++i)
        {
            float angle = angleOffset + static_cast<float>(i) * 6.28f / static_cast<float>(radialCount);
            // Add organic variation
            angle += mConfig.organicFactor * mRng.nextFloatRange(-0.2f, 0.2f);

            RoadSegment road;
            road.startX = location.centerX;
            road.startY = location.centerY;

            // Road curves slightly
            float curve = mConfig.organicFactor * mRng.nextFloatRange(-0.3f, 0.3f);
            float endAngle = angle + curve;

            road.endX = location.centerX + std::cos(endAngle) * location.radius * 0.95f;
            road.endY = location.centerY + std::sin(endAngle) * location.radius * 0.95f;
            road.width = 120.0f;
            road.isMainRoad = true;
            mRoads.push_back(road);
        }

        // Ring roads at different distances
        if (mConfig.ringRoads && location.radius > 400.0f)
        {
            int ringCount = static_cast<int>(location.radius / 400.0f);
            for (int ring = 1; ring <= ringCount; ++ring)
            {
                float ringRadius = location.radius * static_cast<float>(ring) / static_cast<float>(ringCount + 1);
                int segments = 8 + ring * 4;

                for (int i = 0; i < segments; ++i)
                {
                    float startAngle = static_cast<float>(i) * 6.28f / static_cast<float>(segments);
                    float endAngle = static_cast<float>(i + 1) * 6.28f / static_cast<float>(segments);

                    // Add organic variation
                    float startOffset = mConfig.organicFactor * mRng.nextFloatRange(-0.05f, 0.05f);
                    float endOffset = mConfig.organicFactor * mRng.nextFloatRange(-0.05f, 0.05f);

                    RoadSegment road;
                    road.startX = location.centerX + std::cos(startAngle) * ringRadius * (1.0f + startOffset);
                    road.startY = location.centerY + std::sin(startAngle) * ringRadius * (1.0f + startOffset);
                    road.endX = location.centerX + std::cos(endAngle) * ringRadius * (1.0f + endOffset);
                    road.endY = location.centerY + std::sin(endAngle) * ringRadius * (1.0f + endOffset);
                    road.width = 80.0f;
                    road.isMainRoad = false;
                    mRoads.push_back(road);
                }
            }
        }

        // Small connecting streets
        if (mConfig.irregularStreets)
        {
            int connectionCount = static_cast<int>(mConfig.roadDensity * location.radius / 50.0f);
            for (int i = 0; i < connectionCount; ++i)
            {
                float dist1 = mRng.nextFloatRange(0.1f, 0.8f) * location.radius;
                float dist2 = mRng.nextFloatRange(0.1f, 0.8f) * location.radius;
                float angle1 = mRng.nextFloatRange(0.0f, 6.28f);
                float angle2 = angle1 + mRng.nextFloatRange(0.3f, 1.0f);

                float x1 = location.centerX + std::cos(angle1) * dist1;
                float y1 = location.centerY + std::sin(angle1) * dist1;
                float x2 = location.centerX + std::cos(angle2) * dist2;
                float y2 = location.centerY + std::sin(angle2) * dist2;

                // Check terrain suitability
                float midX = (x1 + x2) * 0.5f;
                float midY = (y1 + y2) * 0.5f;
                if (mGetSlope && mGetSlope(midX, midY) < 0.4f)
                {
                    RoadSegment road;
                    road.startX = x1;
                    road.startY = y1;
                    road.endX = x2;
                    road.endY = y2;
                    road.width = 50.0f + mRng.nextFloatRange(-10.0f, 10.0f);
                    road.isMainRoad = false;
                    mRoads.push_back(road);
                }
            }
        }
    }

    void SettlementGenerator::createBuildingLots(const SettlementLocation& location)
    {
        for (const auto& road : mRoads)
        {
            // Determine district for this road
            float midX = (road.startX + road.endX) * 0.5f;
            float midY = (road.startY + road.endY) * 0.5f;
            DistrictType district = getDistrictAt(midX, midY, location);

            createLotsAlongStreet(road, district);
        }
    }

    void SettlementGenerator::createLotsAlongStreet(const RoadSegment& road, DistrictType district)
    {
        float dx = road.endX - road.startX;
        float dy = road.endY - road.startY;
        float length = std::sqrt(dx * dx + dy * dy);

        if (length < 50.0f)
            return;

        // Normalize direction
        dx /= length;
        dy /= length;

        // Perpendicular direction for lot placement
        float perpX = -dy;
        float perpY = dx;

        // Road direction angle
        float roadAngle = std::atan2(dy, dx);

        // Lot size varies by district
        float lotWidth = 80.0f;
        float lotDepth = 100.0f;

        switch (district)
        {
            case DistrictType::Slums:
                lotWidth = 50.0f;
                lotDepth = 60.0f;
                break;
            case DistrictType::Noble:
                lotWidth = 150.0f;
                lotDepth = 200.0f;
                break;
            case DistrictType::Market:
                lotWidth = 100.0f;
                lotDepth = 80.0f;
                break;
            case DistrictType::Industrial:
                lotWidth = 120.0f;
                lotDepth = 150.0f;
                break;
            default:
                break;
        }

        // Add variation
        lotWidth *= (1.0f + mConfig.organicFactor * mRng.nextFloatRange(-0.2f, 0.2f));
        lotDepth *= (1.0f + mConfig.organicFactor * mRng.nextFloatRange(-0.2f, 0.2f));

        // Calculate how many lots fit
        int lotCount = static_cast<int>(length / lotWidth);
        if (lotCount < 1)
            return;

        float actualSpacing = length / static_cast<float>(lotCount);
        float offset = road.width * 0.5f + lotDepth * 0.5f + 10.0f;

        // Place lots on both sides of road
        for (int side = -1; side <= 1; side += 2)
        {
            for (int i = 0; i < lotCount; ++i)
            {
                // Skip some lots based on density
                if (mRng.nextBool(1.0f - mConfig.buildingDensity))
                    continue;

                float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(lotCount);
                float posX = road.startX + dx * length * t + perpX * offset * static_cast<float>(side);
                float posY = road.startY + dy * length * t + perpY * offset * static_cast<float>(side);

                // Check terrain
                if (mGetSlope && mGetSlope(posX, posY) > 0.35f)
                    continue;

                BuildingLot lot;
                lot.x = posX;
                lot.y = posY;
                lot.width = lotWidth * (1.0f + mRng.nextFloatRange(-0.1f, 0.1f));
                lot.depth = lotDepth * (1.0f + mRng.nextFloatRange(-0.1f, 0.1f));
                lot.rotation = roadAngle + (side > 0 ? 0.0f : 3.14159f);
                lot.district = district;
                lot.isCorner = (i == 0 || i == lotCount - 1);
                lot.facesMainRoad = road.isMainRoad;
                lot.occupied = false;

                mLots.push_back(lot);
            }
        }
    }

    void SettlementGenerator::assignBuildingRoles(SettlementType type)
    {
        if (mLots.empty())
            return;

        // Get required building distribution
        int totalLots = static_cast<int>(mLots.size());
        auto required = getRequiredBuildings(type, totalLots);

        // Track which roles have been assigned
        std::map<BuildingRole, int> assigned;

        // First pass: assign special buildings to best locations
        for (const auto& [role, count] : required)
        {
            if (role == BuildingRole::CommonHouse || role == BuildingRole::PoorHouse ||
                role == BuildingRole::RichHouse)
                continue; // Fill these last

            for (int i = 0; i < count && assigned[role] < count; ++i)
            {
                // Find best lot for this role
                int bestLot = -1;
                float bestScore = -1.0f;

                for (size_t j = 0; j < mLots.size(); ++j)
                {
                    if (mLots[j].occupied)
                        continue;

                    float score = 0.0f;

                    // Score based on role requirements
                    switch (role)
                    {
                        case BuildingRole::TownHall:
                        case BuildingRole::Temple:
                        case BuildingRole::Guildhall:
                            // Want center, main road, importance
                            if (mLots[j].facesMainRoad)
                                score += 0.3f;
                            if (mLots[j].isCorner)
                                score += 0.2f;
                            if (mLots[j].district == DistrictType::Center)
                                score += 0.5f;
                            break;

                        case BuildingRole::Tavern:
                        case BuildingRole::Inn:
                            if (mLots[j].facesMainRoad)
                                score += 0.4f;
                            if (mLots[j].district == DistrictType::Market ||
                                mLots[j].district == DistrictType::Center)
                                score += 0.3f;
                            break;

                        case BuildingRole::GeneralStore:
                        case BuildingRole::Blacksmith:
                        case BuildingRole::Bakery:
                            if (mLots[j].district == DistrictType::Market)
                                score += 0.5f;
                            if (mLots[j].facesMainRoad)
                                score += 0.2f;
                            break;

                        case BuildingRole::Manor:
                        case BuildingRole::Palace:
                            if (mLots[j].district == DistrictType::Noble ||
                                mLots[j].district == DistrictType::Center)
                                score += 0.5f;
                            score += mLots[j].width * 0.001f; // Prefer larger lots
                            break;

                        case BuildingRole::Barracks:
                        case BuildingRole::Armory:
                            if (mLots[j].district == DistrictType::Military)
                                score += 0.5f;
                            break;

                        case BuildingRole::Warehouse:
                        case BuildingRole::Workshop:
                            if (mLots[j].district == DistrictType::Industrial)
                                score += 0.5f;
                            break;

                        default:
                            score = 0.5f;
                            break;
                    }

                    // Add randomness
                    score += mRng.nextFloatRange(0.0f, 0.2f);

                    if (score > bestScore)
                    {
                        bestScore = score;
                        bestLot = static_cast<int>(j);
                    }
                }

                if (bestLot >= 0)
                {
                    mLots[bestLot].occupied = true;
                    // Store role assignment (we'll use it when placing)
                    assigned[role]++;
                }
            }
        }

        // Second pass: fill remaining with residential
        for (auto& lot : mLots)
        {
            if (lot.occupied)
                continue;

            // Assign based on district wealth
            if (lot.district == DistrictType::Noble)
            {
                assigned[BuildingRole::RichHouse]++;
            }
            else if (lot.district == DistrictType::Slums)
            {
                assigned[BuildingRole::PoorHouse]++;
            }
            else
            {
                assigned[BuildingRole::CommonHouse]++;
            }
            lot.occupied = true;
        }
    }

    std::map<BuildingRole, int> SettlementGenerator::getRequiredBuildings(SettlementType type, int total) const
    {
        std::map<BuildingRole, int> req;

        switch (type)
        {
            case SettlementType::Farm:
                req[BuildingRole::CommonHouse] = 1;
                req[BuildingRole::Stable] = 1;
                req[BuildingRole::Warehouse] = 1;
                break;

            case SettlementType::Hamlet:
                req[BuildingRole::CommonHouse] = total - 2;
                req[BuildingRole::Well] = 1;
                req[BuildingRole::Chapel] = mRng.nextBool(0.5f) ? 1 : 0;
                break;

            case SettlementType::Village:
                req[BuildingRole::Inn] = 1;
                req[BuildingRole::Blacksmith] = 1;
                req[BuildingRole::GeneralStore] = 1;
                req[BuildingRole::Chapel] = 1;
                req[BuildingRole::Well] = 2;
                req[BuildingRole::Mill] = mRng.nextBool(0.5f) ? 1 : 0;
                req[BuildingRole::Stable] = 1;
                req[BuildingRole::CommonHouse] = total - 7;
                break;

            case SettlementType::Town:
                req[BuildingRole::TownHall] = 1;
                req[BuildingRole::Temple] = 1;
                req[BuildingRole::Inn] = 2;
                req[BuildingRole::Tavern] = 2;
                req[BuildingRole::Blacksmith] = 2;
                req[BuildingRole::GeneralStore] = 2;
                req[BuildingRole::Alchemist] = 1;
                req[BuildingRole::Guildhall] = 1;
                req[BuildingRole::Barracks] = 1;
                req[BuildingRole::Warehouse] = 2;
                req[BuildingRole::Stable] = 2;
                req[BuildingRole::Well] = 3;
                req[BuildingRole::Fountain] = 1;
                req[BuildingRole::Manor] = 2;
                req[BuildingRole::CommonHouse] = total - 23;
                break;

            case SettlementType::City:
            case SettlementType::Metropolis:
            {
                int scale = (type == SettlementType::Metropolis) ? 2 : 1;
                req[BuildingRole::TownHall] = 1;
                req[BuildingRole::Palace] = scale;
                req[BuildingRole::Temple] = 2 * scale;
                req[BuildingRole::Guildhall] = 3 * scale;
                req[BuildingRole::Courthouse] = scale;
                req[BuildingRole::Inn] = 4 * scale;
                req[BuildingRole::Tavern] = 6 * scale;
                req[BuildingRole::Blacksmith] = 3 * scale;
                req[BuildingRole::GeneralStore] = 4 * scale;
                req[BuildingRole::Alchemist] = 2 * scale;
                req[BuildingRole::Clothier] = 2 * scale;
                req[BuildingRole::Jeweler] = scale;
                req[BuildingRole::BookStore] = scale;
                req[BuildingRole::Bakery] = 3 * scale;
                req[BuildingRole::Butcher] = 2 * scale;
                req[BuildingRole::Bank] = scale;
                req[BuildingRole::Barracks] = 2 * scale;
                req[BuildingRole::Armory] = scale;
                req[BuildingRole::Prison] = scale;
                req[BuildingRole::Warehouse] = 4 * scale;
                req[BuildingRole::Workshop] = 3 * scale;
                req[BuildingRole::Stable] = 3 * scale;
                req[BuildingRole::Well] = 5 * scale;
                req[BuildingRole::Fountain] = 3 * scale;
                req[BuildingRole::Manor] = 5 * scale;
                req[BuildingRole::RichHouse] = total / 10;
                req[BuildingRole::PoorHouse] = total / 8;
                int assigned = 0;
                for (const auto& [r, c] : req)
                    assigned += c;
                req[BuildingRole::CommonHouse] = std::max(0, total - assigned);
                break;
            }

            case SettlementType::Fortress:
                req[BuildingRole::Barracks] = 3;
                req[BuildingRole::Armory] = 2;
                req[BuildingRole::GuardTower] = 4;
                req[BuildingRole::Warehouse] = 2;
                req[BuildingRole::Stable] = 2;
                req[BuildingRole::Well] = 2;
                req[BuildingRole::CommonHouse] = total - 15;
                break;

            case SettlementType::Castle:
                req[BuildingRole::Palace] = 1;
                req[BuildingRole::GuardTower] = 4;
                req[BuildingRole::Barracks] = 1;
                req[BuildingRole::Stable] = 1;
                req[BuildingRole::Chapel] = 1;
                break;

            default:
                req[BuildingRole::CommonHouse] = total;
                break;
        }

        return req;
    }

    void SettlementGenerator::placeBuildings(SettlementLocation& location)
    {
        auto required = getRequiredBuildings(location.type, static_cast<int>(mLots.size()));

        // Track remaining buildings to place
        std::map<BuildingRole, int> remaining = required;

        for (auto& lot : mLots)
        {
            if (!lot.occupied)
                continue;

            // Determine building role based on district and remaining needs
            BuildingRole role = BuildingRole::CommonHouse;
            float wealth = getWealthAt(lot.x, lot.y, location);

            // Try to place needed special buildings first
            for (auto& [r, count] : remaining)
            {
                if (count <= 0)
                    continue;
                if (r == BuildingRole::CommonHouse || r == BuildingRole::PoorHouse ||
                    r == BuildingRole::RichHouse)
                    continue;

                // Check if this lot is appropriate for this role
                bool suitable = false;
                switch (r)
                {
                    case BuildingRole::TownHall:
                    case BuildingRole::Palace:
                    case BuildingRole::Temple:
                    case BuildingRole::Guildhall:
                        suitable = (lot.district == DistrictType::Center && lot.facesMainRoad);
                        break;
                    case BuildingRole::Tavern:
                    case BuildingRole::Inn:
                        suitable = (lot.facesMainRoad ||
                                    lot.district == DistrictType::Market);
                        break;
                    case BuildingRole::GeneralStore:
                    case BuildingRole::Blacksmith:
                    case BuildingRole::Bakery:
                    case BuildingRole::Butcher:
                        suitable = (lot.district == DistrictType::Market);
                        break;
                    case BuildingRole::Barracks:
                    case BuildingRole::Armory:
                        suitable = (lot.district == DistrictType::Military);
                        break;
                    case BuildingRole::Warehouse:
                    case BuildingRole::Workshop:
                        suitable = (lot.district == DistrictType::Industrial);
                        break;
                    case BuildingRole::Manor:
                        suitable = (wealth > 0.7f);
                        break;
                    default:
                        suitable = mRng.nextBool(0.3f);
                        break;
                }

                if (suitable)
                {
                    role = r;
                    remaining[r]--;
                    break;
                }
            }

            // If no special role, assign residential based on wealth
            if (role == BuildingRole::CommonHouse)
            {
                if (lot.district == DistrictType::Noble || wealth > 0.8f)
                {
                    role = BuildingRole::RichHouse;
                }
                else if (lot.district == DistrictType::Slums || wealth < 0.2f)
                {
                    role = BuildingRole::PoorHouse;
                }
            }

            placeBuildingOnLot(lot, role, location);
        }
    }

    void SettlementGenerator::placeBuildingOnLot(BuildingLot& lot, BuildingRole role,
        SettlementLocation& location)
    {
        float wealth = getWealthAt(lot.x, lot.y, location);
        std::string objectId = selectBuildingForRole(role, lot.district, wealth);

        if (objectId.empty())
            return;

        float z = mGetHeight ? mGetHeight(lot.x, lot.y) : 0.0f;

        // Add small rotation variation for organic feel
        float rotation = lot.rotation;
        if (mConfig.organicLayout)
        {
            rotation += mRng.nextFloatRange(-0.1f, 0.1f);
        }

        std::string cellId = "#" + std::to_string(location.cellX) + ", " + std::to_string(location.cellY);
        std::string refId = mCreateReference(objectId, cellId, lot.x, lot.y, z, rotation, 1.0f);

        PlacedBuilding building;
        building.refId = refId;
        building.objectId = objectId;
        building.role = role;
        building.district = lot.district;
        building.worldX = lot.x;
        building.worldY = lot.y;
        building.worldZ = z;
        building.rotation = rotation;
        building.width = lot.width;
        building.depth = lot.depth;
        building.hasInterior = roleNeedsInterior(role);

        mBuildings.push_back(building);
    }

    std::string SettlementGenerator::selectBuildingForRole(BuildingRole role, DistrictType district,
        float wealthLevel)
    {
        // Try to get from asset library first
        if (mAssetLibrary)
        {
            auto buildings = mAssetLibrary->getAssetIds(AssetCategory::Building);
            if (!buildings.empty())
            {
                // Filter by name patterns matching role
                std::vector<std::string> candidates;
                std::string rolePattern;

                switch (role)
                {
                    case BuildingRole::TownHall:
                        rolePattern = "hall";
                        break;
                    case BuildingRole::Temple:
                        rolePattern = "temple";
                        break;
                    case BuildingRole::Inn:
                    case BuildingRole::Tavern:
                        rolePattern = "tavern";
                        break;
                    case BuildingRole::Blacksmith:
                        rolePattern = "smith";
                        break;
                    case BuildingRole::Manor:
                    case BuildingRole::Palace:
                        rolePattern = "manor";
                        break;
                    case BuildingRole::Barracks:
                        rolePattern = "barrack";
                        break;
                    case BuildingRole::Warehouse:
                        rolePattern = "warehouse";
                        break;
                    default:
                        rolePattern = "house";
                        break;
                }

                // Find matching buildings
                for (const auto& id : buildings)
                {
                    std::string lower = id;
                    std::transform(lower.begin(), lower.end(), lower.begin(),
                        [](unsigned char c) { return std::tolower(c); });
                    if (lower.find(rolePattern) != std::string::npos)
                    {
                        candidates.push_back(id);
                    }
                }

                // If no specific match, use general buildings
                if (candidates.empty())
                {
                    candidates = buildings;
                }

                if (!candidates.empty())
                {
                    return candidates[mRng.nextInt(static_cast<uint32_t>(candidates.size()))];
                }
            }
        }

        // Fallback defaults based on wealth
        std::vector<std::string> defaults;
        if (wealthLevel > 0.7f)
        {
            defaults = {"ex_hlaalu_manor_01", "ex_hlaalu_manor_02", "ex_redoran_manor_01"};
        }
        else if (wealthLevel > 0.3f)
        {
            defaults = {"ex_common_house_01", "ex_common_house_02", "ex_common_house_03",
                "ex_hlaalu_house_01", "ex_hlaalu_house_02"};
        }
        else
        {
            defaults = {"ex_common_shack_01", "ex_common_shack_02", "ex_common_hut_01"};
        }

        if (!defaults.empty())
        {
            return defaults[mRng.nextInt(static_cast<uint32_t>(defaults.size()))];
        }

        return "ex_common_house_01";
    }

    void SettlementGenerator::addStreetDetails(const SettlementLocation& location)
    {
        if (!mConfig.addClutter)
            return;

        std::string cellId = "#" + std::to_string(location.cellX) + ", " + std::to_string(location.cellY);

        // Add street furniture near buildings
        for (const auto& building : mBuildings)
        {
            // Skip some buildings
            if (!mRng.nextBool(mConfig.clutterDensity))
                continue;

            // Choose clutter type based on building role
            std::vector<std::string> clutterIds;
            if (mAssetLibrary)
            {
                switch (building.role)
                {
                    case BuildingRole::GeneralStore:
                    case BuildingRole::Warehouse:
                        clutterIds = mAssetLibrary->getAssetIds(AssetCategory::Container);
                        break;
                    case BuildingRole::Tavern:
                    case BuildingRole::Inn:
                        clutterIds = mAssetLibrary->getAssetIds(AssetCategory::Furniture);
                        break;
                    default:
                        clutterIds = mAssetLibrary->getAssetIds(AssetCategory::Clutter);
                        break;
                }
            }

            if (clutterIds.empty())
            {
                clutterIds = {"contain_crate_01", "contain_barrel_01", "furn_bench_01"};
            }

            // Place 1-3 items near building
            int count = mRng.nextIntRange(1, 3);
            for (int i = 0; i < count; ++i)
            {
                float offset = building.width * 0.4f + mRng.nextFloatRange(10.0f, 50.0f);
                float angle = building.rotation + mRng.nextFloatRange(-0.5f, 0.5f);

                float x = building.worldX + std::cos(angle) * offset;
                float y = building.worldY + std::sin(angle) * offset;
                float z = mGetHeight ? mGetHeight(x, y) : building.worldZ;

                std::string objectId = clutterIds[mRng.nextInt(static_cast<uint32_t>(clutterIds.size()))];
                mCreateReference(objectId, cellId, x, y, z, mRng.nextFloatRange(0.0f, 6.28f), 1.0f);
            }
        }

        // Add street lighting if enabled
        if (mConfig.addLighting)
        {
            std::vector<std::string> lightIds;
            if (mAssetLibrary)
            {
                lightIds = mAssetLibrary->getAssetIds(AssetCategory::Light);
            }
            if (lightIds.empty())
            {
                lightIds = {"light_de_lantern_01", "light_com_torch_01"};
            }

            // Place lights along main roads
            for (const auto& road : mRoads)
            {
                if (!road.isMainRoad)
                    continue;

                float dx = road.endX - road.startX;
                float dy = road.endY - road.startY;
                float length = std::sqrt(dx * dx + dy * dy);
                dx /= length;
                dy /= length;

                float spacing = 150.0f;
                int lightCount = static_cast<int>(length / spacing);

                for (int i = 0; i < lightCount; ++i)
                {
                    float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(lightCount);
                    float x = road.startX + dx * length * t;
                    float y = road.startY + dy * length * t;
                    float z = mGetHeight ? mGetHeight(x, y) : location.centerZ;

                    // Place on one side of road
                    float perpX = -dy * (road.width * 0.5f + 20.0f);
                    float perpY = dx * (road.width * 0.5f + 20.0f);

                    std::string objectId = lightIds[mRng.nextInt(static_cast<uint32_t>(lightIds.size()))];
                    mCreateReference(objectId, cellId, x + perpX, y + perpY, z, 0.0f, 1.0f);
                }
            }
        }
    }

    void SettlementGenerator::placeRoads(const SettlementLocation& location)
    {
        // Get road objects from asset library
        std::vector<std::string> roadIds;
        if (mAssetLibrary)
        {
            roadIds = mAssetLibrary->getAssetIds(AssetCategory::CobblestoneRoad);
            if (roadIds.empty())
            {
                roadIds = mAssetLibrary->getAssetIds(AssetCategory::Road);
            }
        }

        if (roadIds.empty())
            return; // No road objects available

        std::string cellId = "#" + std::to_string(location.cellX) + ", " + std::to_string(location.cellY);

        for (const auto& road : mRoads)
        {
            float dx = road.endX - road.startX;
            float dy = road.endY - road.startY;
            float length = std::sqrt(dx * dx + dy * dy);

            if (length < 10.0f)
                continue;

            dx /= length;
            dy /= length;
            float angle = std::atan2(dy, dx);

            float spacing = 50.0f;
            int segmentCount = static_cast<int>(length / spacing);

            for (int i = 0; i < segmentCount; ++i)
            {
                float t = static_cast<float>(i) / static_cast<float>(segmentCount);
                float x = road.startX + dx * length * t;
                float y = road.startY + dy * length * t;
                float z = mGetHeight ? mGetHeight(x, y) : location.centerZ;

                std::string objectId = roadIds[mRng.nextInt(static_cast<uint32_t>(roadIds.size()))];
                mCreateReference(objectId, cellId, x, y, z, angle, 1.0f);
            }
        }
    }

    void SettlementGenerator::placeWalls(const SettlementLocation& location, const SettlementParams& params)
    {
        std::vector<std::string> wallIds, gateIds, towerIds;
        if (mAssetLibrary)
        {
            wallIds = mAssetLibrary->getAssetIds(AssetCategory::Wall);
            gateIds = mAssetLibrary->getAssetIds(AssetCategory::WallGate);
            towerIds = mAssetLibrary->getAssetIds(AssetCategory::WallTower);
        }

        if (wallIds.empty())
            return;

        std::string cellId = "#" + std::to_string(location.cellX) + ", " + std::to_string(location.cellY);

        float wallRadius = params.wallRadius > 0.0f ? params.wallRadius : location.radius * 0.95f;
        float circumference = 2.0f * 3.14159f * wallRadius;
        int segmentCount = static_cast<int>(circumference / 80.0f);

        int gateCount = params.wallGateCount;
        float gateInterval = static_cast<float>(segmentCount) / static_cast<float>(std::max(1, gateCount));
        float towerInterval = 8.0f; // Tower every 8 segments

        for (int i = 0; i < segmentCount; ++i)
        {
            float angle = static_cast<float>(i) * 2.0f * 3.14159f / static_cast<float>(segmentCount);
            float x = location.centerX + std::cos(angle) * wallRadius;
            float y = location.centerY + std::sin(angle) * wallRadius;
            float z = mGetHeight ? mGetHeight(x, y) : location.centerZ;

            float rotation = angle + 1.5708f;

            // Determine element type
            std::string objectId;
            bool isGate = false;
            for (int g = 0; g < gateCount; ++g)
            {
                if (std::abs(static_cast<float>(i) - static_cast<float>(g) * gateInterval) < 1.0f)
                {
                    isGate = true;
                    break;
                }
            }

            if (isGate && !gateIds.empty())
            {
                objectId = gateIds[mRng.nextInt(static_cast<uint32_t>(gateIds.size()))];
            }
            else if (static_cast<int>(i) % static_cast<int>(towerInterval) == 0 && !towerIds.empty())
            {
                objectId = towerIds[mRng.nextInt(static_cast<uint32_t>(towerIds.size()))];
            }
            else
            {
                objectId = wallIds[mRng.nextInt(static_cast<uint32_t>(wallIds.size()))];
            }

            mCreateReference(objectId, cellId, x, y, z, rotation, 1.0f);
        }
    }

    void SettlementGenerator::generateInteriors(SettlementLocation& location)
    {
        // Interior generation would go here
        // For now, just mark buildings that need interiors
        for (auto& building : mBuildings)
        {
            if (building.hasInterior)
            {
                building.interiorCellId = location.name + "_" + building.refId + "_Interior";
            }
        }
    }

    DistrictType SettlementGenerator::getDistrictAt(float x, float y, const SettlementLocation& location) const
    {
        DistrictType best = DistrictType::Residential;
        float bestDist = 1e10f;

        for (const auto& district : mDistricts)
        {
            float dx = x - district.centerX;
            float dy = y - district.centerY;
            float dist = std::sqrt(dx * dx + dy * dy);

            if (dist < district.radius && dist < bestDist)
            {
                bestDist = dist;
                best = district.type;
            }
        }

        return best;
    }

    float SettlementGenerator::getImportanceAt(float x, float y, const SettlementLocation& location) const
    {
        float importance = 0.0f;
        float totalWeight = 0.0f;

        for (const auto& district : mDistricts)
        {
            float dx = x - district.centerX;
            float dy = y - district.centerY;
            float dist = std::sqrt(dx * dx + dy * dy);

            if (dist < district.radius)
            {
                float weight = 1.0f - (dist / district.radius);
                importance += district.importance * weight;
                totalWeight += weight;
            }
        }

        if (totalWeight > 0.0f)
            return importance / totalWeight;

        // Default: importance based on distance to center
        float dx = x - location.centerX;
        float dy = y - location.centerY;
        float dist = std::sqrt(dx * dx + dy * dy);
        return std::max(0.0f, 1.0f - (dist / location.radius));
    }

    float SettlementGenerator::getWealthAt(float x, float y, const SettlementLocation& location) const
    {
        DistrictType district = getDistrictAt(x, y, location);

        switch (district)
        {
            case DistrictType::Noble:
            case DistrictType::Castle:
                return 0.9f;
            case DistrictType::Center:
                return 0.7f;
            case DistrictType::Temple:
            case DistrictType::Market:
                return 0.6f;
            case DistrictType::Residential:
                return 0.5f;
            case DistrictType::Industrial:
                return 0.4f;
            case DistrictType::Slums:
                return 0.15f;
            default:
                return 0.5f;
        }
    }

    float SettlementGenerator::distanceToCenter(float x, float y, const SettlementLocation& location) const
    {
        float dx = x - location.centerX;
        float dy = y - location.centerY;
        return std::sqrt(dx * dx + dy * dy);
    }

    bool SettlementGenerator::roleNeedsInterior(BuildingRole role)
    {
        switch (role)
        {
            case BuildingRole::Well:
            case BuildingRole::Fountain:
            case BuildingRole::Statue:
            case BuildingRole::MarketStall:
            case BuildingRole::GuardTower:
            case BuildingRole::Graveyard:
                return false;
            default:
                return true;
        }
    }

    std::string SettlementGenerator::getDistrictName(DistrictType type)
    {
        switch (type)
        {
            case DistrictType::Center:
                return "Center";
            case DistrictType::Market:
                return "Market";
            case DistrictType::Residential:
                return "Residential";
            case DistrictType::Noble:
                return "Noble Quarter";
            case DistrictType::Slums:
                return "Slums";
            case DistrictType::Industrial:
                return "Industrial";
            case DistrictType::Temple:
                return "Temple District";
            case DistrictType::Military:
                return "Military Quarter";
            case DistrictType::Dock:
                return "Docks";
            case DistrictType::Garden:
                return "Gardens";
            case DistrictType::Castle:
                return "Castle Grounds";
            default:
                return "Unknown";
        }
    }

    std::string SettlementGenerator::getBuildingRoleName(BuildingRole role)
    {
        switch (role)
        {
            case BuildingRole::TownHall:
                return "Town Hall";
            case BuildingRole::Temple:
                return "Temple";
            case BuildingRole::Inn:
                return "Inn";
            case BuildingRole::Tavern:
                return "Tavern";
            case BuildingRole::Blacksmith:
                return "Blacksmith";
            case BuildingRole::GeneralStore:
                return "General Store";
            case BuildingRole::CommonHouse:
                return "House";
            case BuildingRole::RichHouse:
                return "Manor";
            case BuildingRole::PoorHouse:
                return "Shack";
            default:
                return "Building";
        }
    }

    std::pair<float, float> SettlementGenerator::getBuildingSizeForRole(BuildingRole role)
    {
        switch (role)
        {
            case BuildingRole::Palace:
            case BuildingRole::Temple:
            case BuildingRole::TownHall:
                return {200.0f, 250.0f};
            case BuildingRole::Manor:
            case BuildingRole::Barracks:
            case BuildingRole::Warehouse:
                return {150.0f, 180.0f};
            case BuildingRole::Inn:
            case BuildingRole::Tavern:
            case BuildingRole::Guildhall:
                return {120.0f, 150.0f};
            case BuildingRole::CommonHouse:
            case BuildingRole::GeneralStore:
            case BuildingRole::Blacksmith:
                return {80.0f, 100.0f};
            case BuildingRole::PoorHouse:
                return {50.0f, 60.0f};
            case BuildingRole::Well:
            case BuildingRole::Fountain:
            case BuildingRole::Statue:
                return {30.0f, 30.0f};
            default:
                return {80.0f, 100.0f};
        }
    }

    void SettlementGenerator::reportProgress(int current, int total, const std::string& message)
    {
        if (mProgressCallback)
        {
            mProgressCallback(current, total, message);
        }
    }
}
