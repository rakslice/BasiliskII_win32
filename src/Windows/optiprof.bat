REM A batch file to to optimize uae cpu core (Profile).

cd Basilisk
copy cpuemu.asm cpuemu0.asm
rem copy cpuemu0.asm cpuemu.asm
..\vc5opti\Release\vc5opti.exe cpuemu.asm
ml /c /coff cpuemu.asm
cd ..
