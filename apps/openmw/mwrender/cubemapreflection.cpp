#include "cubemapreflection.hpp"

#include <osg/ClipPlane>
#include <osg/Depth>
#include <osg/FrontFace>
#include <osg/Texture2D>

#include <osgUtil/CullVisitor>

#include <components/resource/resourcesystem.hpp>
#include <components/resource/scenemanager.hpp>
#include <components/sceneutil/depth.hpp>
#include <components/settings/values.hpp>

#include "vismask.hpp"

namespace MWRender
{
    // Cubemap face directions (OpenGL standard)
    static const osg::Vec3f sFaceDirections[6] = {
        osg::Vec3f(1, 0, 0),  // +X
        osg::Vec3f(-1, 0, 0), // -X
        osg::Vec3f(0, 1, 0),  // +Y
        osg::Vec3f(0, -1, 0), // -Y
        osg::Vec3f(0, 0, 1),  // +Z
        osg::Vec3f(0, 0, -1)  // -Z
    };

    static const osg::Vec3f sFaceUpVectors[6] = {
        osg::Vec3f(0, -1, 0), // +X
        osg::Vec3f(0, -1, 0), // -X
        osg::Vec3f(0, 0, 1),  // +Y
        osg::Vec3f(0, 0, -1), // -Y
        osg::Vec3f(0, -1, 0), // +Z
        osg::Vec3f(0, -1, 0)  // -Z
    };

    CubemapReflection::CubemapReflection(osg::Group* sceneRoot, Resource::ResourceSystem* resourceSystem,
        int cubemapSize, CubemapReflectionType type, float reflectionDistance)
        : mSceneRoot(sceneRoot)
        , mResourceSystem(resourceSystem)
        , mType(type)
        , mCubemapSize(cubemapSize)
        , mWaterLevel(0.0f)
        , mReflectionDistance(reflectionDistance)
        , mLastUpdateHour(-1.0f)
        , mCameraPosition(0, 0, 0)
        , mNodeMask(Mask_Scene | Mask_Sky | Mask_Terrain | Mask_Static | Mask_Lighting)
        , mReflectActors(type == CubemapReflectionType::Dynamic)
        , mDirty(true)
        , mCurrentFace(0)
        , mFacesPerFrame(type == CubemapReflectionType::Dynamic ? 2 : 6)
    {
        // Create cubemap texture
        mCubemap = new osg::TextureCubeMap;
        mCubemap->setTextureSize(mCubemapSize, mCubemapSize);
        mCubemap->setInternalFormat(GL_RGB8);
        mCubemap->setSourceFormat(GL_RGB);
        mCubemap->setSourceType(GL_UNSIGNED_BYTE);
        mCubemap->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
        mCubemap->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);
        mCubemap->setWrap(osg::Texture::WRAP_R, osg::Texture::CLAMP_TO_EDGE);
        mCubemap->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR);
        mCubemap->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);

        createCubemapCameras();
    }

    CubemapReflection::~CubemapReflection()
    {
        for (auto& camera : mCameras)
        {
            if (camera)
                removeChild(camera);
        }
    }

    void CubemapReflection::createCubemapCameras()
    {
        for (int face = 0; face < 6; ++face)
        {
            mCameras[face] = new osg::Camera;
            setupCubemapCamera(face);
            addChild(mCameras[face]);
        }
    }

    void CubemapReflection::setupCubemapCamera(int face)
    {
        osg::Camera* camera = mCameras[face].get();

        camera->setName("CubemapReflectionCamera_" + std::to_string(face));
        camera->setReferenceFrame(osg::Camera::ABSOLUTE_RF);
        camera->setRenderOrder(osg::Camera::PRE_RENDER, -100 + face);
        camera->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
        camera->setViewport(0, 0, mCubemapSize, mCubemapSize);

        // 90 degree FOV for cubemap face
        camera->setProjectionMatrixAsPerspective(90.0, 1.0, 1.0, mReflectionDistance);

        // Attach to cubemap face
        camera->attach(osg::Camera::COLOR_BUFFER, mCubemap.get(), 0, face);

        // Setup clear and cull settings
        camera->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        camera->setClearColor(osg::Vec4(0.5f, 0.6f, 0.7f, 1.0f)); // Sky color fallback
        camera->setComputeNearFarMode(osg::CullSettings::DO_NOT_COMPUTE_NEAR_FAR);

        // Small feature culling for performance
        camera->setSmallFeatureCullingPixelSize(Settings::water().mSmallFeatureCullingPixelSize * 2.0f);

        // Setup reflection-specific state
        osg::StateSet* stateSet = camera->getOrCreateStateSet();

        // Flip front face for reflections
        osg::ref_ptr<osg::FrontFace> frontFace = new osg::FrontFace;
        frontFace->setMode(osg::FrontFace::CLOCKWISE);
        stateSet->setAttributeAndModes(frontFace, osg::StateAttribute::ON);

        // Add water clip plane
        osg::ref_ptr<osg::ClipPlane> clipPlane = new osg::ClipPlane(0);
        clipPlane->setClipPlane(osg::Plane(osg::Vec3d(0, 0, 1), osg::Vec3d(0, 0, mWaterLevel)));
        stateSet->setAttributeAndModes(clipPlane, osg::StateAttribute::ON);

        // Mark as reflection for shaders
        stateSet->addUniform(new osg::Uniform("isReflection", true));

        // Initially invisible
        camera->setNodeMask(0);
    }

    osg::Matrix CubemapReflection::computeFaceMatrix(int face) const
    {
        // Position camera at water level, looking at face direction
        osg::Vec3f pos = mCameraPosition;
        pos.z() = mWaterLevel; // Reflect from water surface

        return osg::Matrix::lookAt(pos, pos + sFaceDirections[face], sFaceUpVectors[face]);
    }

    void CubemapReflection::setWaterLevel(float waterLevel)
    {
        mWaterLevel = waterLevel;
        mDirty = true;

        // Update clip planes
        for (int face = 0; face < 6; ++face)
        {
            if (mCameras[face])
            {
                osg::StateSet* stateSet = mCameras[face]->getOrCreateStateSet();
                osg::ClipPlane* clipPlane
                    = dynamic_cast<osg::ClipPlane*>(stateSet->getAttribute(osg::StateAttribute::CLIPPLANE, 0));
                if (clipPlane)
                    clipPlane->setClipPlane(osg::Plane(osg::Vec3d(0, 0, 1), osg::Vec3d(0, 0, mWaterLevel)));
            }
        }
    }

    void CubemapReflection::setCameraPosition(const osg::Vec3f& pos)
    {
        if ((pos - mCameraPosition).length2() > 100.0f) // Only update if moved significantly
        {
            mCameraPosition = pos;
            mDirty = true;
        }
    }

    void CubemapReflection::setScene(osg::Node* scene)
    {
        mScene = scene;
        for (auto& camera : mCameras)
        {
            if (camera)
            {
                camera->removeChildren(0, camera->getNumChildren());
                if (scene)
                    camera->addChild(scene);
            }
        }
        mDirty = true;
    }

    bool CubemapReflection::needsUpdate(float currentGameHour) const
    {
        if (mType == CubemapReflectionType::Dynamic)
            return true;

        // Static cubemap - check if time of day changed significantly
        float updateInterval = Settings::water().mStaticCubemapUpdateInterval;
        if (updateInterval <= 0.0f)
        {
            // Update on any time change (1 game minute = 1/60 hour)
            return std::abs(currentGameHour - mLastUpdateHour) > 0.0167f;
        }
        return std::abs(currentGameHour - mLastUpdateHour) >= updateInterval;
    }

    void CubemapReflection::update(float currentGameHour, bool forceUpdate)
    {
        if (!mScene)
            return;

        bool shouldUpdate = forceUpdate || mDirty || needsUpdate(currentGameHour);
        if (!shouldUpdate)
            return;

        // Set node mask based on reflection type
        unsigned int mask = mNodeMask;
        if (!mReflectActors)
            mask &= ~(Mask_Actor | Mask_Player);

        // For dynamic mode, only update subset of faces per frame
        int startFace = 0;
        int endFace = 6;

        if (mType == CubemapReflectionType::Dynamic && !forceUpdate && !mDirty)
        {
            startFace = mCurrentFace;
            endFace = std::min(mCurrentFace + mFacesPerFrame, 6);
            mCurrentFace = (mCurrentFace + mFacesPerFrame) % 6;
        }

        for (int face = startFace; face < endFace; ++face)
        {
            if (mCameras[face])
            {
                mCameras[face]->setViewMatrix(computeFaceMatrix(face));
                mCameras[face]->setCullMask(mask);
                mCameras[face]->setNodeMask(Mask_RenderToTexture);
            }
        }

        // Hide other faces this frame
        for (int face = 0; face < startFace; ++face)
            mCameras[face]->setNodeMask(0);
        for (int face = endFace; face < 6; ++face)
            mCameras[face]->setNodeMask(0);

        mLastUpdateHour = currentGameHour;
        mDirty = false;
    }

    std::unique_ptr<CubemapReflection> createCubemapReflection(
        osg::Group* sceneRoot, Resource::ResourceSystem* resourceSystem)
    {
        int method = Settings::water().mReflectionMethod;
        int cubemapSize = Settings::water().mCubemapSize;
        float distance = Settings::water().mCubemapReflectionDistance;

        CubemapReflectionType type = (method == 2) ? CubemapReflectionType::Static : CubemapReflectionType::Dynamic;

        return std::make_unique<CubemapReflection>(sceneRoot, resourceSystem, cubemapSize, type, distance);
    }
}
