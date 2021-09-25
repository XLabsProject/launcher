#pragma once

#include "cef/cef_ui.hpp"
#include "utils/nt.hpp"
#include "utils/properties.hpp"

class Launcher
{
public:
    static int Run(HINSTANCE instance);

    inline static utils::Properties properties;

private:
    static std::string GetAppdataPath();
    static void SetWorkingDirectory();
    static void PrepareEnvironment(HINSTANCE instance);

    static bool TryLockTerminationBarrier();
    static void EnableDpiAwareness();
    static void RunAsSingleton();
    static bool IsSubprocess();
    static bool IsDedi();
    static void AddCommands(cef::cef_ui& cef_ui);

    static void RunWatchdog();
    static int RunSubProcess();
    static void ShowLauncherWindow();

    inline static utils::nt::library lib;
    inline static std::filesystem::path base_path;
    inline static std::filesystem::path ui_path;
};
