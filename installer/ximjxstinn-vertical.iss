; xImJxstinn Vertical – Windows-Installer (Inno Setup)
#define MyAppName "xImJxstinn Vertical"
#ifndef MyAppVersion
  #define MyAppVersion "2.0.0"
#endif

[Setup]
AppId={{6C1E4B0A-9D8E-4F3B-A1C2-58D7E0F3B9A4}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher=xImJxstinn
AppPublisherURL=https://www.twitch.tv/ximjxstinn
DefaultDirName={commonappdata}\obs-studio\plugins\ximjxstinn-vertical
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputBaseFilename=ximjxstinn-vertical-{#MyAppVersion}-windows-installer
Compression=lzma
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
UninstallDisplayName={#MyAppName}

[Languages]
Name: "german"; MessagesFile: "compiler:Languages\German.isl"

[InstallDelete]
; Alte Vertical-Plugins entfernen (Aitum Vertical / xImJxstinn Vertical 1.x) – Szenen bleiben in OBS erhalten
Type: filesandordirs; Name: "{commonappdata}\obs-studio\plugins\vertical-canvas"

[Files]
Source: "..\release\RelWithDebInfo\ximjxstinn-vertical\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Messages]
german.WelcomeLabel2=Dieses Programm installiert [name/ver] für OBS Studio.%n%nBitte schließe OBS vor der Installation. Ein vorhandenes Aitum Vertical bzw. xImJxstinn Vertical 1.x wird dabei ersetzt – deine Hochformat-Szenen bleiben erhalten.
