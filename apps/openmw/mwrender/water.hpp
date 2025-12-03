#ifndef OPENMW_MWRENDER_WATER_H
#define OPENMW_MWRENDER_WATER_H

#include <memory>
#include <vector>

#include <osg/Vec3d>
#include <osg/Vec3f>
#include <osg/ref_ptr>

#include <components/settings/settings.hpp>
#include <components/vfs/pathutil.hpp>

namespace osg
{
    class Group;
    class PositionAttitudeTransform;
    class Geometry;
    class Node;
    class Callback;
}

namespace osgUtil
{
    class IncrementalCompileOperation;
}

namespace Resource
{
    class ResourceSystem;
}

namespace MWWorld
{
    class CellStore;
    class Ptr;
}

namespace Fallback
{
    class Map;
}

namespace MWRender
{

    class Refraction;
    class Reflection;
    class RippleSimulation;
    class RainSettingsUpdater;
    class Ripples;
    class CubemapReflection;

    /// Reflection method types for water rendering
    enum class WaterReflectionMethod
    {
        Planar = 0,        // High quality planar reflections (real-time)
        DynamicCubemap = 1, // Medium quality, updates near FOV
        StaticCubemap = 2   // Low quality, updates on time-of-day change
    };

    /// Water rendering with optimized reflection system
    class Water
    {
        osg::ref_ptr<RainSettingsUpdater> mRainSettingsUpdater;

        osg::ref_ptr<osg::Group> mParent;
        osg::ref_ptr<osg::Group> mSceneRoot;
        osg::ref_ptr<osg::PositionAttitudeTransform> mWaterNode;
        osg::ref_ptr<osg::Geometry> mWaterGeom;
        Resource::ResourceSystem* mResourceSystem;
        osg::ref_ptr<osgUtil::IncrementalCompileOperation> mIncrementalCompileOperation;

        std::unique_ptr<RippleSimulation> mSimulation;

        osg::ref_ptr<Refraction> mRefraction;
        osg::ref_ptr<Reflection> mReflection;
        osg::ref_ptr<Ripples> mRipples;
        
        // Cubemap-based reflection for optimized rendering
        std::unique_ptr<CubemapReflection> mCubemapReflection;
        WaterReflectionMethod mReflectionMethod;
        float mLastTimeOfDay;

        bool mEnabled;
        bool mToggled;
        float mTop;
        bool mInterior;
        bool mShowWorld;

        osg::Callback* mCullCallback;
        osg::ref_ptr<osg::Callback> mShaderWaterStateSetUpdater;

        osg::Vec3f getSceneNodeCoordinates(int gridX, int gridY);
        void updateVisible();

        void createSimpleWaterStateSet(osg::Node* node, float alpha);

        void createShaderWaterStateSet(osg::Node* node);
        
        void createCubemapReflection();

        void updateWaterMaterial();

    public:
        Water(osg::Group* parent, osg::Group* sceneRoot, Resource::ResourceSystem* resourceSystem,
            osgUtil::IncrementalCompileOperation* ico);
        ~Water();

        void setCullCallback(osg::Callback* callback);

        void listAssetsToPreload(std::vector<VFS::Path::Normalized>& textures);

        void setEnabled(bool enabled);

        bool toggle();

        bool isUnderwater(const osg::Vec3f& pos) const;

        /// adds an emitter, position will be tracked automatically using its scene node
        void addEmitter(const MWWorld::Ptr& ptr, float scale = 1.f, float force = 1.f);
        void removeEmitter(const MWWorld::Ptr& ptr);
        void updateEmitterPtr(const MWWorld::Ptr& old, const MWWorld::Ptr& ptr);
        void emitRipple(const osg::Vec3f& pos);

        void removeCell(const MWWorld::CellStore* store); ///< remove all emitters in this cell

        void clearRipples();

        void changeCell(const MWWorld::CellStore* store);
        void setHeight(const float height);
        void setRainIntensity(const float rainIntensity);
        void setRainRipplesEnabled(bool enableRipples);

        void update(float dt, bool paused);
        
        /// Update time of day for static cubemap reflections
        void updateTimeOfDay(float gameHour);
        
        /// Update camera position for cubemap reflections
        void updateCameraPosition(const osg::Vec3f& cameraPos);

        osg::Vec3d getPosition() const;

        void processChangedSettings(const Settings::CategorySettingVector& settings);

        void showWorld(bool show);
        
        /// Get current reflection method
        WaterReflectionMethod getReflectionMethod() const { return mReflectionMethod; }
    };

}

#endif
