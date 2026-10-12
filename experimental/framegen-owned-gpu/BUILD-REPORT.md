# Diagnostic 0.3 — native control-file API correction

The user supplied a v0.2 log with CONTROL_STOP state=3 errno=1. This establishes that the lstat STOP lookup failed with EPERM before any GPU initialization. It does not establish the existence of STOP or successful compute.

Version 0.3 probes control files through SDK-exported sceKernelOpen, sceKernelFstat and sceKernelClose, using public OpenOrbis declarations and compile-checked native stat ABI (120 bytes, mode offset 8, size offset 72). O_NOFOLLOW rejects links and O_NONBLOCK avoids waiting on FIFOs. An opened enable marker must still be a regular empty file. STOP has precedence; unexpected native errors remain disabling, never silently treated as absence. Each probe logs raw API return codes. No filesystem permission elevation, kernel patches, game hooks or GPU backend changes.

Full native ELF/FSELF compilation and integrity inspection passed. The 12 control filesystem scenarios and 11 native API mock scenarios pass with ASan/UBSan. Prior 10 backend suites and exact gfx1013 shader compilation concern the unchanged backend; they were not rerun for this adapter-only correction. Hardware execution and 12.70 native API behavior for this new version remain untested.

Close the diagnostic, replace its eboot.bin and sce_sys/param.json, retain the empty ENABLE_GPU_TEST file and existing companion module, then launch once and retrieve diagnostic.log. Do not remove an intentional STOP. This is a native application, not a Payload Manager payload. HEN and embedded kstuff are unchanged.
