#pragma once

#include "cef/cef_ui.hpp"
#include "utils/nt.hpp"

class Launcher
{
public:
    static const std::string& GetUiPath();

    static int Run(HINSTANCE instance);

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
    inline static std::string base_path;
    inline static std::string ui_path;
};
