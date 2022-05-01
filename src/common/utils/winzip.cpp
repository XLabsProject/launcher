#include "winzip.hpp"

#include <ShlDisp.h>

namespace utils::zip
{
	bool unzip(const std::filesystem::path& file, const std::filesystem::path& into)
	{
		HRESULT h_result;
		IShellDispatch* p_shelldispatch;

		auto _ = CoInitialize(NULL);

		h_result = CoCreateInstance
		(CLSID_Shell, NULL, CLSCTX_INPROC_SERVER, IID_IShellDispatch, (void**)&p_shelldispatch);

		if (SUCCEEDED(h_result))
		{
			VARIANT variant_dir, variant_file, variant_opt;

			VariantInit(&variant_dir);
			variant_dir.vt = VT_BSTR;
			variant_dir.bstrVal = SysAllocString(into.wstring().c_str());
			// Destination is our zip file
			Folder* p_to_folder = NULL;
			h_result = p_shelldispatch->NameSpace(variant_dir, &p_to_folder);

			if (SUCCEEDED(h_result))
			{
				Folder* p_folder_origin = NULL;
				VariantInit(&variant_file);
				variant_file.vt = VT_BSTR;
				variant_file.bstrVal = SysAllocString(file.wstring().c_str());

				p_shelldispatch->NameSpace(variant_file, &p_folder_origin);
				FolderItems* fi = NULL;
				p_folder_origin->Items(&fi);

				VariantInit(&variant_opt);
				variant_opt.vt = VT_I4;

#if DEBUG
				variant_opt.lVal = FOF_NOCONFIRMMKDIR | FOF_NOCONFIRMATION | FOF_SIMPLEPROGRESS;
#else
				variant_opt.lVal = FOF_NO_UI; // Windows vista+ only, but this is okay
#endif
				// Creating a new Variant with pointer to FolderItems to be copied
				VARIANT variant_to;
				VariantInit(&variant_to);
				variant_to.vt = VT_DISPATCH;
				variant_to.pdispVal = fi;

				h_result = p_to_folder->CopyHere(variant_to, variant_opt);

				p_folder_origin->Release();
				p_to_folder->Release();


				if (SUCCEEDED(h_result)) 
				{
					return true;
				}
				else
				{
					MessageBoxW(nullptr, std::format(L"Could not unzip file {} into {}!", file.wstring(), into.wstring()).data(), nullptr, MB_ICONERROR);

					return false;
				}

			}
		}

		return false;
	}
}
