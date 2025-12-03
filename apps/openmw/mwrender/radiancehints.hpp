#ifndef OPENMW_MWRENDER_RADIANCEHINTS_H
#define OPENMW_MWRENDER_RADIANCEHINTS_H

#include <osg/Group>
#include <osg/Uniform>
#include <osg/ref_ptr>
#include <osg/Vec3f>

#include <memory>
#include <vector>

namespace osg
{
    class StateSet;
    class Texture3D;
    class FrameBufferObject;
}

namespace Resource
{
    class ResourceSystem;
}

namespace MWRender
{
    /// Radiance Hints Global Illumination system
    /// Provides dynamic indirect lighting using a 3D probe grid
    class RadianceHints
    {
    public:
        RadianceHints(Resource::ResourceSystem* resourceSystem, osg::Group* rootNode);
        ~RadianceHints();

        /// Update the GI probes
        void update(float dt, const osg::Vec3f& playerPos);

        /// Apply GI to a state set
        void applyToStateSet(osg::StateSet* stateSet);

        /// Enable/disable the system
        void setEnabled(bool enabled);
        bool isEnabled() const { return mEnabled; }

        /// Set quality level (0-3)
        void setQuality(int quality);

        /// Process changed settings
        void processChangedSettings();

    private:
        void createProbeGrid();
        void updateProbes(const osg::Vec3f& center);
        void clearResources();

        Resource::ResourceSystem* mResourceSystem;
        osg::ref_ptr<osg::Group> mRootNode;

        bool mEnabled;
        int mQuality;
        int mProbeGridResolution;
        float mMaxDistance;
        float mIntensity;
        int mUpdateRate;
        int mFrameCounter;

        // Shadow fragments for dynamic objects
        bool mShadowFragmentsEnabled;
        int mShadowFragmentResolution;
        bool mActorDynamicShadows;
        bool mPlayerDynamicShadows;
        bool mObjectDynamicShadows;

        // Probe data
        osg::ref_ptr<osg::Texture3D> mProbeTexture;
        osg::ref_ptr<osg::Uniform> mProbeUniform;
        osg::ref_ptr<osg::Uniform> mGIIntensityUniform;
        osg::ref_ptr<osg::Uniform> mProbeGridCenterUniform;
        osg::ref_ptr<osg::Uniform> mProbeGridSizeUniform;

        osg::Vec3f mLastProbeCenter;
    };

    /// Shadow Fragments system for dynamic object shadows
    class ShadowFragments
    {
    public:
        ShadowFragments(Resource::ResourceSystem* resourceSystem);
        ~ShadowFragments();

        /// Update shadow fragments for visible actors
        void update(const osg::Vec3f& playerPos, const std::vector<osg::Vec3f>& actorPositions);

        /// Apply shadow fragments to state set
        void applyToStateSet(osg::StateSet* stateSet);

        /// Enable/disable
        void setEnabled(bool enabled) { mEnabled = enabled; }
        bool isEnabled() const { return mEnabled; }

        /// Set resolution
        void setResolution(int resolution);

    private:
        Resource::ResourceSystem* mResourceSystem;
        bool mEnabled;
        int mResolution;

        osg::ref_ptr<osg::Texture3D> mShadowFragmentTexture;
        osg::ref_ptr<osg::Uniform> mShadowFragmentUniform;
    };
}

#endif
