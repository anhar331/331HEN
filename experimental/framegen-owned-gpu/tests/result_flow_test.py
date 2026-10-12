# GPL-3.0-or-later. Static source routing checks, not PS5 execution.
from pathlib import Path
import hashlib
root=Path(__file__).resolve().parents[1]
main=(root/'src/native_main.c').read_text()
positions=[main.index(s) for s in ('CHECK(vkWaitForFences','CHECK(vkInvalidateMappedMemoryRanges','if(!fg_verify','const struct fg_result_view view=','fg_run_owned_display(&view,stop)')]
assert positions==sorted(positions)
assert 'view={mapped[0]+FG_GUARD,mapped[1]+FG_GUARD,mapped[2]+FG_GUARD,FG_PIXELS}' in main
for name in ('native_display.c','result_view.h','result_scanout.h'):
 s=(root/'src'/name).read_text()
 assert 'fg_blend(' not in s and 'fg_pixel(' not in s
assert hashlib.sha256((root/'build/owned.spv').read_bytes()).hexdigest()=='9706ef9b8d76dde8451bfd5eafab0d18f411563914a0985768db8c8ce6eeb2f5'
print('PASS: source routing orders fence -> invalidate -> validation -> mapped GPU output view -> display; display code does not synthesize a blend; compiled shader hash unchanged. Static checks, not GPU execution.')
