#ifndef OPENMW_COMPONENTS_SETTINGS_CATEGORIES_ENTITYCULLING_H
#define OPENMW_COMPONENTS_SETTINGS_CATEGORIES_ENTITYCULLING_H

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
    struct EntityCullingCategory : WithIndex
    {
        using WithIndex::WithIndex;

        // Enable entity culling optimization for large entity counts
        SettingValue<bool> mEnableEntityCulling{ mIndex, "Entity Culling", "enable entity culling" };

        // Maximum number of visible entities near player before culling kicks in
        SettingValue<int> mMaxVisibleEntities{ mIndex, "Entity Culling", "max visible entities",
            makeClampSanitizerInt(50, 1000) };

        // Distance threshold for aggressive culling (units from player)
        SettingValue<float> mCullingDistance{ mIndex, "Entity Culling", "culling distance",
            makeClampSanitizerFloat(1024.0f, 16384.0f) };

        // Priority distance - entities closer than this get higher priority
        SettingValue<float> mPriorityDistance{ mIndex, "Entity Culling", "priority distance",
            makeClampSanitizerFloat(256.0f, 4096.0f) };

        // Enable LOD-based entity culling
        SettingValue<bool> mEnableLodCulling{ mIndex, "Entity Culling", "enable lod culling" };

        // Minimum screen size for entities (pixels)
        SettingValue<float> mMinScreenSize{ mIndex, "Entity Culling", "min screen size",
            makeClampSanitizerFloat(1.0f, 32.0f) };

        // Enable frustum culling optimization
        SettingValue<bool> mEnableFrustumCulling{ mIndex, "Entity Culling", "enable frustum culling" };

        // Enable occlusion culling for entities
        SettingValue<bool> mEnableOcclusionCulling{ mIndex, "Entity Culling", "enable occlusion culling" };

        // Actor culling - cull actors that are not visible
        SettingValue<bool> mCullActors{ mIndex, "Entity Culling", "cull actors" };

        // Static object culling
        SettingValue<bool> mCullStatics{ mIndex, "Entity Culling", "cull statics" };
    };
}

#endif
