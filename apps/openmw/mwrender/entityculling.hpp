#ifndef OPENMW_MWRENDER_ENTITYCULLING_H
#define OPENMW_MWRENDER_ENTITYCULLING_H

#include <osg/Node>
#include <osg/Vec3f>
#include <osg/BoundingBox>
#include <osg/ref_ptr>

#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace osg
{
    class Group;
    class Camera;
}

namespace MWWorld
{
    class Ptr;
}

namespace MWRender
{
    /// Entity culling optimization system for managing large numbers of entities
    /// near the player without impacting performance
    class EntityCulling
    {
    public:
        EntityCulling();
        ~EntityCulling();

        /// Update culling system each frame
        void update(const osg::Vec3f& playerPos, const osg::Vec3f& cameraDir, float dt);

        /// Register an entity for culling management
        void registerEntity(const MWWorld::Ptr* ptr, osg::Node* node);

        /// Unregister an entity
        void unregisterEntity(const MWWorld::Ptr* ptr);

        /// Check if an entity should be rendered
        bool shouldRender(const MWWorld::Ptr* ptr) const;

        /// Process changed settings
        void processChangedSettings();

        /// Get statistics
        struct Stats
        {
            int totalEntities;
            int visibleEntities;
            int culledEntities;
            int priorityEntities;
        };
        Stats getStats() const { return mStats; }

        /// Set camera for frustum culling
        void setCamera(osg::Camera* camera) { mCamera = camera; }

    private:
        struct EntityData
        {
            osg::ref_ptr<osg::Node> node;
            osg::Vec3f lastPosition;
            float distanceToPlayer;
            float screenSize;
            bool isVisible;
            bool isPriority;
            float lastUpdateTime;
        };

        void performCulling(const osg::Vec3f& playerPos);
        void updateEntityVisibility(EntityData& entity, const osg::Vec3f& playerPos);
        float calculateScreenSize(const osg::BoundingBox& bb, float distance) const;
        bool isInFrustum(const osg::BoundingBox& bb) const;

        std::unordered_map<const MWWorld::Ptr*, EntityData> mEntities;
        std::unordered_set<const MWWorld::Ptr*> mVisibleEntities;

        osg::ref_ptr<osg::Camera> mCamera;
        osg::Vec3f mPlayerPos;
        osg::Vec3f mCameraDir;

        // Settings
        bool mEnabled;
        int mMaxVisibleEntities;
        float mCullingDistance;
        float mPriorityDistance;
        bool mLodCullingEnabled;
        float mMinScreenSize;
        bool mFrustumCullingEnabled;
        bool mOcclusionCullingEnabled;
        bool mCullActors;
        bool mCullStatics;

        Stats mStats;
        float mUpdateTimer;
        static constexpr float UPDATE_INTERVAL = 0.1f; // Update every 100ms
    };

    /// Priority-based entity renderer that ensures important entities are always visible
    class EntityPriorityManager
    {
    public:
        EntityPriorityManager();
        ~EntityPriorityManager();

        /// Set entity priority (higher = more important)
        void setPriority(const MWWorld::Ptr* ptr, int priority);

        /// Get entity priority
        int getPriority(const MWWorld::Ptr* ptr) const;

        /// Get entities sorted by priority
        std::vector<const MWWorld::Ptr*> getSortedEntities() const;

    private:
        std::unordered_map<const MWWorld::Ptr*, int> mPriorities;
    };
}

#endif
