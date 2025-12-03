#ifndef OPENMW_COMPONENTS_SETTINGS_CATEGORIES_WATER_H
#define OPENMW_COMPONENTS_SETTINGS_CATEGORIES_WATER_H

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
    // Water reflection method types
    // 0 = Planar reflections (highest quality, real-time)
    // 1 = Dynamic cubemap (medium quality, updates near FOV only)
    // 2 = Static cubemap (lowest quality, updates only on time-of-day change)
    
    struct WaterCategory : WithIndex
    {
        using WithIndex::WithIndex;

        SettingValue<bool> mShader{ mIndex, "Water", "shader" };
        SettingValue<int> mRttSize{ mIndex, "Water", "rtt size", makeMaxSanitizerInt(1) };
        SettingValue<bool> mRefraction{ mIndex, "Water", "refraction" };
        SettingValue<int> mReflectionDetail{ mIndex, "Water", "reflection detail", makeClampSanitizerInt(0, 5) };
        SettingValue<int> mRainRippleDetail{ mIndex, "Water", "rain ripple detail", makeClampSanitizerInt(0, 2) };
        SettingValue<float> mSmallFeatureCullingPixelSize{ mIndex, "Water", "small feature culling pixel size",
            makeMaxStrictSanitizerFloat(0) };
        SettingValue<float> mRefractionScale{ mIndex, "Water", "refraction scale", makeClampSanitizerFloat(0, 1) };
        SettingValue<bool> mSunlightScattering{ mIndex, "Water", "sunlight scattering" };
        SettingValue<bool> mWobblyShores{ mIndex, "Water", "wobbly shores" };
        
        // New optimized reflection settings
        // Reflection method: 0=Planar (best), 1=Dynamic Cubemap (fast), 2=Static Cubemap (fastest)
        SettingValue<int> mReflectionMethod{ mIndex, "Water", "reflection method", makeClampSanitizerInt(0, 2) };
        // Distance for dynamic cubemap updates (only objects within this range get reflected)
        SettingValue<float> mCubemapReflectionDistance{ mIndex, "Water", "cubemap reflection distance",
            makeClampSanitizerFloat(500.0f, 8192.0f) };
        // Cubemap resolution for reflection (power of 2)
        SettingValue<int> mCubemapSize{ mIndex, "Water", "cubemap size", makeClampSanitizerInt(64, 1024) };
        // Update interval for static cubemap in game hours (0 = update on every time-of-day change)
        SettingValue<float> mStaticCubemapUpdateInterval{ mIndex, "Water", "static cubemap update interval",
            makeMaxSanitizerFloat(0.0f) };
        // Enable planar reflection frustum optimization
        SettingValue<bool> mPlanarReflectionFrustumCull{ mIndex, "Water", "planar reflection frustum cull" };
        // Planar reflection LOD bias (reduces geometry detail in reflections)
        SettingValue<float> mPlanarReflectionLodBias{ mIndex, "Water", "planar reflection lod bias",
            makeClampSanitizerFloat(0.0f, 4.0f) };
    };
}

#endif
