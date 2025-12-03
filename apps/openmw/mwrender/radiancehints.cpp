#include "radiancehints.hpp"

#include <osg/StateSet>
#include <osg/Texture3D>
#include <osg/Image>

#include <components/resource/resourcesystem.hpp>
#include <components/resource/scenemanager.hpp>
#include <components/settings/values.hpp>

#include <algorithm>
#include <cmath>

namespace MWRender
{
    RadianceHints::RadianceHints(Resource::ResourceSystem* resourceSystem, osg::Group* rootNode)
        : mResourceSystem(resourceSystem)
        , mRootNode(rootNode)
        , mEnabled(false)
        , mQuality(1)
        , mProbeGridResolution(16)
        , mMaxDistance(4096.0f)
        , mIntensity(1.0f)
        , mUpdateRate(2)
        , mFrameCounter(0)
        , mShadowFragmentsEnabled(false)
        , mShadowFragmentResolution(128)
        , mActorDynamicShadows(true)
        , mPlayerDynamicShadows(true)
        , mObjectDynamicShadows(false)
        , mLastProbeCenter(0, 0, 0)
    {
        processChangedSettings();

        if (mEnabled)
        {
            createProbeGrid();
        }
    }

    RadianceHints::~RadianceHints()
    {
        clearResources();
    }

    void RadianceHints::processChangedSettings()
    {
        const auto& settings = Settings::radianceHints();

        mEnabled = settings.mEnableRadianceHints;
        mQuality = settings.mQuality;
        mProbeGridResolution = settings.mProbeGridResolution;
        mMaxDistance = settings.mMaxDistance;
        mIntensity = settings.mIntensity;
        mUpdateRate = settings.mUpdateRate;

        mShadowFragmentsEnabled = settings.mEnableShadowFragments;
        mShadowFragmentResolution = settings.mShadowFragmentResolution;
        mActorDynamicShadows = settings.mActorDynamicShadows;
        mPlayerDynamicShadows = settings.mPlayerDynamicShadows;
        mObjectDynamicShadows = settings.mObjectDynamicShadows;

        // Adjust settings based on quality level
        switch (mQuality)
        {
            case 0: // Low
                mProbeGridResolution = std::min(mProbeGridResolution, 8);
                mUpdateRate = std::max(mUpdateRate, 4);
                break;
            case 1: // Medium
                mProbeGridResolution = std::min(mProbeGridResolution, 16);
                mUpdateRate = std::max(mUpdateRate, 2);
                break;
            case 2: // High
                mProbeGridResolution = std::min(mProbeGridResolution, 32);
                break;
            case 3: // Ultra
                // Use full settings
                break;
        }

        if (mEnabled && !mProbeTexture)
        {
            createProbeGrid();
        }
        else if (!mEnabled && mProbeTexture)
        {
            clearResources();
        }
    }

    void RadianceHints::createProbeGrid()
    {
        // Create 3D texture for storing radiance hints
        mProbeTexture = new osg::Texture3D;
        mProbeTexture->setTextureSize(mProbeGridResolution, mProbeGridResolution, mProbeGridResolution);
        mProbeTexture->setInternalFormat(GL_RGBA16F);
        mProbeTexture->setSourceFormat(GL_RGBA);
        mProbeTexture->setSourceType(GL_FLOAT);
        mProbeTexture->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR);
        mProbeTexture->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
        mProbeTexture->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
        mProbeTexture->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);
        mProbeTexture->setWrap(osg::Texture::WRAP_R, osg::Texture::CLAMP_TO_EDGE);

        // Initialize with neutral lighting
        osg::ref_ptr<osg::Image> image = new osg::Image;
        image->allocateImage(mProbeGridResolution, mProbeGridResolution, mProbeGridResolution,
            GL_RGBA, GL_FLOAT);
        float* data = reinterpret_cast<float*>(image->data());
        for (int i = 0; i < mProbeGridResolution * mProbeGridResolution * mProbeGridResolution * 4; i += 4)
        {
            data[i + 0] = 0.1f; // R - ambient
            data[i + 1] = 0.1f; // G - ambient
            data[i + 2] = 0.1f; // B - ambient
            data[i + 3] = 1.0f; // A - valid flag
        }
        mProbeTexture->setImage(image);

        // Create uniforms
        mProbeUniform = new osg::Uniform("radianceProbes", 8); // Texture unit 8
        mGIIntensityUniform = new osg::Uniform("giIntensity", mIntensity);
        mProbeGridCenterUniform = new osg::Uniform("probeGridCenter", osg::Vec3f(0, 0, 0));
        mProbeGridSizeUniform = new osg::Uniform("probeGridSize", mMaxDistance);
    }

    void RadianceHints::clearResources()
    {
        mProbeTexture = nullptr;
        mProbeUniform = nullptr;
        mGIIntensityUniform = nullptr;
        mProbeGridCenterUniform = nullptr;
        mProbeGridSizeUniform = nullptr;
    }

    void RadianceHints::update(float dt, const osg::Vec3f& playerPos)
    {
        if (!mEnabled || !mProbeTexture)
            return;

        mFrameCounter++;
        if (mFrameCounter < mUpdateRate)
            return;

        mFrameCounter = 0;

        // Update probe grid center based on player position
        updateProbes(playerPos);
    }

    void RadianceHints::updateProbes(const osg::Vec3f& center)
    {
        // Only update if player has moved significantly
        osg::Vec3f delta = center - mLastProbeCenter;
        float moveThreshold = mMaxDistance / (mProbeGridResolution * 2.0f);

        if (delta.length() < moveThreshold)
            return;

        mLastProbeCenter = center;

        if (mProbeGridCenterUniform)
            mProbeGridCenterUniform->set(center);

        // In a full implementation, this would:
        // 1. Render scene from probe positions
        // 2. Sample lighting at each probe
        // 3. Update the 3D texture with new radiance data
        // For now, we just track the center position
    }

    void RadianceHints::applyToStateSet(osg::StateSet* stateSet)
    {
        if (!mEnabled || !mProbeTexture || !stateSet)
            return;

        stateSet->setTextureAttributeAndModes(8, mProbeTexture, osg::StateAttribute::ON);
        stateSet->addUniform(mProbeUniform);
        stateSet->addUniform(mGIIntensityUniform);
        stateSet->addUniform(mProbeGridCenterUniform);
        stateSet->addUniform(mProbeGridSizeUniform);
    }

    void RadianceHints::setEnabled(bool enabled)
    {
        mEnabled = enabled;
        if (enabled && !mProbeTexture)
            createProbeGrid();
        else if (!enabled && mProbeTexture)
            clearResources();
    }

    void RadianceHints::setQuality(int quality)
    {
        mQuality = std::clamp(quality, 0, 3);
        processChangedSettings();
    }

    // ShadowFragments implementation

    ShadowFragments::ShadowFragments(Resource::ResourceSystem* resourceSystem)
        : mResourceSystem(resourceSystem)
        , mEnabled(false)
        , mResolution(128)
    {
    }

    ShadowFragments::~ShadowFragments()
    {
        mShadowFragmentTexture = nullptr;
        mShadowFragmentUniform = nullptr;
    }

    void ShadowFragments::update(const osg::Vec3f& playerPos, const std::vector<osg::Vec3f>& actorPositions)
    {
        if (!mEnabled)
            return;

        // In a full implementation, this would:
        // 1. Render shadow volumes from dynamic objects
        // 2. Store fragments in a 3D texture
        // 3. Use for shadow ray marching
    }

    void ShadowFragments::applyToStateSet(osg::StateSet* stateSet)
    {
        if (!mEnabled || !mShadowFragmentTexture || !stateSet)
            return;

        stateSet->setTextureAttributeAndModes(9, mShadowFragmentTexture, osg::StateAttribute::ON);
        stateSet->addUniform(mShadowFragmentUniform);
    }

    void ShadowFragments::setResolution(int resolution)
    {
        mResolution = std::clamp(resolution, 64, 512);
    }
}
