#ifndef CSM_PROCS_PROCEDURALSTATE_H
#define CSM_PROCS_PROCEDURALSTATE_H

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace CSMProcs
{
    // Forward declaration
    class AssetLibrary;

    /// Parameters for terrain generation
    struct TerrainParams
    {
        float baseHeight = 0.0f;
        float heightVariation = 1000.0f;
        float roughness = 0.5f;
        float erosionStrength = 0.3f;
        int octaves = 6;
        float persistence = 0.5f;
        float lacunarity = 2.0f;
        bool generateWater = true;
        float waterLevel = 0.0f;
        float mountainFrequency = 0.1f;
        float valleyDepth = 0.3f;
    };

    /// Parameters for object placement
    struct ObjectPlacementParams
    {
        float treeDensity = 0.3f;
        float rockDensity = 0.2f;
        float grassDensity = 0.5f;
        float buildingDensity = 0.05f;
        float minSpacing = 100.0f;
        float clusteringFactor = 0.5f;
        bool alignToTerrain = true;
        float scaleVariation = 0.2f;
        float rotationVariation = 1.0f;
        
        // Extended parameters for user-configured assets
        float bushDensity = 0.2f;
        float cityPropDensity = 0.1f;
        
        // Use asset library for object selection instead of hardcoded lists
        bool useAssetLibrary = true;
    };

    /// Parameters for interior generation
    struct InteriorParams
    {
        int minRooms = 3;
        int maxRooms = 12;
        float roomSizeMin = 200.0f;
        float roomSizeMax = 800.0f;
        float corridorWidth = 150.0f;
        float ceilingHeight = 300.0f;
        bool generateLighting = true;
        bool generateContainers = true;
        bool generateNPCs = false;
        float clutter = 0.4f;
        
        // Use asset library for interior object selection
        bool useAssetLibrary = true;
    };

    /// Settlement type enumeration
    enum class SettlementType
    {
        None,      // No settlement generation
        Farm,      // 1-3 buildings, no walls
        Hamlet,    // 5-10 buildings, no walls
        Village,   // 10-30 buildings, optional palisade
        Town,      // 30-100 buildings, walls common
        City,      // 100-500 buildings, walls
        Metropolis,// 500+ buildings, multiple wall rings
        Fortress,  // Military settlement with strong walls
        Castle     // Single large structure with grounds
    };

    /// Parameters for settlement generation
    struct SettlementParams
    {
        SettlementType type = SettlementType::None;
        int settlementCount = 1; // Number of settlements to generate
        bool autoPlaceSettlements = true; // Auto-select good locations
        
        // Manual placement (used when autoPlaceSettlements is false)
        std::vector<std::pair<int, int>> manualLocations; // Cell coordinates
        
        // Wall configuration
        bool generateWalls = false; // Auto-decided based on settlement type if not set
        bool userOverrideWalls = false; // Whether user explicitly set wall generation
        float wallRadius = 0.0f; // 0 = auto-calculate based on building count
        int wallGateCount = 2;
        bool generateMoat = false;
        float moatWidth = 100.0f;
        
        // Road configuration
        bool generateRoads = true;
        bool generateMainSquare = true;
        float mainSquareSize = 500.0f;
        
        // Building configuration
        float buildingSpacing = 150.0f;
        bool generateBuildingInteriors = true;
        
        // Special structures
        bool generateCastle = false; // Add a castle to the settlement
        bool generateTemple = false;
        bool generateMarket = true;
        bool generateInn = true;
    };

    /// Parameters for cave and dungeon generation
    struct CaveDungeonParams
    {
        bool generateCaves = true;
        bool generateDungeons = true;
        int caveCount = 3; // Per world
        int dungeonCount = 2; // Per world
        
        // Cave parameters
        int caveMinRooms = 3;
        int caveMaxRooms = 10;
        float caveRoomSizeMin = 300.0f;
        float caveRoomSizeMax = 800.0f;
        bool caveUnderwater = false;
        
        // Dungeon parameters
        int dungeonMinRooms = 5;
        int dungeonMaxRooms = 20;
        float dungeonRoomSizeMin = 200.0f;
        float dungeonRoomSizeMax = 600.0f;
        int dungeonFloors = 1; // Multiple floor dungeons
        
        // Content
        bool generateCreatures = false;
        bool generateLoot = true;
        bool generateTraps = false;
    };

    /// Parameters for region-based content distribution
    struct RegionParams
    {
        std::string regionId;
        float weatherChangeChance = 0.2f;
        std::vector<std::string> allowedCreatures;
        std::vector<std::string> allowedFlora;
        std::map<std::string, float> textureWeights;
    };

    /// Analysis results from studying a reference worldspace
    struct AnalysisResults
    {
        // Terrain statistics
        float avgHeight = 0.0f;
        float heightStdDev = 0.0f;
        float minHeight = 0.0f;
        float maxHeight = 0.0f;
        float avgRoughness = 0.0f;
        
        // Texture distribution
        std::map<std::string, float> textureFrequency;
        
        // Object placement patterns
        std::map<std::string, float> objectDensityByType;
        std::map<std::string, std::vector<std::pair<float, float>>> objectClusters;
        
        // Learned rules
        std::vector<std::pair<std::string, std::string>> objectPairings;
        float avgObjectSpacing = 0.0f;
        
        bool isValid = false;
    };

    /// Main state structure for procedural generation
    struct ProceduralState
    {
        // World dimensions
        int worldSizeX = 10;
        int worldSizeY = 10;
        int originX = 0;
        int originY = 0;
        
        // Generation seed
        uint64_t seed = 0;
        
        // Reference worldspace for style learning
        std::string referenceWorldspace;
        bool useReference = false;
        
        // Sub-parameters
        TerrainParams terrain;
        ObjectPlacementParams objects;
        InteriorParams interiors;
        std::vector<RegionParams> regions;
        
        // Settlement generation
        SettlementParams settlement;
        
        // Cave and dungeon generation
        CaveDungeonParams caveDungeon;
        
        // Asset library for user-configured objects
        std::shared_ptr<AssetLibrary> assetLibrary;
        
        // Analysis results from reference
        AnalysisResults analysis;
        
        // Generation options
        bool generateExteriors = true;
        bool generateInteriors = true;
        bool generatePathgrids = true;
        bool generateSettlements = false;
        bool generateCavesAndDungeons = false;
        bool overwriteExisting = false;
        
        // Preview options
        bool previewMode = false;
        int previewCellX = 0;
        int previewCellY = 0;
    };
    
    /// Helper function to get settlement type name
    inline std::string getSettlementTypeName(SettlementType type)
    {
        switch (type)
        {
            case SettlementType::None: return "None";
            case SettlementType::Farm: return "Farm";
            case SettlementType::Hamlet: return "Hamlet";
            case SettlementType::Village: return "Village";
            case SettlementType::Town: return "Town";
            case SettlementType::City: return "City";
            case SettlementType::Metropolis: return "Metropolis";
            case SettlementType::Fortress: return "Fortress";
            case SettlementType::Castle: return "Castle";
            default: return "Unknown";
        }
    }
    
    /// Get building count range for settlement type
    inline std::pair<int, int> getSettlementBuildingRange(SettlementType type)
    {
        switch (type)
        {
            case SettlementType::Farm: return {1, 3};
            case SettlementType::Hamlet: return {5, 10};
            case SettlementType::Village: return {10, 30};
            case SettlementType::Town: return {30, 100};
            case SettlementType::City: return {100, 500};
            case SettlementType::Metropolis: return {500, 2000};
            case SettlementType::Fortress: return {5, 20};
            case SettlementType::Castle: return {1, 5};
            default: return {0, 0};
        }
    }
    
    /// Check if settlement type should have walls by default
    inline bool settlementDefaultWalls(SettlementType type)
    {
        switch (type)
        {
            case SettlementType::Town:
            case SettlementType::City:
            case SettlementType::Metropolis:
            case SettlementType::Fortress:
            case SettlementType::Castle:
                return true;
            default:
                return false;
        }
    }
}

#endif
