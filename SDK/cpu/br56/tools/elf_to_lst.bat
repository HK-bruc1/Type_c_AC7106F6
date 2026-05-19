cd %~dp0

set OBJDUMP=C:\JL\pi32\bin\llvm-objdump.exe
set OBJCOPY=C:\JL\pi32\bin\llvm-objcopy.exe
set ELFFILE=sdk.elf

%OBJDUMP% -D -address-mask=0x1ffffff -print-dbg sdk.elf > sdk.lst