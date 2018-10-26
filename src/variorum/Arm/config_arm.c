#include <stdio.h>
#include <stdlib.h>

#include <config_arm.h>
#include <config_architecture.h>
#include <variorum_error.h>

uint64_t *detect_arm_arch(void)
{
    uint64_t *model = (uint64_t *) malloc(sizeof(uint64_t));
    model = 0;

    return model;
}

int set_arm_func_ptrs(void)
{
    int err = 0;

    if (*g_platform.arm_arch == JUNO)
    {
    }
    else
    {
        return VARIORUM_ERROR_UNSUPPORTED_PLATFORM;
    }

    return err;
}
