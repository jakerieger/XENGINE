@echo off
setlocal EnableDelayedExpansion

Rem Generates C header files for embedded editor resources located in this directory (fonts/icons).

set ROOT=%~dp0..\..\..\..
set BIN2CC=%ROOT%\build\Release\bin\Tools\Bin2CC\Bin2CC.exe

set ICONS_INPUT=%ROOT%\Source\Tools\XED\Resource\icons
set INTER_INPUT=%ROOT%\Source\Tools\XED\Resource\fonts\InterRegular.ttf
set INTER_BOLD_INPUT=%ROOT%\Source\Tools\XED\Resource\fonts\InterBold.ttf

set ICONS_OUTPUT=%ROOT%\Source\Tools\XED\Source\Resource\EditorIcons.h
set INTER_OUTPUT=%ROOT%\Source\Tools\XED\Source\Resource\InterRegular.h
set INTER_BOLD_OUTPUT=%ROOT%\Source\Tools\XED\Source\Resource\InterBold.h

Rem == Generate icons
set "ICON_FILE_LIST="
for %%i in ("%ICONS_INPUT%\*") do (
    Rem %%i for full file path, %%~nxi for just the filename.ext
    echo Embedding %%~nxi
    set "ICON_FILE_LIST=!ICON_FILE_LIST! "%%~fi""
)
Rem Strip the leading space
set "ICON_FILE_LIST=!ICON_FILE_LIST:~1!"
%BIN2CC% --raw -o %ICONS_OUTPUT% %ICON_FILE_LIST%

Rem == Generate fonts
%BIN2CC% -o %INTER_OUTPUT% %INTER_INPUT%
%BIN2CC% -o %INTER_BOLD_OUTPUT% %INTER_BOLD_INPUT%

echo Done.