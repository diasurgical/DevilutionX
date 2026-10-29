rem Written by VTSTech (Nigel Todman) (veritas@vts-tech.org)
rem AI Assisted by Super-Z (GLM-5.3-Flash)
rem License: MIT

@echo off
rmdir /q /s .\shaders\
echo Downloading libretro slang shaders ...
git clone https://github.com/libretro/slang-shaders shaders
cd shaders
echo Downloading PNG assets ...
git lfs pull
cd ..
echo Done
pause
