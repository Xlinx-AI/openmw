#include "grassbindings.hpp"

#include <components/esm/refid.hpp>
#include <components/lua/luastate.hpp>
#include <components/settings/values.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"
#include "../mwrender/grassinteraction.hpp"
#include "../mwworld/cellstore.hpp"

#include "context.hpp"
#include "luabindings.hpp"

namespace MWLua
{
    sol::table initGrassPackage(const Context& context)
    {
        sol::state_view lua = context.sol();
        sol::table api(lua, sol::create);

        // Grass interaction configuration
        api["isEnabled"] = []() -> bool {
            return Settings::game().mGrassInteraction;
        };

        api["setEnabled"] = [&context](bool enabled) {
            if (context.mType != Context::Global && context.mType != Context::Menu)
                throw std::runtime_error("Only global scripts can modify grass settings");
            MWRender::getGrassInteractionSystem().setEnabled(enabled);
        };

        api["getInteractionRadius"] = []() -> float {
            return Settings::game().mGrassInteractionRadius;
        };

        api["setInteractionRadius"] = [&context](float radius) {
            if (context.mType != Context::Global && context.mType != Context::Menu)
                throw std::runtime_error("Only global scripts can modify grass settings");
            if (radius < 50.0f || radius > 300.0f)
                throw std::runtime_error("Grass interaction radius must be between 50 and 300");
            MWRender::getGrassInteractionSystem().setInteractionRadius(radius);
        };

        api["getBendIntensity"] = []() -> float {
            return Settings::game().mGrassBendIntensity;
        };

        api["setBendIntensity"] = [&context](float intensity) {
            if (context.mType != Context::Global && context.mType != Context::Menu)
                throw std::runtime_error("Only global scripts can modify grass settings");
            if (intensity < 0.0f || intensity > 2.0f)
                throw std::runtime_error("Grass bend intensity must be between 0 and 2");
            MWRender::getGrassInteractionSystem().setBendIntensity(intensity);
        };

        api["getRecoverySpeed"] = []() -> float {
            return Settings::game().mGrassRecoverySpeed;
        };

        api["setRecoverySpeed"] = [&context](float speed) {
            if (context.mType != Context::Global && context.mType != Context::Menu)
                throw std::runtime_error("Only global scripts can modify grass settings");
            if (speed < 0.5f || speed > 5.0f)
                throw std::runtime_error("Grass recovery speed must be between 0.5 and 5");
            MWRender::getGrassInteractionSystem().setRecoverySpeed(speed);
        };

        // Add actor to grass interaction (using Object wrapper)
        api["registerActor"] = [&context](const LObject& actor) {
            if (context.mType == Context::Menu)
                throw std::runtime_error("Cannot register actors in menu context");
            
            const MWWorld::Ptr& ptr = actor.ptr();
            if (!ptr.isEmpty())
            {
                osg::Vec3f position = ptr.getRefData().getPosition().asVec3();
                osg::Vec3f velocity(0, 0, 0); // Will be updated each frame
                MWRender::getGrassInteractionSystem().updateActorPosition(ptr, position, velocity);
            }
        };

        // Remove actor from grass interaction
        api["unregisterActor"] = [&context](const LObject& actor) {
            if (context.mType == Context::Menu)
                throw std::runtime_error("Cannot unregister actors in menu context");
                
            const MWWorld::Ptr& ptr = actor.ptr();
            if (!ptr.isEmpty())
            {
                MWRender::getGrassInteractionSystem().removeActor(ptr);
            }
        };

        // Backward compatibility: maintain previous behavior
        api["VERSION"] = 1;

        return LuaUtil::makeReadOnly(api);
    }
}
