@echo off
:: Remove old zip
del HaloCEVR.zip
:: Copy bindings/manifest files nto correct subfolder
xcopy "./Bindings" "./Output/VR/OpenVR/" /E /I
:: Remove any dev files used to generate the bindings
del ".\Output\VR\OpenVR\*.py"
:: Copy .dlls to top level
robocopy "./Release" "./Output" "*.dll"
:: Zip everything
tar -acf HaloCEVR.zip -C Output *.*
:: Remove temp directory
rmdir /s /q Output