del BuildLog.htm
del *.ncb
del *.user
del *.log
del *.err
del *.wrn
rmdir /s /q  objchk_w2k_x86
rmdir /s /q  objfre_w2k_x86
rmdir /s /q  objchk_wxp_x86
rmdir /s /q  objfre_wxp_x86
rmdir /s /q  objchk_wlh_x86
rmdir /s /q  objfre_wlh_x86
rmdir /s /q  x64
rmdir /s /q  Debug
rmdir /s /q  Release
rmdir /s /q  "Debug NT"
rmdir /s /q  "Release NT"
rmdir /s /q  "Debug Vista"
rmdir /s /q  "Release Vista"

cd vista
cmd /c clean.bat
cd ..

cd nt
cmd /c clean.bat
cd ..