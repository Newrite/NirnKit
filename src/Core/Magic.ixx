module;

#include "Prelude.hpp"

export module NirnKit.Magic;

namespace NK
{
    namespace detail
    {
        using ActiveEffectVisitResult = RE::BSContainer::ForEachResult;

        template <class R>
        concept ActiveEffectCallbackResult =
            std::same_as<R, void> ||
            std::same_as<R, bool> ||
            std::same_as<R, ActiveEffectVisitResult>;

        template <class F, class Arg>
        concept ActiveEffectCallback =
            requires(F& fn, Arg arg)
            {
                requires ActiveEffectCallbackResult<
                    std::invoke_result_t<F&, Arg>
                >;
            };

        template <class Pred, class Arg>
        concept EffectPredicate =
            requires(Pred& pred, Arg arg)
            {
                { std::invoke(pred, arg) } -> std::convertible_to<bool>;
            };

        template <class F, class Arg>
        auto InvokeActiveEffectCallback(F& fn, Arg arg)
            -> ActiveEffectVisitResult
        {
            using R = std::invoke_result_t<F&, Arg>;

            if constexpr (std::same_as<R, void>) {
                std::invoke(fn, arg);
                return ActiveEffectVisitResult::kContinue;
            } else if constexpr (std::same_as<R, bool>) {
                return std::invoke(fn, arg)
                    ? ActiveEffectVisitResult::kContinue
                    : ActiveEffectVisitResult::kStop;
            } else {
                return std::invoke(fn, arg);
            }
        }
    }

    export struct ActiveEffectView
    {
        RE::ActiveEffect* activeEffect = nullptr;
        RE::Effect* effect = nullptr;
        RE::EffectSetting* baseEffect = nullptr;

        [[nodiscard]]
        auto IsInactive() const -> bool
        {
            if (!activeEffect) return true;
            return activeEffect->flags.any(RE::ActiveEffect::Flag::kInactive);
        }

        [[nodiscard]]
        auto IsActive() const -> bool
        {
            return !IsInactive();
        }

        [[nodiscard]]
        auto IsRecover() const -> bool
        {
            if (!baseEffect) return false;
            return baseEffect->data.flags.any(
                RE::EffectSetting::EffectSettingData::Flag::kRecover
            );
        }

        [[nodiscard]]
        auto IsDetrimental() const -> bool
        {
            if (!baseEffect) return false;
            return baseEffect->data.flags.any(
                RE::EffectSetting::EffectSettingData::Flag::kDetrimental
            );
        }
        
        [[nodiscard]]
        auto IsHostile() const -> bool
        {
            if (!baseEffect) return false;
            return baseEffect->data.flags.any(
                RE::EffectSetting::EffectSettingData::Flag::kHostile
            );
        }
        
        [[nodiscard]]
        auto IsHideInUI() const -> bool
        {
            if (!baseEffect) return false;
            return baseEffect->data.flags.any(
                RE::EffectSetting::EffectSettingData::Flag::kHideInUI
            );
        }
        
        [[nodiscard]]
        auto IsContainsAV(const RE::ActorValue av, const bool strictArchetype = true) const -> bool
        {
            if (!baseEffect) return false;
            if (!strictArchetype) return baseEffect->data.primaryAV == av || baseEffect->data.secondaryAV == av;
            
            if (!IsAVArchetype()) return false;
            
            if (baseEffect->data.archetype == RE::EffectSetting::Archetype::kValueModifier) return baseEffect->data.primaryAV == av;
            if (baseEffect->data.archetype == RE::EffectSetting::Archetype::kAbsorb) return baseEffect->data.primaryAV == av;
            if (baseEffect->data.archetype == RE::EffectSetting::Archetype::kPeakValueModifier) return baseEffect->data.primaryAV == av;
            
            if (baseEffect->data.archetype == RE::EffectSetting::Archetype::kDualValueModifier)
                return baseEffect->data.primaryAV == av || baseEffect->data.secondaryAV == av;
            
            return false;
        }
        
        [[nodiscard]]
        auto IsAVArchetype() const -> bool
        {
            if (!baseEffect) return false;
            
            if (baseEffect->data.archetype == RE::EffectSetting::Archetype::kValueModifier) return true;
            if (baseEffect->data.archetype == RE::EffectSetting::Archetype::kAbsorb) return true;
            if (baseEffect->data.archetype == RE::EffectSetting::Archetype::kPeakValueModifier) return true;
            if (baseEffect->data.archetype == RE::EffectSetting::Archetype::kDualValueModifier) return true;
            
            return false;
        }

        [[nodiscard]]
        auto HasKeyword(const RE::BGSKeyword* keyword) const -> bool
        {
            return keyword && baseEffect && baseEffect->HasKeyword(keyword);
        }

        [[nodiscard]]
        auto MagnitudeActive() const -> float
        {
            if (!activeEffect) return 0.f;
            return activeEffect->GetMagnitude();
        }
        
        [[nodiscard]]
        auto DurationActive() const -> float
        {
            if (!activeEffect) return 0.f;
            return activeEffect->duration;
        }
        
        [[nodiscard]]
        auto MagnitudeBase() const -> float
        {
            if (!effect) return 0.f;
            return effect->GetMagnitude();
        }
        
        [[nodiscard]]
        auto DurationBase() const -> float
        {
            if (!effect) return 0.f;
            return effect->GetDuration();
        }
        
    };

    export [[nodiscard]]
    inline auto TryMakeActiveEffectView(RE::ActiveEffect* activeEffect)
        -> std::optional<ActiveEffectView>
    {
        if (!activeEffect)
            return std::nullopt;

        if (!activeEffect->effect)
            return std::nullopt;

        if (!activeEffect->effect->baseEffect)
            return std::nullopt;

        return ActiveEffectView{
            .activeEffect = activeEffect,
            .effect = activeEffect->effect,
            .baseEffect = activeEffect->effect->baseEffect,
        };
    }

    template <class F>
        requires detail::ActiveEffectCallback<F, RE::ActiveEffect*>
    struct RawActiveEffectVisitor final : RE::MagicTarget::ForEachActiveEffectVisitor
    {
        explicit RawActiveEffectVisitor(F fn) :
            callback(std::move(fn))
        {
        }

        auto Accept(RE::ActiveEffect* activeEffect)
            -> RE::BSContainer::ForEachResult override
        {
            return detail::InvokeActiveEffectCallback(callback, activeEffect);
        }

        F callback;
    };

    template <class F>
    RawActiveEffectVisitor(F) -> RawActiveEffectVisitor<F>;

    template <class F>
        requires detail::ActiveEffectCallback<F, ActiveEffectView>
    struct ActiveEffectViewVisitor final : RE::MagicTarget::ForEachActiveEffectVisitor
    {
        explicit ActiveEffectViewVisitor(F fn) :
            callback(std::move(fn))
        {
        }

        auto Accept(RE::ActiveEffect* activeEffect)
            -> RE::BSContainer::ForEachResult override
        {
            auto view = TryMakeActiveEffectView(activeEffect);

            if (!view)
                return RE::BSContainer::ForEachResult::kContinue;

            return detail::InvokeActiveEffectCallback(callback, *view);
        }

        F callback;
    };

    template <class F>
    ActiveEffectViewVisitor(F) -> ActiveEffectViewVisitor<F>;

    template <class Pred, class F>
        requires
            detail::EffectPredicate<Pred, ActiveEffectView> &&
            detail::ActiveEffectCallback<F, ActiveEffectView>
    struct FilteredActiveEffectViewVisitor final : RE::MagicTarget::ForEachActiveEffectVisitor
    {
        FilteredActiveEffectViewVisitor(Pred pred, F fn) :
            predicate(std::move(pred)),
            callback(std::move(fn))
        {
        }

        auto Accept(RE::ActiveEffect* activeEffect)
            -> RE::BSContainer::ForEachResult override
        {
            auto view = TryMakeActiveEffectView(activeEffect);

            if (!view)
                return RE::BSContainer::ForEachResult::kContinue;

            if (!std::invoke(predicate, *view))
                return RE::BSContainer::ForEachResult::kContinue;

            return detail::InvokeActiveEffectCallback(callback, *view);
        }

        Pred predicate;
        F callback;
    };

    template <class Pred, class F>
    FilteredActiveEffectViewVisitor(Pred, F) -> FilteredActiveEffectViewVisitor<Pred, F>;

    export template <class F>
        requires detail::ActiveEffectCallback<std::decay_t<F>, RE::ActiveEffect*>
    auto VisitActiveEffectsRaw(RE::MagicTarget* target, F&& fn) -> void
    {
        if (!target)
            return;

        auto visitor = RawActiveEffectVisitor<std::decay_t<F>>{
            std::forward<F>(fn)
        };

        target->VisitEffects(visitor);
    }

    export template <class F>
        requires detail::ActiveEffectCallback<std::decay_t<F>, RE::ActiveEffect*>
    auto VisitActiveEffectsRaw(RE::Actor* actor, F&& fn) -> void
    {
        if (!actor)
            return;

        VisitActiveEffectsRaw(actor->AsMagicTarget(), std::forward<F>(fn));
    }

    export template <class F>
        requires detail::ActiveEffectCallback<std::decay_t<F>, ActiveEffectView>
    auto VisitActiveEffects(RE::MagicTarget* target, F&& fn) -> void
    {
        if (!target)
            return;

        auto visitor = ActiveEffectViewVisitor<std::decay_t<F>>{
            std::forward<F>(fn)
        };

        target->VisitEffects(visitor);
    }

    export template <class F>
        requires detail::ActiveEffectCallback<std::decay_t<F>, ActiveEffectView>
    auto VisitActiveEffects(RE::Actor* actor, F&& fn) -> void
    {
        if (!actor)
            return;

        VisitActiveEffects(actor->AsMagicTarget(), std::forward<F>(fn));
    }

    export template <class Pred, class F>
        requires
            detail::EffectPredicate<std::decay_t<Pred>, ActiveEffectView> &&
            detail::ActiveEffectCallback<std::decay_t<F>, ActiveEffectView>
    auto VisitActiveEffectsWhere(RE::MagicTarget* target, Pred&& pred, F&& fn) -> void
    {
        if (!target)
            return;

        auto visitor = FilteredActiveEffectViewVisitor<
            std::decay_t<Pred>,
            std::decay_t<F>
        >{
            std::forward<Pred>(pred),
            std::forward<F>(fn)
        };

        target->VisitEffects(visitor);
    }

    export template <class Pred, class F>
        requires
            detail::EffectPredicate<std::decay_t<Pred>, ActiveEffectView> &&
            detail::ActiveEffectCallback<std::decay_t<F>, ActiveEffectView>
    auto VisitActiveEffectsWhere(RE::Actor* actor, Pred&& pred, F&& fn) -> void
    {
        if (!actor)
            return;

        VisitActiveEffectsWhere(
            actor->AsMagicTarget(),
            std::forward<Pred>(pred),
            std::forward<F>(fn)
        );
    }
    
    export inline auto ValidateEffectData(const RE::Effect* effect) -> bool
    {
        return effect && effect->baseEffect;
    }
    
    export inline auto ValidateActiveEffectData(const RE::ActiveEffect* activeEffect) -> bool
    {
        return activeEffect && ValidateEffectData(activeEffect->effect);
    }
    
}
