#include "std_include.hpp"

#include "launcher.hpp"

#include "cef/cef_ui.hpp"
#include "updater/updater.hpp"

#include <utils/com.hpp>
#include <utils/string.hpp>
#include <utils/named_mutex.hpp>
#include <utils/exit_callback.hpp>
#include <utils/properties.hpp>

#include "updater/file_updater.hpp"
#include "updater/updater_ui.hpp"

bool Launcher::TryLockTerminationBarrier()
{
    static std::atomic_bool barrier{false};

    auto expected = false;
    return barrier.compare_exchange_strong(expected, true);
}

std::string Launcher::GetAppdataPath()
{
    PWSTR path;
    if (!SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path)))
    {
        throw std::runtime_error("Failed to read APPDATA path!");
    }

    auto _ = gsl::finally([&path]()
    {
        CoTaskMemFree(path);
    });

    return utils::string::convert(path) + "/xlabs/";
}

void Launcher::SetWorkingDirectory()
{
	SetCurrentDirectoryA(base_path.string().data());
}

void Launcher::PrepareEnvironment(HINSTANCE instance)
{
	lib = utils::nt::library(instance);

#ifdef CI_BUILD
	const auto appDataPath = absolute(std::filesystem::path(GetAppdataPath()));
	base_path = appDataPath;
	ui_path = (appDataPath / "data/launcher-ui");
	download_path = appDataPath;
	properties = utils::Properties((appDataPath / "user/properties.json").string());

	SetWorkingDirectory();
#else
    const std::filesystem::path basePath = absolute(std::filesystem::path("runtime"));
	base_path = basePath;
	ui_path = absolute(basePath / std::filesystem::path("../../src/launcher-ui/"));
	download_path = basePath / "download";
	properties = utils::Properties((basePath / "user/properties.json").string());
#endif
}

void Launcher::EnableDpiAwareness()
{
    const utils::nt::library user32{"user32.dll"};
    const auto set_dpi = user32
                             ? user32.get_proc<BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT)>(
                                 "SetProcessDpiAwarenessContext")
                             : nullptr;
    if (set_dpi)
    {
        set_dpi(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    }
}

void Launcher::RunAsSingleton()
{
    static utils::named_mutex mutex{"xlabs-launcher"};
    if (!mutex.try_lock(3s))
    {
        throw std::runtime_error{"X Labs launcher is already running"};
    }
}

bool Launcher::IsSubprocess()
{
    return strstr(GetCommandLineA(), "--xlabs-subprocess");
}

bool Launcher::IsDedi()
{
    return !IsSubprocess() && (strstr(GetCommandLineA(), "-dedicated") || strstr(GetCommandLineA(), "-update"));
}

void Launcher::RunWatchdog()
{
    std::thread([]()
    {
        const auto parent = utils::nt::get_parent_pid();
        if (utils::nt::wait_for_process(parent))
        {
            std::this_thread::sleep_for(3s);
            utils::nt::terminate();
        }
    }).detach();
}

int Launcher::RunSubProcess()
{
    const cef::cef_ui cef_ui{lib, base_path};
    return cef_ui.run_process();
}

void Launcher::AddCommands(cef::cef_ui& cef_ui)
{
    cef_ui.add_command("launch-aw", [&cef_ui](const rapidjson::Value& value, auto&)
    {
        if (!value.IsString())
        {
            return;
        }

        const std::string arg{value.GetString(), value.GetStringLength()};

        static const std::unordered_map<std::string, std::string> arg_mapping = {
            {"aw-sp", "-singleplayer"},
            {"aw-mp", "-multiplayer"},
            {"aw-zm", "-zombies"},
            {"aw-survival", "-survival"},
        };

        const auto mapped_arg = arg_mapping.find(arg);
        if (mapped_arg == arg_mapping.end())
        {
            return;
        }

        const auto aw_install = properties.Load("aw-install");
        if (!aw_install)
        {
            return;
        }

        if (!TryLockTerminationBarrier())
        {
            return;
        }

        SetEnvironmentVariableA("XLABS_AW_INSTALL", aw_install->data());

        const auto s1x_exe = base_path / "data/s1x/s1x.exe";
        utils::nt::launch_process(s1x_exe.string(), mapped_arg->second);

        cef_ui.close_browser();
    });

    cef_ui.add_command("launch-ghosts", [&cef_ui](const rapidjson::Value& value, auto&)
    {
        if (!value.IsString())
        {
            return;
        }

        const std::string arg{value.GetString(), value.GetStringLength()};

        static const std::unordered_map<std::string, std::string> arg_mapping = {
            {"ghosts-sp", "-singleplayer"},
            {"ghosts-mp", "-multiplayer"},
        };

        const auto mapped_arg = arg_mapping.find(arg);
        if (mapped_arg == arg_mapping.end())
        {
            return;
        }

        const auto ghosts_install = properties.Load("ghosts-install");
        if (!ghosts_install)
        {
            return;
        }

        if (!TryLockTerminationBarrier())
        {
            return;
        }

        SetEnvironmentVariableA("XLABS_GHOSTS_INSTALL", ghosts_install->data());

        const auto s1x_exe = base_path / "data/iw6x/iw6x.exe";
        utils::nt::launch_process(s1x_exe.string(), mapped_arg->second);

        cef_ui.close_browser();
    });

    cef_ui.add_command("launch-mw2", [&cef_ui](const rapidjson::Value& value, auto&)
    {
        if (!value.IsString())
        {
            return;
        }

        const std::string arg{value.GetString(), value.GetStringLength()};

        static const std::unordered_map<std::string, std::string> arg_mapping = {
            {"mw2-mp", "-multiplayer"},
        };

        const auto mapped_arg = arg_mapping.find(arg);
        if (mapped_arg == arg_mapping.end())
        {
            return;
        }

        const auto mw2_install = properties.Load("mw2-install");
        if (!mw2_install)
        {
            return;
        }

        const std::filesystem::path mw2_install_path(mw2_install.value());

        if (!TryLockTerminationBarrier())
        {
            return;
        }

        // We update iw4x upon launch
        updater::updater_ui updater_ui{};
        const updater::file_updater file_updater{updater_ui, mw2_install.value() + "/", ""};
        file_updater.update_iw4x_if_necessary();

        const auto iw4x_exe = mw2_install_path / "iw4x.exe";
        utils::nt::launch_process(iw4x_exe.string(), mapped_arg->second);

        cef_ui.close_browser();
    });

    cef_ui.add_command("browse-folder", [](const auto&, rapidjson::Document& response)
    {
        response.SetNull();

        std::string folder{};
        if (utils::com::select_folder(folder))
        {
            response.SetString(folder, response.GetAllocator());
        }
    });

    cef_ui.add_command("close", [&cef_ui](const auto&, auto&)
    {
        cef_ui.close_browser();
    });

    cef_ui.add_command("minimize", [&cef_ui](const auto&, auto&)
    {
        ShowWindow(cef_ui.get_window(), SW_MINIMIZE);
    });

    cef_ui.add_command("show", [&cef_ui](const auto&, auto&)
    {
        auto* const window = cef_ui.get_window();
        ShowWindow(window, SW_SHOWDEFAULT);
        SetForegroundWindow(window);

        PostMessageA(window, WM_DELAYEDDPICHANGE, 0, 0);
    });

    cef_ui.add_command("get-property", [](const rapidjson::Value& value, rapidjson::Document& response)
    {
        response.SetNull();

        if (!value.IsString())
        {
            return;
        }

        const std::string key{value.GetString(), value.GetStringLength()};
        const auto property = properties.Load(key);
        if (!property)
        {
            return;
        }

        response.SetString(*property, response.GetAllocator());
    });

    cef_ui.add_command("set-property", [](const rapidjson::Value& value, auto&)
    {
        if (!value.IsObject())
        {
            return;
        }

        const auto _ = properties.Lock();

        for (auto i = value.MemberBegin(); i != value.MemberEnd(); ++i)
        {
            if (!i->value.IsString())
            {
                continue;
            }

            const std::string key{i->name.GetString(), i->name.GetStringLength()};
            const std::string val{i->value.GetString(), i->value.GetStringLength()};

            properties.Store(key, val);
        }
    });

    cef_ui.add_command("get-channel", [](auto&, rapidjson::Document& response)
    {
        const std::string channel = updater::is_main_channel() ? "main" : "dev";
        response.SetString(channel, response.GetAllocator());
    });

    cef_ui.add_command("switch-channel", [&cef_ui](const rapidjson::Value& value, auto&)
    {
        if (!value.IsString())
        {
            return;
        }

        const std::string channel{value.GetString(), value.GetStringLength()};
        const auto* const command_line = channel == "main" ? "--xlabs-channel-main" : "--xlabs-channel-develop";

        utils::at_exit([command_line]()
        {
            utils::nt::relaunch_self(command_line);
        });

        cef_ui.close_browser();
    });
}

void Launcher::ShowLauncherWindow()
{
	cef::cef_ui cef_ui{ lib, base_path.string() };
	AddCommands(cef_ui);
	cef_ui.create(ui_path.string(), "main.html");
	cef_ui.work();
}

int Launcher::Run(HINSTANCE instance)
{
    try
    {
        PrepareEnvironment(instance);

        if (IsSubprocess())
        {
            RunWatchdog();
            return RunSubProcess();
        }

        EnableDpiAwareness();

#if defined(CI_BUILD) && !defined(DEBUG)
		RunAsSingleton();

        if (!strstr(GetCommandLineA(), "-noupdate"))
        {
		    updater::run(download_path.string() + "/");
        }
#endif

        if (!IsDedi())
        {
            ShowLauncherWindow();
        }

        return 0;
    }
    catch (updater::update_cancelled&)
    {
        return 0;
    }
}
