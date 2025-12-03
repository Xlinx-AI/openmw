#ifndef OPENMW_MWRENDER_CUBEMAPREFLECTION_H
#define OPENMW_MWRENDER_CUBEMAPREFLECTION_H

#include <osg/Camera>
#include <osg/FrameBufferObject>
#include <osg/Group>
#include <osg/TextureCubeMap>
#include <osg/ref_ptr>

#include <array>
#include <memory>

namespace osg
{
    class Node;
}

namespace Resource
{
    class ResourceSystem;
}

namespace MWRender
{
    /// Cubemap reflection types
    enum class CubemapReflectionType
    {
        Dynamic, // Updates each frame for objects within FOV and distance
        Static   // Updates only on time-of-day changes, excludes dynamic actors
    };

    /// High-performance cubemap-based water reflection system
    /// Supports both dynamic (real-time) and static (time-of-day) update modes
    class CubemapReflection : public osg::Group
    {
    public:
        CubemapReflection(osg::Group* sceneRoot, Resource::ResourceSystem* resourceSystem,
            int cubemapSize, CubemapReflectionType type, float reflectionDistance);
        ~CubemapReflection();

        /// Get the cubemap texture for use in water shaders
        osg::TextureCubeMap* getCubemapTexture() { return mCubemap.get(); }

        /// Set the water level for clipping
        void setWaterLevel(float waterLevel);

        /// Set camera position for dynamic updates
        void setCameraPosition(const osg::Vec3f& pos);

        /// Set the scene to reflect
        void setScene(osg::Node* scene);

        /// Update the cubemap - called each frame
        /// For static type, only updates when forceUpdate is true or time changed
        void update(float currentGameHour, bool forceUpdate = false);

        /// Set the maximum distance for including objects in reflection
        void setReflectionDistance(float distance) { mReflectionDistance = distance; }

        /// Check if cubemap needs update (for static type)
        bool needsUpdate(float currentGameHour) const;

        /// Set visibility mask for what gets reflected
        void setNodeMask(unsigned int mask) { mNodeMask = mask; }

        /// Enable/disable actor reflections (for static mode)
        void setReflectActors(bool reflect) { mReflectActors = reflect; }

    private:
        void setupCubemapCamera(int face);
        void createCubemapCameras();
        osg::Matrix computeFaceMatrix(int face) const;

        osg::ref_ptr<osg::TextureCubeMap> mCubemap;
        std::array<osg::ref_ptr<osg::Camera>, 6> mCameras;
        osg::ref_ptr<osg::Group> mSceneRoot;
        osg::ref_ptr<osg::Node> mScene;
        Resource::ResourceSystem* mResourceSystem;

        CubemapReflectionType mType;
        int mCubemapSize;
        float mWaterLevel;
        float mReflectionDistance;
        float mLastUpdateHour;
        osg::Vec3f mCameraPosition;
        unsigned int mNodeMask;
        bool mReflectActors;
        bool mDirty;

        // Face update scheduling for dynamic mode
        // Only update 1-2 faces per frame to spread load
        int mCurrentFace;
        int mFacesPerFrame;
    };

    /// Factory function to create appropriate reflection type based on settings
    std::unique_ptr<CubemapReflection> createCubemapReflection(
        osg::Group* sceneRoot, Resource::ResourceSystem* resourceSystem);
}

#endif
