#include "std_include.hpp"

#include "cef/cef_ui.hpp"
#include "cef/cef_ui_handler.hpp"

#include "utils/string.hpp"

namespace cef
{
	namespace
	{
		using window_enumerator = std::function<bool(HWND)>;

		BOOL child_window_enumerator(const HWND window, const LPARAM data)
		{
			return (*reinterpret_cast<const window_enumerator*>(data))(window) ? TRUE : FALSE;
		}

		void enum_child_windows(const HWND window, const window_enumerator& callback)
		{
			EnumChildWindows(window, &child_window_enumerator, reinterpret_cast<LPARAM>(&callback));
		}
	}

	cef_ui_handler::cef_ui_handler()
	{
		this->draggable_region_ = CreateRectRgn(0, 0, 0, 0);
	}

	cef_ui_handler::~cef_ui_handler()
	{
		DeleteObject(this->draggable_region_);
	}

	void cef_ui_handler::OnAfterCreated(CefRefPtr<CefBrowser> browser)
	{
		CEF_REQUIRE_UI_THREAD();
		this->browser_list.push_back(browser);
		this->setup_event_handler(browser->GetHost()->GetWindowHandle(), true);
	}

	void cef_ui_handler::OnBeforeClose(CefRefPtr<CefBrowser> browser)
	{
		CEF_REQUIRE_UI_THREAD();

		for (auto bit = this->browser_list.begin(); bit != this->browser_list.end(); ++bit)
		{
			if ((*bit)->IsSame(browser))
			{
				this->browser_list.erase(bit);
				break;
			}
		}

		if (this->browser_list.empty())
		{
			CefQuitMessageLoop();
		}
	}

	bool cef_ui_handler::DoClose(CefRefPtr<CefBrowser> browser)
	{
		SetParent(browser->GetHost()->GetWindowHandle(), nullptr);
		return false;
	}

	void cef_ui_handler::OnBeforeContextMenu(CefRefPtr<CefBrowser> /*browser*/, CefRefPtr<CefFrame> /*frame*/,
	                                         CefRefPtr<CefContextMenuParams> /*params*/, CefRefPtr<CefMenuModel> model)
	{
		model->Clear();
	}

	bool cef_ui_handler::OnContextMenuCommand(CefRefPtr<CefBrowser> /*browser*/, CefRefPtr<CefFrame> /*frame*/,
	                                          CefRefPtr<CefContextMenuParams> /*params*/, int /*command_id*/,
	                                          CefContextMenuHandler::EventFlags /*event_flags*/)
	{
		return false;
	}

	void cef_ui_handler::OnContextMenuDismissed(CefRefPtr<CefBrowser> /*browser*/, CefRefPtr<CefFrame> /*frame*/)
	{
	}

	bool cef_ui_handler::RunContextMenu(CefRefPtr<CefBrowser> /*browser*/, CefRefPtr<CefFrame> /*frame*/,
	                                    CefRefPtr<CefContextMenuParams> /*params*/, CefRefPtr<CefMenuModel> /*model*/,
	                                    CefRefPtr<CefRunContextMenuCallback> /*callback*/)
	{
		return false;
	}

	void cef_ui_handler::OnDraggableRegionsChanged(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
	                                               const std::vector<CefDraggableRegion>& regions)
	{
		SetRectRgn(this->draggable_region_, 0, 0, 0, 0);

		for (auto& region : regions)
		{
			RECT rect{
				region.bounds.x, region.bounds.y,
				region.bounds.x + region.bounds.width,
				region.bounds.y + region.bounds.height
			};

			const auto dpi_scale = cef_ui::get_dpi_scale();
			rect.left = static_cast<LONG>(rect.left * dpi_scale);
			rect.top = static_cast<LONG>(rect.top * dpi_scale);
			rect.right = static_cast<LONG>(rect.right * dpi_scale);
			rect.bottom = static_cast<LONG>(rect.bottom * dpi_scale);

			const auto sub_region = CreateRectRgn(rect.left, rect.top, rect.right, rect.bottom);

			CombineRgn(this->draggable_region_, this->draggable_region_, sub_region,
			           region.draggable ? RGN_OR : RGN_DIFF);
			DeleteObject(sub_region);
		}

		this->setup_event_handler(browser->GetHost()->GetWindowHandle(), true);
	}

	bool cef_ui_handler::is_closed(CefRefPtr<CefBrowser> browser)
	{
		for (const auto& browser_entry : this->browser_list)
		{
			if (browser_entry == browser)
			{
				return false;
			}
		}

		return true;
	}

	void cef_ui_handler::setup_event_handler(const HWND window, const bool setup_children, HWND root_window)
	{
		root_window = root_window ? root_window : window;

		const auto target_handler = reinterpret_cast<LONG_PTR>(&cef_ui_handler::static_event_handler);
		const auto proc_handler = GetWindowLongPtrW(window, GWLP_WNDPROC);

		if (proc_handler != target_handler)
		{
			SetPropA(window, "xlabs_root_window", root_window);
			SetPropA(window, "xlabs_ui_handler", this);
			SetPropA(window, "xlabs_proc_handler", reinterpret_cast<HANDLE>(proc_handler));

			SetWindowLongPtrW(window, GWLP_WNDPROC, target_handler);
		}

		if (setup_children)
		{
			enum_child_windows(window, [this, root_window](const HWND child)
			{
				this->setup_event_handler(child, false, root_window);
				return true;
			});
		}
	}

	LRESULT cef_ui_handler::event_handler(const HWND window, const UINT message, const WPARAM w_param,
	                                      const LPARAM l_param) const
	{
		const auto root_window = GetPropA(window, "xlabs_root_window");
		const auto handler = GetPropA(window, "xlabs_proc_handler");
		if (!handler)
		{
			return DefWindowProcW(window, message, w_param, l_param);
		}

		if (message == WM_LBUTTONDOWN)
		{
			const POINTS points = MAKEPOINTS(l_param);
			const POINT point = {points.x, points.y};
			if (PtInRegion(this->draggable_region_, point.x, point.y))
			{
				ReleaseCapture();
				SendMessageA(static_cast<HWND>(root_window), WM_NCLBUTTONDOWN, HTCAPTION, 0);
				return 1;
			}
		}

		const auto handler_func = static_cast<decltype(DefWindowProcW)*>(handler);
		return handler_func(window, message, w_param, l_param);
	}

	LRESULT CALLBACK cef_ui_handler::static_event_handler(const HWND window, const UINT message, const WPARAM w_param,
	                                                      const LPARAM l_param)
	{
		const auto handler = GetPropA(window, "xlabs_ui_handler");
		if (!handler)
		{
			return DefWindowProcW(window, message, w_param, l_param);
		}

		return static_cast<cef_ui_handler*>(handler)->event_handler(window, message, w_param, l_param);
	}
}
