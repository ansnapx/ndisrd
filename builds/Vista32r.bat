rmdir /s /q  Vista32Release
mkdir Vista32Release

cmd /c ..\ddkbuild -WLH fre ..\ansfltr\vista -ceZ
cmd /c ..\ddkbuild -WLH fre ..\ansfw\vista -ceZ
cmd /c ..\ddkbuild -WLH fre ..\sdk\src\fwtest -ceZ
cmd /c ..\ddkbuild -WLH fre ..\sdk\src\fltrtest -ceZ

copy ..\ansfltr\vista\ansfltr.inf .\Vista32Release\

copy ..\ansfltr\vista\objfre_wlh_x86\i386\ansfltr.sys .\Vista32Release\
copy ..\ansfltr\vista\objfre_wlh_x86\i386\ansfltr.pdb .\Vista32Release\
copy ..\ansfw\vista\objfre_wlh_x86\i386\ansfw.sys .\Vista32Release\
copy ..\ansfw\vista\objfre_wlh_x86\i386\ansfw.pdb .\Vista32Release\
copy ..\sdk\src\fwtest\objfre_wlh_x86\i386\fwtest.sys .\Vista32Release\
copy ..\sdk\src\fwtest\objfre_wlh_x86\i386\fwtest.pdb .\Vista32Release\
copy ..\sdk\src\fltrtest\objfre_wlh_x86\i386\fltrtest.sys .\Vista32Release\
copy ..\sdk\src\fltrtest\objfre_wlh_x86\i386\fltrtest.pdb .\Vista32Release\

inf2cat /driver:.\Vista32Release\ /os:Vista_X86