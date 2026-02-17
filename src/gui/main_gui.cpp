#include <windows.h>   // CoInitializeEx / CoUninitialize (must come before Qt)
#include <QApplication>
#include <QFont>

#include "main_window.h"

// -------------------------------------------------------
// GUI entry point
// -------------------------------------------------------
// This is a separate executable (swvcs-gui.exe) that shares
// all backend sources with the CLI (swvcs.exe).
// The CLI (src/main.cpp) is NOT included here.
// -------------------------------------------------------

int main(int argc, char* argv[])
{
    // SolidWorks uses COM (STA).  Initialize before QApplication so that
    // our SwConnection calls land on the correct apartment.
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    QApplication app(argc, argv);
    app.setApplicationName("swvcs");
    app.setApplicationDisplayName("SolidWorks Version Control");
    app.setOrganizationName("swvcs");

    // Use a slightly larger default font for readability
    QFont f = app.font();
    f.setPointSize(10);
    app.setFont(f);

    MainWindow window;
    window.show();

    int result = app.exec();

    CoUninitialize();
    return result;
}
