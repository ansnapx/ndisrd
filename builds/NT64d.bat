rmdir /s /q  NT64Debug
mkdir NT64Debug

cmd /c ..\ddkbuild -WLHNETX64 chk ..\ansfltr\nt -ceZ
cmd /c ..\ddkbuild -WLHNETX64 chk ..\ansfw\nt -ceZ
cmd /c ..\ddkbuild -WLHNETX64 chk ..\sdk\src\fwtest -ceZ
cmd /c ..\ddkbuild -WLHNETX64 chk ..\sdk\src\fltrtest -ceZ

copy ..\ansfltr\nt\ansfltr.inf .\NT64Debug\
copy ..\ansfltr\nt\ansfltr_m.inf .\NT64Debug\

copy ..\ansfltr\nt\objchk_wnet_amd64\amd64\ansfltr.sys .\NT64Debug\
copy ..\ansfltr\nt\objchk_wnet_amd64\amd64\ansfltr.pdb .\NT64Debug\
copy ..\ansfw\nt\objchk_wnet_amd64\amd64\ansfw.sys .\NT64Debug\
copy ..\ansfw\nt\objchk_wnet_amd64\amd64\ansfw.pdb .\NT64Debug\
copy ..\sdk\src\fwtest\objchk_wnet_amd64\amd64\fwtest.sys .\NT64Debug\
copy ..\sdk\src\fwtest\objchk_wnet_amd64\amd64\fwtest.pdb .\NT64Debug\
copy ..\sdk\src\fltrtest\objchk_wnet_amd64\amd64\fltrtest.sys .\NT64Debug\
copy ..\sdk\src\fltrtest\objchk_wnet_amd64\amd64\fltrtest.pdb .\NT64Debug\

inf2cat /driver:.\NT64Debug\ /os:XP_X64