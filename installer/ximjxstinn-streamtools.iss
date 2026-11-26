; xImJxstinn Vertical – Windows-Installer (Inno Setup)
#define MyAppName "xImJxstinn Stream Tools"
#ifndef MyAppVersion
  #define MyAppVersion "3.0.0"
#endif

[Setup]
AppId={{6C1E4B0A-9D8E-4F3B-A1C2-58D7E0F3B9A4}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher=xImJxstinn
AppPublisherURL=https://www.twitch.tv/ximjxstinn
DefaultDirName={commonappdata}\obs-studio\plugins\ximjxstinn-streamtools
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputBaseFilename=ximjxstinn-streamtools-{#MyAppVersion}-windows-installer
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
Type: filesandordirs; Name: "{commonappdata}\obs-studio\plugins\ximjxstinn-vertical"

[Files]
Source: "..\release\RelWithDebInfo\ximjxstinn-streamtools\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Messages]
german.WelcomeLabel2=Dieses Programm installiert [name/ver] für OBS Studio: Viewer Counter und Multichat (Twitch + TikTok) direkt in OBS.%n%nBitte schließe OBS vor der Installation. Das alte Plugin xImJxstinn Vertical bzw. Aitum Vertical wird dabei entfernt.
