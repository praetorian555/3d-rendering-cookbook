set EXE=tools\fetch-data\FetchData.exe
set ASSETS=%CD%/assets

%EXE% https://casual-effects.com/g3d/data10/research/model/bistro/PropTextures %ASSETS%/bistro/PropTextures
%EXE% https://casual-effects.com/g3d/data10/research/model/bistro/OtherTextures %ASSETS%/bistro/OtherTextures
%EXE% https://casual-effects.com/g3d/data10/research/model/bistro/BuildingTextures %ASSETS%/bistro/BuildingTextures
%EXE% https://casual-effects.com/g3d/data10/research/model/bistro/Interior.zip %ASSETS%/bistro/Interior
%EXE% https://casual-effects.com/g3d/data10/research/model/bistro/Exterior.zip %ASSETS%/bistro/Exterior

if not exist "%ASSETS%/gltf-Sample-Assets/Models" (
    %EXE% https://github.com/KhronosGroup/glTF-Sample-Assets/archive/refs/heads/main.zip %ASSETS%/temp
    if %ERRORLEVEL% EQU 0 (
        xcopy "%ASSETS%/temp/gltf-Sample-Assets-main/Models" "%ASSETS%/gltf-Sample-Assets/Models" /E /I /H /C /Y /Q
        rmdir "%ASSETS%/temp" /S /Q
    )
)
pause
