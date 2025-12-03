#ifndef OPENMW_COMPONENTS_SETTINGS_CATEGORIES_LIGHTCLUSTERING_H
#define OPENMW_COMPONENTS_SETTINGS_CATEGORIES_LIGHTCLUSTERING_H

#include <components/settings/sanitizerimpl.hpp>
#include <components/settings/settingvalue.hpp>

#include <osg/Math>
#include <osg/Vec2f>
#include <osg/Vec3f>

#include <cstdint>
#include <string>
#include <string_view>

namespace Settings
{
    struct LightClusteringCategory : WithIndex
    {
        using WithIndex::WithIndex;

        // Enable light clustering for improved performance
        SettingValue<bool> mEnabled{ mIndex, "Light Clustering", "enabled" };

        // Number of clusters along X axis (screen width)
        SettingValue<int> mClustersX{ mIndex, "Light Clustering", "clusters x",
            makeClampSanitizerInt(1, 64) };

        // Number of clusters along Y axis (screen height)
        SettingValue<int> mClustersY{ mIndex, "Light Clustering", "clusters y",
            makeClampSanitizerInt(1, 64) };

        // Number of clusters along Z axis (depth)
        SettingValue<int> mClustersZ{ mIndex, "Light Clustering", "clusters z",
            makeClampSanitizerInt(1, 64) };

        // Maximum lights per cluster
        SettingValue<int> mMaxLightsPerCluster{ mIndex, "Light Clustering", "max lights per cluster",
            makeClampSanitizerInt(1, 128) };

        // Near plane for cluster slicing (in game units)
        SettingValue<float> mClusterNear{ mIndex, "Light Clustering", "cluster near",
            makeMaxStrictSanitizerFloat(0) };

        // Far plane for cluster slicing (in game units)
        SettingValue<float> mClusterFar{ mIndex, "Light Clustering", "cluster far",
            makeMaxStrictSanitizerFloat(0) };
    };
}

#endif
