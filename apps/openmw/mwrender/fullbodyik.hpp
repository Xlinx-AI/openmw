#ifndef OPENMW_MWRENDER_FULLBODYIK_H
#define OPENMW_MWRENDER_FULLBODYIK_H

#include <osg/MatrixTransform>
#include <osg/Vec3f>
#include <osg/Quat>
#include <osg/ref_ptr>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace osg
{
    class Node;
}

namespace SceneUtil
{
    class Skeleton;
}

namespace MWRender
{
    /// IK chain definition
    struct IKChain
    {
        std::string rootBone;
        std::string endBone;
        std::vector<std::string> intermediateBones;
        float totalLength = 0.0f;

        // Constraints per joint
        struct JointConstraint
        {
            float minYaw = -180.0f;
            float maxYaw = 180.0f;
            float minPitch = -180.0f;
            float maxPitch = 180.0f;
            float minRoll = -180.0f;
            float maxRoll = 180.0f;
        };
        std::map<std::string, JointConstraint> constraints;
    };

    /// IK target information
    struct IKTarget
    {
        osg::Vec3f position;
        osg::Quat rotation;
        float weight = 1.0f; // Blend weight with animation
        bool useRotation = false;
    };

    /// Spring constraint for physics-based animation
    struct SpringConstraint
    {
        float stiffness = 5.0f;
        float damping = 0.5f;
        osg::Vec3f velocity = osg::Vec3f(0, 0, 0);
        osg::Vec3f offset = osg::Vec3f(0, 0, 0);
        osg::Vec3f targetOffset = osg::Vec3f(0, 0, 0);

        void update(float dt);
        void applyImpulse(const osg::Vec3f& impulse);
        void reset();
    };

    /// Full Body Inverse Kinematics system
    /// Provides procedural animation for natural-looking limb placement
    class FullBodyIK
    {
    public:
        FullBodyIK();
        ~FullBodyIK();

        /// Initialize IK for a skeleton
        void initialize(SceneUtil::Skeleton* skeleton);

        /// Set IK target for a chain (e.g., hand reaching for object)
        void setTarget(const std::string& chainName, const IKTarget& target);

        /// Clear IK target for a chain
        void clearTarget(const std::string& chainName);

        /// Add predefined IK chain
        void addChain(const IKChain& chain);

        /// Update IK solution
        void solve(float dt);

        /// Get solved bone rotation
        bool getBoneRotation(const std::string& boneName, osg::Quat& rotation) const;

        /// Check if IK is active for any chain
        bool isActive() const;

        /// Enable/disable IK globally
        void setEnabled(bool enabled) { mEnabled = enabled; }
        bool isEnabled() const { return mEnabled; }

        // Predefined chain setup for humanoid skeletons
        void setupHumanoidChains();

    private:
        // FABRIK (Forward And Backward Reaching Inverse Kinematics) solver
        void solveFABRIK(IKChain& chain, const IKTarget& target, int iterations = 10);

        // Get bone transform in world space
        osg::Matrix getBoneWorldTransform(const std::string& boneName) const;

        // Apply joint constraints
        void applyConstraints(IKChain& chain);

        SceneUtil::Skeleton* mSkeleton;
        std::map<std::string, IKChain> mChains;
        std::map<std::string, IKTarget> mTargets;
        std::map<std::string, osg::Quat> mSolvedRotations;
        bool mEnabled;
    };

    /// Spring-based procedural animation system (Euphoria-like)
    /// Adds physics-driven secondary motion to animations
    class SpringBodyAnimation
    {
    public:
        SpringBodyAnimation();
        ~SpringBodyAnimation();

        /// Initialize for a skeleton
        void initialize(SceneUtil::Skeleton* skeleton);

        /// Apply impulse to the body (e.g., from hit)
        void applyImpulse(const osg::Vec3f& worldImpulse, const osg::Vec3f& hitPoint);

        /// Apply uniform force (e.g., wind, movement)
        void applyForce(const osg::Vec3f& force);

        /// Update spring simulation
        void update(float dt);

        /// Get spring-modified bone offset
        bool getBoneOffset(const std::string& boneName, osg::Vec3f& offset) const;

        /// Get spring-modified bone rotation offset
        bool getBoneRotationOffset(const std::string& boneName, osg::Quat& rotOffset) const;

        /// Set spring parameters
        void setStiffness(float stiffness);
        void setDamping(float damping);

        /// Enable/disable spring animation
        void setEnabled(bool enabled) { mEnabled = enabled; }
        bool isEnabled() const { return mEnabled; }

        /// Reset all springs to rest position
        void reset();

        /// Check if springs are currently active (have motion)
        bool hasActiveMotion() const;

    private:
        void setupSpringBones();
        float computeBoneWeight(const std::string& boneName) const;

        SceneUtil::Skeleton* mSkeleton;
        std::map<std::string, SpringConstraint> mBoneSprings;
        float mGlobalStiffness;
        float mGlobalDamping;
        bool mEnabled;
        osg::Vec3f mCumulativeImpulse;
    };

    /// Active Ragdoll system - blends animation with physics
    class ActiveRagdoll
    {
    public:
        ActiveRagdoll();
        ~ActiveRagdoll();

        /// Initialize for skeleton
        void initialize(SceneUtil::Skeleton* skeleton);

        /// Set blend factor (0 = full animation, 1 = full physics)
        void setBlendFactor(float factor) { mBlendFactor = std::clamp(factor, 0.0f, 1.0f); }
        float getBlendFactor() const { return mBlendFactor; }

        /// Apply physics impulse
        void applyImpulse(const osg::Vec3f& impulse, const osg::Vec3f& point);

        /// Check if impact should trigger ragdoll transition
        bool shouldTriggerRagdoll(float impulseMagnitude) const;

        /// Smoothly transition to/from ragdoll
        void transitionTo(float targetBlend, float duration);

        /// Update ragdoll blend
        void update(float dt);

        /// Get blended bone transform
        bool getBlendedTransform(const std::string& boneName, const osg::Matrix& animTransform,
            osg::Matrix& blendedTransform) const;

        /// Enable/disable active ragdoll
        void setEnabled(bool enabled) { mEnabled = enabled; }
        bool isEnabled() const { return mEnabled; }

        /// Check if currently transitioning
        bool isTransitioning() const { return mTransitionTimer > 0.0f; }

    private:
        SceneUtil::Skeleton* mSkeleton;
        float mBlendFactor;
        float mTargetBlend;
        float mTransitionTimer;
        float mTransitionDuration;
        bool mEnabled;

        // Per-bone physics state
        struct BonePhysicsState
        {
            osg::Vec3f velocity;
            osg::Vec3f angularVelocity;
            osg::Vec3f position;
            osg::Quat rotation;
        };
        std::map<std::string, BonePhysicsState> mBoneStates;
    };
}

#endif
