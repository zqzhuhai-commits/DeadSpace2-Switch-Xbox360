from pathlib import Path
import subprocess,sys,os
root=Path(__file__).resolve().parent
os.chdir(root)
asm=['.section .rdata,"dr"']
for i in range(8):
 asm+=['.balign 16',f'.global v04_data_{i}',f'v04_data_{i}:',f'.incbin "assets/data_{i}.bin"']
(root/'payload_v04.S').write_text('\n'.join(asm)+'\n')
zig=sys.argv[1] if len(sys.argv)>1 else 'zig'
subprocess.run([zig,'cc','-target','x86_64-windows-gnu','-O2','-municode','-Wl,--subsystem,windows','viv_native_v04.c','payload_v04.S','-o','DS2_Tool_v04.exe','-lcomdlg32','-lcomctl32','-lgdi32','-luser32'],check=True)
print(root/'DS2_Tool_v04.exe')
