#!/usr/bin/env python3
# Copyright (C) 2026 ANHAR. GPL-3.0-or-later.
import hashlib,json,os,shutil,struct,subprocess
from pathlib import Path
R=Path(__file__).resolve().parents[1];V=R/'upstream/ps5-vulkan';F=R/'upstream/ps5-native-app-boilerplate';G=R/'upstream/ps5-agc-gears';L=R/'lab/projects/logging_server/client';S=Path(os.environ.get('PS5_PAYLOAD_SDK','/opt/ps5-payload-sdk'));B=R/'build';D=R/'Release/PPSA99993-app';B.mkdir(exist_ok=True);D.mkdir(parents=True,exist_ok=True)
def run(*a):subprocess.run([str(x) for x in a],check=True)
for p,sha in [(V,'e6d3c3f82f3c8a574e28c174b15b287f9c0ecc6d'),(F,'37dd53602bdead63936f718004555ba10154be48'),(G,'1ae1f9182abd2770c131b97419034fb85173c2dc'),(V/'third_party/psbc-reference','4fe7264a0cab9feb531a156e8a810b3e39cfe42c')]:
 assert subprocess.check_output(['git','-C',str(p),'rev-parse','HEAD'],text=True).strip()==sha
run('glslangValidator','-V','--target-env','vulkan1.0',R/'shaders/owned_blend.comp','-o',B/'owned.spv');run('spirv-val','--target-env','vulkan1.0',B/'owned.spv')
b=(B/'owned.spv').read_bytes();words=struct.unpack('<'+str(len(b)//4)+'I',b);(B/'owned_shader.h').write_text('#include <stdint.h>\nstatic const uint32_t owned_shader[]={'+','.join(hex(x)+'u' for x in words)+'};\n')
cc=['sh',str(F/'tooling/prospero-clang18')];inc=[R/'src',B,V/'src',V/'native',V/'third_party/vulkan-headers/include',G/'include',G/'src',L,V/'third_party/psbc-reference',V/'third_party/psbc-reference/src',V/'third_party/psbc-reference/libpsbc',V/'third_party/opengnm/include']
flags=['-std=c11','-O2','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-DPS5VK_RUNTIME_COMPILER=1','-DPS5VK_NO_OFFLINE_LIBRARY=1']+['-I'+str(p) for p in inc]
sources=list(V.glob('src/vk_*.c'))+[V/p for p in ['src/spirv_ubo_layout.c','src/color_detile.c','src/depth_detile.c','src/graphics_program.c','src/texture_copy.c','src/texture_format.c','src/texture_layout.c','native/platform_ps5.c','native/memory_ps5.c','native/queue_ps5.c','src/compute_commands.c','src/dispatch_encode.c','src/descriptor_encode.c','src/texture_descriptor.c','src/depth_layout.c','src/compilation_cache.c','src/ps5vk_compiler.c','src/ps5_compiler_shims.c']]+[V/'native/command_arena_ps5.c',R/'src/native_main.c',R/'src/native_display.c',R/'src/no_presentation.c',L/'ps5log.c']+[G/('src/'+n) for n in ['ps5_surface.c','ps5_videoout.c','ps5_present.c','ps5_agc_writer.c','ps5_gpu_span.c','ps5_event_adapter.c','ps5_frame_completion.c']]
objects=[]
for i,p in enumerate(sources):
 o=B/(str(i)+'-'+p.stem+'.o');run(*cc,*flags,'-c',p,'-o',o);objects.append(o)
crt=B/'crt.o';run(*cc,'-std=c++20','-O2','-fno-exceptions','-fno-rtti','-c',F/'tooling/native/app_crt.cpp','-o',crt)
link=S/'bin/prospero-lld';stubs=[]
for name in ['libSceAgc','libSceAgcDriver']:
 o=B/(name+'.o');so=B/(name+'.so');run(*cc,'-fPIC','-I'+str(G/'include'),'-c',G/('native/stubs/'+name+'.c'),'-o',o);run(link,'--shared','-soname',name+'.prx','-o',so,o);stubs.append(so)
libs=[V/'build/libpsbc.ps5.a']+[S/('target/lib/'+n) for n in ['libc++.a','libc++abi.a','libunwind.a','libpthread.a','libc.a']]
run(link,'-L'+str(S/'target/lib'),'-T',V/'native/ps5-pie.ld','--eh-frame-hdr','--gc-sections','--version-script',V/'native/app-symbols.map','-e','_start','-o',B/'diagnostic.pie.elf',crt,*objects,*libs,'--as-needed',*sorted((S/'target/lib').glob('*.so')),*stubs)
builder=F/'build/runtime-shim/ps5-native-tool';assert builder.is_file()
run(builder,'link','--in',B/'diagnostic.pie.elf','--out',B/'331-FrameGen-GPU-Diagnostic.elf','--stub-dir',S/'target/lib','--module-sdk','0x02000009','--stub',stubs[0],'--stub',stubs[1],'--companion-sdk','0x08050001','--file-name','eboot.elf')
run(builder,'self','--sign','--in',B/'331-FrameGen-GPU-Diagnostic.elf','--out',D/'eboot.bin','--magic','0x1D3D154F')
(D/'sce_sys').mkdir(exist_ok=True);(D/'sce_module').mkdir(exist_ok=True)
p=json.loads((G/'sce_sys/param.json').read_text());p.update(titleId='PPSA99993',conceptId='99993',contentId='UP9000-PPSA99993_00-331FGDIAG0000001',contentVersion='00.004.000');p['localizedParameters']['en-US']['titleName']='331 FrameGen Display Diagnostic';(D/'sce_sys/param.json').write_text(json.dumps(p,indent=2)+'\n')
shutil.copyfile(F/'runtime/libc.prx',D/'sce_module/libc.prx');shutil.copyfile(F/'sce_sys/icon0.png',D/'sce_sys/icon0.png');shutil.copyfile(B/'331-FrameGen-GPU-Diagnostic.elf',R/'Release/331-FrameGen-GPU-Diagnostic.elf')
manifest={'diagnostic_version':'0.4','stage':'owned-image compute and opt-in owned scanout diagnostic','ps5_tested':False,'game_interception':False,'presentation':'experimental-owned-surfaces-only; untested','frame_generation':False,'native_title':'PPSA99993','gpu_dispatch_default_enabled':False,'display_default_enabled':False,'display_test_requires':'ENABLE_GPU_TEST and ENABLE_DISPLAY_TEST; STOP absent','shader_sha256':hashlib.sha256(b).hexdigest(),'elf_sha256':hashlib.sha256((B/'331-FrameGen-GPU-Diagnostic.elf').read_bytes()).hexdigest(),'native_fself_sha256':hashlib.sha256((D/'eboot.bin').read_bytes()).hexdigest()}
(R/'Release/BUILD-MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n');print(json.dumps(manifest,indent=2))
