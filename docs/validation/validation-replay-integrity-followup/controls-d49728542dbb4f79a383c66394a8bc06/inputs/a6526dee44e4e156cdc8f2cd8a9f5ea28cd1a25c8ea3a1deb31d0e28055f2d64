/* Documentary helpers for the manually reconstructed tree walker.
 * They express the observed big-endian loads, not original application functions.
 * stat scratch capacity96 is inferred from stack+0x4b8 .. stack+0x518.
 * Only mode+8 and the displayed 32-bit size word+0x34 are asserted.
 */
#include <stdint.h>
#define PPC_BE16(p) ((uint16_t)(((const uint8_t *)(p))[0] << 8 | ((const uint8_t *)(p))[1]))
#define PPC_BE32(p) ((uint32_t)((uint32_t)((const uint8_t *)(p))[0] << 24 | (uint32_t)((const uint8_t *)(p))[1] << 16 | (uint32_t)((const uint8_t *)(p))[2] << 8 | ((const uint8_t *)(p))[3]))
struct RecoveredTreeSkipList_layout { uint32_t name_addresses[16]; int32_t count; };
