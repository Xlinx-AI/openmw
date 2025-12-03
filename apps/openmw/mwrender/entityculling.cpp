#include "entityculling.hpp"

#include <osg/Camera>
#include <osg/ComputeBoundsVisitor>

#include <components/settings/values.hpp>

#include <algorithm>
#include <cmath>

namespace MWRender
{
    EntityCulling::EntityCulling()
        : mEnabled(true)
        , mMaxVisibleEntities(200)
        , mCullingDistance(8192.0f)
        , mPriorityDistance(1024.0f)
        , mLodCullingEnabled(true)
        , mMinScreenSize(4.0f)
        , mFrustumCullingEnabled(true)
        , mOcclusionCullingEnabled(false)
        , mCullActors(true)
        , mCullStatics(true)
        , mUpdateTimer(0.0f)
    {
        mStats = { 0, 0, 0, 0 };
        processChangedSettings();
    }

    EntityCulling::~EntityCulling()
    {
        mEntities.clear();
        mVisibleEntities.clear();
    }

    void EntityCulling::processChangedSettings()
    {
        const auto& settings = Settings::entityCulling();

        mEnabled = settings.mEnableEntityCulling;
        mMaxVisibleEntities = settings.mMaxVisibleEntities;
        mCullingDistance = settings.mCullingDistance;
        mPriorityDistance = settings.mPriorityDistance;
        mLodCullingEnabled = settings.mEnableLodCulling;
        mMinScreenSize = settings.mMinScreenSize;
        mFrustumCullingEnabled = settings.mEnableFrustumCulling;
        mOcclusionCullingEnabled = settings.mEnableOcclusionCulling;
        mCullActors = settings.mCullActors;
        mCullStatics = settings.mCullStatics;
    }

    void EntityCulling::update(const osg::Vec3f& playerPos, const osg::Vec3f& cameraDir, float dt)
    {
        if (!mEnabled)
            return;

        mPlayerPos = playerPos;
        mCameraDir = cameraDir;

        mUpdateTimer += dt;
        if (mUpdateTimer < UPDATE_INTERVAL)
            return;

        mUpdateTimer = 0.0f;
        performCulling(playerPos);
    }

    void EntityCulling::performCulling(const osg::Vec3f& playerPos)
    {
        mVisibleEntities.clear();
        mStats.totalEntities = static_cast<int>(mEntities.size());
        mStats.visibleEntities = 0;
        mStats.culledEntities = 0;
        mStats.priorityEntities = 0;

        // First pass: update all entity data
        std::vector<std::pair<const MWWorld::Ptr*, EntityData*>> sortedEntities;
        sortedEntities.reserve(mEntities.size());

        for (auto& pair : mEntities)
        {
            updateEntityVisibility(pair.second, playerPos);
            sortedEntities.emplace_back(pair.first, &pair.second);
        }

        // Sort by priority (distance-based) - closer entities first
        std::sort(sortedEntities.begin(), sortedEntities.end(),
            [](const auto& a, const auto& b)
            {
                // Priority entities always come first
                if (a.second->isPriority != b.second->isPriority)
                    return a.second->isPriority;
                // Then sort by distance
                return a.second->distanceToPlayer < b.second->distanceToPlayer;
            });

        // Second pass: select visible entities up to max count
        int visibleCount = 0;
        for (auto& pair : sortedEntities)
        {
            const MWWorld::Ptr* ptr = pair.first;
            EntityData& entity = *pair.second;

            bool shouldShow = false;

            // Priority entities always visible
            if (entity.isPriority)
            {
                shouldShow = true;
                mStats.priorityEntities++;
            }
            // Check visibility constraints
            else if (entity.isVisible && visibleCount < mMaxVisibleEntities)
            {
                shouldShow = true;
            }

            if (shouldShow)
            {
                mVisibleEntities.insert(ptr);
                visibleCount++;
                mStats.visibleEntities++;

                if (entity.node)
                    entity.node->setNodeMask(~0u);
            }
            else
            {
                mStats.culledEntities++;
                if (entity.node)
                    entity.node->setNodeMask(0u);
            }
        }
    }

    void EntityCulling::updateEntityVisibility(EntityData& entity, const osg::Vec3f& playerPos)
    {
        if (!entity.node)
        {
            entity.isVisible = false;
            entity.isPriority = false;
            return;
        }

        // Calculate distance to player
        osg::Vec3f entityPos = entity.node->getBound().center();
        entity.distanceToPlayer = (entityPos - playerPos).length();

        // Check if priority entity (very close to player)
        entity.isPriority = entity.distanceToPlayer < mPriorityDistance;

        // Check culling distance
        if (entity.distanceToPlayer > mCullingDistance)
        {
            entity.isVisible = false;
            return;
        }

        // Check screen size (LOD culling)
        if (mLodCullingEnabled)
        {
            osg::BoundingBox bb;
            osg::ComputeBoundsVisitor cbv;
            entity.node->accept(cbv);
            bb = cbv.getBoundingBox();

            entity.screenSize = calculateScreenSize(bb, entity.distanceToPlayer);
            if (entity.screenSize < mMinScreenSize)
            {
                entity.isVisible = false;
                return;
            }
        }

        // Check frustum
        if (mFrustumCullingEnabled && mCamera)
        {
            osg::BoundingBox bb;
            osg::ComputeBoundsVisitor cbv;
            entity.node->accept(cbv);
            bb = cbv.getBoundingBox();

            if (!isInFrustum(bb))
            {
                entity.isVisible = false;
                return;
            }
        }

        entity.isVisible = true;
    }

    float EntityCulling::calculateScreenSize(const osg::BoundingBox& bb, float distance) const
    {
        if (distance < 1.0f)
            distance = 1.0f;

        // Approximate screen size in pixels
        float radius = bb.radius();
        float fov = 60.0f; // Default FOV
        float screenHeight = 1080.0f; // Assume 1080p

        float projectedSize = (radius / distance) * (screenHeight / std::tan(fov * 0.5f * 3.14159f / 180.0f));
        return projectedSize;
    }

    bool EntityCulling::isInFrustum(const osg::BoundingBox& bb) const
    {
        if (!mCamera)
            return true;

        // Simple frustum check using camera view matrix
        // In a full implementation, this would use proper frustum planes
        osg::Vec3f center = bb.center();
        osg::Vec3f toCenter = center - mPlayerPos;

        // Check if behind camera
        float dot = toCenter * mCameraDir;
        if (dot < -bb.radius())
            return false;

        return true;
    }

    void EntityCulling::registerEntity(const MWWorld::Ptr* ptr, osg::Node* node)
    {
        if (!ptr || !node)
            return;

        EntityData& data = mEntities[ptr];
        data.node = node;
        data.isVisible = true;
        data.isPriority = false;
        data.distanceToPlayer = 0.0f;
        data.screenSize = 100.0f;
        data.lastUpdateTime = 0.0f;
    }

    void EntityCulling::unregisterEntity(const MWWorld::Ptr* ptr)
    {
        if (!ptr)
            return;

        mEntities.erase(ptr);
        mVisibleEntities.erase(ptr);
    }

    bool EntityCulling::shouldRender(const MWWorld::Ptr* ptr) const
    {
        if (!mEnabled)
            return true;

        return mVisibleEntities.count(ptr) > 0;
    }

    // EntityPriorityManager implementation

    EntityPriorityManager::EntityPriorityManager()
    {
    }

    EntityPriorityManager::~EntityPriorityManager()
    {
        mPriorities.clear();
    }

    void EntityPriorityManager::setPriority(const MWWorld::Ptr* ptr, int priority)
    {
        if (ptr)
            mPriorities[ptr] = priority;
    }

    int EntityPriorityManager::getPriority(const MWWorld::Ptr* ptr) const
    {
        auto it = mPriorities.find(ptr);
        if (it != mPriorities.end())
            return it->second;
        return 0;
    }

    std::vector<const MWWorld::Ptr*> EntityPriorityManager::getSortedEntities() const
    {
        std::vector<std::pair<const MWWorld::Ptr*, int>> sorted(mPriorities.begin(), mPriorities.end());
        std::sort(sorted.begin(), sorted.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });

        std::vector<const MWWorld::Ptr*> result;
        result.reserve(sorted.size());
        for (const auto& pair : sorted)
            result.push_back(pair.first);

        return result;
    }
}
