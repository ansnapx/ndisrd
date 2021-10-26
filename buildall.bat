REM build drivers

cd builds
cmd /c NT32d.bat
cmd /c NT32r.bat
cmd /c NT64d.bat
cmd /c NT64r.bat
cmd /c Vista32d.bat
cmd /c Vista32r.bat
cmd /c Vista64d.bat
cmd /c Vista64r.bat
cd..

REM make sdk
cd sdk
cmd /c make.bat
cd ..

REM clean solution
REM cmd /c clean.bat
