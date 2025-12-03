#ifndef OPENMW_COMPONENTS_SETTINGS_CATEGORIES_ANIMATIONLOD_H
#define OPENMW_COMPONENTS_SETTINGS_CATEGORIES_ANIMATIONLOD_H

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
    struct AnimationLODCategory : WithIndex
    {
        using WithIndex::WithIndex;

        // Enable Animation LOD system for performance optimization
        SettingValue<bool> mEnabled{ mIndex, "Animation LOD", "enabled" };

        // Skip animation updates for actors outside camera FOV
        SettingValue<bool> mFOVCulling{ mIndex, "Animation LOD", "fov culling" };

        // Distance at which animations are updated at full rate (in game units)
        SettingValue<float> mFullRateDistance{ mIndex, "Animation LOD", "full rate distance",
            makeClampSanitizerFloat(0, 8192) };

        // Distance beyond which animations are updated at minimum rate (in game units)
        SettingValue<float> mMinRateDistance{ mIndex, "Animation LOD", "min rate distance",
            makeClampSanitizerFloat(0, 16384) };

        // Minimum animation update interval for distant actors (in seconds)
        SettingValue<float> mMinUpdateInterval{ mIndex, "Animation LOD", "min update interval",
            makeClampSanitizerFloat(0.0f, 1.0f) };

        // Extra FOV margin for culling (in degrees) to prevent popping
        SettingValue<float> mFOVMargin{ mIndex, "Animation LOD", "fov margin",
            makeClampSanitizerFloat(0, 45) };
    };
}

#endif
