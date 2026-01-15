#ifndef CSM_PROCS_ASSETLIBRARY_H
#define CSM_PROCS_ASSETLIBRARY_H

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
    /// Asset category types for procedural generation
    enum class AssetCategory
    {
        Tree,
        Rock,
        Grass,
        Bush,
        Building,
        BuildingInterior, // Interior markers for buildings
        Road,
        CobblestoneRoad,
        Wall,
        WallGate,
        WallTower,
        CityProp, // Benches, signs, streetlights, etc.
        CaveEntrance,
        CaveInterior,
        DungeonEntrance,
        DungeonInterior,
        CastleWall,
        CastleGallery,
        CastleStairs,
        Bridge,
        Dock,
        Farm,
        Fence,
        Light, // Torches, lamps for interiors
        Container, // Chests, barrels, crates
        Furniture, // Tables, chairs, beds
        Clutter, // Misc decorative items
        LandscapeTexture
    };

    /// Information about a single asset
    struct AssetInfo
    {
        std::string id; // The ESM RefId string
        std::string name; // Human-readable name
        std::string source; // BSA or loose file path
        AssetCategory category = AssetCategory::Tree;
        
        // Placement hints
        float preferredScale = 1.0f;
        float minScale = 0.8f;
        float maxScale = 1.2f;
        bool alignToTerrain = true;
        float slopeLimit = 0.5f; // Max slope where this can be placed
        float minHeight = -10000.0f; // Min terrain height
        float maxHeight = 10000.0f; // Max terrain height
        
        // For buildings with interiors
        std::string linkedInteriorId; // Interior cell ID pattern
        bool hasInterior = false;
        
        // For grouped assets (e.g., wall segments)
        std::string groupId; // Group identifier for related assets
        int groupIndex = 0; // Order within group (e.g., wall start, middle, end)
    };

    /// Configuration for settlement walls
    struct WallConfig
    {
        bool enabled = false;
        std::vector<std::string> wallSegmentIds; // Wall segment objects
        std::vector<std::string> gateIds; // Gate objects
        std::vector<std::string> towerIds; // Tower objects
        float wallHeight = 500.0f;
        float towerSpacing = 300.0f; // Distance between towers
        bool moat = false;
        float moatWidth = 100.0f;
        int gateCount = 1; // Number of gates
    };

    /// Configuration for settlement roads
    struct RoadConfig
    {
        bool enabled = true;
        std::vector<std::string> mainRoadTextureIds; // Main road textures
        std::vector<std::string> sideRoadTextureIds; // Side street textures
        std::vector<std::string> cobblestoneIds; // Cobblestone objects if using 3D
        float mainRoadWidth = 200.0f;
        float sideRoadWidth = 100.0f;
        bool use3DRoads = false; // Use cobblestone objects vs texture
    };

    /// Configuration for interior generation
    struct InteriorConfig
    {
        std::vector<std::string> floorIds;
        std::vector<std::string> wallIds;
        std::vector<std::string> ceilingIds;
        std::vector<std::string> doorIds;
        std::vector<std::string> lightIds;
        std::vector<std::string> furnitureIds;
        std::vector<std::string> containerIds;
        std::vector<std::string> clutterIds;
        float ceilingHeight = 300.0f;
        float lightSpacing = 200.0f;
        float clutterDensity = 0.4f;
    };

    /// Configuration for cave/dungeon generation
    struct CaveDungeonConfig
    {
        bool enabled = true;
        std::vector<std::string> entranceIds;
        std::vector<std::string> floorIds;
        std::vector<std::string> wallIds;
        std::vector<std::string> ceilingIds;
        std::vector<std::string> rockIds;
        std::vector<std::string> stalactiteIds;
        std::vector<std::string> lightIds; // Torches, glowing mushrooms
        int minRooms = 3;
        int maxRooms = 15;
        float roomSizeMin = 300.0f;
        float roomSizeMax = 1000.0f;
        float corridorWidth = 150.0f;
        bool underwater = false;
        float waterLevel = 0.0f;
    };

    /// User-configurable asset library for procedural generation
    /// Allows users to define their own object lists from BSA and loose files
    class AssetLibrary
    {
    public:
        AssetLibrary();
        explicit AssetLibrary(CSMWorld::Data& data);
        
        /// Scan data for available assets by category
        void scanAvailableAssets(CSMWorld::Data& data);
        
        /// Get all assets in a category
        const std::vector<AssetInfo>& getAssets(AssetCategory category) const;
        
        /// Get asset IDs for a category (simple string list)
        std::vector<std::string> getAssetIds(AssetCategory category) const;
        
        /// Add an asset to a category
        void addAsset(AssetCategory category, const AssetInfo& asset);
        
        /// Remove an asset from a category
        void removeAsset(AssetCategory category, const std::string& id);
        
        /// Clear all assets in a category
        void clearCategory(AssetCategory category);
        
        /// Set assets for a category (replace all)
        void setAssets(AssetCategory category, const std::vector<AssetInfo>& assets);
        
        /// Set asset IDs for a category (simple string list)
        void setAssetIds(AssetCategory category, const std::vector<std::string>& ids);
        
        /// Get settlement configuration
        SettlementType getSettlementType() const { return mSettlementType; }
        void setSettlementType(SettlementType type) { mSettlementType = type; }
        
        /// Get wall configuration
        const WallConfig& getWallConfig() const { return mWallConfig; }
        WallConfig& getWallConfig() { return mWallConfig; }
        void setWallConfig(const WallConfig& config) { mWallConfig = config; }
        
        /// Get road configuration
        const RoadConfig& getRoadConfig() const { return mRoadConfig; }
        RoadConfig& getRoadConfig() { return mRoadConfig; }
        void setRoadConfig(const RoadConfig& config) { mRoadConfig = config; }
        
        /// Get interior configuration
        const InteriorConfig& getInteriorConfig() const { return mInteriorConfig; }
        InteriorConfig& getInteriorConfig() { return mInteriorConfig; }
        void setInteriorConfig(const InteriorConfig& config) { mInteriorConfig = config; }
        
        /// Get cave/dungeon configuration
        const CaveDungeonConfig& getCaveConfig() const { return mCaveConfig; }
        CaveDungeonConfig& getCaveConfig() { return mCaveConfig; }
        void setCaveConfig(const CaveDungeonConfig& config) { mCaveConfig = config; }
        
        /// Auto-categorize assets based on naming patterns
        static AssetCategory categorizeAsset(const std::string& id);
        
        /// Get human-readable category name
        static std::string getCategoryName(AssetCategory category);
        
        /// Get all category types
        static std::vector<AssetCategory> getAllCategories();
        
        /// Check if any assets are configured
        bool hasAssets() const;
        
        /// Get count of assets across all categories
        int getTotalAssetCount() const;
        
    private:
        std::map<AssetCategory, std::vector<AssetInfo>> mAssets;
        SettlementType mSettlementType = SettlementType::Village;
        WallConfig mWallConfig;
        RoadConfig mRoadConfig;
        InteriorConfig mInteriorConfig;
        CaveDungeonConfig mCaveConfig;
        
        // Empty vector for returning when category not found
        static const std::vector<AssetInfo> sEmptyAssets;
    };
}

#endif
