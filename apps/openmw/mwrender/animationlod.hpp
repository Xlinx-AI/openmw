#ifndef OPENMW_MWRENDER_ANIMATIONLOD_H
#define OPENMW_MWRENDER_ANIMATIONLOD_H

#include <osg/Matrix>
#include <osg/Vec3f>

#include <unordered_map>

#include "../mwworld/ptr.hpp"

namespace MWRender
{
    /// Animation Level of Detail system for performance optimization.
    /// Manages animation update rates based on distance and FOV visibility.
    class AnimationLOD
    {
    public:
        AnimationLOD();

        /// Update the camera state for FOV culling calculations
        /// @param viewMatrix The current camera view matrix
        /// @param projMatrix The current camera projection matrix
        /// @param fovDegrees The current field of view in degrees
        void updateCamera(const osg::Matrixf& viewMatrix, const osg::Matrixf& projMatrix, float fovDegrees);

        /// Check if an actor should be updated this frame based on LOD settings
        /// @param position The world position of the actor
        /// @param currentTime The current simulation time
        /// @param actorId A unique identifier for the actor (for tracking update times)
        /// @return true if the actor should be animated this frame
        bool shouldUpdateAnimation(const osg::Vec3f& position, float currentTime, const void* actorId);

        /// Check if a position is within the camera's FOV (with margin)
        /// @param position The world position to check
        /// @return true if the position is visible or within the margin
        bool isInFOV(const osg::Vec3f& position) const;

        /// Calculate the update interval for an actor at a given distance
        /// @param distance The distance from the camera
        /// @return The minimum time between animation updates
        float getUpdateInterval(float distance) const;

        /// Reset tracking data for an actor (call when actor is removed)
        void resetActor(const void* actorId);

        /// Clear all tracking data
        void clear();

        /// Get the distance from the camera to a world position
        float getDistanceToCamera(const osg::Vec3f& position) const;

        /// Check if the system is enabled
        bool isEnabled() const { return mEnabled; }

    private:
        bool mEnabled;
        bool mFOVCulling;
        float mFullRateDistance;
        float mMinRateDistance;
        float mMinUpdateInterval;
        float mFOVMargin;

        osg::Vec3f mCameraPosition;
        osg::Vec3f mCameraForward;
        osg::Vec3f mCameraRight;
        osg::Vec3f mCameraUp;
        float mFOVCos;        // Cosine of half FOV angle + margin
        float mAspectRatio;

        // Track last update time for each actor
        std::unordered_map<const void*, float> mLastUpdateTimes;

        void loadSettings();
    };
}

#endif // OPENMW_MWRENDER_ANIMATIONLOD_H
