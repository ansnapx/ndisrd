rmdir /s /q  Vista64Release
mkdir Vista64Release

cmd /c ..\ddkbuild -WLHX64 fre ..\ansfltr\vista -ceZ
cmd /c ..\ddkbuild -WLHX64 fre ..\ansfw\vista -ceZ
cmd /c ..\ddkbuild -WLHX64 fre ..\sdk\src\fwtest -ceZ
cmd /c ..\ddkbuild -WLHX64 fre ..\sdk\src\fltrtest -ceZ

copy ..\ansfltr\vista\ansfltr.inf .\Vista64Release\

copy ..\ansfltr\vista\objfre_wlh_amd64\amd64\ansfltr.sys .\Vista64Release\
copy ..\ansfltr\vista\objfre_wlh_amd64\amd64\ansfltr.pdb .\Vista64Release\
copy ..\ansfw\vista\objfre_wlh_amd64\amd64\ansfw.sys .\Vista64Release\
copy ..\ansfw\vista\objfre_wlh_amd64\amd64\ansfw.pdb .\Vista64Release\
copy ..\sdk\src\fwtest\objfre_wlh_amd64\amd64\fwtest.sys .\Vista64Release\
copy ..\sdk\src\fwtest\objfre_wlh_amd64\amd64\fwtest.pdb .\Vista64Release\
copy ..\sdk\src\fltrtest\objfre_wlh_amd64\amd64\fltrtest.sys .\Vista64Release\
copy ..\sdk\src\fltrtest\objfre_wlh_amd64\amd64\fltrtest.pdb .\Vista64Release\

inf2cat /driver:.\Vista64Release\ /os:Vista_X64
