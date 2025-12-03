#ifndef OPENMW_MWRENDER_GRASSINTERACTION_H
#define OPENMW_MWRENDER_GRASSINTERACTION_H

#include <osg/Vec3f>
#include <osg/Vec4f>
#include <osg/Texture2D>
#include <osg/ref_ptr>

#include <map>
#include <vector>

namespace osg
{
    class Group;
    class StateSet;
    class Uniform;
}

namespace MWWorld
{
    class Ptr;
}

namespace MWRender
{
    /// Grass interaction point (actor touching grass)
    struct GrassInteractionPoint
    {
        osg::Vec3f position;
        osg::Vec3f velocity;
        float radius;
        float intensity;
        float timestamp;
    };

    /// Grass Interaction System
    /// Provides GPU-accelerated grass bending based on actor positions
    /// Uses uniform array for efficiency (avoids texture updates each frame)
    class GrassInteractionSystem
    {
    public:
        static constexpr int MAX_INTERACTION_POINTS = 16;

        GrassInteractionSystem();
        ~GrassInteractionSystem();

        /// Update actor position for grass interaction
        void updateActorPosition(const MWWorld::Ptr& actor, const osg::Vec3f& position, const osg::Vec3f& velocity);

        /// Remove actor from interaction system
        void removeActor(const MWWorld::Ptr& actor);

        /// Update interaction simulation
        void update(float dt);

        /// Apply interaction uniforms to a state set (used by groundcover shader)
        void applyToStateSet(osg::StateSet* stateSet);

        /// Get uniform for shader binding
        osg::Uniform* getInteractionPointsUniform() { return mInteractionPointsUniform.get(); }
        osg::Uniform* getInteractionCountUniform() { return mInteractionCountUniform.get(); }

        /// Enable/disable grass interaction
        void setEnabled(bool enabled) { mEnabled = enabled; }
        bool isEnabled() const { return mEnabled; }

        /// Set interaction parameters
        void setInteractionRadius(float radius) { mInteractionRadius = radius; }
        void setBendIntensity(float intensity) { mBendIntensity = intensity; }
        void setRecoverySpeed(float speed) { mRecoverySpeed = speed; }

    private:
        void updateUniforms();
        void pruneOldInteractions(float currentTime);

        std::map<unsigned int, GrassInteractionPoint> mActorInteractions;
        std::vector<GrassInteractionPoint> mDecayingInteractions;

        osg::ref_ptr<osg::Uniform> mInteractionPointsUniform;
        osg::ref_ptr<osg::Uniform> mInteractionCountUniform;
        osg::ref_ptr<osg::Uniform> mInteractionParamsUniform;

        float mInteractionRadius;
        float mBendIntensity;
        float mRecoverySpeed;
        float mCurrentTime;
        bool mEnabled;
    };

    /// Singleton accessor
    GrassInteractionSystem& getGrassInteractionSystem();
}

#endif
