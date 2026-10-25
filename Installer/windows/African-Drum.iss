; African Drum - Windows installer (Inno Setup 6.3+).
;   iscc Installer\windows\African-Drum.iss /DAppVersion=1.0.0 /DBuildDir=C:\path\to\build\DrumForge_artefacts\Release
#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef BuildDir
  #define BuildDir "..\..\build\DrumForge_artefacts\Release"
#endif
#ifndef OutDir
  #define OutDir "..\output"
#endif

[Setup]
AppId={{9A3C71D4-52B8-4F6E-8D10-A6F2C0A90002}
AppName=African Drum
AppVersion={#AppVersion}
AppVerName=African Drum {#AppVersion}
AppPublisher=African Gospel
DefaultDirName={autopf}\African Gospel\African Drum
DefaultGroupName=African Gospel
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
PrivilegesRequired=admin
LicenseFile=..\license.txt
OutputDir={#OutDir}
OutputBaseFilename=African-Drum-Setup-{#AppVersion}-Windows
SetupIconFile=African.ico
WizardImageFile=wizard-large.bmp
WizardSmallImageFile=wizard-small.bmp
WizardStyle=modern
Compression=lzma2/ultra64
SolidCompression=yes
UninstallDisplayIcon={app}\African Drum.exe
UninstallDisplayName=African Drum {#AppVersion}
CloseApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "portuguese"; MessagesFile: "compiler:Languages\Portuguese.isl"
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"

[Components]
Name: "vst3"; Description: "VST3 plugin (Cubase, Reaper, FL Studio, Ableton Live, Studio One, Bitwig...)"; Types: full custom; Flags: fixed
Name: "standalone"; Description: "Standalone application (use without a DAW)"; Types: full

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; Components: standalone; Flags: unchecked

[Files]
; VST3 is a bundle folder: African Drum.vst3\Contents\x86_64-win\African Drum.vst3
Source: "{#BuildDir}\VST3\African Drum.vst3\*"; DestDir: "{commoncf64}\VST3\African Drum.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion; Components: vst3
Source: "{#BuildDir}\Standalone\African Drum.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: standalone
Source: "..\license.txt"; DestDir: "{app}"; Flags: ignoreversion

[Dirs]
; where the user drops extra kits and MIDI files (never removed by the uninstaller)
Name: "{userdocs}\African Gospel\African Drum\Kits"; Flags: uninsneveruninstall
Name: "{userdocs}\African Gospel\African Drum\MIDI Import"; Flags: uninsneveruninstall

[Icons]
Name: "{group}\African Drum"; Filename: "{app}\African Drum.exe"; Components: standalone
Name: "{group}\Pasta de kits (os meus samples)"; Filename: "{userdocs}\African Gospel\African Drum\Kits"
Name: "{group}\Uninstall African Drum"; Filename: "{uninstallexe}"
Name: "{autodesktop}\African Drum"; Filename: "{app}\African Drum.exe"; Tasks: desktopicon; Components: standalone

[Run]
Filename: "{app}\African Drum.exe"; Description: "Launch African Drum (Standalone)"; Flags: nowait postinstall skipifsilent unchecked; Components: standalone
