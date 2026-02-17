#pragma once

// -------------------------------------------------------
// SwConnection
// -------------------------------------------------------
// Manages the COM link to a running SolidWorks instance.
// Call Connect() once at startup; the object stays alive
// for the duration of the program.
//
// All SolidWorks API calls go through this class so that
// COM lifetime and error handling are centralised.
// -------------------------------------------------------

#include "types.h"

#include <Windows.h>
#include <atlbase.h>   // CComPtr, CComBSTR

// Import SolidWorks type library.
// Adjust the path to match the installed SW version on the build machine.
// Common locations:
//   C:\Program Files\SOLIDWORKS Corp\SOLIDWORKS\sldworks.tlb   (2023+)
//   C:\Program Files\SolidWorks Corp\SolidWorks\sldworks.tlb   (older)
//
// If the TLB is not found at build time, comment-out the #import and use
// raw IDispatch calls instead (see sw_connection.cpp for the fallback).
#ifdef SWVCS_HAS_TLB
    #import "C:\\Program Files\\SOLIDWORKS Corp\\SOLIDWORKS\\sldworks.tlb" \
            no_namespace named_guids rename("GetObject","SWGetObject")
#endif

#include <string>

struct ActiveDocInfo {
    std::string path;        // full file path
    std::string title;       // display name
    std::string type;        // "Part" | "Assembly" | "Drawing" | "Unknown"
    bool        is_dirty;    // unsaved changes present
};

class SwConnection {
public:
    SwConnection() = default;
    ~SwConnection();

    // Attach to an already-running SolidWorks process via COM.
    // Returns SwConnectStatus::OK on success.
    SwConnectStatus Connect();

    // Release the COM reference.
    void Disconnect();

    bool IsConnected() const { return connected_; }

    // -------------------------------------------------------
    // Document helpers
    // -------------------------------------------------------

    // Get info about whichever document is currently active.
    Result GetActiveDocInfo(ActiveDocInfo& out);

    // Ask SolidWorks to save the active document to its current path.
    Result SaveActiveDoc();

    // Close the active document (prompts SW if there are unsaved changes
    // unless force_close = true, which discards them).
    Result CloseActiveDoc(bool force_close = false);

    // Open a file in SolidWorks.
    Result OpenDoc(const std::string& file_path);

    // -------------------------------------------------------
    // Metadata helpers (best-effort — returns 0 if unavailable)
    // -------------------------------------------------------
    Result GetMassProperties(double& mass_kg, double& volume_m3);
    Result GetFeatureCount(int& count);

    // Save a 256x256 BMP thumbnail of the active document.
    // dest_path should end in ".bmp"
    Result SaveThumbnail(const std::string& dest_path);

private:
    bool connected_ = false;

    // Raw IDispatch pointers used when the TLB is not available at compile time.
    // When the TLB is available these are replaced by the strongly-typed ptrs.
    IDispatch* sw_app_  = nullptr;   // SldWorks.Application
    IDispatch* sw_doc_  = nullptr;   // IModelDoc2

    // Helper: invoke a COM method on an IDispatch object by name.
    HRESULT Invoke(IDispatch* disp, const wchar_t* method,
                   WORD flags, VARIANT* result,
                   int arg_count = 0, ...);
};