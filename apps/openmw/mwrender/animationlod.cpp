#include "animationlod.hpp"

#include <cmath>

#include <components/settings/values.hpp>

#include <osg/Math>

namespace MWRender
{
    AnimationLOD::AnimationLOD()
        : mEnabled(false)
        , mFOVCulling(false)
        , mFullRateDistance(1024.0f)
        , mMinRateDistance(4096.0f)
        , mMinUpdateInterval(0.1f)
        , mFOVMargin(15.0f)
        , mCameraPosition(0, 0, 0)
        , mCameraForward(0, 1, 0)
        , mCameraRight(1, 0, 0)
        , mCameraUp(0, 0, 1)
        , mFOVCos(0.0f)
        , mAspectRatio(16.0f / 9.0f)
    {
        loadSettings();
    }

    void AnimationLOD::loadSettings()
    {
        const auto& settings = Settings::animationLOD();
        mEnabled = settings.mEnabled;
        mFOVCulling = settings.mFOVCulling;
        mFullRateDistance = settings.mFullRateDistance;
        mMinRateDistance = settings.mMinRateDistance;
        mMinUpdateInterval = settings.mMinUpdateInterval;
        mFOVMargin = settings.mFOVMargin;
    }

    void AnimationLOD::updateCamera(const osg::Matrixf& viewMatrix, const osg::Matrixf& projMatrix, float fovDegrees)
    {
        // Extract camera position from inverse view matrix
        osg::Matrixf invView = osg::Matrixf::inverse(viewMatrix);
        mCameraPosition = invView.getTrans();

        // Extract camera basis vectors (view matrix rows are transposed camera axes)
        mCameraRight = osg::Vec3f(viewMatrix(0, 0), viewMatrix(1, 0), viewMatrix(2, 0));
        mCameraUp = osg::Vec3f(viewMatrix(0, 1), viewMatrix(1, 1), viewMatrix(2, 1));
        mCameraForward = osg::Vec3f(-viewMatrix(0, 2), -viewMatrix(1, 2), -viewMatrix(2, 2));

        // Calculate FOV cosine with margin for culling
        float halfFOV = (fovDegrees + mFOVMargin) * 0.5f;
        mFOVCos = std::cos(osg::DegreesToRadians(halfFOV));

        // Extract aspect ratio from projection matrix if available
        if (std::abs(projMatrix(0, 0)) > 0.001f)
        {
            mAspectRatio = projMatrix(1, 1) / projMatrix(0, 0);
        }
    }

    bool AnimationLOD::isInFOV(const osg::Vec3f& position) const
    {
        if (!mFOVCulling)
            return true;

        osg::Vec3f toTarget = position - mCameraPosition;
        float distance = toTarget.length();

        if (distance < 0.001f)
            return true;

        toTarget /= distance;

        // Check horizontal FOV
        float horizontalDot = toTarget * mCameraForward;

        // Object is behind the camera
        if (horizontalDot < 0)
            return false;

        // Project onto forward-right plane and check cone
        float cosFOVHorizontal = mFOVCos;

        // Account for aspect ratio in vertical check
        float verticalMarginFactor = 1.0f / mAspectRatio;
        float cosFOVVertical = std::cos(std::acos(mFOVCos) * verticalMarginFactor);

        // Check horizontal angle (left-right)
        osg::Vec3f horizontalDir = toTarget - mCameraUp * (toTarget * mCameraUp);
        horizontalDir.normalize();
        float hAngle = horizontalDir * mCameraForward;

        // Check vertical angle (up-down)
        osg::Vec3f verticalDir = toTarget - mCameraRight * (toTarget * mCameraRight);
        verticalDir.normalize();
        float vAngle = verticalDir * mCameraForward;

        return hAngle >= cosFOVHorizontal && vAngle >= cosFOVVertical;
    }

    float AnimationLOD::getDistanceToCamera(const osg::Vec3f& position) const
    {
        return (position - mCameraPosition).length();
    }

    float AnimationLOD::getUpdateInterval(float distance) const
    {
        if (distance <= mFullRateDistance)
            return 0.0f; // Update every frame

        if (distance >= mMinRateDistance)
            return mMinUpdateInterval;

        // Linear interpolation between full rate and min rate
        float t = (distance - mFullRateDistance) / (mMinRateDistance - mFullRateDistance);
        return t * mMinUpdateInterval;
    }

    bool AnimationLOD::shouldUpdateAnimation(const osg::Vec3f& position, float currentTime, const void* actorId)
    {
        if (!mEnabled)
            return true;

        // Check FOV culling
        if (mFOVCulling && !isInFOV(position))
            return false;

        // Calculate distance-based update interval
        float distance = getDistanceToCamera(position);
        float updateInterval = getUpdateInterval(distance);

        if (updateInterval <= 0.0f)
            return true; // Full rate update

        // Check if enough time has passed since last update
        auto it = mLastUpdateTimes.find(actorId);
        if (it == mLastUpdateTimes.end())
        {
            mLastUpdateTimes[actorId] = currentTime;
            return true;
        }

        float timeSinceLastUpdate = currentTime - it->second;
        if (timeSinceLastUpdate >= updateInterval)
        {
            it->second = currentTime;
            return true;
        }

        return false;
    }

    void AnimationLOD::resetActor(const void* actorId)
    {
        mLastUpdateTimes.erase(actorId);
    }

    void AnimationLOD::clear()
    {
        mLastUpdateTimes.clear();
    }
}
