module;

#include "Prelude.hpp"

export module NirnKit.Actor;

import NirnKit.Magic;

namespace NK
{
    export auto ActorHasAbsoluteKeyword(RE::Actor* actor, const RE::BGSKeyword* keyword)
    {
        if (!actor || !keyword) return false;
        
        return actor->HasKeyword(keyword) || TargetHasEffectWithKeyword(actor, keyword);
    }
}