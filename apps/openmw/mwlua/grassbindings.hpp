#ifndef MWLUA_GRASSBINDINGS_H
#define MWLUA_GRASSBINDINGS_H

#include <sol/forward.hpp>

namespace MWLua
{
    struct Context;

    sol::table initGrassPackage(const Context& context);
}

#endif // MWLUA_GRASSBINDINGS_H
