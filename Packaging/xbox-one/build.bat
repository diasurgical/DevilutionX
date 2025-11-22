call VsDevCmd.bat

mkdir ..\..\build
cd ..\..\build

git clone --branch SDL2 https://github.com/libsdl-org/SDL.git
git -C SDL reset --hard 10135b2d7bbed6ea0cba24410ebc12887d92968d
msbuild /p:PlatformToolset=v143;TargetPlatformVersion=10.0.26100.0;TargetPlatformMinVersion=10.0.14393.0;ConfigurationType=StaticLibrary;Configuration=Release;Platform=x64 SDL\VisualC-WinRT\SDL-UWP.vcxproj

cmake -DUWP_LIB=1 -DUWP_SDL2_DIR="%CD%/SDL" -DCMAKE_BUILD_TYPE=x64-Release ..

msbuild /p:Configuration=Release;Platform=x64 /m DevilutionX.sln

powershell "Get-Content ..\uwp-project\Package.appxmanifest.template | %% {$_ -replace '__PROJECT_VERSION__',$(Select-String -Path ..\VERSION -Pattern \d+\.\d+\.\d+).Matches[0].Value} | Out-File -FilePath ..\uwp-project\Package.appxmanifest -encoding ASCII"

msbuild /p:Configuration=Release;Platform=x64;AppxBundle=Always;AppxBundlePlatforms=x64 /m ..\uwp-project\devilutionx.sln

powershell "Get-Childitem -Path uwp-project\AppxPackages, uwp-project\Release -Include Microsoft.VCLibs.x64.*.appx, devilutionX_*_x64.appx -File -Recurse | Compress-Archive -DestinationPath ..\devilutionx.zip"
