rmdir /s /q  Vista32Debug
mkdir Vista32Debug

cmd /c ..\ddkbuild -WLH chk ..\ansfltr\vista -ceZ
cmd /c ..\ddkbuild -WLH chk ..\ansfw\vista -ceZ
cmd /c ..\ddkbuild -WLH chk ..\sdk\src\fwtest -ceZ
cmd /c ..\ddkbuild -WLH chk ..\sdk\src\fltrtest -ceZ

copy ..\ansfltr\vista\ansfltr.inf .\Vista32Debug\

copy ..\ansfltr\vista\objchk_wlh_x86\i386\ansfltr.sys .\Vista32Debug\
copy ..\ansfltr\vista\objchk_wlh_x86\i386\ansfltr.pdb .\Vista32Debug\
copy ..\ansfw\vista\objchk_wlh_x86\i386\ansfw.sys .\Vista32Debug\
copy ..\ansfw\vista\objchk_wlh_x86\i386\ansfw.pdb .\Vista32Debug\
copy ..\sdk\src\fwtest\objchk_wlh_x86\i386\fwtest.sys .\Vista32Debug\
copy ..\sdk\src\fwtest\objchk_wlh_x86\i386\fwtest.pdb .\Vista32Debug\
copy ..\sdk\src\fltrtest\objchk_wlh_x86\i386\fltrtest.sys .\Vista32Debug\
copy ..\sdk\src\fltrtest\objchk_wlh_x86\i386\fltrtest.pdb .\Vista32Debug\

inf2cat /driver:.\Vista32Debug\ /os:Vista_X86