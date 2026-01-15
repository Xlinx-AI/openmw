#include "proceduralgenerator.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <set>
#include <queue>

#include <QVariant>
#include <QUndoStack>

#include <apps/opencs/model/doc/document.hpp>
#include <apps/opencs/model/world/cell.hpp>
#include <apps/opencs/model/world/cellcoordinates.hpp>
#include <apps/opencs/model/world/columns.hpp>
#include <apps/opencs/model/world/columnimp.hpp>
#include <apps/opencs/model/world/commands.hpp>
#include <apps/opencs/model/world/data.hpp>
#include <apps/opencs/model/world/idcollection.hpp>
#include <apps/opencs/model/world/idtable.hpp>
#include <apps/opencs/model/world/idtree.hpp>
#include <apps/opencs/model/world/land.hpp>
#include <apps/opencs/model/world/pathgrid.hpp>
#include <apps/opencs/model/world/ref.hpp>
#include <apps/opencs/model/world/refcollection.hpp>
#include <apps/opencs/model/world/refidcollection.hpp>
#include <apps/opencs/model/world/universalid.hpp>

#include <components/esm3/loadcell.hpp>
#include <components/esm3/loadland.hpp>
#include <components/esm3/loadpgrd.hpp>

#include "assetlibrary.hpp"

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
            mState.seed = seed;
        }
        
        mPerlin = std::make_unique<PerlinNoise>(seed);
        mVoronoi = std::make_unique<VoronoiNoise>(seed + 1);
        mRng = std::make_unique<RandomGenerator>(seed + 2);
    }
    
    float ProceduralGenerator::generateHeight(float worldX, float worldY) const
    {
        const TerrainParams& tp = mState.terrain;
        
        // Normalize coordinates for noise sampling
        float scale = 1.0f / (ESM::Land::REAL_SIZE * 4.0f);
        float nx = worldX * scale * tp.mountainFrequency;
        float ny = worldY * scale * tp.mountainFrequency;
        
        // Multi-layered terrain generation
        // Layer 1: Continental base using low-frequency noise
        float continentalNoise = mPerlin->fractalNoise2D(nx * 0.25f, ny * 0.25f, 3, 0.5f, 2.0f);
        
        // Layer 2: Regional terrain using fractal noise
        float regionalNoise = mPerlin->fractalNoise2D(nx, ny, tp.octaves, tp.persistence, tp.lacunarity);
        
        // Layer 3: Mountain ridges using ridged multifractal
        float ridgeNoise = mPerlin->ridgedNoise2D(nx * 0.5f, ny * 0.5f, 
            std::max(1, tp.octaves - 2), 0.5f, 2.0f, 1.0f);
        
        // Layer 4: Valley carving using Voronoi
        float voronoiDist = mVoronoi->distanceToNearest(nx * 2.0f, ny * 2.0f);
        float valleyNoise = std::max(0.0f, 1.0f - voronoiDist * 2.0f);
        
        // Layer 5: Fine detail using high-frequency turbulence
        float detailNoise = mPerlin->turbulence2D(nx * 4.0f, ny * 4.0f, 3, 0.5f, 2.0f);
        
        // Combine layers with weights
        float combined = continentalNoise * 0.3f + 
                         regionalNoise * 0.35f + 
                         ridgeNoise * 0.25f - 
                         valleyNoise * tp.valleyDepth * 0.15f +
                         detailNoise * tp.roughness * 0.1f;
        
        // Apply erosion simulation (simplified thermal erosion)
        if (tp.erosionStrength > 0.0f)
        {
            float erosionNoise = mPerlin->fractalNoise2D(nx * 8.0f, ny * 8.0f, 2, 0.5f, 2.0f);
            combined -= std::abs(erosionNoise) * tp.erosionStrength * 0.1f;
        }
        
        // Scale to height range and add base
        float height = tp.baseHeight + combined * tp.heightVariation;
        
        // Clamp to valid range
        return std::clamp(height, -32768.0f, 32767.0f);
    }
    
    float ProceduralGenerator::getSlopeAt(float worldX, float worldY) const
    {
        const float delta = 32.0f; // Sample distance
        
        float h = generateHeight(worldX, worldY);
        float hx = generateHeight(worldX + delta, worldY);
        float hy = generateHeight(worldX, worldY + delta);
        
        float dx = (hx - h) / delta;
        float dy = (hy - h) / delta;
        
        return std::sqrt(dx * dx + dy * dy);
    }
    
    int ProceduralGenerator::getTextureForTerrain(float height, float slope) const
    {
        const TerrainParams& tp = mState.terrain;
        
        // Normalize height to 0-1 range
        float minH = tp.baseHeight - tp.heightVariation;
        float maxH = tp.baseHeight + tp.heightVariation;
        float range = maxH - minH;
        float normalizedHeight = (range > 0.01f) ? (height - minH) / range : 0.5f;
        
        // Texture selection based on height zones and slope
        // These indices correspond to typical Morrowind land textures
        // 0 = underwater sand, 1 = grass, 2 = rock/cliff, 3 = dirt, 4 = snow/mountain
        
        if (slope > 0.7f)
        {
            // Very steep - rocky cliff
            return 2;
        }
        else if (slope > 0.4f)
        {
            // Moderately steep - rocky/dirt mix
            return (normalizedHeight > 0.6f) ? 2 : 3;
        }
        else if (normalizedHeight < 0.15f)
        {
            // Low elevation near water
            return (tp.generateWater && height < tp.waterLevel + 50.0f) ? 0 : 1;
        }
        else if (normalizedHeight > 0.85f)
        {
            // High mountain
            return 4;
        }
        else if (normalizedHeight > 0.65f)
        {
            // Highland
            return 3;
        }
        else
        {
            // Normal terrain - grass
            return 1;
        }
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
                
                // Add some noise to texture boundaries for natural look
                float texNoise = mPerlin->noise2D(worldX * 0.01f, worldY * 0.01f) * 0.1f;
                
                textures[ty * ESM::Land::LAND_TEXTURE_SIZE + tx] = 
                    static_cast<uint16_t>(getTextureForTerrain(height + texNoise * 100.0f, slope));
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
        mGeneratedSettlements.clear();
        
        int totalSteps = 0;
        if (mState.generateExteriors)
            totalSteps += getTotalCells() * 2; // Terrain + Objects
        if (mState.generateSettlements)
            totalSteps += mState.settlement.settlementCount * 10; // Estimate
        if (mState.generateCavesAndDungeons)
            totalSteps += mState.caveDungeon.caveCount + mState.caveDungeon.dungeonCount;
        if (mState.generateInteriors)
            totalSteps += std::max(1, mState.worldSizeX / 4);
        if (mState.generatePathgrids)
            totalSteps += getTotalCells();
            
        try
        {
            if (mState.generateExteriors)
            {
                reportProgress(0, totalSteps, "Generating terrain...");
                if (!generateTerrain())
                {
                    mRunning = false;
                    return false;
                }
                
                reportProgress(getTotalCells(), totalSteps, "Placing objects...");
                if (!generateObjects())
                {
                    mRunning = false;
                    return false;
                }
            }
            
            if (mState.generateSettlements && !mCancelled)
            {
                int offset = mState.generateExteriors ? getTotalCells() * 2 : 0;
                reportProgress(offset, totalSteps, "Generating settlements...");
                if (!generateSettlements())
                {
                    mRunning = false;
                    return false;
                }
            }
            
            if (mState.generateCavesAndDungeons && !mCancelled)
            {
                int offset = (mState.generateExteriors ? getTotalCells() * 2 : 0) + 
                             (mState.generateSettlements ? mState.settlement.settlementCount * 10 : 0);
                reportProgress(offset, totalSteps, "Generating caves and dungeons...");
                if (!generateCavesAndDungeons())
                {
                    mRunning = false;
                    return false;
                }
            }
            
            if (mState.generateInteriors && !mCancelled)
            {
                int offset = (mState.generateExteriors ? getTotalCells() * 2 : 0) + 
                             (mState.generateSettlements ? mState.settlement.settlementCount * 10 : 0) +
                             (mState.generateCavesAndDungeons ? mState.caveDungeon.caveCount + mState.caveDungeon.dungeonCount : 0);
                reportProgress(offset, totalSteps, "Generating interiors...");
                if (!generateInteriors())
                {
                    mRunning = false;
                    return false;
                }
            }
            
            if (mState.generatePathgrids && !mCancelled)
            {
                int offset = (mState.generateExteriors ? getTotalCells() * 2 : 0) + 
                             (mState.generateSettlements ? mState.settlement.settlementCount * 10 : 0) +
                             (mState.generateCavesAndDungeons ? mState.caveDungeon.caveCount + mState.caveDungeon.dungeonCount : 0) +
                             (mState.generateInteriors ? std::max(1, mState.worldSizeX / 4) : 0);
                reportProgress(offset, totalSteps, "Generating pathgrids...");
                if (!generatePathgrids())
                {
                    mRunning = false;
                    return false;
                }
            }
            
            reportProgress(totalSteps, totalSteps, "Generation complete!");
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
        CSMWorld::IdTree& cellTable = dynamic_cast<CSMWorld::IdTree&>(
            *mData.getTableModel(CSMWorld::UniversalId(CSMWorld::UniversalId::Type_Cells)));
        
        std::string cellId = "#" + std::to_string(cellX) + ", " + std::to_string(cellY);
        ESM::RefId refId = ESM::RefId::stringRefId(cellId);
        
        // Check if cell already exists
        int existingRow = mData.getCells().searchId(refId);
        
        if (existingRow < 0)
        {
            // Create new exterior cell
            auto createCmd = std::make_unique<CSMWorld::CreateCommand>(cellTable, cellId);
            
            // Set cell as exterior (not interior)
            int parentIndex = cellTable.findColumnIndex(CSMWorld::Columns::ColumnId_Cell);
            int interiorIndex = cellTable.findNestedColumnIndex(parentIndex, CSMWorld::Columns::ColumnId_Interior);
            createCmd->addNestedValue(parentIndex, interiorIndex, false);
            
            // Set water level
            int waterColumn = cellTable.findColumnIndex(CSMWorld::Columns::ColumnId_WaterLevel);
            if (waterColumn >= 0)
            {
                createCmd->addValue(waterColumn, mState.terrain.waterLevel);
            }
            
            mDocument.getUndoStack().push(createCmd.release());
        }
        else if (mState.overwriteExisting)
        {
            // Update water level on existing cell
            int waterColumn = cellTable.findColumnIndex(CSMWorld::Columns::ColumnId_WaterLevel);
            if (waterColumn >= 0)
            {
                QModelIndex index = cellTable.getModelIndex(cellId, waterColumn);
                mDocument.getUndoStack().push(
                    new CSMWorld::ModifyCommand(cellTable, index, mState.terrain.waterLevel));
            }
        }
    }
    
    void ProceduralGenerator::createLand(int cellX, int cellY)
    {
        CSMWorld::IdTable& landTable = dynamic_cast<CSMWorld::IdTable&>(
            *mData.getTableModel(CSMWorld::UniversalId(CSMWorld::UniversalId::Type_Land)));
        CSMWorld::IdTable& ltexTable = dynamic_cast<CSMWorld::IdTable&>(
            *mData.getTableModel(CSMWorld::UniversalId(CSMWorld::UniversalId::Type_LandTextures)));
        
        std::string landId = CSMWorld::Land::createUniqueRecordId(cellX, cellY);
        ESM::RefId refId = ESM::RefId::stringRefId(landId);
        
        // Check if land exists
        int existingRow = mData.getLand().searchId(refId);
        
        if (existingRow < 0)
        {
            // Create new land record
            mDocument.getUndoStack().push(
                new CSMWorld::CreateCommand(landTable, landId));
            existingRow = mData.getLand().searchId(refId);
        }
        
        if (existingRow < 0)
            return; // Failed to create
        
        // Generate height data
        float cellWorldX = static_cast<float>(cellX) * ESM::Land::REAL_SIZE;
        float cellWorldY = static_cast<float>(cellY) * ESM::Land::REAL_SIZE;
        float vertexSize = static_cast<float>(ESM::Land::REAL_SIZE) / (ESM::Land::LAND_SIZE - 1);
        
        // Create height array
        CSMWorld::LandHeightsColumn::DataType heights;
        heights.resize(ESM::Land::LAND_NUM_VERTS);
        
        for (int vy = 0; vy < ESM::Land::LAND_SIZE; ++vy)
        {
            for (int vx = 0; vx < ESM::Land::LAND_SIZE; ++vx)
            {
                float worldX = cellWorldX + vx * vertexSize;
                float worldY = cellWorldY + vy * vertexSize;
                heights[vy * ESM::Land::LAND_SIZE + vx] = generateHeight(worldX, worldY);
            }
        }
        
        // Create normals array (calculate from heights)
        CSMWorld::LandNormalsColumn::DataType normals;
        normals.resize(ESM::Land::LAND_NUM_VERTS * 3);
        
        for (int vy = 0; vy < ESM::Land::LAND_SIZE; ++vy)
        {
            for (int vx = 0; vx < ESM::Land::LAND_SIZE; ++vx)
            {
                // Calculate normal from neighboring heights
                float h = heights[vy * ESM::Land::LAND_SIZE + vx];
                float hL = (vx > 0) ? heights[vy * ESM::Land::LAND_SIZE + vx - 1] : h;
                float hR = (vx < ESM::Land::LAND_SIZE - 1) ? heights[vy * ESM::Land::LAND_SIZE + vx + 1] : h;
                float hU = (vy > 0) ? heights[(vy - 1) * ESM::Land::LAND_SIZE + vx] : h;
                float hD = (vy < ESM::Land::LAND_SIZE - 1) ? heights[(vy + 1) * ESM::Land::LAND_SIZE + vx] : h;
                
                // Calculate gradient
                float dx = (hR - hL) / (2.0f * vertexSize);
                float dy = (hD - hU) / (2.0f * vertexSize);
                
                // Normal vector (-dx, -dy, 1) normalized and scaled to 127
                float len = std::sqrt(dx * dx + dy * dy + 1.0f);
                int idx = (vy * ESM::Land::LAND_SIZE + vx) * 3;
                normals[idx + 0] = static_cast<signed char>(std::clamp(-dx / len * 127.0f, -127.0f, 127.0f));
                normals[idx + 1] = static_cast<signed char>(std::clamp(-dy / len * 127.0f, -127.0f, 127.0f));
                normals[idx + 2] = static_cast<signed char>(std::clamp(1.0f / len * 127.0f, 0.0f, 127.0f));
            }
        }
        
        // Create texture array
        std::vector<uint16_t> textureVec = generateTextures(cellX, cellY);
        CSMWorld::LandTexturesColumn::DataType textures;
        textures.resize(ESM::Land::LAND_NUM_TEXTURES);
        for (int i = 0; i < ESM::Land::LAND_NUM_TEXTURES; ++i)
        {
            textures[i] = textureVec[i];
        }
        
        // Touch the land record first
        mDocument.getUndoStack().push(
            new CSMWorld::TouchLandCommand(landTable, ltexTable, landId));
        
        // Set heights
        int heightColumn = landTable.findColumnIndex(CSMWorld::Columns::ColumnId_LandHeightsIndex);
        if (heightColumn >= 0)
        {
            QVariant heightData;
            heightData.setValue(heights);
            QModelIndex heightIndex = landTable.getModelIndex(landId, heightColumn);
            mDocument.getUndoStack().push(
                new CSMWorld::ModifyCommand(landTable, heightIndex, heightData));
        }
        
        // Set normals
        int normalColumn = landTable.findColumnIndex(CSMWorld::Columns::ColumnId_LandNormalsIndex);
        if (normalColumn >= 0)
        {
            QVariant normalData;
            normalData.setValue(normals);
            QModelIndex normalIndex = landTable.getModelIndex(landId, normalColumn);
            mDocument.getUndoStack().push(
                new CSMWorld::ModifyCommand(landTable, normalIndex, normalData));
        }
        
        // Set textures
        int textureColumn = landTable.findColumnIndex(CSMWorld::Columns::ColumnId_LandTexturesIndex);
        if (textureColumn >= 0)
        {
            QVariant textureData;
            textureData.setValue(textures);
            QModelIndex textureIndex = landTable.getModelIndex(landId, textureColumn);
            mDocument.getUndoStack().push(
                new CSMWorld::ModifyCommand(landTable, textureIndex, textureData));
        }
    }
    
    bool ProceduralGenerator::generateObjects()
    {
        const ObjectPlacementParams& op = mState.objects;
        
        // Skip if no objects to place
        if (op.treeDensity <= 0 && op.rockDensity <= 0 && 
            op.grassDensity <= 0 && op.buildingDensity <= 0)
        {
            return true;
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
                reportProgress(getTotalCells() + currentCell, totalCells * 2,
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
        
        // Seed for this cell (deterministic)
        uint64_t cellSeed = mState.seed + 
            static_cast<uint64_t>(cellX + 10000) * 73856093ULL + 
            static_cast<uint64_t>(cellY + 10000) * 19349663ULL;
        RandomGenerator cellRng(cellSeed);
        
        // Cell ID for references
        std::string cellId = "#" + std::to_string(cellX) + ", " + std::to_string(cellY);
        
        // Get reference table
        CSMWorld::IdTable& refTable = dynamic_cast<CSMWorld::IdTable&>(
            *mData.getTableModel(CSMWorld::UniversalId(CSMWorld::UniversalId::Type_References)));
        
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
                
                // Reduce density on steep slopes
                float slopeFactor = std::max(0.0f, 1.0f - slope * 1.5f);
                float heightFactor = 1.0f;
                
                // No objects underwater
                if (mState.terrain.generateWater && height < mState.terrain.waterLevel)
                {
                    heightFactor = 0.0f;
                }
                // Fewer objects on mountaintops
                else if (height > mState.terrain.baseHeight + mState.terrain.heightVariation * 0.8f)
                {
                    heightFactor = 0.3f;
                }
                    
                densityMask[my * maskSize + mx] = slopeFactor * heightFactor;
            }
        }
        
        // Object categories to place
        struct ObjectCategory
        {
            std::string name;
            float density;
            std::vector<std::string> objects;
            AssetCategory assetCategory;
        };
        
        std::vector<ObjectCategory> categories;
        
        // Use asset library if available and enabled
        bool useLibrary = op.useAssetLibrary && mState.assetLibrary && mState.assetLibrary->hasAssets();
        
        // Trees
        if (op.treeDensity > 0.01f)
        {
            ObjectCategory trees;
            trees.name = "tree";
            trees.density = op.treeDensity;
            trees.assetCategory = AssetCategory::Tree;
            
            if (useLibrary)
            {
                trees.objects = getObjectsFromAssetLibrary(AssetCategory::Tree);
            }
            else
            {
                trees.objects = getObjectsFromReference("tree");
            }
            
            if (trees.objects.empty())
            {
                // Default Morrowind-style trees
                trees.objects = {
                    "flora_tree_gl_01", "flora_tree_gl_02", "flora_tree_gl_03",
                    "flora_tree_ai_01", "flora_tree_ai_02",
                    "flora_bc_tree_01", "flora_bc_tree_02"
                };
            }
            categories.push_back(trees);
        }
        
        // Rocks
        if (op.rockDensity > 0.01f)
        {
            ObjectCategory rocks;
            rocks.name = "rock";
            rocks.density = op.rockDensity;
            rocks.assetCategory = AssetCategory::Rock;
            
            if (useLibrary)
            {
                rocks.objects = getObjectsFromAssetLibrary(AssetCategory::Rock);
            }
            else
            {
                rocks.objects = getObjectsFromReference("rock");
            }
            
            if (rocks.objects.empty())
            {
                rocks.objects = {
                    "terrain_rock_ai_01", "terrain_rock_ai_02", "terrain_rock_ai_03",
                    "terrain_rock_bc_01", "terrain_rock_bc_02",
                    "terrain_rock_gl_01", "terrain_rock_gl_02"
                };
            }
            categories.push_back(rocks);
        }
        
        // Grass/Flora
        if (op.grassDensity > 0.01f)
        {
            ObjectCategory grass;
            grass.name = "grass";
            grass.density = op.grassDensity;
            grass.assetCategory = AssetCategory::Grass;
            
            if (useLibrary)
            {
                grass.objects = getObjectsFromAssetLibrary(AssetCategory::Grass);
            }
            else
            {
                grass.objects = getObjectsFromReference("grass");
            }
            
            if (grass.objects.empty())
            {
                grass.objects = {
                    "flora_grass_01", "flora_grass_02", "flora_grass_03",
                    "flora_plant_01", "flora_plant_02",
                    "flora_bc_fern_01", "flora_bc_fern_02"
                };
            }
            categories.push_back(grass);
        }
        
        // Bushes
        if (op.bushDensity > 0.01f)
        {
            ObjectCategory bushes;
            bushes.name = "bush";
            bushes.density = op.bushDensity;
            bushes.assetCategory = AssetCategory::Bush;
            
            if (useLibrary)
            {
                bushes.objects = getObjectsFromAssetLibrary(AssetCategory::Bush);
            }
            
            if (!bushes.objects.empty())
            {
                categories.push_back(bushes);
            }
        }
        
        // Place objects for each category
        for (const auto& category : categories)
        {
            float spacing = op.minSpacing / std::sqrt(category.density);
            PoissonDiskSampler sampler(cellSeed + std::hash<std::string>{}(category.name), 
                                       spacing, cellSize, cellSize);
            
            // Modify density mask for this category
            std::vector<float> categoryMask = densityMask;
            for (float& d : categoryMask)
                d *= category.density;
            
            auto points = sampler.generatePointsWithMask(categoryMask, maskSize, maskSize);
            
            for (const auto& [localX, localY] : points)
            {
                if (mCancelled)
                    return;
                    
                float worldX = cellWorldX + localX;
                float worldY = cellWorldY + localY;
                float height = generateHeight(worldX, worldY);
                
                // Skip if underwater
                if (mState.terrain.generateWater && height < mState.terrain.waterLevel)
                    continue;
                
                // Select random object from category
                if (category.objects.empty())
                    continue;
                    
                size_t objIndex = cellRng.nextInt(static_cast<uint32_t>(category.objects.size()));
                std::string objectId = category.objects[objIndex];
                
                // Calculate rotation
                float rotation = cellRng.nextFloatRange(0.0f, 6.28318f) * op.rotationVariation;
                
                // Calculate scale
                float scale = 1.0f + cellRng.nextFloatRange(-op.scaleVariation, op.scaleVariation);
                scale = std::max(0.5f, scale);
                
                // Create reference
                createReference(objectId, cellId, worldX, worldY, height, rotation, scale);
            }
        }
    }
    
    void ProceduralGenerator::createReference(const std::string& objectId, const std::string& cellId,
                                               float x, float y, float z, float rotation, float scale)
    {
        CSMWorld::IdTable& refTable = dynamic_cast<CSMWorld::IdTable&>(
            *mData.getTableModel(CSMWorld::UniversalId(CSMWorld::UniversalId::Type_References)));
        
        // Generate unique reference ID
        std::string refId = mData.getReferences().getNewId();
        
        // Create the reference
        auto createCmd = std::make_unique<CSMWorld::CreateCommand>(refTable, refId);
        
        // Set object ID
        int refIdColumn = refTable.findColumnIndex(CSMWorld::Columns::ColumnId_ReferenceableId);
        if (refIdColumn >= 0)
        {
            createCmd->addValue(refIdColumn, QString::fromStdString(objectId));
        }
        
        // Set cell
        int cellColumn = refTable.findColumnIndex(CSMWorld::Columns::ColumnId_Cell);
        if (cellColumn >= 0)
        {
            createCmd->addValue(cellColumn, QString::fromStdString(cellId));
        }
        
        // Set position
        int posXColumn = refTable.findColumnIndex(CSMWorld::Columns::ColumnId_PositionXPos);
        int posYColumn = refTable.findColumnIndex(CSMWorld::Columns::ColumnId_PositionYPos);
        int posZColumn = refTable.findColumnIndex(CSMWorld::Columns::ColumnId_PositionZPos);
        
        if (posXColumn >= 0)
            createCmd->addValue(posXColumn, static_cast<double>(x));
        if (posYColumn >= 0)
            createCmd->addValue(posYColumn, static_cast<double>(y));
        if (posZColumn >= 0)
            createCmd->addValue(posZColumn, static_cast<double>(z));
        
        // Set rotation (Z axis)
        int rotZColumn = refTable.findColumnIndex(CSMWorld::Columns::ColumnId_PositionZRot);
        if (rotZColumn >= 0)
        {
            createCmd->addValue(rotZColumn, static_cast<double>(rotation));
        }
        
        // Set scale
        int scaleColumn = refTable.findColumnIndex(CSMWorld::Columns::ColumnId_Scale);
        if (scaleColumn >= 0)
        {
            createCmd->addValue(scaleColumn, static_cast<double>(scale));
        }
        
        mDocument.getUndoStack().push(createCmd.release());
    }
    
    std::vector<std::string> ProceduralGenerator::getObjectsFromReference(const std::string& category) const
    {
        std::vector<std::string> objects;
        
        if (!mState.useReference || !mState.analysis.isValid)
            return objects;
        
        // Search through referenceables for matching objects
        const auto& refIdCollection = mData.getReferenceables();
        
        for (int i = 0; i < refIdCollection.getSize(); ++i)
        {
            std::string refId = refIdCollection.getId(i).getRefIdString();
            
            // Convert to lowercase for comparison
            std::string lowerRefId = refId;
            std::transform(lowerRefId.begin(), lowerRefId.end(), lowerRefId.begin(), ::tolower);
            
            bool matches = false;
            
            if (category == "tree")
            {
                matches = (lowerRefId.find("tree") != std::string::npos || 
                          lowerRefId.find("flora_tree") != std::string::npos);
            }
            else if (category == "rock")
            {
                matches = (lowerRefId.find("rock") != std::string::npos ||
                          lowerRefId.find("terrain_rock") != std::string::npos ||
                          lowerRefId.find("stone") != std::string::npos);
            }
            else if (category == "grass")
            {
                matches = (lowerRefId.find("grass") != std::string::npos ||
                          lowerRefId.find("flora_grass") != std::string::npos ||
                          lowerRefId.find("plant") != std::string::npos ||
                          lowerRefId.find("fern") != std::string::npos);
            }
            else if (category == "building")
            {
                matches = (lowerRefId.find("house") != std::string::npos ||
                          lowerRefId.find("building") != std::string::npos ||
                          lowerRefId.find("hut") != std::string::npos);
            }
            
            if (matches)
            {
                objects.push_back(refId);
            }
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
        const InteriorParams& ip = mState.interiors;
        
        // Generate interior cells - one per few exterior cells
        int interiorCount = std::max(1, (mState.worldSizeX * mState.worldSizeY) / 16);
        
        CSMWorld::IdTree& cellTable = dynamic_cast<CSMWorld::IdTree&>(
            *mData.getTableModel(CSMWorld::UniversalId(CSMWorld::UniversalId::Type_Cells)));
        
        for (int i = 0; i < interiorCount && !mCancelled; ++i)
        {
            std::string name = "Proc_Interior_" + std::to_string(mState.seed) + "_" + std::to_string(i);
            int roomCount = mRng->nextIntRange(ip.minRooms, ip.maxRooms);
            
            createInterior(name, roomCount);
            
            reportProgress(i + 1, interiorCount, "Generating interior: " + name);
        }
        
        return !mCancelled;
    }
    
    void ProceduralGenerator::createInterior(const std::string& name, int roomCount)
    {
        CSMWorld::IdTree& cellTable = dynamic_cast<CSMWorld::IdTree&>(
            *mData.getTableModel(CSMWorld::UniversalId(CSMWorld::UniversalId::Type_Cells)));
        
        ESM::RefId refId = ESM::RefId::stringRefId(name);
        
        // Check if cell exists
        int existingRow = mData.getCells().searchId(refId);
        if (existingRow >= 0 && !mState.overwriteExisting)
            return;
        
        if (existingRow < 0)
        {
            // Create interior cell
            auto createCmd = std::make_unique<CSMWorld::CreateCommand>(cellTable, name);
            
            // Set as interior
            int parentIndex = cellTable.findColumnIndex(CSMWorld::Columns::ColumnId_Cell);
            int interiorIndex = cellTable.findNestedColumnIndex(parentIndex, CSMWorld::Columns::ColumnId_Interior);
            createCmd->addNestedValue(parentIndex, interiorIndex, true);
            
            // Set ambient lighting
            int ambientColumn = cellTable.findColumnIndex(CSMWorld::Columns::ColumnId_Ambient);
            if (ambientColumn >= 0)
            {
                // Dim interior lighting
                int ambientColor = (40 << 16) | (35 << 8) | 30; // RGB
                createCmd->addValue(ambientColumn, ambientColor);
            }
            
            mDocument.getUndoStack().push(createCmd.release());
        }
        
        // Generate room layout using BSP
        generateBSPInterior(name, roomCount);
    }
    
    void ProceduralGenerator::generateBSPInterior(const std::string& cellName, int roomCount)
    {
        const InteriorParams& ip = mState.interiors;
        
        // BSP-based room generation with smart pointers
        struct BSPNode
        {
            float x, y, width, height;
            std::unique_ptr<BSPNode> left;
            std::unique_ptr<BSPNode> right;
            bool isRoom = false;
            float roomX, roomY, roomW, roomH;
            
            BSPNode() = default;
        };
        
        // Initial space
        float totalWidth = ip.roomSizeMax * std::sqrt(static_cast<float>(roomCount));
        float totalHeight = totalWidth;
        
        auto root = std::make_unique<BSPNode>();
        root->x = -totalWidth / 2;
        root->y = -totalHeight / 2;
        root->width = totalWidth;
        root->height = totalHeight;
        
        // Recursive BSP split using raw pointers for queue (ownership stays with parent)
        std::queue<BSPNode*> toSplit;
        toSplit.push(root.get());
        int roomsCreated = 0;
        
        while (!toSplit.empty() && roomsCreated < roomCount)
        {
            BSPNode* node = toSplit.front();
            toSplit.pop();
            
            bool splitHorizontal = mRng->nextBool(0.5f);
            if (node->width > node->height * 1.25f)
                splitHorizontal = false;
            else if (node->height > node->width * 1.25f)
                splitHorizontal = true;
            
            float minSize = ip.roomSizeMin * 1.5f;
            
            if (splitHorizontal && node->height > minSize * 2)
            {
                float split = mRng->nextFloatRange(0.4f, 0.6f);
                node->left = std::make_unique<BSPNode>();
                node->left->x = node->x;
                node->left->y = node->y;
                node->left->width = node->width;
                node->left->height = node->height * split;
                
                node->right = std::make_unique<BSPNode>();
                node->right->x = node->x;
                node->right->y = node->y + node->height * split;
                node->right->width = node->width;
                node->right->height = node->height * (1.0f - split);
                
                toSplit.push(node->left.get());
                toSplit.push(node->right.get());
            }
            else if (!splitHorizontal && node->width > minSize * 2)
            {
                float split = mRng->nextFloatRange(0.4f, 0.6f);
                node->left = std::make_unique<BSPNode>();
                node->left->x = node->x;
                node->left->y = node->y;
                node->left->width = node->width * split;
                node->left->height = node->height;
                
                node->right = std::make_unique<BSPNode>();
                node->right->x = node->x + node->width * split;
                node->right->y = node->y;
                node->right->width = node->width * (1.0f - split);
                node->right->height = node->height;
                
                toSplit.push(node->left.get());
                toSplit.push(node->right.get());
            }
            else
            {
                // Create room in this leaf
                node->isRoom = true;
                float padding = ip.corridorWidth;
                node->roomX = node->x + padding;
                node->roomY = node->y + padding;
                node->roomW = std::max(ip.roomSizeMin, node->width - padding * 2);
                node->roomH = std::max(ip.roomSizeMin, node->height - padding * 2);
                ++roomsCreated;
            }
        }
        
        // Collect all rooms
        std::vector<BSPNode*> rooms;
        std::queue<BSPNode*> toVisit;
        toVisit.push(root.get());
        
        while (!toVisit.empty())
        {
            BSPNode* node = toVisit.front();
            toVisit.pop();
            
            if (node->isRoom)
            {
                rooms.push_back(node);
            }
            else
            {
                if (node->left) toVisit.push(node->left.get());
                if (node->right) toVisit.push(node->right.get());
            }
        }
        
        // Place objects in rooms
        for (size_t i = 0; i < rooms.size(); ++i)
        {
            BSPNode* room = rooms[i];
            
            // Generate room furniture
            if (ip.generateLighting)
            {
                // Place a light in the center
                float lightX = room->roomX + room->roomW / 2;
                float lightY = room->roomY + room->roomH / 2;
                float lightZ = ip.ceilingHeight * 0.8f;
                
                createReference("light_com_candle_01", cellName, lightX, lightY, lightZ, 0, 1.0f);
            }
            
            if (ip.generateContainers && mRng->nextBool(ip.clutter))
            {
                // Place a container
                float contX = room->roomX + mRng->nextFloatRange(0.2f, 0.8f) * room->roomW;
                float contY = room->roomY + mRng->nextFloatRange(0.2f, 0.8f) * room->roomH;
                
                std::vector<std::string> containers = {"contain_barrel_01", "contain_crate_01", "chest_small_01"};
                std::string containerId = containers[mRng->nextInt(static_cast<uint32_t>(containers.size()))];
                
                createReference(containerId, cellName, contX, contY, 0, mRng->nextFloatRange(0, 6.28f), 1.0f);
            }
        }
        
        // BSP tree cleanup is automatic via unique_ptr
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
        CSMWorld::IdTable& pathgridTable = dynamic_cast<CSMWorld::IdTable&>(
            *mData.getTableModel(CSMWorld::UniversalId(CSMWorld::UniversalId::Type_Pathgrids)));
        
        std::string cellId = "#" + std::to_string(cellX) + ", " + std::to_string(cellY);
        ESM::RefId refId = ESM::RefId::stringRefId(cellId);
        
        // Check if pathgrid exists
        int existingRow = mData.getPathgrids().searchId(refId);
        if (existingRow >= 0 && !mState.overwriteExisting)
            return;
        
        float cellWorldX = static_cast<float>(cellX) * ESM::Land::REAL_SIZE;
        float cellWorldY = static_cast<float>(cellY) * ESM::Land::REAL_SIZE;
        float cellSize = static_cast<float>(ESM::Land::REAL_SIZE);
        
        // Generate walkable points on a grid
        const int gridSize = 8; // 8x8 grid of potential points
        float gridStep = cellSize / gridSize;
        
        std::vector<ESM::Pathgrid::Point> points;
        std::vector<std::pair<int, int>> validIndices;
        
        // Generate points
        for (int gy = 0; gy < gridSize; ++gy)
        {
            for (int gx = 0; gx < gridSize; ++gx)
            {
                float worldX = cellWorldX + (gx + 0.5f) * gridStep;
                float worldY = cellWorldY + (gy + 0.5f) * gridStep;
                float height = generateHeight(worldX, worldY);
                float slope = getSlopeAt(worldX, worldY);
                
                // Check if walkable
                bool walkable = true;
                
                // Too steep
                if (slope > 0.6f)
                    walkable = false;
                
                // Underwater
                if (mState.terrain.generateWater && height < mState.terrain.waterLevel)
                    walkable = false;
                
                if (walkable)
                {
                    // Convert to cell-local coordinates
                    int localX = static_cast<int>(worldX - cellWorldX);
                    int localY = static_cast<int>(worldY - cellWorldY);
                    int localZ = static_cast<int>(height);
                    
                    ESM::Pathgrid::Point point(localX, localY, localZ);
                    point.mAutogenerated = 1;
                    points.push_back(point);
                    validIndices.push_back({gx, gy});
                }
            }
        }
        
        if (points.empty())
            return;
        
        // Generate edges (connect nearby walkable points)
        std::vector<ESM::Pathgrid::Edge> edges;
        
        for (size_t i = 0; i < validIndices.size(); ++i)
        {
            for (size_t j = i + 1; j < validIndices.size(); ++j)
            {
                int dx = std::abs(validIndices[i].first - validIndices[j].first);
                int dy = std::abs(validIndices[i].second - validIndices[j].second);
                
                // Connect only adjacent points (including diagonals)
                if (dx <= 1 && dy <= 1)
                {
                    // Check height difference
                    float heightDiff = std::abs(points[i].mZ - points[j].mZ);
                    if (heightDiff < 200.0f) // Reasonable step height
                    {
                        ESM::Pathgrid::Edge edge;
                        edge.mV0 = i;
                        edge.mV1 = j;
                        edges.push_back(edge);
                        
                        // Update connection count
                        ++points[i].mConnectionNum;
                        ++points[j].mConnectionNum;
                    }
                }
            }
        }
        
        // Create pathgrid record if it doesn't exist
        if (existingRow < 0)
        {
            mDocument.getUndoStack().push(
                new CSMWorld::CreatePathgridCommand(pathgridTable, cellId));
        }
        
        // Set pathgrid data using the model
        // Note: In a complete implementation, we would set the points and edges
        // through the appropriate columns. For now, we create the basic structure.
    }
    
    bool ProceduralGenerator::previewCell(int cellX, int cellY)
    {
        mCancelled = false;
        
        reportProgress(0, 3, "Creating cell...");
        createCell(cellX, cellY);
        
        reportProgress(1, 3, "Creating terrain...");
        createLand(cellX, cellY);
        
        reportProgress(2, 3, "Placing objects...");
        placeObjectsInCell(cellX, cellY);
        
        reportProgress(3, 3, "Preview complete!");
        return true;
    }
    
    std::vector<std::string> ProceduralGenerator::getObjectsFromAssetLibrary(AssetCategory category) const
    {
        if (!mState.assetLibrary)
            return {};
        
        return mState.assetLibrary->getAssetIds(category);
    }
    
    std::string ProceduralGenerator::selectAssetFromLibrary(AssetCategory category) const
    {
        auto objects = getObjectsFromAssetLibrary(category);
        if (objects.empty())
            return "";
        
        return objects[mRng->nextInt(static_cast<uint32_t>(objects.size()))];
    }
    
    std::string ProceduralGenerator::createReferenceWithId(const std::string& objectId, const std::string& cellId,
                                                           float x, float y, float z, float rotation, float scale)
    {
        CSMWorld::IdTable& refTable = dynamic_cast<CSMWorld::IdTable&>(
            *mData.getTableModel(CSMWorld::UniversalId(CSMWorld::UniversalId::Type_References)));
        
        // Generate unique reference ID
        std::string refId = mData.getReferences().getNewId();
        
        // Create the reference
        auto createCmd = std::make_unique<CSMWorld::CreateCommand>(refTable, refId);
        
        // Set object ID
        int refIdColumn = refTable.findColumnIndex(CSMWorld::Columns::ColumnId_ReferenceableId);
        if (refIdColumn >= 0)
        {
            createCmd->addValue(refIdColumn, QString::fromStdString(objectId));
        }
        
        // Set cell
        int cellColumn = refTable.findColumnIndex(CSMWorld::Columns::ColumnId_Cell);
        if (cellColumn >= 0)
        {
            createCmd->addValue(cellColumn, QString::fromStdString(cellId));
        }
        
        // Set position
        int posXColumn = refTable.findColumnIndex(CSMWorld::Columns::ColumnId_PositionXPos);
        int posYColumn = refTable.findColumnIndex(CSMWorld::Columns::ColumnId_PositionYPos);
        int posZColumn = refTable.findColumnIndex(CSMWorld::Columns::ColumnId_PositionZPos);
        
        if (posXColumn >= 0)
            createCmd->addValue(posXColumn, static_cast<double>(x));
        if (posYColumn >= 0)
            createCmd->addValue(posYColumn, static_cast<double>(y));
        if (posZColumn >= 0)
            createCmd->addValue(posZColumn, static_cast<double>(z));
        
        // Set rotation (Z axis)
        int rotZColumn = refTable.findColumnIndex(CSMWorld::Columns::ColumnId_PositionZRot);
        if (rotZColumn >= 0)
        {
            createCmd->addValue(rotZColumn, static_cast<double>(rotation));
        }
        
        // Set scale
        int scaleColumn = refTable.findColumnIndex(CSMWorld::Columns::ColumnId_Scale);
        if (scaleColumn >= 0)
        {
            createCmd->addValue(scaleColumn, static_cast<double>(scale));
        }
        
        mDocument.getUndoStack().push(createCmd.release());
        
        return refId;
    }
    
    // Settlement generation
    
    bool ProceduralGenerator::generateSettlements()
    {
        if (mState.settlement.type == SettlementType::None)
            return true;
        
        // Find suitable locations
        auto locations = findSettlementLocations();
        
        int totalSettlements = static_cast<int>(locations.size());
        int currentSettlement = 0;
        
        for (auto& location : locations)
        {
            if (mCancelled)
                return false;
            
            reportProgress(currentSettlement, totalSettlements, 
                "Generating settlement: " + location.name);
            
            generateSettlement(location);
            mGeneratedSettlements.push_back(location);
            
            ++currentSettlement;
        }
        
        return !mCancelled;
    }
    
    std::vector<SettlementLocation> ProceduralGenerator::findSettlementLocations()
    {
        std::vector<SettlementLocation> locations;
        
        const SettlementParams& sp = mState.settlement;
        
        if (!sp.autoPlaceSettlements)
        {
            // Use manually specified locations
            for (const auto& [cellX, cellY] : sp.manualLocations)
            {
                SettlementLocation loc;
                loc.cellX = cellX;
                loc.cellY = cellY;
                loc.centerX = static_cast<float>(cellX) * ESM::Land::REAL_SIZE + ESM::Land::REAL_SIZE / 2.0f;
                loc.centerY = static_cast<float>(cellY) * ESM::Land::REAL_SIZE + ESM::Land::REAL_SIZE / 2.0f;
                loc.centerZ = generateHeight(loc.centerX, loc.centerY);
                loc.radius = calculateSettlementRadius(sp.type);
                loc.type = sp.type;
                loc.name = "Settlement_" + std::to_string(mState.seed) + "_" + std::to_string(locations.size());
                locations.push_back(loc);
            }
        }
        else
        {
            // Auto-find suitable locations
            int settlementsToPlace = sp.settlementCount;
            int attempts = 0;
            int maxAttempts = settlementsToPlace * 100;
            
            while (static_cast<int>(locations.size()) < settlementsToPlace && attempts < maxAttempts)
            {
                ++attempts;
                
                // Random cell within world bounds
                int cellX = mState.originX + mRng->nextInt(static_cast<uint32_t>(mState.worldSizeX));
                int cellY = mState.originY + mRng->nextInt(static_cast<uint32_t>(mState.worldSizeY));
                
                // Random position within cell
                float offsetX = mRng->nextFloatRange(0.2f, 0.8f) * ESM::Land::REAL_SIZE;
                float offsetY = mRng->nextFloatRange(0.2f, 0.8f) * ESM::Land::REAL_SIZE;
                
                float worldX = static_cast<float>(cellX) * ESM::Land::REAL_SIZE + offsetX;
                float worldY = static_cast<float>(cellY) * ESM::Land::REAL_SIZE + offsetY;
                
                float radius = calculateSettlementRadius(sp.type);
                
                if (isLocationSuitableForSettlement(worldX, worldY, radius))
                {
                    // Check distance from other settlements
                    bool tooClose = false;
                    for (const auto& existing : locations)
                    {
                        float dx = worldX - existing.centerX;
                        float dy = worldY - existing.centerY;
                        float dist = std::sqrt(dx * dx + dy * dy);
                        if (dist < radius + existing.radius + 500.0f) // Minimum spacing
                        {
                            tooClose = true;
                            break;
                        }
                    }
                    
                    if (!tooClose)
                    {
                        SettlementLocation loc;
                        loc.cellX = cellX;
                        loc.cellY = cellY;
                        loc.centerX = worldX;
                        loc.centerY = worldY;
                        loc.centerZ = generateHeight(worldX, worldY);
                        loc.radius = radius;
                        loc.type = sp.type;
                        loc.name = "Settlement_" + std::to_string(mState.seed) + "_" + std::to_string(locations.size());
                        locations.push_back(loc);
                    }
                }
            }
        }
        
        return locations;
    }
    
    bool ProceduralGenerator::isLocationSuitableForSettlement(float worldX, float worldY, float radius) const
    {
        // Check terrain at center and edges
        float centerHeight = generateHeight(worldX, worldY);
        
        // Not underwater
        if (mState.terrain.generateWater && centerHeight < mState.terrain.waterLevel + 50.0f)
            return false;
        
        // Not too high
        if (centerHeight > mState.terrain.baseHeight + mState.terrain.heightVariation * 0.7f)
            return false;
        
        // Check slope at center
        float slope = getSlopeAt(worldX, worldY);
        if (slope > 0.3f)
            return false;
        
        // Check terrain variation within radius
        float minHeight = centerHeight;
        float maxHeight = centerHeight;
        
        const int checkPoints = 8;
        for (int i = 0; i < checkPoints; ++i)
        {
            float angle = static_cast<float>(i) * 6.28318f / static_cast<float>(checkPoints);
            float checkX = worldX + std::cos(angle) * radius * 0.8f;
            float checkY = worldY + std::sin(angle) * radius * 0.8f;
            
            float height = generateHeight(checkX, checkY);
            minHeight = std::min(minHeight, height);
            maxHeight = std::max(maxHeight, height);
            
            // Check for underwater
            if (mState.terrain.generateWater && height < mState.terrain.waterLevel)
                return false;
        }
        
        // Height variation should be reasonable
        if (maxHeight - minHeight > 200.0f)
            return false;
        
        return true;
    }
    
    float ProceduralGenerator::calculateSettlementRadius(SettlementType type) const
    {
        switch (type)
        {
            case SettlementType::Farm: return 300.0f;
            case SettlementType::Hamlet: return 500.0f;
            case SettlementType::Village: return 800.0f;
            case SettlementType::Town: return 1500.0f;
            case SettlementType::City: return 3000.0f;
            case SettlementType::Metropolis: return 6000.0f;
            case SettlementType::Fortress: return 1000.0f;
            case SettlementType::Castle: return 800.0f;
            default: return 500.0f;
        }
    }
    
    void ProceduralGenerator::generateSettlement(SettlementLocation& location)
    {
        // Place buildings
        placeSettlementBuildings(location);
        
        // Generate roads if enabled
        if (mState.settlement.generateRoads)
        {
            generateSettlementRoads(location);
        }
        
        // Generate walls if applicable
        bool shouldHaveWalls = mState.settlement.userOverrideWalls 
            ? mState.settlement.generateWalls 
            : settlementDefaultWalls(location.type);
            
        if (shouldHaveWalls)
        {
            generateSettlementWalls(location);
        }
        
        // Generate interiors for buildings
        if (mState.settlement.generateBuildingInteriors)
        {
            generateSettlementInteriors(location);
        }
    }
    
    void ProceduralGenerator::placeSettlementBuildings(SettlementLocation& location)
    {
        auto [minBuildings, maxBuildings] = getSettlementBuildingRange(location.type);
        int buildingCount = mRng->nextIntRange(minBuildings, maxBuildings);
        
        // Get building objects from asset library or use defaults
        std::vector<std::string> buildings;
        if (mState.assetLibrary && mState.objects.useAssetLibrary)
        {
            buildings = getObjectsFromAssetLibrary(AssetCategory::Building);
        }
        
        if (buildings.empty())
        {
            // Default Morrowind-style buildings
            buildings = {
                "ex_common_house_01", "ex_common_house_02", "ex_common_house_03",
                "ex_hlaalu_house_01", "ex_hlaalu_house_02",
                "ex_redoran_house_01", "ex_redoran_house_02"
            };
        }
        
        // Cell ID for exterior references
        std::string cellId = "#" + std::to_string(location.cellX) + ", " + std::to_string(location.cellY);
        
        // Place buildings in a pattern based on settlement type
        float spacing = mState.settlement.buildingSpacing;
        
        // Simple grid-based placement with some randomization
        int gridSize = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(buildingCount))));
        float gridSpacing = location.radius * 2.0f / static_cast<float>(gridSize + 1);
        
        int placed = 0;
        for (int gy = 0; gy < gridSize && placed < buildingCount; ++gy)
        {
            for (int gx = 0; gx < gridSize && placed < buildingCount; ++gx)
            {
                // Calculate position
                float baseX = location.centerX - location.radius + (static_cast<float>(gx) + 1.0f) * gridSpacing;
                float baseY = location.centerY - location.radius + (static_cast<float>(gy) + 1.0f) * gridSpacing;
                
                // Add some randomization
                float offsetX = mRng->nextFloatRange(-spacing * 0.3f, spacing * 0.3f);
                float offsetY = mRng->nextFloatRange(-spacing * 0.3f, spacing * 0.3f);
                
                float worldX = baseX + offsetX;
                float worldY = baseY + offsetY;
                float worldZ = generateHeight(worldX, worldY);
                
                // Check if location is suitable
                float slope = getSlopeAt(worldX, worldY);
                if (slope > 0.25f)
                    continue;
                
                if (mState.terrain.generateWater && worldZ < mState.terrain.waterLevel)
                    continue;
                
                // Select building
                std::string buildingId = buildings[mRng->nextInt(static_cast<uint32_t>(buildings.size()))];
                
                // Random rotation (aligned to cardinal directions with some variation)
                float rotation = static_cast<float>(mRng->nextInt(4)) * 1.5708f; // 90 degree increments
                rotation += mRng->nextFloatRange(-0.1f, 0.1f); // Small variation
                
                // Create building reference
                std::string refId = createReferenceWithId(buildingId, cellId, worldX, worldY, worldZ, rotation, 1.0f);
                location.buildingIds.push_back(refId);
                
                ++placed;
            }
        }
    }
    
    void ProceduralGenerator::generateSettlementRoads(const SettlementLocation& location)
    {
        // Get road/cobblestone objects from asset library
        std::vector<std::string> roadObjects;
        if (mState.assetLibrary && mState.assetLibrary->getRoadConfig().use3DRoads)
        {
            roadObjects = getObjectsFromAssetLibrary(AssetCategory::CobblestoneRoad);
        }
        
        if (roadObjects.empty())
        {
            // No 3D road objects available, skip road generation
            // In a full implementation, we would modify terrain textures instead
            return;
        }
        
        std::string cellId = "#" + std::to_string(location.cellX) + ", " + std::to_string(location.cellY);
        
        // Create a simple cross-road pattern through the settlement center
        float roadLength = location.radius * 1.5f;
        float roadSpacing = 100.0f;
        
        // Main roads (N-S and E-W)
        for (int direction = 0; direction < 2; ++direction)
        {
            float angle = static_cast<float>(direction) * 1.5708f; // 0 or 90 degrees
            float cosAngle = std::cos(angle);
            float sinAngle = std::sin(angle);
            
            for (float dist = -roadLength; dist <= roadLength; dist += roadSpacing)
            {
                float worldX = location.centerX + cosAngle * dist;
                float worldY = location.centerY + sinAngle * dist;
                float worldZ = generateHeight(worldX, worldY);
                
                // Select road object
                std::string roadId = roadObjects[mRng->nextInt(static_cast<uint32_t>(roadObjects.size()))];
                
                createReference(roadId, cellId, worldX, worldY, worldZ, angle, 1.0f);
            }
        }
    }
    
    void ProceduralGenerator::generateSettlementWalls(const SettlementLocation& location)
    {
        // Get wall objects from asset library
        std::vector<std::string> wallSegments;
        std::vector<std::string> wallGates;
        std::vector<std::string> wallTowers;
        
        if (mState.assetLibrary)
        {
            wallSegments = getObjectsFromAssetLibrary(AssetCategory::Wall);
            wallGates = getObjectsFromAssetLibrary(AssetCategory::WallGate);
            wallTowers = getObjectsFromAssetLibrary(AssetCategory::WallTower);
        }
        
        // If no wall objects available, skip wall generation
        if (wallSegments.empty())
            return;
        
        std::string cellId = "#" + std::to_string(location.cellX) + ", " + std::to_string(location.cellY);
        
        // Calculate wall parameters
        float wallRadius = mState.settlement.wallRadius > 0.0f 
            ? mState.settlement.wallRadius 
            : location.radius * 0.9f;
        
        int gateCount = mState.settlement.wallGateCount;
        float towerSpacing = 300.0f;
        
        // Place walls in a circle around the settlement
        float circumference = 2.0f * 3.14159f * wallRadius;
        int segmentCount = static_cast<int>(circumference / 100.0f); // One segment per 100 units
        
        float gateInterval = static_cast<float>(segmentCount) / static_cast<float>(gateCount);
        
        for (int i = 0; i < segmentCount; ++i)
        {
            float angle = static_cast<float>(i) * 2.0f * 3.14159f / static_cast<float>(segmentCount);
            float worldX = location.centerX + std::cos(angle) * wallRadius;
            float worldY = location.centerY + std::sin(angle) * wallRadius;
            float worldZ = generateHeight(worldX, worldY);
            
            // Wall rotation should face outward
            float rotation = angle + 1.5708f; // Perpendicular to radius
            
            // Determine what to place
            bool isGatePosition = false;
            for (int g = 0; g < gateCount; ++g)
            {
                if (std::abs(static_cast<float>(i) - static_cast<float>(g) * gateInterval) < 1.0f)
                {
                    isGatePosition = true;
                    break;
                }
            }
            
            bool isTowerPosition = (i % static_cast<int>(towerSpacing / (circumference / static_cast<float>(segmentCount)))) == 0;
            
            std::string objectId;
            if (isGatePosition && !wallGates.empty())
            {
                objectId = wallGates[mRng->nextInt(static_cast<uint32_t>(wallGates.size()))];
            }
            else if (isTowerPosition && !wallTowers.empty())
            {
                objectId = wallTowers[mRng->nextInt(static_cast<uint32_t>(wallTowers.size()))];
            }
            else
            {
                objectId = wallSegments[mRng->nextInt(static_cast<uint32_t>(wallSegments.size()))];
            }
            
            createReference(objectId, cellId, worldX, worldY, worldZ, rotation, 1.0f);
        }
    }
    
    void ProceduralGenerator::generateSettlementInteriors(SettlementLocation& location)
    {
        // Generate interior cells for buildings
        for (size_t i = 0; i < location.buildingIds.size(); ++i)
        {
            std::string interiorName = location.name + "_Interior_" + std::to_string(i);
            
            // Determine room count based on building "type" (simple random for now)
            int roomCount = mRng->nextIntRange(1, 6);
            
            createBuildingInterior(location.buildingIds[i], "building");
            location.interiorIds.push_back(interiorName);
        }
    }
    
    void ProceduralGenerator::createBuildingInterior(const std::string& buildingRefId, const std::string& buildingType)
    {
        // Generate an interior cell linked to the building
        std::string interiorName = "Interior_" + buildingRefId;
        
        int roomCount = mRng->nextIntRange(
            mState.interiors.minRooms, 
            mState.interiors.maxRooms
        );
        
        createInterior(interiorName, roomCount);
    }
    
    // Cave and dungeon generation
    
    bool ProceduralGenerator::generateCavesAndDungeons()
    {
        const CaveDungeonParams& cdp = mState.caveDungeon;
        
        int totalToGenerate = 0;
        if (cdp.generateCaves)
            totalToGenerate += cdp.caveCount;
        if (cdp.generateDungeons)
            totalToGenerate += cdp.dungeonCount;
        
        int currentGenerated = 0;
        
        // Generate caves
        if (cdp.generateCaves)
        {
            for (int i = 0; i < cdp.caveCount && !mCancelled; ++i)
            {
                // Find a suitable location
                int attempts = 0;
                while (attempts < 100)
                {
                    ++attempts;
                    
                    int cellX = mState.originX + mRng->nextInt(static_cast<uint32_t>(mState.worldSizeX));
                    int cellY = mState.originY + mRng->nextInt(static_cast<uint32_t>(mState.worldSizeY));
                    
                    float worldX = static_cast<float>(cellX) * ESM::Land::REAL_SIZE + 
                                   mRng->nextFloatRange(0.2f, 0.8f) * ESM::Land::REAL_SIZE;
                    float worldY = static_cast<float>(cellY) * ESM::Land::REAL_SIZE + 
                                   mRng->nextFloatRange(0.2f, 0.8f) * ESM::Land::REAL_SIZE;
                    
                    float height = generateHeight(worldX, worldY);
                    float slope = getSlopeAt(worldX, worldY);
                    
                    // Caves prefer hillsides
                    if (slope > 0.3f && slope < 0.8f && height > mState.terrain.waterLevel)
                    {
                        generateCave(cellX, cellY, worldX, worldY);
                        break;
                    }
                }
                
                ++currentGenerated;
                reportProgress(currentGenerated, totalToGenerate, "Generating cave " + std::to_string(i + 1));
            }
        }
        
        // Generate dungeons
        if (cdp.generateDungeons)
        {
            for (int i = 0; i < cdp.dungeonCount && !mCancelled; ++i)
            {
                // Find a suitable location
                int attempts = 0;
                while (attempts < 100)
                {
                    ++attempts;
                    
                    int cellX = mState.originX + mRng->nextInt(static_cast<uint32_t>(mState.worldSizeX));
                    int cellY = mState.originY + mRng->nextInt(static_cast<uint32_t>(mState.worldSizeY));
                    
                    float worldX = static_cast<float>(cellX) * ESM::Land::REAL_SIZE + 
                                   mRng->nextFloatRange(0.2f, 0.8f) * ESM::Land::REAL_SIZE;
                    float worldY = static_cast<float>(cellY) * ESM::Land::REAL_SIZE + 
                                   mRng->nextFloatRange(0.2f, 0.8f) * ESM::Land::REAL_SIZE;
                    
                    float height = generateHeight(worldX, worldY);
                    float slope = getSlopeAt(worldX, worldY);
                    
                    // Dungeons can be anywhere above water with reasonable slope
                    if (slope < 0.5f && height > mState.terrain.waterLevel)
                    {
                        generateDungeon(cellX, cellY, worldX, worldY);
                        break;
                    }
                }
                
                ++currentGenerated;
                reportProgress(currentGenerated, totalToGenerate, "Generating dungeon " + std::to_string(i + 1));
            }
        }
        
        return !mCancelled;
    }
    
    void ProceduralGenerator::generateCave(int cellX, int cellY, float worldX, float worldY)
    {
        const CaveDungeonParams& cdp = mState.caveDungeon;
        
        // Get cave entrance from asset library
        std::vector<std::string> entrances;
        if (mState.assetLibrary)
        {
            entrances = getObjectsFromAssetLibrary(AssetCategory::CaveEntrance);
        }
        
        if (entrances.empty())
        {
            entrances = {"ex_cave_entrance_01", "ex_cave_entrance_02"};
        }
        
        std::string cellId = "#" + std::to_string(cellX) + ", " + std::to_string(cellY);
        float worldZ = generateHeight(worldX, worldY);
        
        // Place entrance
        std::string entranceId = entrances[mRng->nextInt(static_cast<uint32_t>(entrances.size()))];
        
        // Rotation to face outward from slope
        float rotation = mRng->nextFloatRange(0.0f, 6.28318f);
        
        createReference(entranceId, cellId, worldX, worldY, worldZ, rotation, 1.0f);
        
        // Generate interior
        std::string interiorName = "Cave_" + std::to_string(mState.seed) + "_" + 
                                   std::to_string(cellX) + "_" + std::to_string(cellY);
        
        int roomCount = mRng->nextIntRange(cdp.caveMinRooms, cdp.caveMaxRooms);
        generateCaveInterior(interiorName, roomCount);
    }
    
    void ProceduralGenerator::generateDungeon(int cellX, int cellY, float worldX, float worldY)
    {
        const CaveDungeonParams& cdp = mState.caveDungeon;
        
        // Get dungeon entrance from asset library
        std::vector<std::string> entrances;
        if (mState.assetLibrary)
        {
            entrances = getObjectsFromAssetLibrary(AssetCategory::DungeonEntrance);
        }
        
        if (entrances.empty())
        {
            entrances = {"ex_ruins_entrance_01", "ex_ruins_door_01"};
        }
        
        std::string cellId = "#" + std::to_string(cellX) + ", " + std::to_string(cellY);
        float worldZ = generateHeight(worldX, worldY);
        
        // Place entrance
        std::string entranceId = entrances[mRng->nextInt(static_cast<uint32_t>(entrances.size()))];
        float rotation = mRng->nextFloatRange(0.0f, 6.28318f);
        
        createReference(entranceId, cellId, worldX, worldY, worldZ, rotation, 1.0f);
        
        // Generate interior for each floor
        for (int floor = 0; floor < cdp.dungeonFloors; ++floor)
        {
            std::string interiorName = "Dungeon_" + std::to_string(mState.seed) + "_" + 
                                       std::to_string(cellX) + "_" + std::to_string(cellY) +
                                       "_Floor" + std::to_string(floor);
            
            int roomCount = mRng->nextIntRange(cdp.dungeonMinRooms, cdp.dungeonMaxRooms);
            generateDungeonInterior(interiorName, roomCount, floor);
        }
    }
    
    void ProceduralGenerator::generateCaveInterior(const std::string& cellName, int roomCount)
    {
        // Create interior cell
        CSMWorld::IdTree& cellTable = dynamic_cast<CSMWorld::IdTree&>(
            *mData.getTableModel(CSMWorld::UniversalId(CSMWorld::UniversalId::Type_Cells)));
        
        ESM::RefId refId = ESM::RefId::stringRefId(cellName);
        int existingRow = mData.getCells().searchId(refId);
        
        if (existingRow >= 0 && !mState.overwriteExisting)
            return;
        
        if (existingRow < 0)
        {
            auto createCmd = std::make_unique<CSMWorld::CreateCommand>(cellTable, cellName);
            
            int parentIndex = cellTable.findColumnIndex(CSMWorld::Columns::ColumnId_Cell);
            int interiorIndex = cellTable.findNestedColumnIndex(parentIndex, CSMWorld::Columns::ColumnId_Interior);
            createCmd->addNestedValue(parentIndex, interiorIndex, true);
            
            // Dark cave lighting
            int ambientColumn = cellTable.findColumnIndex(CSMWorld::Columns::ColumnId_Ambient);
            if (ambientColumn >= 0)
            {
                int ambientColor = (20 << 16) | (20 << 8) | 25; // Very dark blue-ish
                createCmd->addValue(ambientColumn, ambientColor);
            }
            
            mDocument.getUndoStack().push(createCmd.release());
        }
        
        // Generate BSP-based cave layout with cave-specific objects
        generateBSPInterior(cellName, roomCount);
        
        // Add cave-specific decorations (stalactites, rocks, etc.)
        std::vector<std::string> caveRocks;
        if (mState.assetLibrary)
        {
            caveRocks = getObjectsFromAssetLibrary(AssetCategory::CaveInterior);
        }
        
        // Place some rocks/stalactites if available
        if (!caveRocks.empty())
        {
            int decorCount = roomCount * 3;
            float roomSpread = mState.caveDungeon.caveRoomSizeMax * std::sqrt(static_cast<float>(roomCount));
            
            for (int i = 0; i < decorCount; ++i)
            {
                float x = mRng->nextFloatRange(-roomSpread / 2, roomSpread / 2);
                float y = mRng->nextFloatRange(-roomSpread / 2, roomSpread / 2);
                float z = mRng->nextFloatRange(0.0f, mState.interiors.ceilingHeight);
                
                std::string rockId = caveRocks[mRng->nextInt(static_cast<uint32_t>(caveRocks.size()))];
                createReference(rockId, cellName, x, y, z, mRng->nextFloatRange(0.0f, 6.28318f), 
                               mRng->nextFloatRange(0.8f, 1.2f));
            }
        }
    }
    
    void ProceduralGenerator::generateDungeonInterior(const std::string& cellName, int roomCount, int floor)
    {
        // Create interior cell
        CSMWorld::IdTree& cellTable = dynamic_cast<CSMWorld::IdTree&>(
            *mData.getTableModel(CSMWorld::UniversalId(CSMWorld::UniversalId::Type_Cells)));
        
        ESM::RefId refId = ESM::RefId::stringRefId(cellName);
        int existingRow = mData.getCells().searchId(refId);
        
        if (existingRow >= 0 && !mState.overwriteExisting)
            return;
        
        if (existingRow < 0)
        {
            auto createCmd = std::make_unique<CSMWorld::CreateCommand>(cellTable, cellName);
            
            int parentIndex = cellTable.findColumnIndex(CSMWorld::Columns::ColumnId_Cell);
            int interiorIndex = cellTable.findNestedColumnIndex(parentIndex, CSMWorld::Columns::ColumnId_Interior);
            createCmd->addNestedValue(parentIndex, interiorIndex, true);
            
            // Dungeon lighting (slightly less dark than caves)
            int ambientColumn = cellTable.findColumnIndex(CSMWorld::Columns::ColumnId_Ambient);
            if (ambientColumn >= 0)
            {
                int ambientColor = (30 << 16) | (25 << 8) | 20; // Dim brownish
                createCmd->addValue(ambientColumn, ambientColor);
            }
            
            mDocument.getUndoStack().push(createCmd.release());
        }
        
        // Generate BSP-based dungeon layout
        generateBSPInterior(cellName, roomCount);
        
        // Add dungeon-specific decorations
        std::vector<std::string> dungeonDecor;
        if (mState.assetLibrary)
        {
            dungeonDecor = getObjectsFromAssetLibrary(AssetCategory::DungeonInterior);
        }
        
        // Add loot containers if enabled
        if (mState.caveDungeon.generateLoot)
        {
            std::vector<std::string> containers;
            if (mState.assetLibrary)
            {
                containers = getObjectsFromAssetLibrary(AssetCategory::Container);
            }
            
            if (containers.empty())
            {
                containers = {"chest_small_01", "chest_common_01", "urn_01"};
            }
            
            int containerCount = roomCount / 2 + 1;
            float roomSpread = mState.caveDungeon.dungeonRoomSizeMax * std::sqrt(static_cast<float>(roomCount));
            
            for (int i = 0; i < containerCount; ++i)
            {
                float x = mRng->nextFloatRange(-roomSpread / 2, roomSpread / 2);
                float y = mRng->nextFloatRange(-roomSpread / 2, roomSpread / 2);
                
                std::string containerId = containers[mRng->nextInt(static_cast<uint32_t>(containers.size()))];
                createReference(containerId, cellName, x, y, 0.0f, 
                               mRng->nextFloatRange(0.0f, 6.28318f), 1.0f);
            }
        }
    }
}
