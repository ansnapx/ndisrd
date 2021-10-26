rmdir /s /q  NT32Release
mkdir NT32Release

cmd /c ..\ddkbuild -WLHXP fre ..\ansfltr\nt -ceZ
cmd /c ..\ddkbuild -WLHXP fre ..\ansfw\nt -ceZ
cmd /c ..\ddkbuild -WLHXP fre ..\sdk\src\fwtest -ceZ
cmd /c ..\ddkbuild -WLHXP fre ..\sdk\src\fltrtest -ceZ

copy ..\ansfltr\nt\ansfltr.inf .\NT32Release\
copy ..\ansfltr\nt\ansfltr_m.inf .\NT32Release\

copy ..\ansfltr\nt\objfre_wxp_x86\i386\ansfltr.sys .\NT32Release\
copy ..\ansfltr\nt\objfre_wxp_x86\i386\ansfltr.pdb .\NT32Release\
copy ..\ansfw\nt\objfre_wxp_x86\i386\ansfw.sys .\NT32Release\
copy ..\ansfw\nt\objfre_wxp_x86\i386\ansfw.pdb .\NT32Release\
copy ..\sdk\src\fwtest\objfre_wxp_x86\i386\fwtest.sys .\NT32Release\
copy ..\sdk\src\fwtest\objfre_wxp_x86\i386\fwtest.pdb .\NT32Release\
copy ..\sdk\src\fltrtest\objfre_wxp_x86\i386\fltrtest.sys .\NT32Release\
copy ..\sdk\src\fltrtest\objfre_wxp_x86\i386\fltrtest.pdb .\NT32Release\

inf2cat /driver:.\NT32Release\ /os:XP_X86