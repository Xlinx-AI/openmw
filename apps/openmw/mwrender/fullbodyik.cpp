#include "fullbodyik.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include <osg/NodeVisitor>

#include <components/sceneutil/skeleton.hpp>
#include <components/settings/values.hpp>

namespace MWRender
{
    // ======================== SpringConstraint ========================

    void SpringConstraint::update(float dt)
    {
        // Spring physics: F = -k * x - c * v
        osg::Vec3f springForce = (targetOffset - offset) * stiffness;
        osg::Vec3f dampingForce = velocity * (-damping);
        osg::Vec3f acceleration = springForce + dampingForce;

        velocity += acceleration * dt;
        offset += velocity * dt;

        // Clamp to prevent explosion
        float maxOffset = 50.0f;
        float len = offset.length();
        if (len > maxOffset)
            offset *= maxOffset / len;
    }

    void SpringConstraint::applyImpulse(const osg::Vec3f& impulse)
    {
        velocity += impulse;
    }

    void SpringConstraint::reset()
    {
        velocity = osg::Vec3f(0, 0, 0);
        offset = osg::Vec3f(0, 0, 0);
        targetOffset = osg::Vec3f(0, 0, 0);
    }

    // ======================== FullBodyIK ========================

    FullBodyIK::FullBodyIK()
        : mSkeleton(nullptr)
        , mEnabled(true)
    {
    }

    FullBodyIK::~FullBodyIK() = default;

    void FullBodyIK::initialize(SceneUtil::Skeleton* skeleton)
    {
        mSkeleton = skeleton;
        if (Settings::game().mFullBodyIK)
            setupHumanoidChains();
    }

    void FullBodyIK::setupHumanoidChains()
    {
        // Left arm chain
        IKChain leftArm;
        leftArm.rootBone = "Bip01 L Clavicle";
        leftArm.endBone = "Bip01 L Hand";
        leftArm.intermediateBones = { "Bip01 L UpperArm", "Bip01 L Forearm" };
        addChain(leftArm);

        // Right arm chain
        IKChain rightArm;
        rightArm.rootBone = "Bip01 R Clavicle";
        rightArm.endBone = "Bip01 R Hand";
        rightArm.intermediateBones = { "Bip01 R UpperArm", "Bip01 R Forearm" };
        addChain(rightArm);

        // Left leg chain
        IKChain leftLeg;
        leftLeg.rootBone = "Bip01 L Thigh";
        leftLeg.endBone = "Bip01 L Foot";
        leftLeg.intermediateBones = { "Bip01 L Calf" };
        addChain(leftLeg);

        // Right leg chain
        IKChain rightLeg;
        rightLeg.rootBone = "Bip01 R Thigh";
        rightLeg.endBone = "Bip01 R Foot";
        rightLeg.intermediateBones = { "Bip01 R Calf" };
        addChain(rightLeg);

        // Spine chain for looking/leaning
        IKChain spine;
        spine.rootBone = "Bip01 Pelvis";
        spine.endBone = "Bip01 Head";
        spine.intermediateBones = { "Bip01 Spine", "Bip01 Spine1", "Bip01 Spine2", "Bip01 Neck" };
        addChain(spine);
    }

    void FullBodyIK::addChain(const IKChain& chain)
    {
        mChains[chain.endBone] = chain;
    }

    void FullBodyIK::setTarget(const std::string& chainName, const IKTarget& target)
    {
        mTargets[chainName] = target;
    }

    void FullBodyIK::clearTarget(const std::string& chainName)
    {
        mTargets.erase(chainName);
    }

    bool FullBodyIK::isActive() const
    {
        return mEnabled && !mTargets.empty();
    }

    void FullBodyIK::solve(float dt)
    {
        if (!mEnabled || !mSkeleton || mTargets.empty())
            return;

        mSolvedRotations.clear();

        for (auto& [chainName, target] : mTargets)
        {
            auto chainIt = mChains.find(chainName);
            if (chainIt != mChains.end())
            {
                solveFABRIK(chainIt->second, target);
            }
        }
    }

    void FullBodyIK::solveFABRIK(IKChain& chain, const IKTarget& target, int iterations)
    {
        // Simplified FABRIK algorithm
        // In a full implementation, this would iterate forward and backward through the chain

        if (chain.intermediateBones.empty())
            return;

        // Get current bone positions
        std::vector<osg::Vec3f> positions;
        std::vector<float> lengths;

        osg::Matrix rootTransform = getBoneWorldTransform(chain.rootBone);
        positions.push_back(rootTransform.getTrans());

        for (const auto& bone : chain.intermediateBones)
        {
            osg::Matrix transform = getBoneWorldTransform(bone);
            osg::Vec3f pos = transform.getTrans();
            if (!positions.empty())
                lengths.push_back((pos - positions.back()).length());
            positions.push_back(pos);
        }

        osg::Matrix endTransform = getBoneWorldTransform(chain.endBone);
        positions.push_back(endTransform.getTrans());
        if (!positions.empty() && positions.size() > 1)
            lengths.push_back((endTransform.getTrans() - positions[positions.size() - 2]).length());

        if (positions.size() < 2)
            return;

        osg::Vec3f targetPos = target.position;

        // FABRIK iterations
        for (int iter = 0; iter < iterations; ++iter)
        {
            // Backward pass
            positions.back() = targetPos;
            for (int i = static_cast<int>(positions.size()) - 2; i >= 0; --i)
            {
                osg::Vec3f dir = positions[i] - positions[i + 1];
                dir.normalize();
                positions[i] = positions[i + 1] + dir * lengths[i];
            }

            // Forward pass
            positions[0] = rootTransform.getTrans();
            for (size_t i = 1; i < positions.size(); ++i)
            {
                osg::Vec3f dir = positions[i] - positions[i - 1];
                dir.normalize();
                positions[i] = positions[i - 1] + dir * lengths[i - 1];
            }
        }

        // Calculate rotations from solved positions
        // Simplified - just calculate the direction each bone should point
        std::vector<std::string> allBones = { chain.rootBone };
        allBones.insert(allBones.end(), chain.intermediateBones.begin(), chain.intermediateBones.end());

        for (size_t i = 0; i < allBones.size() && i + 1 < positions.size(); ++i)
        {
            osg::Vec3f direction = positions[i + 1] - positions[i];
            direction.normalize();

            // Convert direction to rotation (simplified)
            osg::Quat rotation;
            rotation.makeRotate(osg::Vec3f(0, 1, 0), direction); // Assuming bones point along Y

            // Blend with original animation
            osg::Matrix origTransform = getBoneWorldTransform(allBones[i]);
            osg::Quat origRotation = origTransform.getRotate();
            osg::Quat blendedRotation;
            blendedRotation.slerp(target.weight, origRotation, rotation);

            mSolvedRotations[allBones[i]] = blendedRotation;
        }
    }

    osg::Matrix FullBodyIK::getBoneWorldTransform(const std::string& boneName) const
    {
        if (!mSkeleton)
            return osg::Matrix::identity();

        // Find bone in skeleton and get world transform
        SceneUtil::Bone* bone = mSkeleton->getBone(boneName);
        if (bone)
            return bone->mMatrixInSkeletonSpace;

        return osg::Matrix::identity();
    }

    bool FullBodyIK::getBoneRotation(const std::string& boneName, osg::Quat& rotation) const
    {
        auto it = mSolvedRotations.find(boneName);
        if (it != mSolvedRotations.end())
        {
            rotation = it->second;
            return true;
        }
        return false;
    }

    void FullBodyIK::applyConstraints(IKChain& chain)
    {
        // Apply joint angle constraints
        // This would clamp rotations to valid ranges
    }

    // ======================== SpringBodyAnimation ========================

    SpringBodyAnimation::SpringBodyAnimation()
        : mSkeleton(nullptr)
        , mGlobalStiffness(5.0f)
        , mGlobalDamping(0.5f)
        , mEnabled(true)
        , mCumulativeImpulse(0, 0, 0)
    {
    }

    SpringBodyAnimation::~SpringBodyAnimation() = default;

    void SpringBodyAnimation::initialize(SceneUtil::Skeleton* skeleton)
    {
        mSkeleton = skeleton;
        if (Settings::game().mSpringBodyAnimation)
        {
            mGlobalStiffness = Settings::game().mSpringStiffness;
            mGlobalDamping = Settings::game().mSpringDamping;
            setupSpringBones();
        }
    }

    void SpringBodyAnimation::setupSpringBones()
    {
        // Setup springs for key bones that should respond to physics
        std::vector<std::string> springBones = {
            "Bip01 Head",
            "Bip01 Neck",
            "Bip01 Spine2",
            "Bip01 Spine1",
            "Bip01 Spine",
            "Bip01 L UpperArm",
            "Bip01 L Forearm",
            "Bip01 R UpperArm",
            "Bip01 R Forearm"
        };

        for (const auto& bone : springBones)
        {
            SpringConstraint spring;
            spring.stiffness = mGlobalStiffness * computeBoneWeight(bone);
            spring.damping = mGlobalDamping;
            mBoneSprings[bone] = spring;
        }
    }

    float SpringBodyAnimation::computeBoneWeight(const std::string& boneName) const
    {
        // Head and upper body respond more to impacts
        if (boneName.find("Head") != std::string::npos)
            return 1.5f;
        if (boneName.find("Neck") != std::string::npos)
            return 1.3f;
        if (boneName.find("Spine2") != std::string::npos)
            return 1.2f;
        if (boneName.find("Arm") != std::string::npos)
            return 0.8f;
        return 1.0f;
    }

    void SpringBodyAnimation::applyImpulse(const osg::Vec3f& worldImpulse, const osg::Vec3f& hitPoint)
    {
        if (!mEnabled)
            return;

        mCumulativeImpulse += worldImpulse;

        // Distribute impulse to springs based on distance from hit point
        for (auto& [boneName, spring] : mBoneSprings)
        {
            // All bones receive some impulse, scaled by position in hierarchy
            float weight = computeBoneWeight(boneName);
            spring.applyImpulse(worldImpulse * weight * 0.1f);
        }
    }

    void SpringBodyAnimation::applyForce(const osg::Vec3f& force)
    {
        if (!mEnabled)
            return;

        for (auto& [boneName, spring] : mBoneSprings)
        {
            float weight = computeBoneWeight(boneName);
            spring.targetOffset = force * weight * 0.01f;
        }
    }

    void SpringBodyAnimation::update(float dt)
    {
        if (!mEnabled)
            return;

        for (auto& [boneName, spring] : mBoneSprings)
        {
            spring.update(dt);
        }

        // Decay cumulative impulse
        mCumulativeImpulse *= 0.95f;
    }

    bool SpringBodyAnimation::getBoneOffset(const std::string& boneName, osg::Vec3f& offset) const
    {
        auto it = mBoneSprings.find(boneName);
        if (it != mBoneSprings.end())
        {
            offset = it->second.offset;
            return true;
        }
        return false;
    }

    bool SpringBodyAnimation::getBoneRotationOffset(const std::string& boneName, osg::Quat& rotOffset) const
    {
        auto it = mBoneSprings.find(boneName);
        if (it != mBoneSprings.end())
        {
            // Convert offset to rotation
            const osg::Vec3f& offset = it->second.offset;
            float angle = offset.length() * 0.01f; // Scale factor
            if (angle > 0.001f)
            {
                osg::Vec3f axis = offset ^ osg::Vec3f(0, 0, 1);
                if (axis.length() < 0.001f)
                    axis = osg::Vec3f(1, 0, 0);
                axis.normalize();
                rotOffset.makeRotate(angle, axis);
                return true;
            }
        }
        rotOffset = osg::Quat();
        return false;
    }

    void SpringBodyAnimation::setStiffness(float stiffness)
    {
        mGlobalStiffness = stiffness;
        for (auto& [boneName, spring] : mBoneSprings)
        {
            spring.stiffness = stiffness * computeBoneWeight(boneName);
        }
    }

    void SpringBodyAnimation::setDamping(float damping)
    {
        mGlobalDamping = damping;
        for (auto& [boneName, spring] : mBoneSprings)
        {
            spring.damping = damping;
        }
    }

    void SpringBodyAnimation::reset()
    {
        for (auto& [boneName, spring] : mBoneSprings)
        {
            spring.reset();
        }
        mCumulativeImpulse = osg::Vec3f(0, 0, 0);
    }

    bool SpringBodyAnimation::hasActiveMotion() const
    {
        for (const auto& [boneName, spring] : mBoneSprings)
        {
            if (spring.velocity.length2() > 0.01f || spring.offset.length2() > 0.01f)
                return true;
        }
        return false;
    }

    // ======================== ActiveRagdoll ========================

    ActiveRagdoll::ActiveRagdoll()
        : mSkeleton(nullptr)
        , mBlendFactor(0.0f)
        , mTargetBlend(0.0f)
        , mTransitionTimer(0.0f)
        , mTransitionDuration(0.0f)
        , mEnabled(true)
    {
    }

    ActiveRagdoll::~ActiveRagdoll() = default;

    void ActiveRagdoll::initialize(SceneUtil::Skeleton* skeleton)
    {
        mSkeleton = skeleton;
        mEnabled = Settings::game().mActiveRagdoll;
        mBlendFactor = Settings::game().mRagdollBlendFactor;
    }

    void ActiveRagdoll::applyImpulse(const osg::Vec3f& impulse, const osg::Vec3f& point)
    {
        if (!mEnabled)
            return;

        // Apply impulse to bone physics states
        for (auto& [boneName, state] : mBoneStates)
        {
            state.velocity += impulse * 0.1f;
            // Add some angular velocity based on impact direction
            state.angularVelocity += (impulse ^ osg::Vec3f(0, 1, 0)) * 0.01f;
        }

        // Check if impulse is strong enough to trigger ragdoll
        if (shouldTriggerRagdoll(impulse.length()))
        {
            transitionTo(1.0f, 0.5f);
        }
    }

    bool ActiveRagdoll::shouldTriggerRagdoll(float impulseMagnitude) const
    {
        float threshold = Settings::game().mImpactImpulseThreshold;
        return impulseMagnitude >= threshold;
    }

    void ActiveRagdoll::transitionTo(float targetBlend, float duration)
    {
        mTargetBlend = std::clamp(targetBlend, 0.0f, 1.0f);
        mTransitionDuration = duration;
        mTransitionTimer = duration;
    }

    void ActiveRagdoll::update(float dt)
    {
        if (!mEnabled)
            return;

        // Update transition
        if (mTransitionTimer > 0.0f)
        {
            mTransitionTimer -= dt;
            if (mTransitionTimer <= 0.0f)
            {
                mBlendFactor = mTargetBlend;
                mTransitionTimer = 0.0f;
            }
            else
            {
                float t = 1.0f - (mTransitionTimer / mTransitionDuration);
                // Smooth interpolation
                t = t * t * (3.0f - 2.0f * t);
                mBlendFactor = mBlendFactor + (mTargetBlend - mBlendFactor) * t * dt * 5.0f;
            }
        }

        // Update bone physics (simplified - in full implementation would use Bullet)
        for (auto& [boneName, state] : mBoneStates)
        {
            // Simple gravity
            state.velocity += osg::Vec3f(0, 0, -9.81f) * dt;
            state.position += state.velocity * dt;
            state.angularVelocity *= 0.98f; // Damping

            // Apply angular velocity to rotation
            float angle = state.angularVelocity.length() * dt;
            if (angle > 0.001f)
            {
                osg::Vec3f axis = state.angularVelocity;
                axis.normalize();
                osg::Quat deltaRot;
                deltaRot.makeRotate(angle, axis);
                state.rotation = deltaRot * state.rotation;
            }
        }
    }

    bool ActiveRagdoll::getBlendedTransform(
        const std::string& boneName, const osg::Matrix& animTransform, osg::Matrix& blendedTransform) const
    {
        if (!mEnabled || mBlendFactor < 0.001f)
        {
            blendedTransform = animTransform;
            return false;
        }

        auto it = mBoneStates.find(boneName);
        if (it == mBoneStates.end())
        {
            blendedTransform = animTransform;
            return false;
        }

        const BonePhysicsState& state = it->second;

        // Create physics transform
        osg::Matrix physicsTransform;
        physicsTransform.setRotate(state.rotation);
        physicsTransform.setTrans(state.position);

        // Blend animation and physics
        if (mBlendFactor >= 0.999f)
        {
            blendedTransform = physicsTransform;
        }
        else
        {
            // Blend position
            osg::Vec3f animPos = animTransform.getTrans();
            osg::Vec3f physPos = physicsTransform.getTrans();
            osg::Vec3f blendedPos = animPos * (1.0f - mBlendFactor) + physPos * mBlendFactor;

            // Blend rotation
            osg::Quat animRot = animTransform.getRotate();
            osg::Quat physRot = physicsTransform.getRotate();
            osg::Quat blendedRot;
            blendedRot.slerp(mBlendFactor, animRot, physRot);

            blendedTransform.setRotate(blendedRot);
            blendedTransform.setTrans(blendedPos);
        }

        return true;
    }
}
