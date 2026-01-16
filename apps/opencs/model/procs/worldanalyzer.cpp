#include "worldanalyzer.hpp"

#include <algorithm>
#include <cmath>
#include <regex>
#include <set>

#include <apps/opencs/model/world/data.hpp>
#include <apps/opencs/model/world/idcollection.hpp>
#include <apps/opencs/model/world/land.hpp>
#include <apps/opencs/model/world/ref.hpp>
#include <apps/opencs/model/world/refcollection.hpp>
#include <apps/opencs/model/world/refidcollection.hpp>

#include <components/esm3/loadland.hpp>
#include <components/esm3/loadregn.hpp>

namespace CSMProcs
{
    WorldAnalyzer::WorldAnalyzer(CSMWorld::Data& data)
        : mData(data)
    {
    }
    
    bool WorldAnalyzer::parseCellCoords(const std::string& cellId, int& x, int& y) const
    {
        // Cell IDs for exterior cells are in format "#x,y"
        std::regex pattern(R"(#(-?\d+),\s*(-?\d+))");
        std::smatch match;
        
        if (std::regex_match(cellId, match, pattern))
        {
            x = std::stoi(match[1].str());
            y = std::stoi(match[2].str());
            return true;
        }
        return false;
    }
    
    bool WorldAnalyzer::parseCoordinateRange(const std::string& input, int& minX, int& minY, int& maxX, int& maxY) const
    {
        if (input.empty())
            return false;
        
        // Try to parse range format: "#x1,y1 to #x2,y2" or "#x1, y1 to #x2, y2"
        std::regex rangePattern(R"(#(-?\d+)\s*,\s*(-?\d+)\s+to\s+#(-?\d+)\s*,\s*(-?\d+))");
        std::smatch match;
        
        if (std::regex_match(input, match, rangePattern))
        {
            int x1 = std::stoi(match[1].str());
            int y1 = std::stoi(match[2].str());
            int x2 = std::stoi(match[3].str());
            int y2 = std::stoi(match[4].str());
            
            // Ensure min <= max
            minX = std::min(x1, x2);
            maxX = std::max(x1, x2);
            minY = std::min(y1, y2);
            maxY = std::max(y1, y2);
            
            return true;
        }
        
        // Try single cell format: "#x,y"
        std::regex singlePattern(R"(#(-?\d+)\s*,\s*(-?\d+))");
        if (std::regex_match(input, match, singlePattern))
        {
            minX = maxX = std::stoi(match[1].str());
            minY = maxY = std::stoi(match[2].str());
            return true;
        }
        
        // If input is just "#", include all exterior cells
        if (input == "#")
        {
            // Use a very large range to include all cells
            minX = minY = -10000;
            maxX = maxY = 10000;
            return true;
        }
        
        return false;
    }
    
    bool WorldAnalyzer::matchesCellFilter(int cellX, int cellY, const std::string& cellIdPrefix,
                                          bool hasRange, int minX, int minY, int maxX, int maxY) const
    {
        if (cellIdPrefix.empty())
            return true;
        
        // If a coordinate range was parsed, use it
        if (hasRange)
        {
            return cellX >= minX && cellX <= maxX && cellY >= minY && cellY <= maxY;
        }
        
        // Otherwise, fall back to substring matching (original behavior)
        std::string cellId = "#" + std::to_string(cellX) + "," + std::to_string(cellY);
        return cellId.find(cellIdPrefix) != std::string::npos;
    }
    
    float WorldAnalyzer::calculateRoughness(const std::vector<float>& heights, int width, int height) const
    {
        if (heights.empty() || width < 2 || height < 2)
            return 0.0f;
            
        float totalGradient = 0.0f;
        int count = 0;
        
        for (int y = 0; y < height - 1; ++y)
        {
            for (int x = 0; x < width - 1; ++x)
            {
                float h = heights[y * width + x];
                float hRight = heights[y * width + x + 1];
                float hDown = heights[(y + 1) * width + x];
                
                float dx = hRight - h;
                float dy = hDown - h;
                totalGradient += std::sqrt(dx * dx + dy * dy);
                ++count;
            }
        }
        
        return count > 0 ? totalGradient / count : 0.0f;
    }
    
    float WorldAnalyzer::calculateStdDev(const std::vector<float>& values, float mean)
    {
        if (values.empty())
            return 0.0f;
            
        float sumSqDiff = 0.0f;
        for (float v : values)
        {
            float diff = v - mean;
            sumSqDiff += diff * diff;
        }
        
        return std::sqrt(sumSqDiff / values.size());
    }
    
    AnalysisResults WorldAnalyzer::analyzeWorldspace(const std::string& cellIdPrefix)
    {
        AnalysisResults results;
        
        analyzeTerrainHeights(results, cellIdPrefix);
        analyzeTerrainTextures(results, cellIdPrefix);
        analyzeObjectPlacement(results, cellIdPrefix);
        analyzeRegions(results);
        
        results.isValid = true;
        return results;
    }
    
    void WorldAnalyzer::analyzeTerrainHeights(AnalysisResults& results, const std::string& cellIdPrefix)
    {
        const auto& landCollection = mData.getLand();
        
        // Parse coordinate range if provided
        int minX, minY, maxX, maxY;
        bool hasRange = parseCoordinateRange(cellIdPrefix, minX, minY, maxX, maxY);
        
        std::vector<float> allHeights;
        std::vector<float> roughnessValues;
        
        for (int i = 0; i < landCollection.getSize(); ++i)
        {
            const auto& record = landCollection.getRecord(i);
            if (record.isDeleted())
                continue;
                
            const CSMWorld::Land& land = record.get();
            std::string landId = CSMWorld::Land::createUniqueRecordId(land.mX, land.mY);
            
            // Check cell filter
            if (!matchesCellFilter(land.mX, land.mY, cellIdPrefix, hasRange, minX, minY, maxX, maxY))
                continue;
            
            // Load land data if available
            if (land.mDataTypes & ESM::Land::DATA_VHGT)
            {
                const ESM::Land::LandData* landData = land.getLandData(ESM::Land::DATA_VHGT);
                if (landData)
                {
                    std::vector<float> cellHeights;
                    cellHeights.reserve(ESM::Land::LAND_NUM_VERTS);
                    
                    for (int v = 0; v < ESM::Land::LAND_NUM_VERTS; ++v)
                    {
                        float h = landData->mHeights[v];
                        allHeights.push_back(h);
                        cellHeights.push_back(h);
                    }
                    
                    float roughness = calculateRoughness(cellHeights, ESM::Land::LAND_SIZE, ESM::Land::LAND_SIZE);
                    roughnessValues.push_back(roughness);
                }
            }
        }
        
        // Calculate statistics
        if (!allHeights.empty())
        {
            results.minHeight = *std::min_element(allHeights.begin(), allHeights.end());
            results.maxHeight = *std::max_element(allHeights.begin(), allHeights.end());
            
            float sum = 0.0f;
            for (float h : allHeights)
                sum += h;
            results.avgHeight = sum / allHeights.size();
            
            results.heightStdDev = calculateStdDev(allHeights, results.avgHeight);
        }
        
        if (!roughnessValues.empty())
        {
            float sum = 0.0f;
            for (float r : roughnessValues)
                sum += r;
            results.avgRoughness = sum / roughnessValues.size();
        }
    }
    
    void WorldAnalyzer::analyzeTerrainTextures(AnalysisResults& results, const std::string& cellIdPrefix)
    {
        const auto& landCollection = mData.getLand();
        const auto& ltexCollection = mData.getLandTextures();
        
        // Parse coordinate range if provided
        int minX, minY, maxX, maxY;
        bool hasRange = parseCoordinateRange(cellIdPrefix, minX, minY, maxX, maxY);
        
        std::map<int, int> textureUsageCount;
        int totalTextures = 0;
        
        for (int i = 0; i < landCollection.getSize(); ++i)
        {
            const auto& record = landCollection.getRecord(i);
            if (record.isDeleted())
                continue;
                
            const CSMWorld::Land& land = record.get();
            
            // Check cell filter
            if (!matchesCellFilter(land.mX, land.mY, cellIdPrefix, hasRange, minX, minY, maxX, maxY))
                continue;
            
            // Load texture data if available
            if (land.mDataTypes & ESM::Land::DATA_VTEX)
            {
                const ESM::Land::LandData* landData = land.getLandData(ESM::Land::DATA_VTEX);
                if (landData)
                {
                    for (int t = 0; t < ESM::Land::LAND_NUM_TEXTURES; ++t)
                    {
                        int texIdx = landData->mTextures[t];
                        textureUsageCount[texIdx]++;
                        ++totalTextures;
                    }
                }
            }
        }
        
        // Convert to frequency map with texture names
        if (totalTextures > 0)
        {
            for (const auto& pair : textureUsageCount)
            {
                int texIdx = pair.first;
                int count = pair.second;
                
                // Try to find texture name from index
                std::string texName = "texture_" + std::to_string(texIdx);
                
                // Search for matching land texture
                for (int i = 0; i < ltexCollection.getSize(); ++i)
                {
                    const auto& ltexRecord = ltexCollection.getRecord(i);
                    if (!ltexRecord.isDeleted())
                    {
                        const ESM::LandTexture& ltex = ltexRecord.get();
                        if (ltex.mIndex == texIdx)
                        {
                            texName = ltex.mTexture;
                            break;
                        }
                    }
                }
                
                results.textureFrequency[texName] = static_cast<float>(count) / totalTextures;
            }
        }
    }
    
    void WorldAnalyzer::analyzeObjectPlacement(AnalysisResults& results, const std::string& cellIdPrefix)
    {
        const auto& refs = mData.getReferences();
        const auto& cells = mData.getCells();
        
        // Parse coordinate range if provided
        int minX, minY, maxX, maxY;
        bool hasRange = parseCoordinateRange(cellIdPrefix, minX, minY, maxX, maxY);
        
        // Count objects by type
        std::map<std::string, int> objectCounts;
        std::vector<std::pair<float, float>> allPositions;
        int totalObjects = 0;
        
        // Determine which cells are exterior and match prefix
        std::set<std::string> validCells;
        for (int i = 0; i < cells.getSize(); ++i)
        {
            const auto& record = cells.getRecord(i);
            if (record.isDeleted())
                continue;
                
            const CSMWorld::Cell& cell = record.get();
            
            // Only exterior cells
            if (!cell.isExterior())
                continue;
            
            // Parse cell coordinates and check filter
            std::string cellId = cell.mId.getRefIdString();
            int cellX, cellY;
            if (parseCellCoords(cellId, cellX, cellY))
            {
                if (matchesCellFilter(cellX, cellY, cellIdPrefix, hasRange, minX, minY, maxX, maxY))
                {
                    validCells.insert(cellId);
                }
            }
        }
        
        // Analyze references in valid cells
        for (int i = 0; i < refs.getSize(); ++i)
        {
            const auto& record = refs.getRecord(i);
            if (record.isDeleted())
                continue;
                
            const CSMWorld::CellRef& ref = record.get();
            std::string cellId = ref.mCell.getRefIdString();
            
            if (validCells.find(cellId) == validCells.end())
                continue;
                
            // Get object type from refId
            std::string refId = ref.mRefID.getRefIdString();
            
            // Categorize by prefix pattern
            std::string objectType = "misc";
            if (refId.find("tree") != std::string::npos || refId.find("flora") != std::string::npos)
                objectType = "tree";
            else if (refId.find("rock") != std::string::npos || refId.find("stone") != std::string::npos)
                objectType = "rock";
            else if (refId.find("grass") != std::string::npos || refId.find("plant") != std::string::npos)
                objectType = "grass";
            else if (refId.find("house") != std::string::npos || refId.find("build") != std::string::npos)
                objectType = "building";
            else if (refId.find("furn") != std::string::npos)
                objectType = "furniture";
            else if (refId.find("light") != std::string::npos)
                objectType = "light";
            else if (refId.find("contain") != std::string::npos || refId.find("chest") != std::string::npos)
                objectType = "container";
            else if (refId.find("door") != std::string::npos)
                objectType = "door";
                
            objectCounts[objectType]++;
            ++totalObjects;
            
            // Store position for spacing analysis
            allPositions.emplace_back(ref.mPos.pos[0], ref.mPos.pos[1]);
        }
        
        // Calculate density by type
        float worldArea = validCells.size() * ESM::Land::REAL_SIZE * ESM::Land::REAL_SIZE;
        if (worldArea > 0)
        {
            for (const auto& pair : objectCounts)
            {
                const std::string& type = pair.first;
                int count = pair.second;
                results.objectDensityByType[type] = static_cast<float>(count) / worldArea * 1000000.0f; // per million sq units
            }
        }
        
        // Calculate average spacing between objects
        if (allPositions.size() > 1)
        {
            float totalMinDist = 0.0f;
            int spacingCount = 0;
            
            // Sample spacing calculation (full O(n^2) would be too slow)
            int sampleSize = std::min(1000, static_cast<int>(allPositions.size()));
            for (int i = 0; i < sampleSize; ++i)
            {
                float minDist = std::numeric_limits<float>::max();
                
                for (int j = 0; j < static_cast<int>(allPositions.size()); ++j)
                {
                    if (i != j)
                    {
                        float dx = allPositions[i].first - allPositions[j].first;
                        float dy = allPositions[i].second - allPositions[j].second;
                        float dist = std::sqrt(dx * dx + dy * dy);
                        minDist = std::min(minDist, dist);
                    }
                }
                
                if (minDist < std::numeric_limits<float>::max())
                {
                    totalMinDist += minDist;
                    ++spacingCount;
                }
            }
            
            if (spacingCount > 0)
            {
                results.avgObjectSpacing = totalMinDist / spacingCount;
            }
        }
    }
    
    void WorldAnalyzer::analyzeRegions(AnalysisResults& results)
    {
        // Region analysis - extract patterns from region definitions
        // This can be expanded to capture more region-specific data
    }
    
    TerrainParams WorldAnalyzer::deriveTerrainParams(const AnalysisResults& results) const
    {
        TerrainParams params;
        
        params.baseHeight = results.avgHeight;
        params.heightVariation = results.heightStdDev * 3.0f; // Cover 99% of variation
        params.roughness = std::clamp(results.avgRoughness / 100.0f, 0.0f, 1.0f);
        
        // Derive generation parameters from patterns
        if (results.maxHeight - results.minHeight > 0)
        {
            // Mountain frequency based on how much terrain varies
            params.mountainFrequency = std::clamp(
                results.heightStdDev / (results.maxHeight - results.minHeight),
                0.05f, 0.5f);
                
            // Valley depth based on height distribution
            params.valleyDepth = std::clamp(
                (results.avgHeight - results.minHeight) / (results.maxHeight - results.minHeight),
                0.1f, 0.5f);
        }
        
        return params;
    }
    
    ObjectPlacementParams WorldAnalyzer::deriveObjectParams(const AnalysisResults& results) const
    {
        ObjectPlacementParams params;
        
        // Extract densities from analysis
        auto findDensity = [&results](const std::string& type) -> float {
            auto it = results.objectDensityByType.find(type);
            return it != results.objectDensityByType.end() ? it->second : 0.0f;
        };
        
        params.treeDensity = std::clamp(findDensity("tree") * 0.1f, 0.0f, 1.0f);
        params.rockDensity = std::clamp(findDensity("rock") * 0.1f, 0.0f, 1.0f);
        params.grassDensity = std::clamp(findDensity("grass") * 0.1f, 0.0f, 1.0f);
        params.buildingDensity = std::clamp(findDensity("building") * 0.1f, 0.0f, 0.3f);
        
        // Use analyzed spacing
        if (results.avgObjectSpacing > 0)
        {
            params.minSpacing = results.avgObjectSpacing * 0.8f;
        }
        
        return params;
    }
}
