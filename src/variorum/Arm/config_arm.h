#ifndef CONFIG_ARM_H_INCLUDE
#define CONFIG_ARM_H_INCLUDE

#include <inttypes.h>

uint64_t *detect_arm_arch(void);

int set_arm_func_ptrs(void);

#endif
