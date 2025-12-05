#include "vrammanagement.hpp"

#include <osg/StateSet>
#include <osg/Image>
#include <osg/Texture2D>
#include <osg/Array>

#include <components/resource/resourcesystem.hpp>
#include <components/resource/scenemanager.hpp>
#include <components/sceneutil/color.hpp>
#include <components/sceneutil/glextensions.hpp>
#include <components/settings/values.hpp>

#include <algorithm>
#include <functional>

namespace MWRender
{
    VRAMManagement::VRAMManagement(Resource::ResourceSystem* resourceSystem)
        : mResourceSystem(resourceSystem)
        , mEnabled(true)
        , mMaxVRAMUsage(0)
        , mUnloadStaticGeometry(true)
        , mUnloadDuplicateTextures(true)
        , mEnableTextureCompression(true)
        , mEnableGeometryDeduplication(true)
        , mUnloadDelay(5.0f)
        , mEnableSwapToRAM(true)
        , mMaxRAMSwapUsage(0)
        , mEnableTextureStreaming(false)
        , mMipmapBias(0.0f)
        , mEnableGeometryLOD(true)
        , mEstimatedVRAMUsage(0)
        , mEstimatedRAMSwapUsage(0)
        , mUpdateTimer(0.0f)
    {
        processChangedSettings();
    }

    VRAMManagement::~VRAMManagement()
    {
        mTextures.clear();
        mGeometry.clear();
        mTextureDeduplication.clear();
    }

    void VRAMManagement::processChangedSettings()
    {
        const auto& settings = Settings::vramManagement();

        mEnabled = settings.mEnableVramManagement;
        mMaxVRAMUsage = static_cast<size_t>(settings.mMaxVramUsage) * 1024 * 1024; // Convert MB to bytes
        mUnloadStaticGeometry = settings.mUnloadStaticGeometry;
        mUnloadDuplicateTextures = settings.mUnloadDuplicateTextures;
        mEnableTextureCompression = settings.mEnableTextureCompression;
        mEnableGeometryDeduplication = settings.mEnableGeometryDeduplication;
        mUnloadDelay = settings.mUnloadDelay;
        mEnableSwapToRAM = settings.mEnableSwapToRam;
        mMaxRAMSwapUsage = static_cast<size_t>(settings.mMaxRamSwapUsage) * 1024 * 1024;
        mEnableTextureStreaming = settings.mEnableTextureStreaming;
        mMipmapBias = settings.mMipmapBias;
        mEnableGeometryLOD = settings.mEnableGeometryLod;

        // Auto-detect VRAM if not specified
        if (mMaxVRAMUsage == 0)
        {
            // Default to 1.5GB for a 2GB card like GTX 960, leaving headroom for system
            mMaxVRAMUsage = 1536 * 1024 * 1024;
        }

        // Auto-detect RAM swap if not specified
        if (mMaxRAMSwapUsage == 0)
        {
            // Default to 2GB for 12GB system
            mMaxRAMSwapUsage = 2048ULL * 1024 * 1024;
        }
    }

    void VRAMManagement::update(float dt)
    {
        if (!mEnabled)
            return;

        mUpdateTimer += dt;
        if (mUpdateTimer < UPDATE_INTERVAL)
            return;

        mUpdateTimer = 0.0f;

        // Check if we need to unload resources
        if (mEstimatedVRAMUsage > mMaxVRAMUsage)
        {
            unloadUnusedResources();
        }

        // Detect duplicates periodically
        if (mUnloadDuplicateTextures || mEnableGeometryDeduplication)
        {
            detectDuplicates();
        }
    }

    void VRAMManagement::unloadUnusedResources()
    {
        auto now = std::chrono::steady_clock::now();

        // Collect textures to unload
        std::vector<osg::Texture*> texturesToUnload;
        for (auto& pair : mTextures)
        {
            if (!pair.second.isLoaded)
                continue;

            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - pair.second.lastUsed).count();

            if (elapsed > static_cast<long>(mUnloadDelay))
            {
                texturesToUnload.push_back(pair.first);
            }
        }

        // Sort by size (unload largest first)
        std::sort(texturesToUnload.begin(), texturesToUnload.end(),
            [this](osg::Texture* a, osg::Texture* b)
            {
                return mTextures[a].estimatedSize > mTextures[b].estimatedSize;
            });

        // Unload until we're under the limit
        for (osg::Texture* tex : texturesToUnload)
        {
            if (mEstimatedVRAMUsage <= mMaxVRAMUsage * 0.9f) // Leave 10% headroom
                break;

            if (mEnableSwapToRAM && mEstimatedRAMSwapUsage < mMaxRAMSwapUsage)
            {
                swapTextureToRAM(mTextures[tex]);
            }
            else
            {
                unloadTexture(tex);
            }
        }

        // Similar process for geometry if enabled
        if (mUnloadStaticGeometry)
        {
            std::vector<osg::Geometry*> geometryToUnload;
            for (auto& pair : mGeometry)
            {
                if (!pair.second.isLoaded)
                    continue;

                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    now - pair.second.lastUsed).count();

                if (elapsed > static_cast<long>(mUnloadDelay))
                {
                    geometryToUnload.push_back(pair.first);
                }
            }

            for (osg::Geometry* geom : geometryToUnload)
            {
                if (mEstimatedVRAMUsage <= mMaxVRAMUsage * 0.9f)
                    break;

                unloadGeometry(geom);
            }
        }
    }

    void VRAMManagement::detectDuplicates()
    {
        if (!mUnloadDuplicateTextures)
            return;

        // Build hash map for texture deduplication
        std::unordered_map<std::string, std::vector<TextureEntry*>> hashGroups;

        for (auto& pair : mTextures)
        {
            if (pair.second.hash.empty())
            {
                pair.second.hash = computeTextureHash(pair.first);
            }

            if (!pair.second.hash.empty())
            {
                hashGroups[pair.second.hash].push_back(&pair.second);
            }
        }

        // Mark duplicates
        for (auto& group : hashGroups)
        {
            if (group.second.size() > 1)
            {
                // Keep the first one, mark others as duplicates
                for (size_t i = 1; i < group.second.size(); ++i)
                {
                    group.second[i]->isDuplicate = true;
                }
            }
        }
    }

    void VRAMManagement::markResourceUsed(osg::Texture* texture)
    {
        if (!texture)
            return;

        auto it = mTextures.find(texture);
        if (it == mTextures.end())
        {
            // Register new texture
            TextureEntry entry;
            entry.texture = texture;
            entry.lastUsed = std::chrono::steady_clock::now();
            entry.estimatedSize = estimateTextureSize(texture);
            entry.isLoaded = true;
            entry.isDuplicate = false;
            mTextures[texture] = entry;
            mEstimatedVRAMUsage += entry.estimatedSize;
        }
        else
        {
            it->second.lastUsed = std::chrono::steady_clock::now();

            // Reload if it was swapped
            if (!it->second.isLoaded && it->second.swappedImage)
            {
                reloadTextureToVRAM(it->second);
            }
        }
    }

    void VRAMManagement::markResourceUsed(osg::Geometry* geometry)
    {
        if (!geometry)
            return;

        auto it = mGeometry.find(geometry);
        if (it == mGeometry.end())
        {
            GeometryEntry entry;
            entry.geometry = geometry;
            entry.lastUsed = std::chrono::steady_clock::now();
            entry.estimatedSize = estimateGeometrySize(geometry);
            entry.isLoaded = true;
            mGeometry[geometry] = entry;
            mEstimatedVRAMUsage += entry.estimatedSize;
        }
        else
        {
            it->second.lastUsed = std::chrono::steady_clock::now();
        }
    }

    void VRAMManagement::unloadTexture(osg::Texture* texture)
    {
        auto it = mTextures.find(texture);
        if (it == mTextures.end() || !it->second.isLoaded)
            return;

        mEstimatedVRAMUsage -= it->second.estimatedSize;
        it->second.isLoaded = false;

        // Release GPU memory by clearing the image
        if (osg::Texture2D* tex2d = dynamic_cast<osg::Texture2D*>(texture))
        {
            tex2d->setImage(nullptr);
        }
    }

    void VRAMManagement::unloadGeometry(osg::Geometry* geometry)
    {
        auto it = mGeometry.find(geometry);
        if (it == mGeometry.end() || !it->second.isLoaded)
            return;

        mEstimatedVRAMUsage -= it->second.estimatedSize;
        it->second.isLoaded = false;

        // Mark VBOs for release
        geometry->releaseGLObjects();
    }

    void VRAMManagement::swapTextureToRAM(TextureEntry& entry)
    {
        if (!entry.isLoaded || !entry.texture)
            return;

        osg::Texture2D* tex2d = dynamic_cast<osg::Texture2D*>(entry.texture.get());
        if (!tex2d)
            return;

        // Copy image to RAM before unloading
        osg::Image* image = tex2d->getImage();
        if (image)
        {
            entry.swappedImage = new osg::Image(*image, osg::CopyOp::DEEP_COPY_ALL);
            mEstimatedRAMSwapUsage += entry.estimatedSize;
        }

        mEstimatedVRAMUsage -= entry.estimatedSize;
        entry.isLoaded = false;

        // Clear GPU copy
        tex2d->setImage(nullptr);
    }

    void VRAMManagement::reloadTextureToVRAM(TextureEntry& entry)
    {
        if (entry.isLoaded || !entry.swappedImage)
            return;

        osg::Texture2D* tex2d = dynamic_cast<osg::Texture2D*>(entry.texture.get());
        if (!tex2d)
            return;

        tex2d->setImage(entry.swappedImage);
        entry.isLoaded = true;
        mEstimatedVRAMUsage += entry.estimatedSize;

        // Release RAM copy
        mEstimatedRAMSwapUsage -= entry.estimatedSize;
        entry.swappedImage = nullptr;
    }

    size_t VRAMManagement::estimateTextureSize(osg::Texture* texture) const
    {
        if (!texture)
            return 0;

        osg::Texture2D* tex2d = dynamic_cast<osg::Texture2D*>(texture);
        if (!tex2d)
            return 0;

        int width = tex2d->getTextureWidth();
        int height = tex2d->getTextureHeight();

        if (width == 0 || height == 0)
        {
            osg::Image* image = tex2d->getImage();
            if (image)
            {
                width = image->s();
                height = image->t();
            }
        }

        // Estimate bytes per pixel based on internal format
        int bpp = 4; // Default RGBA
        GLenum format = tex2d->getInternalFormat();
        switch (format)
        {
            case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
            case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
                bpp = 1; // 0.5 bytes per pixel, but use 1 for safety
                break;
            case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
            case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
                bpp = 1;
                break;
            case GL_RGB:
            case GL_RGB8:
                bpp = 3;
                break;
            case GL_RGBA:
            case GL_RGBA8:
                bpp = 4;
                break;
            case GL_RGBA16F:
                bpp = 8;
                break;
            default:
                bpp = 4;
        }

        size_t baseSize = static_cast<size_t>(width) * height * bpp;

        // Add mipmaps (roughly 1.33x base size)
        if (tex2d->getUseHardwareMipMapGeneration() ||
            tex2d->getFilter(osg::Texture::MIN_FILTER) != osg::Texture::LINEAR)
        {
            baseSize = static_cast<size_t>(baseSize * 1.33f);
        }

        return baseSize;
    }

    size_t VRAMManagement::estimateGeometrySize(osg::Geometry* geometry) const
    {
        if (!geometry)
            return 0;

        size_t totalSize = 0;

        // Vertex array
        if (osg::Array* vertices = geometry->getVertexArray())
        {
            totalSize += vertices->getTotalDataSize();
        }

        // Normal array
        if (osg::Array* normals = geometry->getNormalArray())
        {
            totalSize += normals->getTotalDataSize();
        }

        // Texture coordinates
        for (unsigned int i = 0; i < geometry->getNumTexCoordArrays(); ++i)
        {
            if (osg::Array* texcoords = geometry->getTexCoordArray(i))
            {
                totalSize += texcoords->getTotalDataSize();
            }
        }

        // Color array
        if (osg::Array* colors = geometry->getColorArray())
        {
            totalSize += colors->getTotalDataSize();
        }

        // Index arrays
        for (unsigned int i = 0; i < geometry->getNumPrimitiveSets(); ++i)
        {
            if (osg::PrimitiveSet* ps = geometry->getPrimitiveSet(i))
            {
                if (osg::DrawElements* de = dynamic_cast<osg::DrawElements*>(ps))
                {
                    totalSize += de->getTotalDataSize();
                }
            }
        }

        return totalSize;
    }

    std::string VRAMManagement::computeTextureHash(osg::Texture* texture) const
    {
        // Simple hash based on texture properties
        // A full implementation would hash the actual pixel data
        osg::Texture2D* tex2d = dynamic_cast<osg::Texture2D*>(texture);
        if (!tex2d)
            return "";

        std::hash<std::string> hasher;
        std::string data = std::to_string(tex2d->getTextureWidth()) + "_" +
                          std::to_string(tex2d->getTextureHeight()) + "_" +
                          std::to_string(tex2d->getInternalFormat());

        return std::to_string(hasher(data));
    }

    std::string VRAMManagement::computeGeometryHash(osg::Geometry* geometry) const
    {
        if (!geometry)
            return "";

        std::hash<std::string> hasher;
        std::string data = std::to_string(reinterpret_cast<uintptr_t>(geometry->getVertexArray())) + "_" +
                          std::to_string(geometry->getNumPrimitiveSets());

        return std::to_string(hasher(data));
    }

    VRAMManagement::Stats VRAMManagement::getStats() const
    {
        Stats stats;
        stats.totalTextures = mTextures.size();
        stats.loadedTextures = 0;
        stats.swappedTextures = 0;
        stats.totalGeometry = mGeometry.size();
        stats.loadedGeometry = 0;

        for (const auto& pair : mTextures)
        {
            if (pair.second.isLoaded)
                stats.loadedTextures++;
            if (pair.second.swappedImage)
                stats.swappedTextures++;
        }

        for (const auto& pair : mGeometry)
        {
            if (pair.second.isLoaded)
                stats.loadedGeometry++;
        }

        stats.estimatedVRAMUsageMB = mEstimatedVRAMUsage / (1024 * 1024);
        stats.targetVRAMUsageMB = mMaxVRAMUsage / (1024 * 1024);

        return stats;
    }

    // TextureStreaming implementation

    TextureStreaming::TextureStreaming(Resource::ResourceSystem* resourceSystem)
        : mResourceSystem(resourceSystem)
        , mMaxLoadsPerFrame(2)
    {
    }

    TextureStreaming::~TextureStreaming()
    {
        mPendingRequests.clear();
        mLoadedTextures.clear();
    }

    void TextureStreaming::requestTexture(const std::string& path, int priority)
    {
        // Check if already loaded
        if (mLoadedTextures.count(path) > 0)
            return;

        // Check if already pending
        for (auto& req : mPendingRequests)
        {
            if (req.path == path)
            {
                req.priority = std::max(req.priority, priority);
                return;
            }
        }

        mPendingRequests.push_back({ path, priority, false });
    }

    void TextureStreaming::update(float dt)
    {
        if (mPendingRequests.empty())
            return;

        // Sort by priority
        std::sort(mPendingRequests.begin(), mPendingRequests.end(),
            [](const auto& a, const auto& b) { return a.priority > b.priority; });

        // Load up to max per frame
        int loaded = 0;
        auto it = mPendingRequests.begin();
        while (it != mPendingRequests.end() && loaded < mMaxLoadsPerFrame)
        {
            // In a full implementation, this would async load the texture
            // For now, we just mark it as ready
            mLoadedTextures[it->path] = nullptr; // Would be actual texture
            it = mPendingRequests.erase(it);
            loaded++;
        }
    }

    bool TextureStreaming::isTextureLoaded(const std::string& path) const
    {
        return mLoadedTextures.count(path) > 0;
    }

    osg::Texture* TextureStreaming::getTexture(const std::string& path)
    {
        auto it = mLoadedTextures.find(path);
        if (it != mLoadedTextures.end())
            return it->second.get();
        return nullptr;
    }
}
