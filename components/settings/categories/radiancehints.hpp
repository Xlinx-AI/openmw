#ifndef OPENMW_COMPONENTS_SETTINGS_CATEGORIES_RADIANCEHINTS_H
#define OPENMW_COMPONENTS_SETTINGS_CATEGORIES_RADIANCEHINTS_H

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
    struct RadianceHintsCategory : WithIndex
    {
        using WithIndex::WithIndex;

        // Enable dynamic Radiance Hints Global Illumination
        SettingValue<bool> mEnableRadianceHints{ mIndex, "Radiance Hints", "enable radiance hints" };

        // Quality preset: 0 = Low, 1 = Medium, 2 = High, 3 = Ultra
        SettingValue<int> mQuality{ mIndex, "Radiance Hints", "quality", makeClampSanitizerInt(0, 3) };

        // Resolution of the radiance hint probe grid
        SettingValue<int> mProbeGridResolution{ mIndex, "Radiance Hints", "probe grid resolution",
            makeClampSanitizerInt(8, 64) };

        // Maximum distance for GI contribution
        SettingValue<float> mMaxDistance{ mIndex, "Radiance Hints", "max distance",
            makeClampSanitizerFloat(512.0f, 16384.0f) };

        // GI intensity multiplier
        SettingValue<float> mIntensity{ mIndex, "Radiance Hints", "intensity",
            makeClampSanitizerFloat(0.0f, 2.0f) };

        // Enable shadow fragments for dynamic objects (NPCs, player, etc.)
        SettingValue<bool> mEnableShadowFragments{ mIndex, "Radiance Hints", "enable shadow fragments" };

        // Shadow fragment resolution
        SettingValue<int> mShadowFragmentResolution{ mIndex, "Radiance Hints", "shadow fragment resolution",
            makeClampSanitizerInt(64, 512) };

        // Enable dynamic shadows for actors
        SettingValue<bool> mActorDynamicShadows{ mIndex, "Radiance Hints", "actor dynamic shadows" };

        // Enable dynamic shadows for player
        SettingValue<bool> mPlayerDynamicShadows{ mIndex, "Radiance Hints", "player dynamic shadows" };

        // Enable dynamic shadows for objects
        SettingValue<bool> mObjectDynamicShadows{ mIndex, "Radiance Hints", "object dynamic shadows" };

        // Update rate for GI (frames between updates, 1 = every frame)
        SettingValue<int> mUpdateRate{ mIndex, "Radiance Hints", "update rate",
            makeClampSanitizerInt(1, 8) };
    };
}

#endif
