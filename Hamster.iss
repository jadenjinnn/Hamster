; Inno Setup script for the Hamster editor (standalone Windows installer).
;
; Build with:  Scripts\package.ps1 -MakeInstaller   (stages dist\staging first,
; then runs this script). Or directly: iscc Hamster.iss  -- but only after the
; staging tree exists, since [Files] pulls from dist\staging.
;
; Output: dist\HamsterSetup.exe
;
; ASCII-only by intent (ISCC reads BOM-less .iss as ANSI).

#define AppName "Hamster"
#define AppVersion "0.1.0"          ; bump as releases are cut
#define AppPublisher "Jaden"
#define AppExeRel "Hamster-Wheel\Hamster-Wheel.exe"
#define StagingDir "dist\staging"

[Setup]
; AppId uniquely identifies the app for upgrade/uninstall tracking -- keep it stable.
AppId={{022557AD-3513-46FA-8A5F-7FE5F7BD32DD}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
; Let the user choose per-machine (admin) or per-user (no admin); {autopf} and
; {group} resolve accordingly. This is the no-admin-install path.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
; 64-bit only -- the exe and the cp311 .pyd are x64.
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=dist
OutputBaseFilename=HamsterSetup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#AppName}
UninstallDisplayIcon={app}\{#AppExeRel}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; The entire staged tree -- exe + bundled runtime under Hamster-Wheel\, and the
; share\Resources tree as a sibling, exactly as the exe resolves them at runtime.
Source: "{#StagingDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeRel}"; WorkingDir: "{app}\Hamster-Wheel"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeRel}"; WorkingDir: "{app}\Hamster-Wheel"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeRel}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent
