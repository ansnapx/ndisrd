REM copy headers
rmdir /s /q  inc

mkdir inc

copy ..\shared\ansfltr.h .\inc\
copy ..\shared\ansfw.h .\inc\
copy ..\shared\iphlp.h .\inc\

REM copy libs
rmdir /s /q  lib

mkdir lib
mkdir lib\wxp
mkdir lib\wxp\i386
mkdir lib\wxp\amd64
mkdir lib\wlh
mkdir lib\wlh\i386
mkdir lib\wlh\amd64

copy ..\ansfw\nt\objfre_wxp_x86\i386\ansfw.lib .\lib\wxp\i386\
copy ..\ansfw\nt\objfre_wnet_amd64\amd64\ansfw.lib .\lib\wxp\amd64\
copy ..\ansfw\vista\objfre_wlh_x86\i386\ansfw.lib .\lib\wlh\i386\
copy ..\ansfw\vista\objfre_wlh_amd64\amd64\ansfw.lib .\lib\wlh\amd64\


REM clean src
cd src\fltrtest
cmd /c clean.bat
cd ..\..

cd src\fwtest
cmd /c clean.bat
cd ..\..


