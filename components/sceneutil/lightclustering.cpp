#include "lightclustering.hpp"

#include <algorithm>
#include <cmath>

#include <components/settings/values.hpp>

namespace SceneUtil
{
    const std::vector<int> LightClustering::sEmptyLightIndices;

    LightClustering::LightClustering()
        : mScreenWidth(1920)
        , mScreenHeight(1080)
    {
        loadSettings();
        initClusters();
    }

    LightClustering::LightClustering(const Settings& settings)
        : mSettings(settings)
        , mScreenWidth(1920)
        , mScreenHeight(1080)
    {
        initClusters();
    }

    void LightClustering::loadSettings()
    {
        const auto& settings = Settings::lightClustering();
        mSettings.enabled = settings.mEnabled;
        mSettings.clustersX = settings.mClustersX;
        mSettings.clustersY = settings.mClustersY;
        mSettings.clustersZ = settings.mClustersZ;
        mSettings.maxLightsPerCluster = settings.mMaxLightsPerCluster;
        mSettings.clusterNear = settings.mClusterNear;
        mSettings.clusterFar = settings.mClusterFar;
    }

    void LightClustering::initClusters()
    {
        int totalClusters = mSettings.clustersX * mSettings.clustersY * mSettings.clustersZ;
        mClusters.resize(totalClusters);

        // Initialize uniforms
        // Cluster info: (clustersX, clustersY, clustersZ, maxLightsPerCluster)
        mClusterInfoUniform = new osg::Uniform(osg::Uniform::FLOAT_VEC4, "omw_ClusterInfo");
        mClusterInfoUniform->set(osg::Vec4f(
            static_cast<float>(mSettings.clustersX),
            static_cast<float>(mSettings.clustersY),
            static_cast<float>(mSettings.clustersZ),
            static_cast<float>(mSettings.maxLightsPerCluster)
        ));

        // Light indices buffer (simplified - stores max lights per cluster for all clusters)
        int maxIndices = totalClusters * mSettings.maxLightsPerCluster;
        mClusterLightIndicesUniform = new osg::Uniform(osg::Uniform::INT, "omw_ClusterLightIndices", maxIndices);

        // Light counts per cluster
        mClusterLightCountsUniform = new osg::Uniform(osg::Uniform::INT, "omw_ClusterLightCounts", totalClusters);
    }

    void LightClustering::updateClusters(const osg::Matrixf& viewMatrix, const osg::Matrixf& projMatrix,
                                         float screenWidth, float screenHeight)
    {
        if (!mSettings.enabled)
            return;

        mInvViewMatrix = osg::Matrixf::inverse(viewMatrix);
        mInvProjMatrix = osg::Matrixf::inverse(projMatrix);
        mScreenWidth = screenWidth;
        mScreenHeight = screenHeight;

        // Recalculate cluster bounds in view space
        for (int z = 0; z < mSettings.clustersZ; ++z)
        {
            for (int y = 0; y < mSettings.clustersY; ++y)
            {
                for (int x = 0; x < mSettings.clustersX; ++x)
                {
                    int index = getClusterIndex(x, y, z);
                    mClusters[index].bounds = calculateClusterBounds(x, y, z);
                    mClusters[index].lightIndices.clear();
                }
            }
        }
    }

    osg::BoundingBoxf LightClustering::calculateClusterBounds(int x, int y, int z) const
    {
        // Calculate normalized screen coordinates for this cluster
        float xMin = static_cast<float>(x) / mSettings.clustersX;
        float xMax = static_cast<float>(x + 1) / mSettings.clustersX;
        float yMin = static_cast<float>(y) / mSettings.clustersY;
        float yMax = static_cast<float>(y + 1) / mSettings.clustersY;

        // Calculate depth range using exponential slicing for better distribution
        float nearDepth = clusterZToDepth(z);
        float farDepth = clusterZToDepth(z + 1);

        // Convert to NDC coordinates (-1 to 1)
        float ndcXMin = xMin * 2.0f - 1.0f;
        float ndcXMax = xMax * 2.0f - 1.0f;
        float ndcYMin = yMin * 2.0f - 1.0f;
        float ndcYMax = yMax * 2.0f - 1.0f;

        // Calculate view space corners
        osg::BoundingBoxf bounds;

        // Transform corners from NDC to view space
        auto transformCorner = [this](float ndcX, float ndcY, float depth) -> osg::Vec3f {
            // For view space, we directly construct the position
            // Using the inverse projection matrix
            float ndcZ = 2.0f * (depth - mSettings.clusterNear) / (mSettings.clusterFar - mSettings.clusterNear) - 1.0f;
            osg::Vec4f ndc(ndcX, ndcY, ndcZ, 1.0f);
            osg::Vec4f view = ndc * mInvProjMatrix;
            if (std::abs(view.w()) > 0.0001f)
            {
                view /= view.w();
            }
            return osg::Vec3f(view.x(), view.y(), view.z());
        };

        // Add all 8 corners of the cluster frustum
        bounds.expandBy(transformCorner(ndcXMin, ndcYMin, nearDepth));
        bounds.expandBy(transformCorner(ndcXMax, ndcYMin, nearDepth));
        bounds.expandBy(transformCorner(ndcXMin, ndcYMax, nearDepth));
        bounds.expandBy(transformCorner(ndcXMax, ndcYMax, nearDepth));
        bounds.expandBy(transformCorner(ndcXMin, ndcYMin, farDepth));
        bounds.expandBy(transformCorner(ndcXMax, ndcYMin, farDepth));
        bounds.expandBy(transformCorner(ndcXMin, ndcYMax, farDepth));
        bounds.expandBy(transformCorner(ndcXMax, ndcYMax, farDepth));

        return bounds;
    }

    float LightClustering::clusterZToDepth(int z) const
    {
        // Exponential depth slicing for better depth distribution
        // More slices near the camera, fewer far away
        float t = static_cast<float>(z) / mSettings.clustersZ;

        // Exponential distribution
        float logNear = std::log(mSettings.clusterNear + 1.0f);
        float logFar = std::log(mSettings.clusterFar + 1.0f);

        return std::exp(logNear + t * (logFar - logNear)) - 1.0f;
    }

    int LightClustering::depthToClusterZ(float linearDepth) const
    {
        if (linearDepth <= mSettings.clusterNear)
            return 0;
        if (linearDepth >= mSettings.clusterFar)
            return mSettings.clustersZ - 1;

        // Inverse of exponential depth slicing
        float logNear = std::log(mSettings.clusterNear + 1.0f);
        float logFar = std::log(mSettings.clusterFar + 1.0f);
        float logDepth = std::log(linearDepth + 1.0f);

        float t = (logDepth - logNear) / (logFar - logNear);
        return std::min(static_cast<int>(t * mSettings.clustersZ), mSettings.clustersZ - 1);
    }

    void LightClustering::assignLights(const std::vector<std::pair<osg::Vec3f, float>>& lightsViewSpace)
    {
        if (!mSettings.enabled)
            return;

        // Clear previous assignments
        for (auto& cluster : mClusters)
        {
            cluster.lightIndices.clear();
        }

        // Assign each light to clusters it potentially affects
        for (size_t lightIdx = 0; lightIdx < lightsViewSpace.size(); ++lightIdx)
        {
            const auto& [position, radius] = lightsViewSpace[lightIdx];

            // Calculate which clusters this light could affect
            // We need to find all clusters whose bounds intersect the light sphere

            // Simple broad-phase: calculate Z range
            float minDepth = -position.y() - radius; // View space Z is negative forward
            float maxDepth = -position.y() + radius;

            int minZ = depthToClusterZ(minDepth);
            int maxZ = depthToClusterZ(maxDepth);

            // Check each potentially affected cluster
            for (int z = minZ; z <= maxZ; ++z)
            {
                for (int y = 0; y < mSettings.clustersY; ++y)
                {
                    for (int x = 0; x < mSettings.clustersX; ++x)
                    {
                        int clusterIdx = getClusterIndex(x, y, z);
                        ClusterData& cluster = mClusters[clusterIdx];

                        // Check if light sphere intersects cluster AABB
                        if (sphereIntersectsAABB(position, radius, cluster.bounds))
                        {
                            if (static_cast<int>(cluster.lightIndices.size()) < mSettings.maxLightsPerCluster)
                            {
                                cluster.lightIndices.push_back(static_cast<int>(lightIdx));
                            }
                        }
                    }
                }
            }
        }
    }

    bool LightClustering::sphereIntersectsAABB(const osg::Vec3f& center, float radius,
                                               const osg::BoundingBoxf& aabb) const
    {
        // Find the closest point on the AABB to the sphere center
        float closestX = std::max(aabb.xMin(), std::min(center.x(), aabb.xMax()));
        float closestY = std::max(aabb.yMin(), std::min(center.y(), aabb.yMax()));
        float closestZ = std::max(aabb.zMin(), std::min(center.z(), aabb.zMax()));

        // Calculate distance from sphere center to closest point
        float distanceSquared =
            (closestX - center.x()) * (closestX - center.x()) +
            (closestY - center.y()) * (closestY - center.y()) +
            (closestZ - center.z()) * (closestZ - center.z());

        return distanceSquared <= (radius * radius);
    }

    int LightClustering::getClusterIndex(int x, int y, int z) const
    {
        return z * mSettings.clustersX * mSettings.clustersY + y * mSettings.clustersX + x;
    }

    void LightClustering::getClusterDimensions(int& x, int& y, int& z) const
    {
        x = mSettings.clustersX;
        y = mSettings.clustersY;
        z = mSettings.clustersZ;
    }

    int LightClustering::getTotalClusters() const
    {
        return mSettings.clustersX * mSettings.clustersY * mSettings.clustersZ;
    }

    const std::vector<int>& LightClustering::getClusterLights(int clusterIndex) const
    {
        if (clusterIndex < 0 || clusterIndex >= static_cast<int>(mClusters.size()))
            return sEmptyLightIndices;
        return mClusters[clusterIndex].lightIndices;
    }

    void LightClustering::applyToUniforms(osg::StateSet* stateSet, size_t frame)
    {
        if (!mSettings.enabled || !stateSet)
            return;

        // Update cluster info uniform
        mClusterInfoUniform->set(osg::Vec4f(
            static_cast<float>(mSettings.clustersX),
            static_cast<float>(mSettings.clustersY),
            static_cast<float>(mSettings.clustersZ),
            static_cast<float>(mSettings.maxLightsPerCluster)
        ));

        // Update light counts and indices
        int totalClusters = getTotalClusters();
        for (int i = 0; i < totalClusters; ++i)
        {
            const auto& lights = mClusters[i].lightIndices;
            mClusterLightCountsUniform->setElement(i, static_cast<int>(lights.size()));

            int baseIdx = i * mSettings.maxLightsPerCluster;
            for (size_t j = 0; j < lights.size() && j < static_cast<size_t>(mSettings.maxLightsPerCluster); ++j)
            {
                mClusterLightIndicesUniform->setElement(baseIdx + static_cast<int>(j), lights[j]);
            }
        }

        // Add uniforms to state set
        stateSet->addUniform(mClusterInfoUniform);
        stateSet->addUniform(mClusterLightCountsUniform);
        stateSet->addUniform(mClusterLightIndicesUniform);
    }

    void LightClustering::clear()
    {
        for (auto& cluster : mClusters)
        {
            cluster.lightIndices.clear();
        }
    }
}
