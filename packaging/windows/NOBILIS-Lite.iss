; Instalador de NOBILIS LITE para Windows (Inno Setup 6): PULSUS, ADEPS, LATUS y VETUS.
; Lo compila GitHub Actions:  iscc /DAppVersion=1.0.0 packaging\windows\NOBILIS-Lite.iss
; Cada plugin se puede destildar; las apps Standalone son opcionales.

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef BuildDir
  #define BuildDir "..\..\build"
#endif

[Setup]
AppId={{A8EF6BB3-A0C7-40AC-AF2F-F2BA8F669B2D}
AppName=NOBILIS LITE
AppVersion={#AppVersion}
AppVerName=NOBILIS LITE {#AppVersion}
AppPublisher=Nobilis
AppPublisherURL=https://nobilis.ar
AppSupportURL=https://nobilis.ar/cuenta/
DefaultDirName={autopf}\Nobilis\Lite
DefaultGroupName=Nobilis
DisableProgramGroupPage=yes
DisableDirPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\..\dist
OutputBaseFilename=NOBILIS-Lite-Setup-Windows
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=NOBILIS LITE (Nobilis)
CloseApplications=yes

[Languages]
Name: "es"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full"; Description: "Los cuatro plugins"
Name: "custom"; Description: "Elegir plugins"; Flags: iscustom

[Components]
Name: "pulsus"; Description: "PULSUS · pump sincronizado (VST3)"; Types: full custom
Name: "adeps";  Description: "ADEPS · gordura instantánea (VST3)"; Types: full custom
Name: "latus";  Description: "LATUS · chorus de sinte (VST3)"; Types: full custom
Name: "vetus";  Description: "VETUS · máquina lo-fi (VST3)"; Types: full custom
Name: "apps";   Description: "Aplicaciones Standalone (para usarlos sin DAW)"; Types: custom

[Files]
; VST3 en la carpeta estándar: C:\Program Files\Common Files\VST3
Source: "{#BuildDir}\PULSUS_artefacts\Release\VST3\PULSUS.vst3\*"; DestDir: "{commoncf64}\VST3\PULSUS.vst3"; Components: pulsus; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BuildDir}\PULSUS_artefacts\Release\Standalone\PULSUS.exe"; DestDir: "{app}"; Components: apps and pulsus; Flags: ignoreversion
Source: "{#BuildDir}\ADEPS_artefacts\Release\VST3\ADEPS.vst3\*"; DestDir: "{commoncf64}\VST3\ADEPS.vst3"; Components: adeps; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BuildDir}\ADEPS_artefacts\Release\Standalone\ADEPS.exe"; DestDir: "{app}"; Components: apps and adeps; Flags: ignoreversion
Source: "{#BuildDir}\LATUS_artefacts\Release\VST3\LATUS.vst3\*"; DestDir: "{commoncf64}\VST3\LATUS.vst3"; Components: latus; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BuildDir}\LATUS_artefacts\Release\Standalone\LATUS.exe"; DestDir: "{app}"; Components: apps and latus; Flags: ignoreversion
Source: "{#BuildDir}\VETUS_artefacts\Release\VST3\VETUS.vst3\*"; DestDir: "{commoncf64}\VST3\VETUS.vst3"; Components: vetus; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BuildDir}\VETUS_artefacts\Release\Standalone\VETUS.exe"; DestDir: "{app}"; Components: apps and vetus; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Nobilis\PULSUS"; Filename: "{app}\PULSUS.exe"; Components: apps and pulsus
Name: "{autoprograms}\Nobilis\ADEPS";  Filename: "{app}\ADEPS.exe";  Components: apps and adeps
Name: "{autoprograms}\Nobilis\LATUS";  Filename: "{app}\LATUS.exe";  Components: apps and latus
Name: "{autoprograms}\Nobilis\VETUS";  Filename: "{app}\VETUS.exe";  Components: apps and vetus

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\PULSUS.vst3"
Type: filesandordirs; Name: "{commoncf64}\VST3\ADEPS.vst3"
Type: filesandordirs; Name: "{commoncf64}\VST3\LATUS.vst3"
Type: filesandordirs; Name: "{commoncf64}\VST3\VETUS.vst3"

[Messages]
es.FinishedLabel=NOBILIS LITE quedó instalado.%n%nAbrí tu DAW, volvé a escanear los plugins si hace falta y buscá PULSUS, ADEPS, LATUS y VETUS (fabricante: Nobilis). La primera vez te pide iniciar sesión con tu cuenta gratis de Nobilis, y con eso se activan los cuatro.
