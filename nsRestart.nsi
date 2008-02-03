Name "nsRestartExplorer"
OutFile "nsRestartExplorer.exe"
ShowInstDetails show

Section "Restart"
    nsRestartExplorer::nsRestartExplorer INFINITE
    ;nsRestartExplorer::nsRestartExplorer "IGNORE"
    ;nsRestartExplorer::nsQuitExplorer INFINITE
    ;nsRestartExplorer::nsStartExplorer INFINITE
    Pop $1
    DetailPrint $1
SectionEnd
