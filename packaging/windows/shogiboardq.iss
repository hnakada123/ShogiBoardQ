; ShogiBoardQ の Windows インストーラの定義（Inno Setup 6）
;
; scripts\build-windows-installer.ps1 から、配布用ディレクトリの中身をそのまま包むために呼ぶ。
; 手順: docs/dev/windows-build-and-release.md
;
; 既定は利用者ごとのインストール（管理者権限が要らず、%LOCALAPPDATA%\Programs に入れる）。
; 全ユーザー向けは、最初のダイアログで選べる（Program Files に入れる）。
; 設定はインストール先の外（%LOCALAPPDATA%\ShogiBoardQ）にあり、アンインストールしても残す。

#ifndef AppVersion
  #error AppVersion is required (/DAppVersion=...)
#endif
#ifndef DeployDir
  #error DeployDir is required (/DDeployDir=...)
#endif
#ifndef OutputDir
  #error OutputDir is required (/DOutputDir=...)
#endif

[Setup]
; AppId は版をまたいで変えない（同じ ID なら上書きで更新される）
AppId={{8979387E-95D5-4E5F-BAD0-02C0CC5F84E6}
AppName=ShogiBoardQ
AppVersion={#AppVersion}
AppPublisher=hnakada123
AppPublisherURL=https://github.com/hnakada123/ShogiBoardQ
AppSupportURL=https://github.com/hnakada123/ShogiBoardQ/issues
AppUpdatesURL=https://github.com/hnakada123/ShogiBoardQ/releases/latest
VersionInfoVersion={#AppVersion}
DefaultDirName={autopf}\ShogiBoardQ
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
LicenseFile={#DeployDir}\LICENSE
SetupIconFile={#SourcePath}..\..\resources\icons\shogiboardq.ico
UninstallDisplayIcon={app}\ShogiBoardQ.exe
WizardStyle=modern
Compression=lzma2/max
SolidCompression=yes
RestartApplications=no
OutputDir={#OutputDir}
OutputBaseFilename=ShogiBoardQ-windows-setup

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "japanese"; MessagesFile: "compiler:Languages\Japanese.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#DeployDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\ShogiBoardQ"; Filename: "{app}\ShogiBoardQ.exe"
Name: "{autodesktop}\ShogiBoardQ"; Filename: "{app}\ShogiBoardQ.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\ShogiBoardQ.exe"; Description: "{cm:LaunchProgram,ShogiBoardQ}"; Flags: nowait postinstall skipifsilent
