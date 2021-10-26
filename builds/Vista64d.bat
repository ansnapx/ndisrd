rmdir /s /q  Vista64Debug
mkdir Vista64Debug

cmd /c ..\ddkbuild -WLHX64 chk ..\ansfltr\vista -ceZ
cmd /c ..\ddkbuild -WLHX64 chk ..\ansfw\vista -ceZ
cmd /c ..\ddkbuild -WLHX64 chk ..\sdk\src\fwtest -ceZ
cmd /c ..\ddkbuild -WLHX64 chk ..\sdk\src\fltrtest -ceZ

copy ..\ansfltr\vista\ansfltr.inf .\Vista64Debug\

copy ..\ansfltr\vista\objchk_wlh_amd64\amd64\ansfltr.sys .\Vista64Debug\
copy ..\ansfltr\vista\objchk_wlh_amd64\amd64\ansfltr.pdb .\Vista64Debug\
copy ..\ansfw\vista\objchk_wlh_amd64\amd64\ansfw.sys .\Vista64Debug\
copy ..\ansfw\vista\objchk_wlh_amd64\amd64\ansfw.pdb .\Vista64Debug\
copy ..\sdk\src\fwtest\objchk_wlh_amd64\amd64\fwtest.sys .\Vista64Debug\
copy ..\sdk\src\fwtest\objchk_wlh_amd64\amd64\fwtest.pdb .\Vista64Debug\
copy ..\sdk\src\fltrtest\objchk_wlh_amd64\amd64\fltrtest.sys .\Vista64Debug\
copy ..\sdk\src\fltrtest\objchk_wlh_amd64\amd64\fltrtest.pdb .\Vista64Debug\

inf2cat /driver:.\Vista64Debug\ /os:Vista_X64