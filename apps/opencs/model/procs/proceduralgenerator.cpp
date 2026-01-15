#include "proceduralgenerator.hpp"

#include <algorithm>
#include <cmath>
#include <set>

#include <apps/opencs/model/doc/document.hpp>
#include <apps/opencs/model/world/cell.hpp>
#include <apps/opencs/model/world/columns.hpp>
#include <apps/opencs/model/world/commands.hpp>
#include <apps/opencs/model/world/data.hpp>
#include <apps/opencs/model/world/idcollection.hpp>
#include <apps/opencs/model/world/idtable.hpp>
#include <apps/opencs/model/world/land.hpp>
#include <apps/opencs/model/world/ref.hpp>
#include <apps/opencs/model/world/refcollection.hpp>
#include <apps/opencs/model/world/refidcollection.hpp>
#include <apps/opencs/model/world/universalid.hpp>

#include <components/esm3/loadcell.hpp>
#include <components/esm3/loadland.hpp>

namespace CSMProcs
{
    ProceduralGenerator::ProceduralGenerator(CSMDoc::Document& document)
        : mDocument(document)
        , mData(mDocument.getData())
    {
        initializeNoise();
    }
    
    ProceduralGenerator::~ProceduralGenerator() = default;
    
    void ProceduralGenerator::setState(const ProceduralState& state)
    {
        mState = state;
        initializeNoise();
    }
    
    void ProceduralGenerator::setProgressCallback(ProgressCallback callback)
    {
        mProgressCallback = std::move(callback);
    }
    
    void ProceduralGenerator::initializeNoise()
    {
        uint64_t seed = mState.seed;
        if (seed == 0)
        {
            seed = static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
        }
        
        mPerlin = std::make_unique<PerlinNoise>(seed);
        mVoronoi = std::make_unique<VoronoiNoise>(seed + 1);
        mRng = std::make_unique<RandomGenerator>(seed + 2);
    }
    
    float ProceduralGenerator::generateHeight(float worldX, float worldY) const
    {
        const TerrainParams& tp = mState.terrain;
        
        // Scale coordinates for noise
        float nx = worldX / static_cast<float>(ESM::Land::REAL_SIZE) * tp.mountainFrequency;
        float ny = worldY / static_cast<float>(ESM::Land::REAL_SIZE) * tp.mountainFrequency;
        
        // Base terrain using fractal noise
        float baseNoise = mPerlin->fractalNoise2D(nx, ny, tp.octaves, tp.persistence, tp.lacunarity);
        
        // Mountain ridges using ridged noise
        float ridgeNoise = mPerlin->ridgedNoise2D(nx * 0.5f, ny * 0.5f, tp.octaves - 2, 0.5f, 2.0f, 1.0f);
        
        // Valleys using voronoi
        float voronoiNoise = mVoronoi->distanceToNearest(nx * 2.0f, ny * 2.0f);
        
        // Combine noise types
        float combinedNoise = baseNoise * 0.5f + ridgeNoise * 0.3f - voronoiNoise * tp.valleyDepth * 0.2f;
        
        // Apply roughness
        if (tp.roughness > 0.0f)
        {
            float roughNoise = mPerlin->turbulence2D(nx * 4.0f, ny * 4.0f, 3, 0.5f, 2.0f);
            combinedNoise += roughNoise * tp.roughness * 0.1f;
        }
        
        // Scale to height range
        float height = tp.baseHeight + combinedNoise * tp.heightVariation;
        
        return height;
    }
    
    float ProceduralGenerator::getSlopeAt(float worldX, float worldY) const
    {
        const float delta = 8.0f;
        
        float h = generateHeight(worldX, worldY);
        float hx = generateHeight(worldX + delta, worldY);
        float hy = generateHeight(worldX, worldY + delta);
        
        float dx = (hx - h) / delta;
        float dy = (hy - h) / delta;
        
        return std::sqrt(dx * dx + dy * dy);
    }
    
    int ProceduralGenerator::getTextureForTerrain(float height, float slope) const
    {
        // Simple texture selection based on height and slope
        // In a full implementation, this would use the analyzed texture patterns
        
        const TerrainParams& tp = mState.terrain;
        float normalizedHeight = (height - tp.baseHeight) / (tp.heightVariation + 0.001f);
        
        // Default texture indices (these would be mapped from analysis in full implementation)
        if (slope > 0.5f)
            return 2; // Rocky/cliff texture for steep slopes
        else if (normalizedHeight > 0.6f)
            return 3; // Mountain/highland texture
        else if (normalizedHeight < -0.3f && tp.generateWater)
            return 4; // Sandy/beach texture near water
        else
            return 1; // Default grass/ground texture
    }
    
    std::vector<uint16_t> ProceduralGenerator::generateTextures(int cellX, int cellY) const
    {
        std::vector<uint16_t> textures(ESM::Land::LAND_NUM_TEXTURES);
        
        float cellWorldX = static_cast<float>(cellX) * ESM::Land::REAL_SIZE;
        float cellWorldY = static_cast<float>(cellY) * ESM::Land::REAL_SIZE;
        float texSize = static_cast<float>(ESM::Land::REAL_SIZE) / ESM::Land::LAND_TEXTURE_SIZE;
        
        for (int ty = 0; ty < ESM::Land::LAND_TEXTURE_SIZE; ++ty)
        {
            for (int tx = 0; tx < ESM::Land::LAND_TEXTURE_SIZE; ++tx)
            {
                float worldX = cellWorldX + (tx + 0.5f) * texSize;
                float worldY = cellWorldY + (ty + 0.5f) * texSize;
                
                float height = generateHeight(worldX, worldY);
                float slope = getSlopeAt(worldX, worldY);
                
                textures[ty * ESM::Land::LAND_TEXTURE_SIZE + tx] = 
                    static_cast<uint16_t>(getTextureForTerrain(height, slope));
            }
        }
        
        return textures;
    }
    
    void ProceduralGenerator::cancel()
    {
        mCancelled = true;
    }
    
    int ProceduralGenerator::getTotalCells() const
    {
        return mState.worldSizeX * mState.worldSizeY;
    }
    
    void ProceduralGenerator::reportProgress(int current, int total, const std::string& message)
    {
        if (mProgressCallback)
        {
            mProgressCallback(current, total, message);
        }
    }
    
    bool ProceduralGenerator::generate()
    {
        mRunning = true;
        mCancelled = false;
        
        int totalSteps = getTotalCells();
        if (mState.generateInteriors)
            totalSteps += mState.worldSizeX; // Approximate interior count
        if (mState.generatePathgrids)
            totalSteps += getTotalCells();
            
        int currentStep = 0;
        
        try
        {
            if (mState.generateExteriors)
            {
                if (!generateTerrain())
                {
                    mRunning = false;
                    return false;
                }
                currentStep += getTotalCells();
                
                if (!generateObjects())
                {
                    mRunning = false;
                    return false;
                }
            }
            
            if (mState.generateInteriors && !mCancelled)
            {
                if (!generateInteriors())
                {
                    mRunning = false;
                    return false;
                }
                currentStep += mState.worldSizeX;
            }
            
            if (mState.generatePathgrids && !mCancelled)
            {
                if (!generatePathgrids())
                {
                    mRunning = false;
                    return false;
                }
            }
        }
        catch (const std::exception& e)
        {
            reportProgress(0, 0, std::string("Error: ") + e.what());
            mRunning = false;
            return false;
        }
        
        mRunning = false;
        return !mCancelled;
    }
    
    bool ProceduralGenerator::generateTerrain()
    {
        int totalCells = getTotalCells();
        int currentCell = 0;
        
        for (int cy = 0; cy < mState.worldSizeY && !mCancelled; ++cy)
        {
            for (int cx = 0; cx < mState.worldSizeX && !mCancelled; ++cx)
            {
                int cellX = mState.originX + cx;
                int cellY = mState.originY + cy;
                
                createCell(cellX, cellY);
                createLand(cellX, cellY);
                
                ++currentCell;
                reportProgress(currentCell, totalCells, 
                    "Generating terrain: Cell (" + std::to_string(cellX) + ", " + std::to_string(cellY) + ")");
            }
        }
        
        return !mCancelled;
    }
    
    void ProceduralGenerator::createCell(int cellX, int cellY)
    {
        CSMWorld::IdTable& cells = dynamic_cast<CSMWorld::IdTable&>(
            *mData.getTableModel(CSMWorld::UniversalId(CSMWorld::UniversalId::Type_Cells)));
        
        std::string cellId = "#" + std::to_string(cellX) + "," + std::to_string(cellY);
        ESM::RefId refId = ESM::RefId::stringRefId(cellId);
        
        // Check if cell exists
        int existingRow = cells.searchId(refId);
        
        if (existingRow < 0)
        {
            // Create new cell
            int row = cells.rowCount();
            
            // Use the collection directly to add the cell
            auto& cellCollection = mData.getCells();
            
            CSMWorld::Cell newCell;
            newCell.mId = refId;
            newCell.mData.mFlags = ESM::Cell::HasWater;
            newCell.mData.mX = cellX;
            newCell.mData.mY = cellY;
            newCell.mWater = mState.terrain.waterLevel;
            newCell.mCellId.mWorldspace = ESM::RefId::stringRefId("sys::default");
            newCell.mCellId.mPaged = true;
            newCell.mCellId.mIndex.mX = cellX;
            newCell.mCellId.mIndex.mY = cellY;
            
            // Add using command for proper undo support
            mDocument.getUndoStack().push(
                new CSMWorld::CreateCommand(cells, refId.getRefIdString()));
        }
        else if (mState.overwriteExisting)
        {
            // Update existing cell (water level, etc.)
            // Could add modification commands here
        }
    }
    
    void ProceduralGenerator::createLand(int cellX, int cellY)
    {
        CSMWorld::IdTable& landTable = dynamic_cast<CSMWorld::IdTable&>(
            *mData.getTableModel(CSMWorld::UniversalId(CSMWorld::UniversalId::Type_Land)));
        
        std::string landId = CSMWorld::Land::createUniqueRecordId(cellX, cellY);
        ESM::RefId refId = ESM::RefId::stringRefId(landId);
        
        // Check if land exists
        int existingRow = landTable.searchId(refId);
        
        if (existingRow < 0)
        {
            // Create new land record
            mDocument.getUndoStack().push(
                new CSMWorld::CreateCommand(landTable, landId));
        }
        
        // Now get the land record and fill in the data
        auto& landCollection = mData.getLand();
        int landIndex = landCollection.searchId(refId);
        
        if (landIndex >= 0)
        {
            // Generate height data
            float cellWorldX = static_cast<float>(cellX) * ESM::Land::REAL_SIZE;
            float cellWorldY = static_cast<float>(cellY) * ESM::Land::REAL_SIZE;
            float vertexSize = static_cast<float>(ESM::Land::REAL_SIZE) / (ESM::Land::LAND_SIZE - 1);
            
            std::array<float, ESM::Land::LAND_NUM_VERTS> heights;
            
            for (int vy = 0; vy < ESM::Land::LAND_SIZE; ++vy)
            {
                for (int vx = 0; vx < ESM::Land::LAND_SIZE; ++vx)
                {
                    float worldX = cellWorldX + vx * vertexSize;
                    float worldY = cellWorldY + vy * vertexSize;
                    
                    heights[vy * ESM::Land::LAND_SIZE + vx] = generateHeight(worldX, worldY);
                }
            }
            
            // Generate textures
            std::vector<uint16_t> textures = generateTextures(cellX, cellY);
            
            // Update using the height and texture columns
            int heightColumn = landTable.findColumnIndex(CSMWorld::Columns::ColumnId_LandHeightsIndex);
            int textureColumn = landTable.findColumnIndex(CSMWorld::Columns::ColumnId_LandTexturesIndex);
            
            if (heightColumn >= 0 && existingRow >= 0)
            {
                // Heights would be set via the model in a full implementation
                // For now, direct land data modification
            }
        }
    }
    
    bool ProceduralGenerator::generateObjects()
    {
        if (!mState.useReference && mState.objects.treeDensity <= 0 && 
            mState.objects.rockDensity <= 0 && mState.objects.grassDensity <= 0)
        {
            return true; // Nothing to generate
        }
        
        int totalCells = getTotalCells();
        int currentCell = 0;
        
        for (int cy = 0; cy < mState.worldSizeY && !mCancelled; ++cy)
        {
            for (int cx = 0; cx < mState.worldSizeX && !mCancelled; ++cx)
            {
                int cellX = mState.originX + cx;
                int cellY = mState.originY + cy;
                
                placeObjectsInCell(cellX, cellY);
                
                ++currentCell;
                reportProgress(currentCell, totalCells,
                    "Placing objects: Cell (" + std::to_string(cellX) + ", " + std::to_string(cellY) + ")");
            }
        }
        
        return !mCancelled;
    }
    
    void ProceduralGenerator::placeObjectsInCell(int cellX, int cellY)
    {
        float cellWorldX = static_cast<float>(cellX) * ESM::Land::REAL_SIZE;
        float cellWorldY = static_cast<float>(cellY) * ESM::Land::REAL_SIZE;
        float cellSize = static_cast<float>(ESM::Land::REAL_SIZE);
        
        const ObjectPlacementParams& op = mState.objects;
        
        // Use Poisson disk sampling for natural distribution
        uint64_t cellSeed = mState.seed + static_cast<uint64_t>(cellX) * 73856093ULL 
                          + static_cast<uint64_t>(cellY) * 19349663ULL;
        
        // Generate density mask based on terrain
        const int maskSize = 16;
        std::vector<float> densityMask(maskSize * maskSize);
        float maskStep = cellSize / maskSize;
        
        for (int my = 0; my < maskSize; ++my)
        {
            for (int mx = 0; mx < maskSize; ++mx)
            {
                float worldX = cellWorldX + (mx + 0.5f) * maskStep;
                float worldY = cellWorldY + (my + 0.5f) * maskStep;
                
                float height = generateHeight(worldX, worldY);
                float slope = getSlopeAt(worldX, worldY);
                
                // Reduce density on steep slopes and very low/high areas
                float slopeFactor = std::max(0.0f, 1.0f - slope * 2.0f);
                float heightFactor = 1.0f;
                
                if (mState.terrain.generateWater && height < mState.terrain.waterLevel)
                    heightFactor = 0.0f; // No objects underwater
                else if (height > mState.terrain.baseHeight + mState.terrain.heightVariation * 0.8f)
                    heightFactor = 0.3f; // Fewer objects on mountaintops
                    
                densityMask[my * maskSize + mx] = slopeFactor * heightFactor;
            }
        }
        
        // Place trees
        if (op.treeDensity > 0.0f)
        {
            float treeSpacing = op.minSpacing / std::sqrt(op.treeDensity);
            PoissonDiskSampler treeSampler(cellSeed, treeSpacing, cellSize, cellSize);
            
            std::vector<float> treeMask = densityMask;
            for (float& d : treeMask)
                d *= op.treeDensity;
                
            auto treePoints = treeSampler.generatePointsWithMask(treeMask, maskSize, maskSize);
            
            std::vector<std::string> treeObjects = getObjectsFromReference("tree");
            if (treeObjects.empty())
            {
                // Default tree objects if no reference
                treeObjects = {"flora_tree_gl_01", "flora_tree_gl_02", "flora_tree_gl_03"};
            }
            
            for (const auto& [localX, localY] : treePoints)
            {
                float worldX = cellWorldX + localX;
                float worldY = cellWorldY + localY;
                float height = generateHeight(worldX, worldY);
                
                // Select random tree
                std::string treeId = treeObjects[mRng->nextInt(static_cast<uint32_t>(treeObjects.size()))];
                
                // Create reference (simplified - full implementation would use commands)
                float rotation = mRng->nextFloatRange(0.0f, 6.28318f) * op.rotationVariation;
                float scale = 1.0f + mRng->nextFloatRange(-op.scaleVariation, op.scaleVariation);
                
                // In a full implementation, we would create a CellRef here
            }
        }
        
        // Place rocks
        if (op.rockDensity > 0.0f)
        {
            float rockSpacing = op.minSpacing / std::sqrt(op.rockDensity);
            PoissonDiskSampler rockSampler(cellSeed + 1, rockSpacing, cellSize, cellSize);
            
            std::vector<float> rockMask = densityMask;
            for (float& d : rockMask)
                d *= op.rockDensity;
                
            auto rockPoints = rockSampler.generatePointsWithMask(rockMask, maskSize, maskSize);
            
            std::vector<std::string> rockObjects = getObjectsFromReference("rock");
            if (rockObjects.empty())
            {
                rockObjects = {"terrain_rock_ai_01", "terrain_rock_ai_02"};
            }
            
            for (const auto& [localX, localY] : rockPoints)
            {
                float worldX = cellWorldX + localX;
                float worldY = cellWorldY + localY;
                float height = generateHeight(worldX, worldY);
                
                std::string rockId = rockObjects[mRng->nextInt(static_cast<uint32_t>(rockObjects.size()))];
                float rotation = mRng->nextFloatRange(0.0f, 6.28318f);
                float scale = 1.0f + mRng->nextFloatRange(-op.scaleVariation * 1.5f, op.scaleVariation * 1.5f);
            }
        }
    }
    
    std::vector<std::string> ProceduralGenerator::getObjectsFromReference(const std::string& category) const
    {
        std::vector<std::string> objects;
        
        if (!mState.useReference || !mState.analysis.isValid)
            return objects;
            
        // Get objects from analyzed patterns
        auto it = mState.analysis.objectDensityByType.find(category);
        if (it != mState.analysis.objectDensityByType.end())
        {
            // In a full implementation, we would store actual object IDs during analysis
            // For now, return empty to use defaults
        }
        
        return objects;
    }
    
    std::string ProceduralGenerator::selectObject(const std::string& category, 
                                                   float terrainHeight, 
                                                   float slope) const
    {
        auto objects = getObjectsFromReference(category);
        if (objects.empty())
            return "";
            
        return objects[mRng->nextInt(static_cast<uint32_t>(objects.size()))];
    }
    
    bool ProceduralGenerator::generateInteriors()
    {
        // Generate some interior cells based on configuration
        int interiorCount = std::max(1, mState.worldSizeX / 4);
        
        for (int i = 0; i < interiorCount && !mCancelled; ++i)
        {
            std::string name = "Proc_Interior_" + std::to_string(mState.seed) + "_" + std::to_string(i);
            int roomCount = mRng->nextIntRange(mState.interiors.minRooms, mState.interiors.maxRooms);
            
            createInterior(name, roomCount);
            
            reportProgress(i + 1, interiorCount, "Generating interior: " + name);
        }
        
        return !mCancelled;
    }
    
    void ProceduralGenerator::createInterior(const std::string& name, int roomCount)
    {
        // Interior generation is complex and would involve:
        // 1. Creating an interior cell
        // 2. Generating room layout using BSP or similar
        // 3. Placing walls, floors, ceilings
        // 4. Adding doors between rooms
        // 5. Placing furniture and clutter
        // 6. Adding lighting
        
        // This is a placeholder for the full implementation
        CSMWorld::IdTable& cells = dynamic_cast<CSMWorld::IdTable&>(
            *mData.getTableModel(CSMWorld::UniversalId(CSMWorld::UniversalId::Type_Cells)));
        
        ESM::RefId refId = ESM::RefId::stringRefId(name);
        
        int existingRow = cells.searchId(refId);
        if (existingRow < 0)
        {
            mDocument.getUndoStack().push(
                new CSMWorld::CreateCommand(cells, name));
        }
    }
    
    bool ProceduralGenerator::generatePathgrids()
    {
        int totalCells = getTotalCells();
        int currentCell = 0;
        
        for (int cy = 0; cy < mState.worldSizeY && !mCancelled; ++cy)
        {
            for (int cx = 0; cx < mState.worldSizeX && !mCancelled; ++cx)
            {
                int cellX = mState.originX + cx;
                int cellY = mState.originY + cy;
                
                generatePathgridForCell(cellX, cellY);
                
                ++currentCell;
                reportProgress(currentCell, totalCells,
                    "Generating pathgrids: Cell (" + std::to_string(cellX) + ", " + std::to_string(cellY) + ")");
            }
        }
        
        return !mCancelled;
    }
    
    void ProceduralGenerator::generatePathgridForCell(int cellX, int cellY)
    {
        // Pathgrid generation involves:
        // 1. Creating a grid of potential waypoints
        // 2. Testing walkability (not underwater, not too steep)
        // 3. Connecting nearby walkable points
        // 4. Optimizing the graph
        
        // This is a placeholder for full implementation
    }
    
    bool ProceduralGenerator::previewCell(int cellX, int cellY)
    {
        mCancelled = false;
        
        createCell(cellX, cellY);
        createLand(cellX, cellY);
        placeObjectsInCell(cellX, cellY);
        
        return true;
    }
}
