/* Idle stub at code-hunk offset 0 (Running a .library returns -1).
 * Own TU + section(".text") + link-first so string literals cannot precede it.
 * Whatever sits first in .text becomes the --gc-sections root; this stub refs
 * nothing, so make_driver keeps the library via -Wl,--undefined=_ROMTag. */
extern "C" int __attribute__((used, section(".text"))) LibNotExecutable(void)
{
    return -1;
}
