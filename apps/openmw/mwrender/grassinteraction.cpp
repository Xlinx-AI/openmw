#include "grassinteraction.hpp"

#include <algorithm>

#include <osg/StateSet>
#include <osg/Uniform>

#include <components/settings/values.hpp>

#include "../mwworld/ptr.hpp"

namespace
{
    std::unique_ptr<MWRender::GrassInteractionSystem> sGrassInteractionSystem;
}

namespace MWRender
{
    GrassInteractionSystem::GrassInteractionSystem()
        : mInteractionRadius(Settings::game().mGrassInteractionRadius)
        , mBendIntensity(Settings::game().mGrassBendIntensity)
        , mRecoverySpeed(Settings::game().mGrassRecoverySpeed)
        , mCurrentTime(0.0f)
        , mEnabled(Settings::game().mGrassInteraction)
    {
        // Create uniforms for shader communication
        // Each interaction point is packed as vec4: xyz = position, w = intensity
        // Plus vec4 for velocity: xyz = velocity, w = radius
        mInteractionPointsUniform = new osg::Uniform(osg::Uniform::FLOAT_VEC4, "grassInteractionPoints",
            MAX_INTERACTION_POINTS * 2);
        mInteractionCountUniform = new osg::Uniform("grassInteractionCount", 0);
        mInteractionParamsUniform = new osg::Uniform("grassInteractionParams",
            osg::Vec4f(mInteractionRadius, mBendIntensity, mRecoverySpeed, 0.0f));

        // Initialize uniform array
        for (int i = 0; i < MAX_INTERACTION_POINTS * 2; ++i)
        {
            mInteractionPointsUniform->setElement(i, osg::Vec4f(0, 0, 0, 0));
        }
    }

    GrassInteractionSystem::~GrassInteractionSystem() = default;

    void GrassInteractionSystem::updateActorPosition(
        const MWWorld::Ptr& actor, const osg::Vec3f& position, const osg::Vec3f& velocity)
    {
        if (!mEnabled || actor.isEmpty())
            return;

        unsigned int actorKey = actor.getCellRef().getRefNum().mIndex;

        GrassInteractionPoint& point = mActorInteractions[actorKey];
        point.position = position;
        point.velocity = velocity;
        point.radius = mInteractionRadius;
        point.intensity = mBendIntensity * std::min(1.0f, velocity.length() / 200.0f + 0.3f);
        point.timestamp = mCurrentTime;
    }

    void GrassInteractionSystem::removeActor(const MWWorld::Ptr& actor)
    {
        if (actor.isEmpty())
            return;

        unsigned int actorKey = actor.getCellRef().getRefNum().mIndex;
        auto it = mActorInteractions.find(actorKey);
        if (it != mActorInteractions.end())
        {
            // Move to decaying list for smooth recovery
            GrassInteractionPoint decaying = it->second;
            decaying.velocity = osg::Vec3f(0, 0, 0);
            mDecayingInteractions.push_back(decaying);
            mActorInteractions.erase(it);
        }
    }

    void GrassInteractionSystem::update(float dt)
    {
        if (!mEnabled)
            return;

        mCurrentTime += dt;

        // Update decaying interactions
        for (auto it = mDecayingInteractions.begin(); it != mDecayingInteractions.end();)
        {
            it->intensity -= mRecoverySpeed * dt;
            if (it->intensity <= 0.0f)
            {
                it = mDecayingInteractions.erase(it);
            }
            else
            {
                ++it;
            }
        }

        // Prune old active interactions (actors that haven't been updated)
        pruneOldInteractions(mCurrentTime);

        // Update shader uniforms
        updateUniforms();
    }

    void GrassInteractionSystem::pruneOldInteractions(float currentTime)
    {
        const float maxAge = 0.5f; // Seconds without update before removal

        for (auto it = mActorInteractions.begin(); it != mActorInteractions.end();)
        {
            if (currentTime - it->second.timestamp > maxAge)
            {
                // Move to decaying
                GrassInteractionPoint decaying = it->second;
                decaying.velocity = osg::Vec3f(0, 0, 0);
                mDecayingInteractions.push_back(decaying);
                it = mActorInteractions.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    void GrassInteractionSystem::updateUniforms()
    {
        int count = 0;
        int uniformIndex = 0;

        // Pack active actor interactions
        for (const auto& [key, point] : mActorInteractions)
        {
            if (count >= MAX_INTERACTION_POINTS)
                break;

            // Position + intensity
            mInteractionPointsUniform->setElement(uniformIndex++,
                osg::Vec4f(point.position.x(), point.position.y(), point.position.z(), point.intensity));
            // Velocity + radius
            mInteractionPointsUniform->setElement(uniformIndex++,
                osg::Vec4f(point.velocity.x(), point.velocity.y(), point.velocity.z(), point.radius));

            ++count;
        }

        // Pack decaying interactions
        for (const auto& point : mDecayingInteractions)
        {
            if (count >= MAX_INTERACTION_POINTS)
                break;

            mInteractionPointsUniform->setElement(uniformIndex++,
                osg::Vec4f(point.position.x(), point.position.y(), point.position.z(), point.intensity));
            mInteractionPointsUniform->setElement(uniformIndex++,
                osg::Vec4f(point.velocity.x(), point.velocity.y(), point.velocity.z(), point.radius));

            ++count;
        }

        // Clear remaining slots
        while (uniformIndex < MAX_INTERACTION_POINTS * 2)
        {
            mInteractionPointsUniform->setElement(uniformIndex++, osg::Vec4f(0, 0, 0, 0));
        }

        mInteractionCountUniform->set(count);
        mInteractionParamsUniform->set(
            osg::Vec4f(mInteractionRadius, mBendIntensity, mRecoverySpeed, mCurrentTime));
    }

    void GrassInteractionSystem::applyToStateSet(osg::StateSet* stateSet)
    {
        if (!stateSet || !mEnabled)
            return;

        stateSet->addUniform(mInteractionPointsUniform);
        stateSet->addUniform(mInteractionCountUniform);
        stateSet->addUniform(mInteractionParamsUniform);
    }

    GrassInteractionSystem& getGrassInteractionSystem()
    {
        if (!sGrassInteractionSystem)
            sGrassInteractionSystem = std::make_unique<GrassInteractionSystem>();
        return *sGrassInteractionSystem;
    }
}
