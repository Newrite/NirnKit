module;

#include "Prelude.hpp"
#include <expected>

export module NirnKit.Core;

import NirnKit.STLExtension;

namespace NK
{
    namespace detail
    {
        #ifdef max
        #undef max
        #endif
        
        template <class T>
        [[nodiscard]]
        auto ResolveFailure(std::string_view input, std::string_view reason) -> Result<T>
        {
            return Internal::ParseFailure<T>("ResolveForm", input, reason);
        }

        [[nodiscard]]
        constexpr auto IEndsWithAscii(std::string_view text, std::string_view suffix) noexcept -> bool
        {
            if (text.size() < suffix.size())
                return false;

            text.remove_prefix(text.size() - suffix.size());
            return Internal::IEqualsAscii(text, suffix);
        }

        [[nodiscard]]
        constexpr auto LooksLikePluginName(std::string_view text) noexcept -> bool
        {
            text = Internal::TrimAsciiView(text);

            return IEndsWithAscii(text, ".esp") ||
                IEndsWithAscii(text, ".esm") ||
                IEndsWithAscii(text, ".esl");
        }

        [[nodiscard]]
        constexpr auto CountChar(std::string_view text, char ch) noexcept -> size_t
        {
            size_t count = 0;

            for (char c : text)
            {
                if (c == ch)
                    ++count;
            }

            return count;
        }

        struct FormSpecParts
        {
            std::string_view left;
            std::string_view right;
            char delimiter{};
        };

        [[nodiscard]]
        auto SplitFormSpec(std::string_view input) -> Result<FormSpecParts>
        {
            const auto original = input;
            input = Internal::TrimAsciiView(input);

            if (input.empty())
                return ResolveFailure<FormSpecParts>(original, "empty form spec");

            const size_t tildeCount = CountChar(input, '~');
            const size_t pipeCount = CountChar(input, '|');
            const size_t delimiterCount = tildeCount + pipeCount;

            if (delimiterCount == 0)
                return ResolveFailure<FormSpecParts>(original, "missing delimiter, expected '~' or '|'");

            if (delimiterCount != 1)
                return ResolveFailure<FormSpecParts>(original, "form spec must contain exactly one delimiter");

            const char delimiter = tildeCount == 1 ? '~' : '|';
            const size_t delimiterPos = input.find(delimiter);

            auto left = Internal::TrimAsciiView(input.substr(0, delimiterPos));
            auto right = Internal::TrimAsciiView(input.substr(delimiterPos + 1));

            if (left.empty() || right.empty())
                return ResolveFailure<FormSpecParts>(original, "form spec contains empty part");

            return FormSpecParts{
                .left = left,
                .right = right,
                .delimiter = delimiter,
            };
        }

        struct ParsedFormSpec
        {
            RE::FormID localFormID{};
            std::string_view modName;
        };

        template <class T>
        concept FormIDInteger =
            std::integral<std::remove_cvref_t<T>> &&
            !std::same_as<std::remove_cvref_t<T>, bool>;

        template <FormIDInteger T>
        [[nodiscard]]
        auto NormalizeLocalFormID(T value) -> Result<RE::FormID>
        {
            using CleanT = std::remove_cvref_t<T>;

            if constexpr (std::signed_integral<CleanT>)
            {
                if (value < 0)
                    return ResolveFailure<RE::FormID>("<integer>", "FormID cannot be negative");
            }

            const auto unsignedValue = static_cast<std::uintmax_t>(value);
            const auto maxValue = static_cast<std::uintmax_t>(std::numeric_limits<RE::FormID>::max()
            )
            ;

            if (unsignedValue > maxValue)
                return ResolveFailure<RE::FormID>("<integer>", "FormID is out of range");

            return static_cast<RE::FormID>(unsignedValue);
        }

        [[nodiscard]]
        constexpr auto StartsWithHexPrefix(std::string_view input) noexcept -> bool
        {
            return input.size() >= 2 &&
                input[0] == '0' &&
                (input[1] == 'x' || input[1] == 'X');
        }

        [[nodiscard]]
        constexpr auto ContainsHexLetter(std::string_view input) noexcept -> bool
        {
            for (const char c : input)
            {
                if ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))
                    return true;
            }

            return false;
        }

        [[nodiscard]]
        auto ParseLocalFormID(std::string_view input) -> Result<RE::FormID>
        {
            const auto original = input;
            input = Internal::TrimAsciiView(input);

            if (input.empty())
                return ResolveFailure<RE::FormID>(original, "empty FormID");

            if (input.front() == '-')
                return ResolveFailure<RE::FormID>(original, "FormID cannot be negative");

            if (input.front() == '+')
            {
                input.remove_prefix(1);

                if (input.empty())
                    return ResolveFailure<RE::FormID>(original, "empty FormID after sign");
            }

            if (StartsWithHexPrefix(input))
            {
                auto parsed = ParseHex<RE::FormID>(input);

                if (!parsed)
                    return ResolveFailure<RE::FormID>(original, "invalid hex FormID");

                return *parsed;
            }

            // Auto policy for string FormIDs:
            //   "2125"   -> decimal 2125
            //   "0x84D"  -> hex 0x84D, also decimal 2125
            //   "84D"    -> bare hex because it contains A-F letters
            //   "800"    -> decimal 800; write "0x800" for hex 0x800.
            auto decimal = ParseUInt<RE::FormID>(input, 10);

            if (decimal)
                return *decimal;

            if (ContainsHexLetter(input))
            {
                auto hex = ParseHex<RE::FormID>(input);

                if (hex)
                    return *hex;
            }

            return ResolveFailure<RE::FormID>(original, "invalid FormID");
        }

        [[nodiscard]]
        auto ParseFormSpec(std::string_view input) -> Result<ParsedFormSpec>
        {
            const auto original = input;

            auto parts = SplitFormSpec(input);

            if (!parts)
                return ResolveFailure<ParsedFormSpec>(original, "invalid form spec");

            const bool leftIsPlugin = LooksLikePluginName(parts->left);
            const bool rightIsPlugin = LooksLikePluginName(parts->right);

            if (leftIsPlugin && rightIsPlugin)
                return ResolveFailure<ParsedFormSpec>(original, "both parts look like plugin names");

            if (!leftIsPlugin && !rightIsPlugin)
                return ResolveFailure<ParsedFormSpec>(
                    original, "one part must be plugin name ending with .esp, .esm or .esl");

            const std::string_view modName = leftIsPlugin ? parts->left : parts->right;
            const std::string_view formIDText = leftIsPlugin ? parts->right : parts->left;

            auto localFormID = ParseLocalFormID(formIDText);

            if (!localFormID)
                return ResolveFailure<ParsedFormSpec>(original, "invalid FormID part");

            return ParsedFormSpec{
                .localFormID = *localFormID,
                .modName = modName,
            };
        }
    }

    using _GetFormEditorID = const char* (*)(std::uint32_t);

    export auto GetEditorId(const RE::TESForm* form) -> const char*
    {
        if (!form)
        {
            return "Null EDID";
        }

        switch (form->GetFormType())
        {
        case RE::FormType::Keyword:
        case RE::FormType::LocationRefType:
        case RE::FormType::Action:
        case RE::FormType::MenuIcon:
        case RE::FormType::Global:
        case RE::FormType::HeadPart:
        case RE::FormType::Race:
        case RE::FormType::Sound:
        case RE::FormType::Script:
        case RE::FormType::Navigation:
        case RE::FormType::Cell:
        case RE::FormType::WorldSpace:
        case RE::FormType::Land:
        case RE::FormType::NavMesh:
        case RE::FormType::Dialogue:
        case RE::FormType::Quest:
        case RE::FormType::Idle:
        case RE::FormType::AnimatedObject:
        case RE::FormType::ImageAdapter:
        case RE::FormType::VoiceType:
        case RE::FormType::Ragdoll:
        case RE::FormType::DefaultObject:
        case RE::FormType::MusicType:
        case RE::FormType::StoryManagerBranchNode:
        case RE::FormType::StoryManagerQuestNode:
        case RE::FormType::StoryManagerEventNode:
            return form->GetFormEditorID();
        default:
            {
                static auto tweaks = GetModuleHandle("po3_Tweaks");
                static auto func = reinterpret_cast<_GetFormEditorID>(GetProcAddress(tweaks, "GetFormEditorID"));
                if (func)
                {
                    auto result = func(form->GetFormID());
                    if (!result) return "Null EDID";
                    return result;
                }
                return "Null EDID";
            }
        }
    }

    export auto GetFormInfo(const RE::TESForm* form) -> std::string
    {
        if (!form)
        {
            return "Null Form";
        }

        const char* fileName = "Null File";
        if (const auto file = form->GetFile(0))
        {
            fileName = file->fileName;
        }

        const char* editorId = GetEditorId(form);
        if (!editorId) editorId = "No EditorID";

        const char* formName = form->GetName();
        if (!formName) formName = "No Name";

        const char* objectTypeName = form->GetObjectTypeName();
        if (!objectTypeName) objectTypeName = "No ObjectTypeName";

        return std::format("{:#x}~{}|{} ({}-{})", form->GetFormID(), fileName, editorId, formName, objectTypeName);
    }

    auto PlaySoundAtImpl(const RE::TESObjectREFR* reference, RE::BGSSoundDescriptorForm* descriptor,
                         const float volume) -> bool
    {
        if (!reference || !descriptor) return false;

        RE::BSSoundHandle sound;
        sound.soundID = RE::BSSoundHandle::kInvalidID;
        sound.assumeSuccess = false;
        sound.state = RE::BSSoundHandle::AssumedState::kPlaying;

        if (const auto manager = RE::BSAudioManager::GetSingleton())
        {
            if (!manager->GetSoundHandle(sound, descriptor))
            {
                return false;
            }

            if (!sound.SetPosition(reference->GetPosition()))
            {
                return false;
            }

            sound.SetObjectToFollow(reference->Get3D());
            sound.SetVolume(volume);
        }

        return sound.Play();
    }

    export auto PlaySoundAt(const RE::TESObjectREFR* reference, RE::BGSSoundDescriptorForm* descriptor,
                            float volume = -1.f) -> bool
    {
        if (!reference || !descriptor) return false;

        if (!descriptor->soundDescriptor) return false;

        const auto sound_category = descriptor->soundDescriptor->category;
        if (volume <= -1.f)
        {
            volume = sound_category ? sound_category->GetCategoryVolume() : 1.f;
        }

        return PlaySoundAtImpl(reference, descriptor, volume);
    }

    export [[nodiscard]]
    auto ResolveForm(RE::FormID localFormID, std::string_view modName) -> Result<RE::TESForm*>
    {
        const auto originalModName = modName;
        modName = Internal::TrimAsciiView(modName);

        if (modName.empty())
            return detail::ResolveFailure<RE::TESForm*>(originalModName, "empty mod name");

        if (!detail::LooksLikePluginName(modName))
            return detail::ResolveFailure<RE::TESForm*>(originalModName, "mod name must end with .esp, .esm or .esl");

        auto* dataHandler = RE::TESDataHandler::GetSingleton();

        if (!dataHandler)
            return detail::ResolveFailure<RE::TESForm*>(originalModName, "TESDataHandler singleton is null");

        auto* form = dataHandler->LookupForm(localFormID, modName);

        if (!form)
            return detail::ResolveFailure<RE::TESForm*>(originalModName, "form not found");

        return form;
    }

    export [[nodiscard]]
    auto ResolveForm(std::string_view formIDText, std::string_view modName) -> Result<RE::TESForm*>
    {
        auto localFormID = detail::ParseLocalFormID(formIDText);

        if (!localFormID)
            return detail::ResolveFailure<RE::TESForm*>(formIDText, "invalid FormID");

        return ResolveForm(*localFormID, modName);
    }

    export template <detail::FormIDInteger T>
    [[nodiscard]]
    auto ResolveForm(T localFormID, std::string_view modName) -> Result<RE::TESForm*>
    {
        auto normalized = detail::NormalizeLocalFormID(localFormID);

        if (!normalized)
            return std::unexpected(normalized.error());

        return ResolveForm(*normalized, modName);
    }


    export [[nodiscard]]
    auto ResolveForm(std::string_view formSpec) -> Result<RE::TESForm*>
    {
        auto parsed = detail::ParseFormSpec(formSpec);

        if (!parsed)
            return detail::ResolveFailure<RE::TESForm*>(formSpec, "invalid form spec");

        return ResolveForm(parsed->localFormID, parsed->modName);
    }

    export template <class T>
        requires std::derived_from<T, RE::TESForm>
    [[nodiscard]]
    auto ResolveFormAs(std::string_view formSpec) -> Result<T*>
    {
        auto form = ResolveForm(formSpec);

        if (!form)
            return std::unexpected(form.error());

        auto* typedForm = (*form)->As<T>();

        if (!typedForm)
            return detail::ResolveFailure<T*>(formSpec, "form has unexpected type");

        return typedForm;
    }

    export template <class T>
        requires std::derived_from<T, RE::TESForm>
    [[nodiscard]]
    auto ResolveFormAs(std::string_view formIDText, std::string_view modName) -> Result<T*>
    {
        auto form = ResolveForm(formIDText, modName);

        if (!form)
            return std::unexpected(form.error());

        auto* typedForm = (*form)->As<T>();

        if (!typedForm)
            return detail::ResolveFailure<T*>(formIDText, "form has unexpected type");

        return typedForm;
    }

    export template <class T, detail::FormIDInteger ID>
        requires std::derived_from<T, RE::TESForm>
    [[nodiscard]]
    auto ResolveFormAs(ID localFormID, std::string_view modName) -> Result<T*>
    {
        auto form = ResolveForm(localFormID, modName);

        if (!form)
            return std::unexpected(form.error());

        auto* typedForm = (*form)->As<T>();

        if (!typedForm)
            return detail::ResolveFailure<T*>("<integer>", "form has unexpected type");

        return typedForm;
    }
}
