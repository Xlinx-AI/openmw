#ifndef CSM_PROCS_INFRASTRUCTUREGENERATOR_H
#define CSM_PROCS_INFRASTRUCTUREGENERATOR_H

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "assetlibrary.hpp"
#include "noise.hpp"
#include "proceduralstate.hpp"

// Forward declaration for SettlementLocation
namespace CSMProcs
{
    struct SettlementLocation;
}

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
    /// Type of infrastructure element
    enum class InfrastructureType
    {
        // Roads and paths
        MainRoad,        // Wide road connecting settlements
        TradeRoute,      // Major commerce route
        LocalRoad,       // Road between nearby locations
        Path,            // Footpath, trail
        
        // Bridges
        StoneBridge,     // Large stone bridge
        WoodBridge,      // Wooden bridge
        Pontoon,         // Floating bridge
        
        // Waypoints
        Signpost,        // Direction signs at crossroads
        Milestone,       // Distance markers
        Shrine,          // Roadside shrine
        Wayshrine,       // Larger religious waypoint
        RestStop,        // Benches, shelter for travelers
        
        // Watchtowers and defense
        Watchtower,      // Tall tower on hills
        GuardPost,       // Small guard station
        Beacon,          // Signal fire location
        Fortlet,         // Small fortification
        
        // Water infrastructure
        Dock,            // Port facility
        Pier,            // Smaller dock
        Lighthouse,      // Coastal light
        FishingHut,      // Fishing station
        WaterMill,       // Mill on river
        
        // Rural structures
        Farm,            // Agricultural building
        Windmill,        // Grain mill
        Barn,            // Storage building
        Silo,            // Grain storage
        Orchard,         // Fruit trees
        Vineyard,        // Wine production
        
        // Mining and industry
        Mine,            // Mining entrance
        Quarry,          // Stone quarry
        LumberCamp,      // Wood harvesting
        CharcoalKiln,    // Charcoal production
        Sawmill,         // Wood processing
        Smelter,         // Metal processing
        
        // Ruins and ancient
        Ruins,           // Abandoned buildings
        AncientTower,    // Old watchtower
        BurialMound,     // Ancient grave
        StandingStone,   // Mystical stone
        AncientShrine,   // Old religious site
        
        // Camps
        BanditCamp,      // Hostile encampment
        HunterCamp,      // Hunter's camp
        TravelerCamp,    // Temporary camp
        MerchantCamp,    // Trade caravan stop
        MilitaryCamp,    // Soldier encampment
        
        // Misc
        Well,            // Water source
        Crossroads,      // Road intersection
        Cemetery,        // Burial ground
        Garden,          // Decorative garden
        Monument         // Memorial structure
    };

    /// Logical context for infrastructure placement
    struct PlacementContext
    {
        // Terrain requirements
        float minHeight = -10000.0f;
        float maxHeight = 10000.0f;
        float maxSlope = 0.5f;
        bool nearWater = false;      // Must be near water
        bool onWater = false;        // Must be in water
        bool avoidWater = true;      // Must not be in water
        bool onHill = false;         // Prefer elevated positions
        bool inValley = false;       // Prefer low areas
        
        // Proximity requirements
        bool nearRoad = false;       // Should be near roads
        float roadDistance = 200.0f; // Max distance to road
        bool nearSettlement = false; // Should be near settlements
        float settlementDistance = 1000.0f;
        bool awayFromSettlement = false; // Should be far from settlements
        float minSettlementDistance = 2000.0f;
        
        // Resource requirements
        bool nearOreDeposit = false; // For mines
        bool nearForest = false;     // For lumber
        bool nearFertileLand = false;// For farms
        
        // Spacing
        float minSpacingFromSame = 500.0f; // Min distance from same type
        float minSpacingFromAny = 100.0f;  // Min distance from any infrastructure
    };

    /// A placed infrastructure element
    struct PlacedInfrastructure
    {
        InfrastructureType type;
        std::string refId;
        std::string objectId;
        int cellX, cellY;
        float worldX, worldY, worldZ;
        float rotation;
        float scale;
        bool hasInterior = false;
        std::string interiorCellId;
        std::vector<std::string> linkedRefs; // Associated objects (e.g., farm fields)
    };

    /// A road or path in the world
    struct WorldRoad
    {
        std::vector<std::pair<float, float>> waypoints;
        float width = 100.0f;
        bool isMainRoad = false;
        std::string startLocation;   // Settlement or POI name
        std::string endLocation;
        std::vector<std::string> placedObjects; // Road segment refs
    };

    /// Configuration for infrastructure generation
    struct InfrastructureConfig
    {
        // Road generation
        bool generateRoads = true;
        bool connectSettlements = true;
        float roadCurviness = 0.3f;      // How much roads follow terrain
        bool avoidSteepSlopes = true;
        float maxRoadSlope = 0.3f;
        
        // Waypoint generation
        bool generateWaypoints = true;
        float signpostFrequency = 0.5f;  // At crossroads
        float shrineFrequency = 0.3f;    // Along roads
        float restStopFrequency = 0.2f;  // Every N distance
        float restStopInterval = 2000.0f;
        
        // Defense structures
        bool generateWatchtowers = true;
        float watchtowerDensity = 0.3f;
        bool generateGuardPosts = true;
        
        // Rural structures
        bool generateFarms = true;
        float farmDensity = 0.4f;
        bool generateMills = true;
        
        // Mining/industry
        bool generateMines = true;
        bool generateLumberCamps = true;
        float industryDensity = 0.2f;
        
        // Ruins
        bool generateRuins = true;
        float ruinDensity = 0.2f;
        float ruinAge = 0.5f; // 0=recent, 1=ancient
        
        // Camps
        bool generateCamps = true;
        float banditCampDensity = 0.1f;
        float travelerCampDensity = 0.2f;
        
        // Water structures
        bool generateDocks = true;
        bool generateLighthouses = true;
        bool generateFishingHuts = true;
    };

    /// Generates logical infrastructure across the world
    class InfrastructureGenerator
    {
    public:
        using ProgressCallback = std::function<void(int current, int total, const std::string& message)>;
        using CreateReferenceFunc = std::function<std::string(const std::string& objectId, 
            const std::string& cellId, float x, float y, float z, float rot, float scale)>;
        using HeightFunc = std::function<float(float x, float y)>;
        using SlopeFunc = std::function<float(float x, float y)>;
        using WaterLevelFunc = std::function<float()>;

        InfrastructureGenerator(CSMDoc::Document& document, RandomGenerator& rng);
        
        /// Set the asset library
        void setAssetLibrary(std::shared_ptr<AssetLibrary> library) { mAssetLibrary = library; }
        
        /// Set callback functions
        void setCreateReferenceFunc(CreateReferenceFunc func) { mCreateReference = std::move(func); }
        void setHeightFunc(HeightFunc func) { mGetHeight = std::move(func); }
        void setSlopeFunc(SlopeFunc func) { mGetSlope = std::move(func); }
        void setWaterLevelFunc(WaterLevelFunc func) { mGetWaterLevel = std::move(func); }
        void setProgressCallback(ProgressCallback callback) { mProgressCallback = std::move(callback); }
        
        /// Set configuration
        void setConfig(const InfrastructureConfig& config) { mConfig = config; }
        
        /// Set world bounds
        void setWorldBounds(int originX, int originY, int sizeX, int sizeY);
        
        /// Set known settlements (for road connections)
        void setSettlements(const std::vector<SettlementLocation>& settlements);
        
        /// Generate all infrastructure
        bool generate();
        
        /// Generate roads connecting settlements
        bool generateRoadNetwork();
        
        /// Generate waypoints along roads
        bool generateWaypoints();
        
        /// Generate watchtowers and defensive structures
        bool generateDefenseStructures();
        
        /// Generate farms and rural structures
        bool generateRuralStructures();
        
        /// Generate mines, lumber camps, etc.
        bool generateIndustry();
        
        /// Generate ruins and ancient sites
        bool generateRuins();
        
        /// Generate camps
        bool generateCamps();
        
        /// Generate water-related structures
        bool generateWaterStructures();
        
        /// Get all placed infrastructure
        const std::vector<PlacedInfrastructure>& getPlacedInfrastructure() const { return mPlaced; }
        
        /// Get generated roads
        const std::vector<WorldRoad>& getRoads() const { return mRoads; }
        
    private:
        CSMDoc::Document& mDocument;
        CSMWorld::Data& mData;
        RandomGenerator& mRng;
        std::shared_ptr<AssetLibrary> mAssetLibrary;
        CreateReferenceFunc mCreateReference;
        HeightFunc mGetHeight;
        SlopeFunc mGetSlope;
        WaterLevelFunc mGetWaterLevel;
        ProgressCallback mProgressCallback;
        InfrastructureConfig mConfig;
        
        // World bounds
        int mOriginX = 0, mOriginY = 0;
        int mSizeX = 10, mSizeY = 10;
        
        // Known locations
        std::vector<SettlementLocation> mSettlements;
        
        // Generated data
        std::vector<PlacedInfrastructure> mPlaced;
        std::vector<WorldRoad> mRoads;
        
        /// Get placement context for infrastructure type
        PlacementContext getContextForType(InfrastructureType type) const;
        
        /// Find valid location for infrastructure
        bool findValidLocation(InfrastructureType type, float& outX, float& outY, float& outZ);
        
        /// Check if location is valid for placement
        bool isValidLocation(float x, float y, const PlacementContext& context) const;
        
        /// Check distance to nearest settlement
        float distanceToNearestSettlement(float x, float y) const;
        
        /// Check distance to nearest road
        float distanceToNearestRoad(float x, float y) const;
        
        /// Check if near water
        bool isNearWater(float x, float y, float radius) const;
        
        /// Check if position is in water
        bool isInWater(float x, float y) const;
        
        /// Check if position is on a hill
        bool isOnHill(float x, float y) const;
        
        /// Find path between two points following terrain
        std::vector<std::pair<float, float>> findPath(float startX, float startY, 
                                                       float endX, float endY);
        
        /// Smooth a path to avoid sharp turns
        void smoothPath(std::vector<std::pair<float, float>>& path) const;
        
        /// Place infrastructure element
        PlacedInfrastructure placeInfrastructure(InfrastructureType type, 
                                                  float x, float y, float z, float rotation);
        
        /// Select object ID for infrastructure type
        std::string selectObjectForType(InfrastructureType type) const;
        
        /// Place road segment objects
        void placeRoadSegments(const WorldRoad& road);
        
        /// Place farm with fields
        void placeFarm(float x, float y, float z);
        
        /// Place mine with entrance
        void placeMine(float x, float y, float z);
        
        /// Place ruins cluster
        void placeRuins(float x, float y, float z);
        
        /// Place camp with tents/bedrolls
        void placeCamp(InfrastructureType type, float x, float y, float z);
        
        /// Get cell ID for world position
        std::string getCellId(float x, float y) const;
        
        /// Report progress
        void reportProgress(int current, int total, const std::string& message);
        
        /// Convert type to string
        static std::string getTypeName(InfrastructureType type);
    };
}

#endif
