#include "std_include.hpp"

#include "cef/cef_ui.hpp"
#include "cef/cef_ui_app.hpp"
#include "cef/cef_ui_handler.hpp"
#include "cef/cef_ui_scheme_handler.hpp"

#include <utils/nt.hpp>
#include <utils/string.hpp>

#include "../updater/updater.hpp"

#define CEF_PATH "data/cef/" CONFIG_NAME

namespace cef
{
	namespace
	{
		void delay_load_cef(const std::string& path)
		{
			static std::atomic<bool> initialized{false};
			auto uninitialized = false;
			if (!initialized.compare_exchange_strong(uninitialized, true))
			{
				return;
			}

			const auto old_directory = utils::nt::library::get_dll_directory();
			utils::nt::library::set_dll_directory(path);
			auto _ = gsl::finally([&]()
			{
				utils::nt::library::set_dll_directory(old_directory);
			});

			if (!utils::nt::library::load("libcef.dll"s) //
				|| !utils::nt::library::delay_load("libcef.dll"))
			{
				throw std::runtime_error("Failed to load CEF");
			}
		}
	}

	void cef_ui::work_once()
	{
		CefDoMessageLoopWork();
	}

	void cef_ui::work()
	{
		CefRunMessageLoop();
	}

	void cef_ui::add_command(std::string command, command_handler handler)
	{
		this->command_handlers_[std::move(command)] = std::move(handler);
	}

	int cef_ui::run_process() const
	{
		const CefMainArgs args(this->process_.get_handle());
		return CefExecuteProcess(args, nullptr, nullptr);
	}

	void cef_ui::create(const std::string& folder, const std::string& file)
	{
		if (this->browser_) return;

		CefMainArgs args(this->process_.get_handle());

		CefSettings settings;
		settings.no_sandbox = TRUE;
		//settings.single_process = TRUE;
		//settings.windowless_rendering_enabled = TRUE;
		//settings.pack_loading_disabled = FALSE;

		if (!updater::is_main_channel())
		{
			settings.remote_debugging_port = 12345;
		}

#ifdef DEBUG
		settings.log_severity = LOGSEVERITY_VERBOSE;
#else
		settings.log_severity = LOGSEVERITY_DISABLE;
#endif

		CefString(&settings.browser_subprocess_path) = this->process_.get_path();
		CefString(&settings.locales_dir_path) = this->path_ + (CEF_PATH "/locales");
		CefString(&settings.resources_dir_path) = this->path_ + CEF_PATH;
		CefString(&settings.log_file) = this->path_ + "user/cef-data/debug.log";
		CefString(&settings.user_data_path) = this->path_ + "user/cef-data/user";
		CefString(&settings.cache_path) = this->path_ + "user/cef-data/cache";
		CefString(&settings.locale) = "en-US";

		this->initialized_ = CefInitialize(args, settings, new cef_ui_app(), nullptr);
		CefRegisterSchemeHandlerFactory("http", "xlabs",
		                                new cef_ui_scheme_handler_factory(folder, this->command_handlers_));

		CefBrowserSettings browser_settings;
		//browser_settings.windowless_frame_rate = 60;

		CefWindowInfo window_info;
		window_info.SetAsPopup(nullptr, "X Labs"s + (updater::is_main_channel() ? "" : " (DEV-BUILD)"));
		window_info.width = 900; //GetSystemMetrics(SM_CXVIRTUALSCREEN);
		window_info.height = 510; //GetSystemMetrics(SM_CYVIRTUALSCREEN);
		window_info.x = (GetSystemMetrics(SM_CXSCREEN) - window_info.width) / 2;
		window_info.y = (GetSystemMetrics(SM_CYSCREEN) - window_info.height) / 2;
		window_info.style = WS_POPUP | WS_THICKFRAME | WS_CAPTION;

		if (!this->ui_handler_)
		{
			this->ui_handler_ = new cef_ui_handler();
		}

		const auto url = "http://xlabs/" + file;
		this->browser_ = CefBrowserHost::CreateBrowserSync(window_info, this->ui_handler_, url, browser_settings,
		                                                   nullptr, nullptr);
	}

	HWND cef_ui::get_window() const
	{
		if (!this->browser_) return nullptr;
		return this->browser_->GetHost()->GetWindowHandle();
	}

	void cef_ui::invoke_close_browser(CefRefPtr<CefBrowser> browser)
	{
		if (!browser) return;
		browser->GetHost()->CloseBrowser(true);
	}

	void cef_ui::close_browser()
	{
		if (!this->browser_) return;
		CefPostTask(TID_UI, base::Bind(&cef_ui::invoke_close_browser, this->browser_));
		this->browser_ = nullptr;
	}

	void cef_ui::reload_browser() const
	{
		if (!this->browser_) return;
		this->browser_->Reload();
	}

	cef_ui::cef_ui(utils::nt::library process, std::string path)
		: process_(std::move(process)), path_(std::move(path))
	{
		delay_load_cef(this->path_ + CEF_PATH);
		CefEnableHighDPISupport();
	}

	cef_ui::~cef_ui()
	{
		if (this->browser_ //
			&& this->ui_handler_ //
			&& !this->ui_handler_->is_closed(this->browser_))
		{
			this->close_browser();
			this->work();
		}

		this->browser_ = {};
		this->ui_handler_ = {};

		if (this->initialized_)
		{
			CefShutdown();
		}
	}
}
