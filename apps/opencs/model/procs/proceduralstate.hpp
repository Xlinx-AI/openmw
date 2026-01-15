#ifndef CSM_PROCS_PROCEDURALSTATE_H
#define CSM_PROCS_PROCEDURALSTATE_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace CSMProcs
{
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
        
        // Analysis results from reference
        AnalysisResults analysis;
        
        // Generation options
        bool generateExteriors = true;
        bool generateInteriors = true;
        bool generatePathgrids = true;
        bool overwriteExisting = false;
        
        // Preview options
        bool previewMode = false;
        int previewCellX = 0;
        int previewCellY = 0;
    };
}

#endif
