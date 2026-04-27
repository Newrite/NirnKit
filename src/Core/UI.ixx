module;

#include "Prelude.hpp"

export module NirnKit.UI;

namespace NK {

  // Checking if Wheeler exists, getting check function if so
  using IsWheelerOpen_t = bool(*)();

  IsWheelerOpen_t GetIsWheelerOpen()
  {
      HMODULE wheelerHandle = GetModuleHandleA("wheeler.dll");

      if (!wheelerHandle)
          return nullptr;

      return reinterpret_cast<IsWheelerOpen_t>(
          GetProcAddress(wheelerHandle, "IsWheelerOpen"));
  }

  // Perform check for wheeler, and do not dodge if the mod exists and menu is open
  auto CheckIfWheelerOpen() -> bool
  {
      if (auto isOpenFunc = GetIsWheelerOpen()) {
          if (isOpenFunc()) {
              return true;
          }
          return false;
      }
      return false;
  }

  export auto IsAnyMenuOpen() -> bool
  {
    const auto ui = RE::UI::GetSingleton();
    const auto player = RE::PlayerCharacter::GetSingleton();
    constexpr std::string_view lootMenu = "LootMenu";
    constexpr std::string_view lootMenuIE = "LootMenuIE";
      
    if (player && ui &&
        (ui->IsMenuOpen(RE::TweenMenu::MENU_NAME) || ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME) ||
         ui->IsMenuOpen(RE::MagicMenu::MENU_NAME) || ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME) ||
         ui->IsMenuOpen(RE::LockpickingMenu::MENU_NAME) || ui->IsMenuOpen(RE::RaceSexMenu::MENU_NAME) ||
         ui->IsMenuOpen(RE::StatsMenu::MENU_NAME) || ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME) ||
         ui->IsMenuOpen(RE::Console::MENU_NAME) || ui->IsMenuOpen(RE::FaderMenu::MENU_NAME) ||
         ui->IsMenuOpen(RE::FavoritesMenu::MENU_NAME) || ui->IsMenuOpen(RE::SleepWaitMenu::MENU_NAME) ||
         ui->IsMenuOpen(RE::JournalMenu::MENU_NAME) || ui->IsMenuOpen(RE::BarterMenu::MENU_NAME) ||
         ui->IsMenuOpen(RE::MainMenu::MENU_NAME) || ui->IsMenuOpen(RE::BookMenu::MENU_NAME) ||
         ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME) || ui->IsMenuOpen(RE::GiftMenu::MENU_NAME) || ui->
         IsMenuOpen(RE::MessageBoxMenu::MENU_NAME) || ui->IsMenuOpen(RE::TrainingMenu::MENU_NAME) || ui->
         IsMenuOpen(RE::MapMenu::MENU_NAME) || ui->IsMenuOpen(RE::TutorialMenu::MENU_NAME) || ui->
         IsMenuOpen(RE::LevelUpMenu::MENU_NAME) || ui->IsMenuOpen(RE::CreditsMenu::MENU_NAME) || ui->
         IsMenuOpen(lootMenuIE) || ui->IsMenuOpen(lootMenu) || CheckIfWheelerOpen())) {
      return true;
    }
    return false;
  }
    
  export auto IsMenuOpen(const std::string_view menuName) -> bool
  {
      auto ui = RE::UI::GetSingleton();
      return ui && ui->IsMenuOpen(menuName);
  }  

}
