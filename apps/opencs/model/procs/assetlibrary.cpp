#include "assetlibrary.hpp"

#include <algorithm>
#include <cctype>

#include <apps/opencs/model/world/data.hpp>
#include <apps/opencs/model/world/refidcollection.hpp>

namespace CSMProcs
{
    const std::vector<AssetInfo> AssetLibrary::sEmptyAssets;

    AssetLibrary::AssetLibrary() = default;

    AssetLibrary::AssetLibrary(CSMWorld::Data& data)
    {
        scanAvailableAssets(data);
    }

    void AssetLibrary::scanAvailableAssets(CSMWorld::Data& data)
    {
        // Clear existing assets
        mAssets.clear();

        // Scan referenceables for categorizable objects
        const auto& refIdCollection = data.getReferenceables();

        for (int i = 0; i < refIdCollection.getSize(); ++i)
        {
            std::string refId = refIdCollection.getId(i).getRefIdString();

            // Skip deleted or modified-as-deleted records
            if (refId.empty())
                continue;

            AssetCategory category = categorizeAsset(refId);

            AssetInfo info;
            info.id = refId;
            info.name = refId; // Use ID as name for now
            info.source = "loaded"; // Mark as loaded from data
            info.category = category;

            // Set default placement hints based on category
            switch (category)
            {
                case AssetCategory::Tree:
                    info.slopeLimit = 0.4f;
                    info.alignToTerrain = false;
                    info.minScale = 0.7f;
                    info.maxScale = 1.3f;
                    break;
                case AssetCategory::Rock:
                    info.slopeLimit = 0.8f;
                    info.alignToTerrain = true;
                    info.minScale = 0.6f;
                    info.maxScale = 1.5f;
                    break;
                case AssetCategory::Grass:
                case AssetCategory::Bush:
                    info.slopeLimit = 0.5f;
                    info.alignToTerrain = true;
                    info.minScale = 0.8f;
                    info.maxScale = 1.2f;
                    break;
                case AssetCategory::Building:
                    info.slopeLimit = 0.2f;
                    info.alignToTerrain = false;
                    info.hasInterior = true; // Assume buildings have interiors
                    break;
                case AssetCategory::Wall:
                case AssetCategory::WallGate:
                case AssetCategory::WallTower:
                    info.slopeLimit = 0.3f;
                    info.alignToTerrain = false;
                    break;
                case AssetCategory::Light:
                case AssetCategory::Container:
                case AssetCategory::Furniture:
                    info.alignToTerrain = false;
                    break;
                default:
                    break;
            }

            mAssets[category].push_back(info);
        }
    }

    const std::vector<AssetInfo>& AssetLibrary::getAssets(AssetCategory category) const
    {
        auto it = mAssets.find(category);
        if (it != mAssets.end())
            return it->second;
        return sEmptyAssets;
    }

    std::vector<std::string> AssetLibrary::getAssetIds(AssetCategory category) const
    {
        std::vector<std::string> ids;
        const auto& assets = getAssets(category);
        ids.reserve(assets.size());
        for (const auto& asset : assets)
        {
            ids.push_back(asset.id);
        }
        return ids;
    }

    void AssetLibrary::addAsset(AssetCategory category, const AssetInfo& asset)
    {
        mAssets[category].push_back(asset);
    }

    void AssetLibrary::removeAsset(AssetCategory category, const std::string& id)
    {
        auto it = mAssets.find(category);
        if (it != mAssets.end())
        {
            auto& assets = it->second;
            assets.erase(std::remove_if(assets.begin(), assets.end(),
                                        [&id](const AssetInfo& info) { return info.id == id; }),
                assets.end());
        }
    }

    void AssetLibrary::clearCategory(AssetCategory category)
    {
        mAssets[category].clear();
    }

    void AssetLibrary::setAssets(AssetCategory category, const std::vector<AssetInfo>& assets)
    {
        mAssets[category] = assets;
    }

    void AssetLibrary::setAssetIds(AssetCategory category, const std::vector<std::string>& ids)
    {
        std::vector<AssetInfo> assets;
        assets.reserve(ids.size());
        for (const auto& id : ids)
        {
            AssetInfo info;
            info.id = id;
            info.name = id;
            info.category = category;
            assets.push_back(info);
        }
        mAssets[category] = assets;
    }

    AssetCategory AssetLibrary::categorizeAsset(const std::string& id)
    {
        // Convert to lowercase for pattern matching
        std::string lower = id;
        std::transform(lower.begin(), lower.end(), lower.begin(),
            [](unsigned char c) { return std::tolower(c); });

        // Trees
        if (lower.find("tree") != std::string::npos || lower.find("flora_tree") != std::string::npos)
            return AssetCategory::Tree;

        // Rocks
        if (lower.find("rock") != std::string::npos || lower.find("terrain_rock") != std::string::npos ||
            lower.find("stone") != std::string::npos || lower.find("boulder") != std::string::npos)
            return AssetCategory::Rock;

        // Grass
        if (lower.find("grass") != std::string::npos || lower.find("flora_grass") != std::string::npos)
            return AssetCategory::Grass;

        // Bushes
        if (lower.find("bush") != std::string::npos || lower.find("shrub") != std::string::npos ||
            lower.find("fern") != std::string::npos)
            return AssetCategory::Bush;

        // Buildings
        if (lower.find("house") != std::string::npos || lower.find("building") != std::string::npos ||
            lower.find("hut") != std::string::npos || lower.find("shack") != std::string::npos ||
            lower.find("manor") != std::string::npos || lower.find("tower") != std::string::npos ||
            lower.find("tavern") != std::string::npos || lower.find("inn") != std::string::npos ||
            lower.find("shop") != std::string::npos || lower.find("temple") != std::string::npos ||
            lower.find("guild") != std::string::npos)
            return AssetCategory::Building;

        // Walls
        if (lower.find("wall") != std::string::npos)
        {
            if (lower.find("gate") != std::string::npos)
                return AssetCategory::WallGate;
            if (lower.find("tower") != std::string::npos)
                return AssetCategory::WallTower;
            return AssetCategory::Wall;
        }

        // Fences
        if (lower.find("fence") != std::string::npos || lower.find("palisade") != std::string::npos)
            return AssetCategory::Fence;

        // Roads
        if (lower.find("road") != std::string::npos || lower.find("path") != std::string::npos)
        {
            if (lower.find("cobble") != std::string::npos)
                return AssetCategory::CobblestoneRoad;
            return AssetCategory::Road;
        }

        // Bridges
        if (lower.find("bridge") != std::string::npos)
            return AssetCategory::Bridge;

        // Docks
        if (lower.find("dock") != std::string::npos || lower.find("pier") != std::string::npos)
            return AssetCategory::Dock;

        // Caves
        if (lower.find("cave") != std::string::npos)
        {
            if (lower.find("entrance") != std::string::npos || lower.find("door") != std::string::npos)
                return AssetCategory::CaveEntrance;
            return AssetCategory::CaveInterior;
        }

        // Dungeons
        if (lower.find("dungeon") != std::string::npos || lower.find("crypt") != std::string::npos ||
            lower.find("tomb") != std::string::npos)
        {
            if (lower.find("entrance") != std::string::npos || lower.find("door") != std::string::npos)
                return AssetCategory::DungeonEntrance;
            return AssetCategory::DungeonInterior;
        }

        // Castles
        if (lower.find("castle") != std::string::npos || lower.find("fort") != std::string::npos)
        {
            if (lower.find("wall") != std::string::npos)
                return AssetCategory::CastleWall;
            if (lower.find("gallery") != std::string::npos || lower.find("walkway") != std::string::npos)
                return AssetCategory::CastleGallery;
            if (lower.find("stair") != std::string::npos || lower.find("ramp") != std::string::npos)
                return AssetCategory::CastleStairs;
            return AssetCategory::Building;
        }

        // Lights
        if (lower.find("light") != std::string::npos || lower.find("torch") != std::string::npos ||
            lower.find("lamp") != std::string::npos || lower.find("candle") != std::string::npos ||
            lower.find("lantern") != std::string::npos)
            return AssetCategory::Light;

        // Containers
        if (lower.find("contain") != std::string::npos || lower.find("chest") != std::string::npos ||
            lower.find("barrel") != std::string::npos || lower.find("crate") != std::string::npos ||
            lower.find("sack") != std::string::npos || lower.find("urn") != std::string::npos)
            return AssetCategory::Container;

        // Furniture
        if (lower.find("table") != std::string::npos || lower.find("chair") != std::string::npos ||
            lower.find("bed") != std::string::npos || lower.find("bench") != std::string::npos ||
            lower.find("shelf") != std::string::npos || lower.find("cabinet") != std::string::npos ||
            lower.find("desk") != std::string::npos || lower.find("throne") != std::string::npos)
            return AssetCategory::Furniture;

        // City props
        if (lower.find("sign") != std::string::npos || lower.find("banner") != std::string::npos ||
            lower.find("flag") != std::string::npos || lower.find("statue") != std::string::npos ||
            lower.find("fountain") != std::string::npos || lower.find("well") != std::string::npos ||
            lower.find("market") != std::string::npos)
            return AssetCategory::CityProp;

        // Farm items
        if (lower.find("farm") != std::string::npos || lower.find("hay") != std::string::npos ||
            lower.find("plow") != std::string::npos || lower.find("wheat") != std::string::npos ||
            lower.find("crop") != std::string::npos)
            return AssetCategory::Farm;

        // Landscape textures
        if (lower.find("tx_") != std::string::npos || lower.find("terrain_") != std::string::npos)
            return AssetCategory::LandscapeTexture;

        // Default to clutter for unrecognized items
        return AssetCategory::Clutter;
    }

    std::string AssetLibrary::getCategoryName(AssetCategory category)
    {
        switch (category)
        {
            case AssetCategory::Tree:
                return "Trees";
            case AssetCategory::Rock:
                return "Rocks";
            case AssetCategory::Grass:
                return "Grass";
            case AssetCategory::Bush:
                return "Bushes";
            case AssetCategory::Building:
                return "Buildings";
            case AssetCategory::BuildingInterior:
                return "Building Interiors";
            case AssetCategory::Road:
                return "Roads";
            case AssetCategory::CobblestoneRoad:
                return "Cobblestone Roads";
            case AssetCategory::Wall:
                return "Walls";
            case AssetCategory::WallGate:
                return "Wall Gates";
            case AssetCategory::WallTower:
                return "Wall Towers";
            case AssetCategory::CityProp:
                return "City Props";
            case AssetCategory::CaveEntrance:
                return "Cave Entrances";
            case AssetCategory::CaveInterior:
                return "Cave Interior";
            case AssetCategory::DungeonEntrance:
                return "Dungeon Entrances";
            case AssetCategory::DungeonInterior:
                return "Dungeon Interior";
            case AssetCategory::CastleWall:
                return "Castle Walls";
            case AssetCategory::CastleGallery:
                return "Castle Galleries";
            case AssetCategory::CastleStairs:
                return "Castle Stairs";
            case AssetCategory::Bridge:
                return "Bridges";
            case AssetCategory::Dock:
                return "Docks";
            case AssetCategory::Farm:
                return "Farm Items";
            case AssetCategory::Fence:
                return "Fences";
            case AssetCategory::Light:
                return "Lights";
            case AssetCategory::Container:
                return "Containers";
            case AssetCategory::Furniture:
                return "Furniture";
            case AssetCategory::Clutter:
                return "Clutter";
            case AssetCategory::LandscapeTexture:
                return "Landscape Textures";
            default:
                return "Unknown";
        }
    }

    std::vector<AssetCategory> AssetLibrary::getAllCategories()
    {
        return {AssetCategory::Tree,          AssetCategory::Rock,            AssetCategory::Grass,
            AssetCategory::Bush,              AssetCategory::Building,        AssetCategory::BuildingInterior,
            AssetCategory::Road,              AssetCategory::CobblestoneRoad, AssetCategory::Wall,
            AssetCategory::WallGate,          AssetCategory::WallTower,       AssetCategory::CityProp,
            AssetCategory::CaveEntrance,      AssetCategory::CaveInterior,    AssetCategory::DungeonEntrance,
            AssetCategory::DungeonInterior,   AssetCategory::CastleWall,      AssetCategory::CastleGallery,
            AssetCategory::CastleStairs,      AssetCategory::Bridge,          AssetCategory::Dock,
            AssetCategory::Farm,              AssetCategory::Fence,           AssetCategory::Light,
            AssetCategory::Container,         AssetCategory::Furniture,       AssetCategory::Clutter,
            AssetCategory::LandscapeTexture};
    }

    bool AssetLibrary::hasAssets() const
    {
        for (const auto& [category, assets] : mAssets)
        {
            if (!assets.empty())
                return true;
        }
        return false;
    }

    int AssetLibrary::getTotalAssetCount() const
    {
        int count = 0;
        for (const auto& [category, assets] : mAssets)
        {
            count += static_cast<int>(assets.size());
        }
        return count;
    }
}
