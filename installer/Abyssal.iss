; Inno Setup script for Abyssal. Build with:  cmake --build build --config Release --target installer
; (or: ISCC.exe /DAppVersion=1.0.0 installer\Abyssal.iss after a Release build)

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#define BuildDir "..\build\Abyssal_artefacts\Release"

[Setup]
AppId={{6F3C2E8A-4B1D-4C7E-9A52-A8B55A1A0001}
AppName=Abyssal
AppVersion={#AppVersion}
AppVerName=Abyssal {#AppVersion}
AppPublisher=NP Audio
DefaultDirName={autopf}\Abyssal
DefaultGroupName=Abyssal
DisableProgramGroupPage=yes
; Admin is needed to write the VST3 into C:\Program Files\Common Files\VST3
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\build\installer
OutputBaseFilename=AbyssalSetup-{#AppVersion}
SetupIconFile=..\Resources\icon.ico
UninstallDisplayIcon={app}\Abyssal.exe
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern

[Types]
Name: "full";   Description: "Standalone app + VST3 plugin"
Name: "custom"; Description: "Custom"; Flags: iscustom

[Components]
Name: "standalone"; Description: "Standalone application (Abyssal.exe)";        Types: full custom
Name: "vst3";       Description: "VST3 plugin (FL Studio, Reaper and other DAWs)"; Types: full custom

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; Components: standalone

[Files]
Source: "{#BuildDir}\Standalone\Abyssal.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "{#BuildDir}\VST3\Abyssal.vst3\*"; DestDir: "{commoncf64}\VST3\Abyssal.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Abyssal"; Filename: "{app}\Abyssal.exe"; Components: standalone
Name: "{group}\Uninstall Abyssal"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Abyssal"; Filename: "{app}\Abyssal.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Abyssal.exe"; Description: "Launch Abyssal"; Components: standalone; Flags: nowait postinstall skipifsilent
