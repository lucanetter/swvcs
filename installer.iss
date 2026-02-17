; swvcs Inno Setup installer script
; Requires Inno Setup 6+ — https://jrsoftware.org/isinfo.php
;
; Before compiling this script:
;   1. Build the project:  cmake --build build-mingw -j
;   2. Deploy Qt DLLs:     cd bin && C:\Qt\6.x.x\mingw_64\bin\windeployqt.exe swvcs-gui.exe
;   3. Open this file in Inno Setup and click Build > Compile (or press F9)
;
; Output: installer\swvcs-setup.exe

; -------------------------------------------------------
; Application identity
; -------------------------------------------------------
[Setup]
AppName=swvcs
AppVersion=0.1.0
AppPublisher=swvcs
AppPublisherURL=https://github.com/lucan/swvcs

; Install to the user's local app data — no administrator rights required.
DefaultDirName={localappdata}\swvcs
DefaultGroupName=swvcs
PrivilegesRequired=lowest

; 64-bit only (SolidWorks is 64-bit only)
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

; Installer output
OutputDir=installer
OutputBaseFilename=swvcs-setup
SetupIconFile=

; Compression
Compression=lzma2/ultra64
SolidCompression=yes

; Show a "Ready to install" page and a finish page that can launch the GUI
DisableProgramGroupPage=yes
UninstallDisplayName=swvcs

; -------------------------------------------------------
; Languages
; -------------------------------------------------------
[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

; -------------------------------------------------------
; Files — everything in bin\ (exe + Qt DLLs + plugin dirs)
; -------------------------------------------------------
[Files]
Source: "bin\swvcs.exe";     DestDir: "{app}"; Flags: ignoreversion
Source: "bin\swvcs-gui.exe"; DestDir: "{app}"; Flags: ignoreversion
; Qt DLLs and plugin subdirectories deployed by windeployqt
Source: "bin\*.dll";         DestDir: "{app}"; Flags: ignoreversion
Source: "bin\generic\*";         DestDir: "{app}\generic";         Flags: ignoreversion recursesubdirs; Check: DirExists(ExpandConstant('{src}\bin\generic'))
Source: "bin\iconengines\*";     DestDir: "{app}\iconengines";     Flags: ignoreversion recursesubdirs; Check: DirExists(ExpandConstant('{src}\bin\iconengines'))
Source: "bin\imageformats\*";    DestDir: "{app}\imageformats";    Flags: ignoreversion recursesubdirs; Check: DirExists(ExpandConstant('{src}\bin\imageformats'))
Source: "bin\networkinformation\*"; DestDir: "{app}\networkinformation"; Flags: ignoreversion recursesubdirs; Check: DirExists(ExpandConstant('{src}\bin\networkinformation'))
Source: "bin\platforms\*";       DestDir: "{app}\platforms";       Flags: ignoreversion recursesubdirs; Check: DirExists(ExpandConstant('{src}\bin\platforms'))
Source: "bin\styles\*";          DestDir: "{app}\styles";          Flags: ignoreversion recursesubdirs; Check: DirExists(ExpandConstant('{src}\bin\styles'))
Source: "bin\tls\*";             DestDir: "{app}\tls";             Flags: ignoreversion recursesubdirs; Check: DirExists(ExpandConstant('{src}\bin\tls'))
Source: "bin\translations\*";    DestDir: "{app}\translations";    Flags: ignoreversion recursesubdirs; Check: DirExists(ExpandConstant('{src}\bin\translations'))

; -------------------------------------------------------
; Start Menu shortcuts
; -------------------------------------------------------
[Icons]
Name: "{group}\swvcs GUI";        Filename: "{app}\swvcs-gui.exe"
Name: "{group}\Uninstall swvcs";  Filename: "{uninstallexe}"

; -------------------------------------------------------
; Add {app} to the user's PATH so 'swvcs' works in any terminal
; -------------------------------------------------------
[Registry]
Root: HKCU; Subkey: "Environment"; ValueType: expandsz; ValueName: "Path"; \
  ValueData: "{olddata};{app}"; \
  Check: NeedsAddPath(ExpandConstant('{app}'))

; -------------------------------------------------------
; Pascal script helpers
; -------------------------------------------------------
[Code]

// Returns true if Param is NOT already present in the user PATH.
function NeedsAddPath(Param: string): boolean;
var
  OrigPath: string;
begin
  if not RegQueryStringValue(HKCU, 'Environment', 'Path', OrigPath) then
  begin
    Result := True;
    exit;
  end;
  // Case-insensitive check, surrounded by semicolons to avoid partial matches
  Result := Pos(';' + Uppercase(Param) + ';',
                ';' + Uppercase(OrigPath) + ';') = 0;
end;

// On uninstall, remove {app} from the user PATH
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  Path, AppDir, NewPath: string;
  P: integer;
begin
  if CurUninstallStep <> usPostUninstall then exit;

  AppDir := ExpandConstant('{app}');
  if not RegQueryStringValue(HKCU, 'Environment', 'Path', Path) then exit;

  // Strip the entry (with surrounding semicolons, handles edge cases)
  NewPath := Path;
  P := Pos(';' + Uppercase(AppDir), Uppercase(NewPath));
  if P > 0 then
    Delete(NewPath, P, Length(';' + AppDir))
  else begin
    P := Pos(Uppercase(AppDir) + ';', Uppercase(NewPath));
    if P > 0 then
      Delete(NewPath, P, Length(AppDir + ';'));
  end;

  if NewPath <> Path then
    RegWriteStringValue(HKCU, 'Environment', 'Path', NewPath);
end;
