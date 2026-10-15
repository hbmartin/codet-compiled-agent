# Full-application GCC 14 port

## Scope and result

The build uses the actual recovered `main`, 32 application subsystem translation units, `src/storage.c`, and `src/text_compat.c`. It includes CLI/TUI, configuration, all four providers, HTTP, tools, workers/subagents, plans, hooks, sessions, documentation search, SQLite, MCP, and bundled cJSON. No application implementation was replaced with a success stub to obtain the link.

The application function manifest contains 362 recovered functions, one new storage initializer, and two text compatibility helpers. Three distinct helpers originally sharing the `get_home` symbol receive subsystem prefixes to prevent duplicate C/link symbols. The independent 19-function subset and its dummy main are excluded from the full application.

## Source adaptations

- Removed Mach-O symbol decoration from C function names and bound imported functions to real Tiger SDK declarations.
- Added current application declarations, SDK headers, explicit integer/float bit reinterpretation, and typed views over recovered global storage.
- Restored eight data/storage regions with 363 native pointer assignments for initialized string/theme tables. `main` initializes these before application work. Literal pointer values in the TUI's local menu tables are also relocated.
- Preserved adjacent and overlapping stack slots in 115 functions using aligned byte frames and typed views at the recovered offsets. This handles decompiler-split argument arrays, callback contexts, pipes, scratch buffers, and worker state without relying on GCC's local-variable ordering.
- Repaired omitted call/format arguments using the recovered instructions and surrounding values, including home-directory lookup, route classification, web fetch/search, JSON number formatting, token-budget reporting, configuration persistence, and error/status output.
- Added printf-format attributes to the application's formatting wrappers so the compiler checks their callers.
- Replaced decompiler references to Mach-O header fields used as small integer offsets with explicit offsets. Standard CRT/SDK code replaces obsolete loader and stdio resolver routines.

The source retains historical recovery annotations, generic names, opaque fields, and low-level address arithmetic. Those annotations describe the input reconstruction; the current build declarations are in `include/app-functions.h`.

## Toolchain and libraries

The pinned image supplies GCC 14.2.0 and the Mac OS X 10.4u SDK. Compilation uses `-m32 -mcpu=750 -mmacosx-version-min=10.4 -std=gnu11 -O2 -fno-strict-aliasing`. The aliasing flag is necessary for recovered storage/stack views. Both the full build and syntax check enforce `-Wall -Wextra -Wformat=2 -Werror`. No warning categories are disabled or demoted.

Linking uses the SDK's curl, SQLite, ncurses, zlib, math, and libSystem interfaces. All direct imports resolve at link time. This differs from the original executable's custom `/opt/tigr` curl/OpenSSL installation: the SDK uses Tiger's system curl and SSL libraries. API behavior, supported curl options, TLS interoperability, and modern service compatibility therefore remain unverified. No claim is made that the system libraries reproduce that newer custom dependency stack.

## Static verification

`make verify` builds as needed and checks:

1. GCC version, target triple, 32-bit pointers, big-endian order, and deployment macro `1040`.
2. Recovered structure assertions and actual SDK `stat`, `dirent`, and `utsname` layouts.
3. Mach-O executable header: CPU PowerPC, subtype 750, 32-bit big-endian encoding.
4. Every manifest function is defined in executable text, including GCC's separate `__text_startup` section for `main`.
5. Dynamic dependencies use system `/usr/lib` paths and the linked file has no execute permission bits.

The linker is not given options to allow unresolved application symbols. The full build is warning-free; a successful link still does not establish functional equivalence. The build does not execute the target, its hooks, child processes, or any vulnerability payload.

## Warning cleanup and behavior preservation

The 458-warning baseline is now warning-free under the same GCC 14.2 toolchain and optimization level, with `-Werror` enabled. The changes preserve buffers, offsets, length limits, integer widths, and the existing error/truncation policy.

- Explicit SDK and recovered-layout pointer views preserve addresses and arithmetic. Format arguments use the types demanded by their existing format strings. Compile-time checks enforce 32-bit pointers, integers, longs, size values, and callback addresses. Borrowed SQLite text retains the existing recovered interfaces; these casts do not add ownership or extend its lifetime.
- Signed views for dimensions, scroll positions, cycling indices, the refresh PID, and the visited-directory count follow original signed comparisons. Examples: `chat_refresh` at `0x18a48`, `0x18a7c`, `0x18a9c`; `tree_walk` at `0x15e3c`; refresh PID at `0x8bc8`; highlighting's `-1` sentinel at `0x20d34`. This repairs the earlier C port's overly broad unsigned-word default.
- UTF-8 byte comparisons in `ascii_fixup` explicitly load unsigned bytes, matching `lbz` at `0x17fbc` and `0x18140` and comparisons against `0x80`, `0x86`, `0x90`, `0x92`, and `0x93`. The earlier signed-char expressions incorrectly made those branches unreachable.
- Parentheses make the existing operator precedence explicit. Unused ABI parameters are marked consumed, stale floating-point spill temporaries are removed, and exec-family argument lists use a pointer-typed null terminator.
- Four fixed format strings now appear as literals with the same bytes as the embedded storage. The conditional config-save message is formatted as `%s`; both possible messages are fixed strings without percent signs.
- Reviewed truncating string calls use `app_copy_padded` and `app_format_truncated`. The former copies up to the existing limit and zero-pads exactly like `strncpy`, including **no added terminator when the field fills**. The latter delegates to `vsnprintf`, preserving truncation, termination, argument side effects, and the would-have-written return count. Its format attribute keeps argument checking enabled. These contracts also avoid misleading overlap diagnostics caused by the decompiler's shared stack-frame array; overlapping source/destination remains outside the libc contract. No buffer was enlarged and no new truncation error policy was added.

`make check-text-host` links only these two newly written helpers and their test fixture. It passed 9,410 comparisons with host libc, covering short/long/high-bit input, empty and full fields, zero capacity, padding, returned values, format arguments, and unchanged bytes around the destination. This is evidence for helper equivalence on the host, not runtime validation of the PPC application. Eighteen unchanged application object files also remained byte-identical to the baseline during the cleanup; that is not a whole-program equivalence claim.

## Remaining uncertainty

The recovered program has not been run or functionally tested. Decompiler-inferred pointer types and callbacks, indirect calls, unions, structure interpretations, and control flow can still contain semantic errors that compilation cannot detect. Maintaining known stack offsets improves fidelity but does not prove original stack-frame or object-layout recovery. The original comments, source organization, macros, and optimized-away behavior cannot be recovered exactly.

Existing security-sensitive behavior, including shell execution and disabled certificate-chain verification in recovered paths, has not been deliberately hardened. Runtime resources such as the original documentation database are not required to compile and are not bundled here.

All port/build changes are confined to this directory. The original executable and five accompanying files remain unchanged. Treat the output as a statically verified build of the reconstruction, pending any separately authorized runtime validation.
