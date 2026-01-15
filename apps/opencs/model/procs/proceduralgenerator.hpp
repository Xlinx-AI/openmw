#ifndef CSM_PROCS_PROCEDURALGENERATOR_H
#define CSM_PROCS_PROCEDURALGENERATOR_H

#include <functional>
#include <memory>
#include <string>
#include <vector>

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
    /// Progress callback function type
    using ProgressCallback = std::function<void(int current, int total, const std::string& message)>;

    /// Main procedural world generator
    class ProceduralGenerator
    {
    public:
        ProceduralGenerator(CSMDoc::Document& document);
        ~ProceduralGenerator();
        
        /// Set the generation state/parameters
        void setState(const ProceduralState& state);
        
        /// Get current state
        const ProceduralState& getState() const { return mState; }
        ProceduralState& getState() { return mState; }
        
        /// Set progress callback
        void setProgressCallback(ProgressCallback callback);
        
        /// Generate the world based on current state
        /// @return true if generation was successful
        bool generate();
        
        /// Generate terrain only
        bool generateTerrain();
        
        /// Generate objects only
        bool generateObjects();
        
        /// Generate interiors only
        bool generateInteriors();
        
        /// Generate pathgrids
        bool generatePathgrids();
        
        /// Preview a single cell
        /// @return true if preview was successful
        bool previewCell(int cellX, int cellY);
        
        /// Cancel ongoing generation
        void cancel();
        
        /// Check if generation is in progress
        bool isRunning() const { return mRunning; }
        
    private:
        CSMDoc::Document& mDocument;
        CSMWorld::Data& mData;
        ProceduralState mState;
        ProgressCallback mProgressCallback;
        bool mRunning = false;
        bool mCancelled = false;
        
        // Noise generators
        std::unique_ptr<PerlinNoise> mPerlin;
        std::unique_ptr<VoronoiNoise> mVoronoi;
        std::unique_ptr<RandomGenerator> mRng;
        
        /// Initialize noise generators with current seed
        void initializeNoise();
        
        /// Generate height for a specific world position
        float generateHeight(float worldX, float worldY) const;
        
        /// Generate terrain textures for a cell
        std::vector<uint16_t> generateTextures(int cellX, int cellY) const;
        
        /// Create or update a cell
        void createCell(int cellX, int cellY);
        
        /// Create or update land data for a cell
        void createLand(int cellX, int cellY);
        
        /// Place objects in a cell
        void placeObjectsInCell(int cellX, int cellY);
        
        /// Select appropriate object based on terrain and rules
        std::string selectObject(const std::string& category, float terrainHeight, float slope) const;
        
        /// Get slope at position
        float getSlopeAt(float worldX, float worldY) const;
        
        /// Get texture based on height and slope
        int getTextureForTerrain(float height, float slope) const;
        
        /// Create an interior cell
        void createInterior(const std::string& name, int roomCount);
        
        /// Generate pathgrid for a cell
        void generatePathgridForCell(int cellX, int cellY);
        
        /// Report progress
        void reportProgress(int current, int total, const std::string& message);
        
        /// Get list of objects from reference for given category
        std::vector<std::string> getObjectsFromReference(const std::string& category) const;
        
        /// Calculate total cells to generate
        int getTotalCells() const;
    };
}

#endif
