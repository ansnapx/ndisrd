rmdir /s /q  NT64Release
mkdir NT64Release

cmd /c ..\ddkbuild -WLHNETX64 fre ..\ansfltr\nt -ceZ
cmd /c ..\ddkbuild -WLHNETX64 fre ..\ansfw\nt -ceZ
cmd /c ..\ddkbuild -WLHNETX64 fre ..\sdk\src\fwtest -ceZ
cmd /c ..\ddkbuild -WLHNETX64 fre ..\sdk\src\fltrtest -ceZ

copy ..\ansfltr\nt\ansfltr.inf .\NT64Release\
copy ..\ansfltr\nt\ansfltr_m.inf .\NT64Release\

copy ..\ansfltr\nt\objfre_wnet_amd64\amd64\ansfltr.sys .\NT64Release\
copy ..\ansfltr\nt\objfre_wnet_amd64\amd64\ansfltr.pdb .\NT64Release\
copy ..\ansfw\nt\objfre_wnet_amd64\amd64\ansfw.sys .\NT64Release\
copy ..\ansfw\nt\objfre_wnet_amd64\amd64\ansfw.pdb .\NT64Release\
copy ..\sdk\src\fwtest\objfre_wnet_amd64\amd64\fwtest.sys .\NT64Release\
copy ..\sdk\src\fwtest\objfre_wnet_amd64\amd64\fwtest.pdb .\NT64Release\
copy ..\sdk\src\fltrtest\objfre_wnet_amd64\amd64\fltrtest.sys .\NT64Release\
copy ..\sdk\src\fltrtest\objfre_wnet_amd64\amd64\fltrtest.pdb .\NT64Release\

inf2cat /driver:.\NT64Release\ /os:XP_X64