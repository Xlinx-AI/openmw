#ifndef OPENMW_COMPONENTS_SETTINGS_CATEGORIES_VRAMMANAGEMENT_H
#define OPENMW_COMPONENTS_SETTINGS_CATEGORIES_VRAMMANAGEMENT_H

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
    struct VRAMManagementCategory : WithIndex
    {
        using WithIndex::WithIndex;

        // Enable VRAM management and unloading
        SettingValue<bool> mEnableVramManagement{ mIndex, "VRAM Management", "enable vram management" };

        // Maximum VRAM usage target in MB (0 = auto-detect)
        SettingValue<int> mMaxVramUsage{ mIndex, "VRAM Management", "max vram usage",
            makeMaxSanitizerInt(0) };

        // Enable aggressive unloading of static geometry
        SettingValue<bool> mUnloadStaticGeometry{ mIndex, "VRAM Management", "unload static geometry" };

        // Enable unloading of duplicate textures
        SettingValue<bool> mUnloadDuplicateTextures{ mIndex, "VRAM Management", "unload duplicate textures" };

        // Enable texture compression to save VRAM
        SettingValue<bool> mEnableTextureCompression{ mIndex, "VRAM Management", "enable texture compression" };

        // Enable geometry deduplication
        SettingValue<bool> mEnableGeometryDeduplication{ mIndex, "VRAM Management", "enable geometry deduplication" };

        // Time in seconds before unused resources are unloaded
        SettingValue<float> mUnloadDelay{ mIndex, "VRAM Management", "unload delay",
            makeClampSanitizerFloat(1.0f, 60.0f) };

        // Enable swap to RAM when VRAM is full
        SettingValue<bool> mEnableSwapToRam{ mIndex, "VRAM Management", "enable swap to ram" };

        // Maximum RAM usage for swapped VRAM data in MB
        SettingValue<int> mMaxRamSwapUsage{ mIndex, "VRAM Management", "max ram swap usage",
            makeMaxSanitizerInt(0) };

        // Enable streaming of large textures
        SettingValue<bool> mEnableTextureStreaming{ mIndex, "VRAM Management", "enable texture streaming" };

        // Mipmap bias for VRAM optimization (negative = lower quality, positive = higher)
        SettingValue<float> mMipmapBias{ mIndex, "VRAM Management", "mipmap bias",
            makeClampSanitizerFloat(-4.0f, 4.0f) };

        // Enable geometry LOD optimization
        SettingValue<bool> mEnableGeometryLod{ mIndex, "VRAM Management", "enable geometry lod" };
    };
}

#endif
