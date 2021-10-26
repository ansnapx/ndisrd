rmdir /s /q  NT32Debug
mkdir NT32Debug

cmd /c ..\ddkbuild -WLHXP chk ..\ansfltr\nt -ceZ
cmd /c ..\ddkbuild -WLHXP chk ..\ansfw\nt -ceZ
cmd /c ..\ddkbuild -WLHXP chk ..\sdk\src\fwtest -ceZ
cmd /c ..\ddkbuild -WLHXP chk ..\sdk\src\fltrtest -ceZ

copy ..\ansfltr\nt\ansfltr.inf .\NT32Debug\
copy ..\ansfltr\nt\ansfltr_m.inf .\NT32Debug\

copy ..\ansfltr\nt\objchk_wxp_x86\i386\ansfltr.sys .\NT32Debug\
copy ..\ansfltr\nt\objchk_wxp_x86\i386\ansfltr.pdb .\NT32Debug\
copy ..\ansfw\nt\objchk_wxp_x86\i386\ansfw.sys .\NT32Debug\
copy ..\ansfw\nt\objchk_wxp_x86\i386\ansfw.pdb .\NT32Debug\
copy ..\sdk\src\fwtest\objchk_wxp_x86\i386\fwtest.sys .\NT32Debug\
copy ..\sdk\src\fwtest\objchk_wxp_x86\i386\fwtest.pdb .\NT32Debug\
copy ..\sdk\src\fltrtest\objchk_wxp_x86\i386\fltrtest.sys .\NT32Debug\
copy ..\sdk\src\fltrtest\objchk_wxp_x86\i386\fltrtest.pdb .\NT32Debug\

inf2cat /driver:.\NT32Debug\ /os:XP_X86