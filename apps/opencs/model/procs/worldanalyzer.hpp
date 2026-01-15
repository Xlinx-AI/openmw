#ifndef CSM_PROCS_WORLDANALYZER_H
#define CSM_PROCS_WORLDANALYZER_H

#include <map>
#include <string>
#include <vector>

#include "proceduralstate.hpp"

namespace CSMWorld
{
    class Data;
}

namespace CSMProcs
{
    /// Analyzes existing worldspaces to extract patterns for procedural generation
    class WorldAnalyzer
    {
    public:
        WorldAnalyzer(CSMWorld::Data& data);
        
        /// Analyze all exterior cells in the worldspace
        /// @param cellIdPrefix Optional prefix to filter cells (e.g. for specific worldspace)
        /// @return Analysis results containing terrain and object patterns
        AnalysisResults analyzeWorldspace(const std::string& cellIdPrefix = "");
        
        /// Analyze terrain height patterns
        void analyzeTerrainHeights(AnalysisResults& results, const std::string& cellIdPrefix);
        
        /// Analyze terrain texture distribution
        void analyzeTerrainTextures(AnalysisResults& results, const std::string& cellIdPrefix);
        
        /// Analyze object placement patterns
        void analyzeObjectPlacement(AnalysisResults& results, const std::string& cellIdPrefix);
        
        /// Analyze region definitions
        void analyzeRegions(AnalysisResults& results);
        
        /// Extract terrain parameters that best match analyzed patterns
        TerrainParams deriveTerrainParams(const AnalysisResults& results) const;
        
        /// Extract object placement parameters that best match analyzed patterns  
        ObjectPlacementParams deriveObjectParams(const AnalysisResults& results) const;
        
    private:
        CSMWorld::Data& mData;
        
        /// Get exterior cell coordinates from cell ID
        bool parseCellCoords(const std::string& cellId, int& x, int& y) const;
        
        /// Calculate terrain roughness from height data
        float calculateRoughness(const std::vector<float>& heights, int width, int height) const;
        
        /// Calculate standard deviation
        static float calculateStdDev(const std::vector<float>& values, float mean);
    };
}

#endif
