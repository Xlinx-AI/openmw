#ifndef OPENMW_MWRENDER_VRAMMANAGEMENT_H
#define OPENMW_MWRENDER_VRAMMANAGEMENT_H

#include <osg/ref_ptr>
#include <osg/Texture>
#include <osg/Geometry>

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <string>
#include <chrono>

namespace osg
{
    class StateSet;
    class Node;
    class Image;
}

namespace Resource
{
    class ResourceSystem;
}

namespace MWRender
{
    /// VRAM Management system for unloading unused resources and managing GPU memory
    class VRAMManagement
    {
    public:
        VRAMManagement(Resource::ResourceSystem* resourceSystem);
        ~VRAMManagement();

        /// Update the VRAM management system
        void update(float dt);

        /// Process changed settings
        void processChangedSettings();

        /// Mark a resource as used this frame
        void markResourceUsed(osg::Texture* texture);
        void markResourceUsed(osg::Geometry* geometry);

        /// Force unload of a specific resource
        void unloadTexture(osg::Texture* texture);
        void unloadGeometry(osg::Geometry* geometry);

        /// Get estimated VRAM usage in MB
        size_t getEstimatedVRAMUsage() const { return mEstimatedVRAMUsage / (1024 * 1024); }

        /// Get statistics
        struct Stats
        {
            size_t totalTextures;
            size_t loadedTextures;
            size_t swappedTextures;
            size_t totalGeometry;
            size_t loadedGeometry;
            size_t estimatedVRAMUsageMB;
            size_t targetVRAMUsageMB;
        };
        Stats getStats() const;

        /// Enable/disable
        void setEnabled(bool enabled) { mEnabled = enabled; }
        bool isEnabled() const { return mEnabled; }

    private:
        struct TextureEntry
        {
            osg::ref_ptr<osg::Texture> texture;
            osg::ref_ptr<osg::Image> swappedImage; // RAM copy when unloaded from VRAM
            std::chrono::steady_clock::time_point lastUsed;
            size_t estimatedSize;
            bool isLoaded;
            bool isDuplicate;
            std::string hash;
        };

        struct GeometryEntry
        {
            osg::ref_ptr<osg::Geometry> geometry;
            std::chrono::steady_clock::time_point lastUsed;
            size_t estimatedSize;
            bool isLoaded;
            std::string hash;
        };

        void unloadUnusedResources();
        void detectDuplicates();
        size_t estimateTextureSize(osg::Texture* texture) const;
        size_t estimateGeometrySize(osg::Geometry* geometry) const;
        std::string computeTextureHash(osg::Texture* texture) const;
        std::string computeGeometryHash(osg::Geometry* geometry) const;
        void swapTextureToRAM(TextureEntry& entry);
        void reloadTextureToVRAM(TextureEntry& entry);

        Resource::ResourceSystem* mResourceSystem;

        std::unordered_map<osg::Texture*, TextureEntry> mTextures;
        std::unordered_map<osg::Geometry*, GeometryEntry> mGeometry;
        std::unordered_map<std::string, osg::ref_ptr<osg::Texture>> mTextureDeduplication;

        // Settings
        bool mEnabled;
        size_t mMaxVRAMUsage; // bytes
        bool mUnloadStaticGeometry;
        bool mUnloadDuplicateTextures;
        bool mEnableTextureCompression;
        bool mEnableGeometryDeduplication;
        float mUnloadDelay;
        bool mEnableSwapToRAM;
        size_t mMaxRAMSwapUsage; // bytes
        bool mEnableTextureStreaming;
        float mMipmapBias;
        bool mEnableGeometryLOD;

        size_t mEstimatedVRAMUsage;
        size_t mEstimatedRAMSwapUsage;
        float mUpdateTimer;
        static constexpr float UPDATE_INTERVAL = 1.0f; // Update every second
    };

    /// Texture streaming system for loading textures on-demand
    class TextureStreaming
    {
    public:
        TextureStreaming(Resource::ResourceSystem* resourceSystem);
        ~TextureStreaming();

        /// Request a texture to be loaded
        void requestTexture(const std::string& path, int priority);

        /// Update streaming (call each frame)
        void update(float dt);

        /// Check if a texture is loaded
        bool isTextureLoaded(const std::string& path) const;

        /// Get loaded texture
        osg::Texture* getTexture(const std::string& path);

    private:
        struct StreamRequest
        {
            std::string path;
            int priority;
            bool isLoading;
        };

        Resource::ResourceSystem* mResourceSystem;
        std::vector<StreamRequest> mPendingRequests;
        std::unordered_map<std::string, osg::ref_ptr<osg::Texture>> mLoadedTextures;
        int mMaxLoadsPerFrame;
    };
}

#endif
