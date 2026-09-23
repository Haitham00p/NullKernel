# Project Summary (updated)

## Objective
- Fix kernel heap overflow (DONE — canary/redzone heap).
- Make Ctrl+C a global interrupt that breaks any running program/loop (DONE & VERIFIED).
- Add a frame-refresh loop and research (web) how to make screen updates smooth at ≥60 fps.

## Critical Discovery
- `ncc/vault_gen.c` bakes in **hardcoded kernel symbol addresses** via `ncc/genvault.sh` (`make vault`, two-pass convergence). `make install` does NOT depend on the `vault` target, so after kernel edits shifted layout, guest API calls resolved to the WRONG host functions (e.g. `cd` → `SelfHostLinkFiles`), crashing the OS. **Regenerating the vault (`make vault` x2 + rebuild) fixed every guest crash.** All addresses now verified matching (`SelfHostLinkFiles`, `printf`, `TerminalWrite32`, `HighlightPrintFile`, `RunProgramOnStack`).

## Completed
- `mm/heap/heap.c`: canary/redzone heap; `kfree` quarantines on corruption instead of chaining smashed `Next`.
- `lib/string/string.c`: NULL-safe `strlen`/`strcmp`/`strcasecmp`/`strncasecmp`/`memcpy`/`memset`.
- `shell/highlight.c`: scanners disabled (return false) + `HighlightPrintFile` plain write — cosmetic, crash-proof. (Scanners had unsafe pointer walks that faulted the OS.)
- `arch/x86_64/interrupts/irq.c`: Ctrl+C abort (rewrites iret frame to `RunAbortResume`).
- `arch/x86_64/program/run.asm`: `RunAbortResume` returns `NULLOS_GUEST_ABORTED` (0x2BAD0B5E).
- `shell/selfhost.c`: prints "program aborted (Ctrl+C)" on abort.
- `ncc/vault_gen.c`: REGENERATED to match current kernel.
- **VERIFIED in QEMU**: boot stable, `cd /bin` works, `./LOOP.EXE` runs (loop 0→3112), Ctrl+C → `program aborted (Ctrl+C)`, prompt returns, `EXCEPTION? 0`.

## Active / Next
- Frame-refresh loop + 60fps smoothness research (WEB). Implement continuous redraw (double-buffer + vblank/timer-driven) so terminal/animations move smoothly.
- Keep OS stable: build order must be `make vault && make && make vault && make install` after kernel symbol changes (else guests crash).

## Build/Test Reference
- Stale-build trap: after editing kernel `.c`, run `make vault` (twice) then `make install`; `rm -rf build .vault-stamp` for a clean slate.
- Disk: `rm -f nullos.img; ./mkdisk.sh nullos.img` then `make install`.
- Test VM: `qemu-system-x86_64 -m 256 -boot order=c -drive file=nullos.img,format=raw,if=ide,index=0 -serial file:/tmp/serial-X.log -monitor unix:/tmp/nullos-mon.sock,server,nowait -display none`. HMP keys: shift-x, spc, ret, dot, slash, minus. Kill: `pkill -9 qemu`.
- Ctrl+C demo: `cd /bin`, `./LOOP.EXE`, then `ctrl-c`.

## Relevant Files
- `ncc/genvault.sh`, `ncc/vault_gen.c` (guest API addresses — regenerate after kernel changes!)
- `mm/heap/heap.c`, `lib/string/string.c`, `shell/highlight.c`, `shell/selfhost.c`
- `arch/x86_64/interrupts/irq.c`, `arch/x86_64/program/run.asm`
- `drivers/video_framebuffer/framebuffer.h` (for frame-refresh task)
