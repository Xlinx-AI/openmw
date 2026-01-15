#ifndef CSM_PROCS_SETTLEMENTGENERATOR_H
#define CSM_PROCS_SETTLEMENTGENERATOR_H

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "assetlibrary.hpp"
#include "noise.hpp"
#include "proceduralstate.hpp"

namespace CSMWorld
{
    class Data;
}

namespace CSMDoc
{
    class Document;
}

namespace CSMProcs
{
    /// District types for city organization
    enum class DistrictType
    {
        Center,      // Town center with important buildings
        Market,      // Commercial area
        Residential, // Housing
        Noble,       // Upper-class housing
        Slums,       // Poor housing
        Industrial,  // Workshops, smithies
        Temple,      // Religious buildings
        Military,    // Barracks, training grounds
        Dock,        // Harbor area
        Garden,      // Parks, gardens
        Castle       // Fortress/castle grounds
    };

    /// Building role in the settlement
    enum class BuildingRole
    {
        // Civic
        TownHall,
        Guildhall,
        Courthouse,
        
        // Commercial
        GeneralStore,
        Blacksmith,
        Alchemist,
        Clothier,
        Jeweler,
        BookStore,
        Bakery,
        Butcher,
        Tavern,
        Inn,
        Bank,
        
        // Residential
        PoorHouse,
        CommonHouse,
        RichHouse,
        Manor,
        Palace,
        
        // Religious
        Temple,
        Chapel,
        Shrine,
        Graveyard,
        
        // Military
        Barracks,
        Armory,
        GuardTower,
        Prison,
        
        // Industrial
        Workshop,
        Warehouse,
        Mill,
        Stable,
        
        // Special
        Fountain,
        Well,
        Statue,
        MarketStall,
        
        // Generic
        Generic
    };

    /// A placed building with metadata
    struct PlacedBuilding
    {
        std::string refId;           // Reference ID in the world
        std::string objectId;        // The static object ID
        BuildingRole role = BuildingRole::Generic;
        DistrictType district = DistrictType::Residential;
        float worldX = 0.0f;
        float worldY = 0.0f;
        float worldZ = 0.0f;
        float rotation = 0.0f;
        float width = 100.0f;        // Approximate building footprint
        float depth = 100.0f;
        bool hasInterior = true;
        std::string interiorCellId;
        std::string linkedDoor;      // Door reference connecting to interior
    };

    /// A district within the settlement
    struct District
    {
        DistrictType type = DistrictType::Residential;
        float centerX = 0.0f;
        float centerY = 0.0f;
        float radius = 200.0f;
        std::vector<PlacedBuilding> buildings;
        float importance = 0.5f;     // 0-1, affects building quality
    };

    /// Road segment
    struct RoadSegment
    {
        float startX, startY;
        float endX, endY;
        float width = 100.0f;
        bool isMainRoad = false;
        std::string textureId;
    };

    /// A lot/plot for building placement
    struct BuildingLot
    {
        float x, y;                  // Center position
        float width, depth;          // Lot dimensions
        float rotation;              // Lot orientation (facing road)
        DistrictType district;
        bool isCorner = false;       // Corner lot (more prominent)
        bool facesMainRoad = false;
        bool occupied = false;
    };

    /// Configuration for realistic settlement generation
    struct RealisticSettlementConfig
    {
        // Layout style
        bool organicLayout = true;       // vs grid-based
        float organicFactor = 0.3f;      // How much randomization (0=grid, 1=chaos)
        
        // District configuration
        bool enableDistricts = true;
        bool segregateWealth = true;     // Separate rich/poor areas
        
        // Road network
        bool radialRoads = true;         // Roads from center
        bool ringRoads = true;           // Circular roads around center
        bool irregularStreets = true;    // Small alleys, shortcuts
        float roadDensity = 0.5f;
        
        // Building placement
        float buildingDensity = 0.6f;    // How tightly packed
        float heightVariation = 0.2f;    // Building height variety
        bool alignToRoads = true;        // Buildings face roads
        
        // Details
        bool addClutter = true;          // Crates, barrels, etc.
        bool addVegetation = true;       // Trees, flowers in gardens
        bool addLighting = true;         // Street lamps, torches
        float clutterDensity = 0.3f;
    };

    /// Advanced settlement generator for realistic cities
    class SettlementGenerator
    {
    public:
        using ProgressCallback = std::function<void(int current, int total, const std::string& message)>;
        using CreateReferenceFunc = std::function<std::string(const std::string& objectId, 
            const std::string& cellId, float x, float y, float z, float rot, float scale)>;
        using HeightFunc = std::function<float(float x, float y)>;
        using SlopeFunc = std::function<float(float x, float y)>;

        SettlementGenerator(CSMDoc::Document& document, RandomGenerator& rng);
        
        /// Set the asset library for object selection
        void setAssetLibrary(std::shared_ptr<AssetLibrary> library) { mAssetLibrary = library; }
        
        /// Set callback for creating references
        void setCreateReferenceFunc(CreateReferenceFunc func) { mCreateReference = std::move(func); }
        
        /// Set height query function
        void setHeightFunc(HeightFunc func) { mGetHeight = std::move(func); }
        
        /// Set slope query function
        void setSlopeFunc(SlopeFunc func) { mGetSlope = std::move(func); }
        
        /// Set progress callback
        void setProgressCallback(ProgressCallback callback) { mProgressCallback = std::move(callback); }
        
        /// Set configuration
        void setConfig(const RealisticSettlementConfig& config) { mConfig = config; }
        
        /// Generate a complete settlement
        void generateSettlement(SettlementLocation& location, const SettlementParams& params);
        
    private:
        CSMDoc::Document& mDocument;
        CSMWorld::Data& mData;
        RandomGenerator& mRng;
        std::shared_ptr<AssetLibrary> mAssetLibrary;
        CreateReferenceFunc mCreateReference;
        HeightFunc mGetHeight;
        SlopeFunc mGetSlope;
        ProgressCallback mProgressCallback;
        RealisticSettlementConfig mConfig;
        
        // Current settlement data
        std::vector<District> mDistricts;
        std::vector<RoadSegment> mRoads;
        std::vector<BuildingLot> mLots;
        std::vector<PlacedBuilding> mBuildings;
        
        /// Plan the district layout
        void planDistricts(const SettlementLocation& location, SettlementType type);
        
        /// Generate road network
        void generateRoadNetwork(const SettlementLocation& location);
        
        /// Create building lots along roads
        void createBuildingLots(const SettlementLocation& location);
        
        /// Assign building roles to lots
        void assignBuildingRoles(SettlementType type);
        
        /// Place actual buildings
        void placeBuildings(SettlementLocation& location);
        
        /// Add street furniture and details
        void addStreetDetails(const SettlementLocation& location);
        
        /// Generate interiors for buildings
        void generateInteriors(SettlementLocation& location);
        
        /// Place roads as objects or modify terrain
        void placeRoads(const SettlementLocation& location);
        
        /// Place walls around the settlement
        void placeWalls(const SettlementLocation& location, const SettlementParams& params);
        
        /// Get buildings needed for settlement type
        std::map<BuildingRole, int> getRequiredBuildings(SettlementType type, int totalBuildings) const;
        
        /// Select appropriate building object for role
        std::string selectBuildingForRole(BuildingRole role, DistrictType district) const;
        
        /// Get district at position
        DistrictType getDistrictAt(float x, float y, const SettlementLocation& location) const;
        
        /// Calculate importance factor at position (0-1)
        float getImportanceAt(float x, float y, const SettlementLocation& location) const;
        
        /// Check if position is on a road
        bool isOnRoad(float x, float y) const;
        
        /// Find nearest road direction at position
        float getNearestRoadDirection(float x, float y) const;
        
        /// Get distance to settlement center
        float distanceToCenter(float x, float y, const SettlementLocation& location) const;
        
        /// Convert district type to string
        static std::string getDistrictName(DistrictType type);
        
        /// Get default building size for role
        static std::pair<float, float> getBuildingSizeForRole(BuildingRole role);
        
        /// Report progress
        void reportProgress(int current, int total, const std::string& message);
    };
}

#endif
