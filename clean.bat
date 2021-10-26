del BuildLog.htm
del *.ncb
del *.user
del *.log
del *.err
del *.wrn

cd builds
rmdir /s /q  NT32Debug
rmdir /s /q  NT32Release
rmdir /s /q  NT64Debug
rmdir /s /q  NT64Release
rmdir /s /q  Vista32Debug
rmdir /s /q  Vista32Release
rmdir /s /q  Vista64Debug
rmdir /s /q  Vista64Release
cd ..

cd ansfltr
cmd /c clean.bat
cd ..

cd ansfw
cmd /c clean.bat
cd ..

cd sdk\src\fltrtest
cmd /c clean.bat
cd ..\..\..

cd sdk\src\fwtest
cmd /c clean.bat
cd ..\..\..

pause