#include "sw_connection.h"
#include "utils.h"

#include <stdexcept>
#include <cstdarg>
#include <iostream>

// -------------------------------------------------------
// COM helpers
// -------------------------------------------------------

// Convert narrow std::string to BSTR (caller must SysFreeString)
static BSTR ToBSTR(const std::string& s) {
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring ws(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, ws.data(), len);
    return SysAllocString(ws.c_str());
}

// Convert BSTR -> narrow std::string
static std::string FromBSTR(BSTR bstr) {
    if (!bstr) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, bstr, -1, nullptr, 0, nullptr, nullptr);
    std::string s(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, bstr, -1, s.data(), len, nullptr, nullptr);
    return s;
}

// -------------------------------------------------------
// Lifecycle
// -------------------------------------------------------

SwConnection::~SwConnection() {
    Disconnect();
}

SwConnectStatus SwConnection::Connect() {
    // Initialise COM (apartment-threaded is fine for a simple GUI/CLI app)
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        std::cerr << "[swvcs] CoInitializeEx failed: 0x" << std::hex << hr << "\n";
        return SwConnectStatus::ComError;
    }

    // Attach to a running SolidWorks instance.
    // GetActiveObject looks up the running object table — SW must already be open.
    CLSID clsid;
    hr = CLSIDFromProgID(L"SldWorks.Application", &clsid);
    if (FAILED(hr)) {
        std::cerr << "[swvcs] SldWorks.Application ProgID not found (is SolidWorks installed?)\n";
        return SwConnectStatus::NotRunning;
    }

    IUnknown* unk = nullptr;
    hr = GetActiveObject(clsid, nullptr, &unk);
    if (FAILED(hr)) {
        std::cerr << "[swvcs] SolidWorks is not running (GetActiveObject failed)\n";
        return SwConnectStatus::NotRunning;
    }

    hr = unk->QueryInterface(IID_IDispatch, reinterpret_cast<void**>(&sw_app_));
    unk->Release();
    if (FAILED(hr)) {
        std::cerr << "[swvcs] Failed to get IDispatch from SolidWorks app\n";
        return SwConnectStatus::ComError;
    }

    connected_ = true;
    std::cout << "[swvcs] Connected to SolidWorks.\n";
    return SwConnectStatus::OK;
}

void SwConnection::Disconnect() {
    if (sw_doc_)  { sw_doc_->Release();  sw_doc_  = nullptr; }
    if (sw_app_)  { sw_app_->Release();  sw_app_  = nullptr; }
    connected_ = false;
    CoUninitialize();
}

// -------------------------------------------------------
// IDispatch invoke helper
// -------------------------------------------------------
// This lets us call any SolidWorks COM method by name without
// needing the compiled TLB at build time.

HRESULT SwConnection::Invoke(IDispatch* disp, const wchar_t* method,
                              WORD flags, VARIANT* result,
                              int arg_count, ...) {
    if (!disp) return E_POINTER;

    // Get DISPID for the method name
    DISPID dispid = DISPID_UNKNOWN;
    LPOLESTR name = const_cast<LPOLESTR>(method);
    HRESULT hr = disp->GetIDsOfNames(IID_NULL, &name, 1,
                                     LOCALE_SYSTEM_DEFAULT, &dispid);
    if (FAILED(hr)) return hr;

    // Pack arguments (right-to-left for COM)
    DISPPARAMS params = {};
    std::vector<VARIANT> args(arg_count);

    if (arg_count > 0) {
        va_list vl;
        va_start(vl, arg_count);
        for (int i = arg_count - 1; i >= 0; --i) {
            args[i] = va_arg(vl, VARIANT);
        }
        va_end(vl);
        params.rgvarg     = args.data();
        params.cArgs      = static_cast<UINT>(arg_count);
    }

    VARIANT ret;
    VariantInit(&ret);
    EXCEPINFO excep = {};
    UINT arg_err    = 0;

    hr = disp->Invoke(dispid, IID_NULL, LOCALE_SYSTEM_DEFAULT,
                      flags, &params, &ret, &excep, &arg_err);

    if (result) *result = ret;
    else        VariantClear(&ret);

    return hr;
}

// -------------------------------------------------------
// Refresh the sw_doc_ pointer from the active document
// -------------------------------------------------------
static HRESULT RefreshDoc(IDispatch* sw_app, IDispatch*& sw_doc) {
    if (sw_doc) { sw_doc->Release(); sw_doc = nullptr; }

    VARIANT result;
    VariantInit(&result);

    // Call IModelDoc2 swApp.ActiveDoc
    DISPID dispid;
    LPOLESTR name = L"ActiveDoc";
    HRESULT hr = sw_app->GetIDsOfNames(IID_NULL, &name, 1,
                                       LOCALE_SYSTEM_DEFAULT, &dispid);
    if (FAILED(hr)) return hr;

    DISPPARAMS params = {};
    EXCEPINFO excep   = {};
    UINT arg_err      = 0;
    hr = sw_app->Invoke(dispid, IID_NULL, LOCALE_SYSTEM_DEFAULT,
                        DISPATCH_PROPERTYGET, &params, &result, &excep, &arg_err);
    if (FAILED(hr)) return hr;

    if (result.vt != VT_DISPATCH || !result.pdispVal)
        return E_NOINTERFACE;

    sw_doc = result.pdispVal;
    sw_doc->AddRef();
    VariantClear(&result);
    return S_OK;
}

// -------------------------------------------------------
// GetActiveDocInfo
// -------------------------------------------------------
Result SwConnection::GetActiveDocInfo(ActiveDocInfo& out) {
    if (!connected_) return Result::failure("Not connected to SolidWorks");

    HRESULT hr = RefreshDoc(sw_app_, sw_doc_);
    if (FAILED(hr) || !sw_doc_)
        return Result::failure("No active document in SolidWorks");

    // --- path ---
    VARIANT vPath; VariantInit(&vPath);
    if (SUCCEEDED(Invoke(sw_doc_, L"GetPathName", DISPATCH_METHOD, &vPath)))
        out.path = FromBSTR(vPath.bstrVal);
    VariantClear(&vPath);

    // --- title ---
    VARIANT vTitle; VariantInit(&vTitle);
    if (SUCCEEDED(Invoke(sw_doc_, L"GetTitle", DISPATCH_METHOD, &vTitle)))
        out.title = FromBSTR(vTitle.bstrVal);
    VariantClear(&vTitle);

    // --- type (GetType returns swDocPART=1, swDocASSEMBLY=2, swDocDRAWING=3) ---
    VARIANT vType; VariantInit(&vType);
    if (SUCCEEDED(Invoke(sw_doc_, L"GetType", DISPATCH_METHOD, &vType))) {
        switch (vType.lVal) {
            case 1:  out.type = "Part";     break;
            case 2:  out.type = "Assembly"; break;
            case 3:  out.type = "Drawing";  break;
            default: out.type = "Unknown";  break;
        }
    }
    VariantClear(&vType);

    // --- dirty flag ---
    VARIANT vDirty; VariantInit(&vDirty);
    if (SUCCEEDED(Invoke(sw_doc_, L"GetSaveFlag", DISPATCH_METHOD, &vDirty)))
        out.is_dirty = (vDirty.boolVal != VARIANT_FALSE);
    VariantClear(&vDirty);

    return Result::success();
}

// -------------------------------------------------------
// SaveActiveDoc
// -------------------------------------------------------
Result SwConnection::SaveActiveDoc() {
    if (!connected_ || !sw_doc_)
        return Result::failure("No active document");

    VARIANT vResult; VariantInit(&vResult);

    // Save(swSaveAsCurrentVersion=0, pSaveAsPath=nullptr, pbHadError)
    // Simplest call: just call Save on the doc
    HRESULT hr = Invoke(sw_doc_, L"Save", DISPATCH_METHOD, &vResult);
    VariantClear(&vResult);

    if (FAILED(hr))
        return Result::failure("Save() COM call failed");

    return Result::success();
}

// -------------------------------------------------------
// CloseActiveDoc
// -------------------------------------------------------
Result SwConnection::CloseActiveDoc(bool force_close) {
    if (!connected_) return Result::failure("Not connected");

    ActiveDocInfo info;
    if (!GetActiveDocInfo(info).ok)
        return Result::failure("No active document to close");

    // CloseDoc(string path)
    VARIANT vPath;
    VariantInit(&vPath);
    vPath.vt      = VT_BSTR;
    vPath.bstrVal = ToBSTR(info.path);

    HRESULT hr = Invoke(sw_app_, L"CloseDoc", DISPATCH_METHOD, nullptr, 1, vPath);
    SysFreeString(vPath.bstrVal);

    if (FAILED(hr))
        return Result::failure("CloseDoc() failed");

    if (sw_doc_) { sw_doc_->Release(); sw_doc_ = nullptr; }
    return Result::success();
}

// -------------------------------------------------------
// OpenDoc
// -------------------------------------------------------
Result SwConnection::OpenDoc(const std::string& file_path) {
    if (!connected_) return Result::failure("Not connected");

    // Determine document type from extension
    int doc_type = 1; // default: Part
    if (file_path.find(".SLDASM") != std::string::npos ||
        file_path.find(".sldasm") != std::string::npos)
        doc_type = 2;
    else if (file_path.find(".SLDDRW") != std::string::npos ||
             file_path.find(".slddrw") != std::string::npos)
        doc_type = 3;

    VARIANT vPath, vType, vOptions, vErrors, vWarnings;
    VariantInit(&vPath);    VariantInit(&vType);
    VariantInit(&vOptions); VariantInit(&vErrors); VariantInit(&vWarnings);

    vPath.vt      = VT_BSTR;
    vPath.bstrVal = ToBSTR(file_path);
    vType.vt      = VT_I4; vType.lVal    = doc_type;
    vOptions.vt   = VT_I4; vOptions.lVal = 1; // swOpenDocOptions_Silent

    // OpenDoc6(path, type, options, configName, errors, warnings)
    VARIANT vResult; VariantInit(&vResult);
    // Simplified: use OpenDoc (older but simpler signature)
    HRESULT hr = Invoke(sw_app_, L"OpenDoc", DISPATCH_METHOD, &vResult, 2, vPath, vType);
    SysFreeString(vPath.bstrVal);
    VariantClear(&vResult);

    if (FAILED(hr))
        return Result::failure("OpenDoc() failed for: " + file_path);

    return Result::success();
}

// -------------------------------------------------------
// GetMassProperties  (best-effort)
// -------------------------------------------------------
Result SwConnection::GetMassProperties(double& mass_kg, double& volume_m3, double& surface_area_m2) {
    mass_kg = volume_m3 = surface_area_m2 = 0.0;
    if (!connected_ || !sw_doc_)
        return Result::failure("No active document");

    VARIANT vExt; VariantInit(&vExt);
    HRESULT hr = Invoke(sw_doc_, L"Extension", DISPATCH_PROPERTYGET, &vExt);
    if (FAILED(hr) || vExt.vt != VT_DISPATCH)
        return Result::failure("Could not get document extension");

    IDispatch* ext = vExt.pdispVal;

    VARIANT vMassProp; VariantInit(&vMassProp);
    hr = Invoke(ext, L"CreateMassProperty", DISPATCH_METHOD, &vMassProp);
    if (SUCCEEDED(hr) && vMassProp.vt == VT_DISPATCH && vMassProp.pdispVal) {
        IDispatch* mp = vMassProp.pdispVal;

        VARIANT vm; VariantInit(&vm);
        if (SUCCEEDED(Invoke(mp, L"Mass", DISPATCH_PROPERTYGET, &vm)))
            mass_kg = vm.dblVal;
        VariantClear(&vm);

        VARIANT vv; VariantInit(&vv);
        if (SUCCEEDED(Invoke(mp, L"Volume", DISPATCH_PROPERTYGET, &vv)))
            volume_m3 = vv.dblVal;
        VariantClear(&vv);

        VARIANT vs; VariantInit(&vs);
        if (SUCCEEDED(Invoke(mp, L"SurfaceArea", DISPATCH_PROPERTYGET, &vs)))
            surface_area_m2 = vs.dblVal;
        VariantClear(&vs);

        mp->Release();
    }

    VariantClear(&vMassProp);
    ext->Release();
    return Result::success();
}

// -------------------------------------------------------
// GetMaterial  (parts only — empty string for assemblies)
// -------------------------------------------------------
Result SwConnection::GetMaterial(std::string& material) {
    material.clear();
    if (!connected_ || !sw_doc_) return Result::failure("No active document");

    // GetMaterialPropertyName(sConfigName) — IPartDoc method, fails on assemblies
    VARIANT vConfig; VariantInit(&vConfig);
    vConfig.vt      = VT_BSTR;
    vConfig.bstrVal = SysAllocString(L"");   // empty = active configuration

    VARIANT vMat; VariantInit(&vMat);
    HRESULT hr = Invoke(sw_doc_, L"GetMaterialPropertyName", DISPATCH_METHOD, &vMat, 1, vConfig);
    SysFreeString(vConfig.bstrVal);

    if (SUCCEEDED(hr) && vMat.vt == VT_BSTR)
        material = FromBSTR(vMat.bstrVal);

    VariantClear(&vMat);
    return Result::success();
}

// -------------------------------------------------------
// GetBoundingBox  (all document types)
// -------------------------------------------------------
Result SwConnection::GetBoundingBox(double& x_mm, double& y_mm, double& z_mm) {
    x_mm = y_mm = z_mm = 0.0;
    if (!connected_ || !sw_doc_) return Result::failure("No active document");

    // GetBox(Top) returns SAFEARRAY of 6 doubles: [minX,minY,minZ,maxX,maxY,maxZ] in metres
    VARIANT vTop; VariantInit(&vTop);
    vTop.vt      = VT_BOOL;
    vTop.boolVal = VARIANT_FALSE;

    VARIANT vBox; VariantInit(&vBox);
    HRESULT hr = Invoke(sw_doc_, L"GetBox", DISPATCH_METHOD, &vBox, 1, vTop);
    if (SUCCEEDED(hr) && (vBox.vt & VT_ARRAY)) {
        SAFEARRAY* sa   = (vBox.vt & VT_BYREF) ? *vBox.pparray : vBox.parray;
        double*    data = nullptr;
        if (SUCCEEDED(SafeArrayAccessData(sa, reinterpret_cast<void**>(&data)))) {
            x_mm = (data[3] - data[0]) * 1000.0;
            y_mm = (data[4] - data[1]) * 1000.0;
            z_mm = (data[5] - data[2]) * 1000.0;
            SafeArrayUnaccessData(sa);
        }
    }
    VariantClear(&vBox);
    return Result::success();
}

// -------------------------------------------------------
// GetConfigCount
// -------------------------------------------------------
Result SwConnection::GetConfigCount(int& count) {
    count = 0;
    if (!connected_ || !sw_doc_) return Result::failure("No active document");

    VARIANT vCount; VariantInit(&vCount);
    if (SUCCEEDED(Invoke(sw_doc_, L"GetConfigurationCount", DISPATCH_METHOD, &vCount))
        && vCount.vt == VT_I4)
        count = vCount.lVal;
    VariantClear(&vCount);
    return Result::success();
}

// -------------------------------------------------------
// GetFeatureCount
// -------------------------------------------------------
Result SwConnection::GetFeatureCount(int& count) {
    count = 0;
    if (!connected_ || !sw_doc_) return Result::failure("No active document");

    VARIANT vFeatMgr; VariantInit(&vFeatMgr);
    if (FAILED(Invoke(sw_doc_, L"FeatureManager", DISPATCH_PROPERTYGET, &vFeatMgr))
        || vFeatMgr.vt != VT_DISPATCH)
        return Result::failure("Could not get FeatureManager");

    VARIANT vCount; VariantInit(&vCount);
    Invoke(vFeatMgr.pdispVal, L"GetFeatureCount", DISPATCH_METHOD, &vCount, 1,
           []{ VARIANT v; VariantInit(&v); v.vt = VT_BOOL; v.boolVal = VARIANT_TRUE; return v; }());
    count = (vCount.vt == VT_I4) ? vCount.lVal : 0;

    VariantClear(&vCount);
    VariantClear(&vFeatMgr);
    return Result::success();
}

// -------------------------------------------------------
// SaveThumbnail
// -------------------------------------------------------
Result SwConnection::SaveThumbnail(const std::string& dest_path) {
    if (!connected_ || !sw_doc_) return Result::failure("No active document");

    VARIANT vPath, vW, vH;
    VariantInit(&vPath); VariantInit(&vW); VariantInit(&vH);
    vPath.vt      = VT_BSTR; vPath.bstrVal = ToBSTR(dest_path);
    vW.vt = VT_I4; vW.lVal = 256;
    vH.vt = VT_I4; vH.lVal = 256;

    HRESULT hr = Invoke(sw_doc_, L"SaveBMP", DISPATCH_METHOD, nullptr, 3, vPath, vW, vH);
    SysFreeString(vPath.bstrVal);

    return SUCCEEDED(hr) ? Result::success()
                         : Result::failure("SaveBMP() failed");
}