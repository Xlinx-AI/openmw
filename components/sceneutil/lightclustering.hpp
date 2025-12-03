#ifndef OPENMW_COMPONENTS_SCENEUTIL_LIGHTCLUSTERING_H
#define OPENMW_COMPONENTS_SCENEUTIL_LIGHTCLUSTERING_H

#include <memory>
#include <vector>

#include <osg/BoundingBox>
#include <osg/Light>
#include <osg/Matrix>
#include <osg/Vec3f>
#include <osg/ref_ptr>
#include <osg/Uniform>
#include <osg/StateSet>

namespace SceneUtil
{
    class LightSource;

    /// Cluster-based light culling system for improved lighting performance.
    /// Divides the view frustum into 3D cells (clusters) and assigns lights to each cluster.
    class LightClustering
    {
    public:
        struct Settings
        {
            bool enabled = true;
            int clustersX = 16;
            int clustersY = 9;
            int clustersZ = 24;
            int maxLightsPerCluster = 32;
            float clusterNear = 1.0f;
            float clusterFar = 8192.0f;
        };

        struct ClusterData
        {
            osg::BoundingBoxf bounds;
            std::vector<int> lightIndices;
        };

        LightClustering();
        explicit LightClustering(const Settings& settings);

        /// Update cluster bounds based on camera matrices
        void updateClusters(const osg::Matrixf& viewMatrix, const osg::Matrixf& projMatrix,
                           float screenWidth, float screenHeight);

        /// Assign lights to clusters based on their positions and radii
        void assignLights(const std::vector<std::pair<osg::Vec3f, float>>& lightsViewSpace);

        /// Get the cluster index for a screen position and depth
        int getClusterIndex(int x, int y, int z) const;

        /// Convert a linear depth value to a cluster Z index
        int depthToClusterZ(float linearDepth) const;

        /// Get the number of clusters in each dimension
        void getClusterDimensions(int& x, int& y, int& z) const;

        /// Get total number of clusters
        int getTotalClusters() const;

        /// Get the light indices for a specific cluster
        const std::vector<int>& getClusterLights(int clusterIndex) const;

        /// Get settings
        const Settings& getSettings() const { return mSettings; }

        /// Check if clustering is enabled
        bool isEnabled() const { return mSettings.enabled; }

        /// Apply cluster data to uniforms
        void applyToUniforms(osg::StateSet* stateSet, size_t frame);

        /// Get cluster info uniform for shader access
        osg::Uniform* getClusterInfoUniform() { return mClusterInfoUniform.get(); }

        /// Get cluster light indices buffer
        osg::Uniform* getClusterLightIndicesUniform() { return mClusterLightIndicesUniform.get(); }

        /// Get cluster light counts buffer
        osg::Uniform* getClusterLightCountsUniform() { return mClusterLightCountsUniform.get(); }

        /// Clear all cluster assignments
        void clear();

    private:
        Settings mSettings;

        std::vector<ClusterData> mClusters;
        osg::Matrixf mInvViewMatrix;
        osg::Matrixf mInvProjMatrix;
        float mScreenWidth;
        float mScreenHeight;

        // Uniform data for shader access
        osg::ref_ptr<osg::Uniform> mClusterInfoUniform;
        osg::ref_ptr<osg::Uniform> mClusterLightIndicesUniform;
        osg::ref_ptr<osg::Uniform> mClusterLightCountsUniform;

        static const std::vector<int> sEmptyLightIndices;

        void initClusters();
        void loadSettings();

        osg::BoundingBoxf calculateClusterBounds(int x, int y, int z) const;
        bool sphereIntersectsAABB(const osg::Vec3f& center, float radius, const osg::BoundingBoxf& aabb) const;

        // Convert depth slice to linear depth
        float clusterZToDepth(int z) const;
    };
}

#endif // OPENMW_COMPONENTS_SCENEUTIL_LIGHTCLUSTERING_H
