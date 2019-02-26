#include <inttypes.h>
#include <stdlib.h>

#include <misc_features.h>
#include <config_architecture.h>
#include <msr_core.h>
#include <variorum_error.h>

/* 02/25/19 SB
 * This format will be used moving forward for Xeon
 * I am currently batching the read of the turbo ratio limit, which is per
 * socket. Should we be? May be too much overhead. Is it necessary to read off
 * both sockets?"
 */
int get_max_non_turbo_ratio(off_t msr_platform_info)
{
    static int init = 0;
    static int nsockets = 0;
    static uint64_t **val = NULL;
    int max_non_turbo_ratio;

    variorum_set_topology(&nsockets, NULL, NULL);
    if (!init)
    {
        val = (uint64_t **) malloc(nsockets * sizeof(uint64_t *));
        allocate_batch(PLATFORM_INFO, nsockets);
        load_socket_batch(msr_platform_info, val, PLATFORM_INFO);
        init = 1;
    }

	read_batch(PLATFORM_INFO);
    max_non_turbo_ratio = (int)(MASK_VAL(*val[0], 15, 8));
    /// Do sockets match?
    if (nsockets != 1)
    {
        if (max_non_turbo_ratio != (int)(MASK_VAL(*val[1], 15, 8)))
        {
            return VARIORUM_ERROR_INVAL;
        }
    }
    /// 100 MHz multiplier
    return max_non_turbo_ratio * 100;
}

/* 02/25/19 SB
 * This format will be used moving forward for Xeon
 * I am currently batching the read of the turbo ratio limit, which is per
 * socket. Should we be? May be too much overhead. Is it necessary to read off
 * both sockets?"
 */
int get_max_efficiency_ratio(off_t msr_platform_info)
{
    static int init = 0;
    static int nsockets = 0;
    static uint64_t **val = NULL;
    int max_efficiency_ratio;
    int socket;

    variorum_set_topology(&nsockets, NULL, NULL);
    if (!init)
    {
        val = (uint64_t **) malloc(nsockets * sizeof(uint64_t *));
        allocate_batch(MAX_EFFICIENCY, nsockets);
        load_socket_batch(msr_platform_info, val, MAX_EFFICIENCY);
        init = 1;
    }

	read_batch(MAX_EFFICIENCY);

    for (socket = 0; socket < nsockets; socket++)
    {
        max_efficiency_ratio = (int)(MASK_VAL(*val[socket], 47, 40));
        printf("Max Efficiency Ratio: Socket %d Ratio %d MHz\n", socket, max_efficiency_ratio*100);
    }
    /// 100 MHz multiplier
    return max_efficiency_ratio * 100;
}

/* 02/25/19 SB
 * This format will be used moving forward for Xeon
 * I am currently batching the read of the turbo ratio limit, which is per
 * socket. Should we be? May be too much overhead. Is it necessary to read off
 * both sockets?"
 */
int get_min_operating_ratio(off_t msr_platform_info)
{
    static int init = 0;
    static int nsockets = 0;
    static uint64_t **val = NULL;
    int min_operating_ratio;
    int socket;

    variorum_set_topology(&nsockets, NULL, NULL);
    if (!init)
    {
        val = (uint64_t **) malloc(nsockets * sizeof(uint64_t *));
        allocate_batch(MIN_OPERATING_RATIO, nsockets);
        load_socket_batch(msr_platform_info, val, MIN_OPERATING_RATIO);
        init = 1;
    }

	read_batch(MIN_OPERATING_RATIO);

    for (socket = 0; socket < nsockets; socket++)
    {
        min_operating_ratio = (int)(MASK_VAL(*val[socket], 55, 48));
        printf("Min Operating Ratio: Socket %d Ratio %d MHz\n", socket, min_operating_ratio*100);
    }
    /// 100 MHz multiplier
    return min_operating_ratio * 100;
}

/* 02/25/19 SB
 * This format will be used moving forward for Xeon
 * I am currently batching the read of the turbo ratio limit, which is per
 * socket. Should we be? May be too much overhead. Is it necessary to read off
 * both sockets?"
 */
int get_turbo_ratio_limits(off_t msr_turbo_ratio_limit, off_t msr_turbo_ratio_limit_cores)
{
    static int init = 0;
    static int nsockets = 0;
    static uint64_t **val = NULL;
    static uint64_t **val2 = NULL;
    int socket, nbits;

    variorum_set_topology(&nsockets, NULL, NULL);
    if (!init)
    {
        val = (uint64_t **) malloc(nsockets * sizeof(uint64_t *));
        val2 = (uint64_t **) malloc(nsockets * sizeof(uint64_t *));
        allocate_batch(TURBO_RATIO_LIMITS, nsockets);
        allocate_batch(TURBO_RATIO_LIMITS_CORES, nsockets);
        load_socket_batch(msr_turbo_ratio_limit, val, TURBO_RATIO_LIMITS);
        load_socket_batch(msr_turbo_ratio_limit_cores, val2, TURBO_RATIO_LIMITS_CORES);
        init = 1;
    }

	read_batch(TURBO_RATIO_LIMITS);
	read_batch(TURBO_RATIO_LIMITS_CORES);

    for (socket = 0; socket < nsockets; socket++)
    {
        for (nbits = 0; nbits < 56; nbits+=8)
        {
            printf("Socket %d nactivecores = %d turbofreq = %d MHz\n", socket,
                   (int)(MASK_VAL(*val2[socket], nbits+7, nbits)),
                   (int)(MASK_VAL(*val[socket], nbits+7, nbits))*100);
        }
    }
}

void get_avx_limits(off_t msr_platform_info, off_t msr_config_tdp_l1, off_t msr_config_tdp_l2, off_t msr_config_tdp_nominal)
{
    static int init = 0;
    static int nsockets = 0;
    static uint64_t **val, **l1, **l2 = NULL;
    int socket;
    uint64_t nominal;

    variorum_set_topology(&nsockets, NULL, NULL);
    if (!init)
    {
        val = (uint64_t **) malloc(nsockets * sizeof(uint64_t *));
        allocate_batch(TDP_DEFS, nsockets);
        load_socket_batch(msr_platform_info, val, TDP_DEFS);
        init = 1;
    }

    /* P0n
     * ...
     * P01
     * P1
     * ...
     * Pn
     * PAVX2
     * PAVX512
     */
	read_batch(TDP_DEFS);

    int nvalues = (int)(MASK_VAL(*val[0], 34, 33));
    switch(nvalues)
    {
        case 0:
            // Read 648h = normal P1
            read_msr_by_coord(0, 0, 0, msr_config_tdp_nominal, &nominal);
            printf("Nominal P1 = %d MHz\n", (int)(MASK_VAL(nominal, 7, 0))*100);
            break;
        case 1:
            l1 = (uint64_t **) malloc(nsockets * sizeof(uint64_t *));
            allocate_batch(TDP_CONFIG_L1, nsockets);
            load_socket_batch(msr_config_tdp_l1, l1, TDP_CONFIG_L1);
            read_batch(TDP_CONFIG_L1);
            printf("AVX512 P1 = %d MHz\n", (int)(MASK_VAL(*l1[0], 23, 16))*100);

            // Read 648h = normal P1
            read_msr_by_coord(0, 0, 0, msr_config_tdp_nominal, &nominal);
            printf("Nominal P1 = %d MHz\n", (int)(MASK_VAL(nominal, 7, 0))*100);
            break;
        case 2:
            // Read 649h and 64a
            l1 = (uint64_t **) malloc(nsockets * sizeof(uint64_t *));
            allocate_batch(TDP_CONFIG_L1, nsockets);
            load_socket_batch(msr_config_tdp_l1, l1, TDP_CONFIG_L1);
            read_batch(TDP_CONFIG_L1);
            printf("AVX512 P1 = %d MHz\n", (int)(MASK_VAL(*l1[0], 23, 16))*100);

            l2 = (uint64_t **) malloc(nsockets * sizeof(uint64_t *));
            allocate_batch(TDP_CONFIG_L2, nsockets);
            load_socket_batch(msr_config_tdp_l2, l2, TDP_CONFIG_L2);
            read_batch(TDP_CONFIG_L2);
            printf("AVX2 P1 = %d MHz\n", (int)(MASK_VAL(*l2[0], 23, 16))*100);

            // Read 648h = normal P1
            read_msr_by_coord(0, 0, 0, msr_config_tdp_nominal, &nominal);
            printf("Nominal P1 = %d MHz\n", (int)(MASK_VAL(nominal, 7, 0))*100);
            break;
        case 3:
            break;
        default:
            printf("This case should not happen.\n");
            break;
    }
}

/// For socket level
int set_turbo_on(off_t msr_misc_enable, unsigned int turbo_mode_disable_bit)
{
	int ret = 0;
    int socket;
    int nsockets;
	uint64_t mask = 0;
	uint64_t msr_val = 0;

    variorum_set_topology(&nsockets, NULL, NULL);
    /// Creates mask for turbo disable bit according to the architecture offset
    /// given.
    mask |= 1LL << turbo_mode_disable_bit;

    for (socket = 0; socket < nsockets; socket++)
    {
        ret = read_msr_by_coord(socket, 0, 0, msr_misc_enable, &msr_val);
        if (ret)
        {
            variorum_error_handler("Error reading turbo MSR", VARIORUM_ERROR_MSR_READ, getenv("HOSTNAME"), __FILE__, __FUNCTION__, __LINE__);
            return ret;
        }

        /// Does the msr value have the turbo disable bit on?
        if (msr_val == (msr_val | mask))
        {
            uint64_t write_val = msr_val & ~mask;
            //fprintf(stdout, "Socket %i: Turning Turbo on\n", socket);
            //fprintf(stdout, "Value trying to be written to MSR %i: 0x%lx\n", msr_misc_enable, write_val);
            ret = write_msr_by_coord(socket, 0, 0, msr_misc_enable, write_val);
        }
        if (ret)
        {
            variorum_error_handler("Error writing turbo MSR", VARIORUM_ERROR_MSR_WRITE, getenv("HOSTNAME"), __FILE__, __FUNCTION__, __LINE__);
            return ret;
        }
        else
        {
            fprintf(stdout, "Turbo enabled on socket %d\n", socket);
        }
    }
	return ret;
}

int set_turbo_off(off_t msr_misc_enable, unsigned int turbo_mode_disable_bit)
{
	int ret = 0;
    int socket;
    int nsockets;
	uint64_t mask = 0;
	uint64_t msr_val = 0;

    variorum_set_topology(&nsockets, NULL, NULL);
    /// Creates mask for turbo disable bit according to the architecture offset
    /// given.
	mask |= 1LL << turbo_mode_disable_bit;

    for (socket = 0; socket < nsockets; socket++)
    {
        ret = read_msr_by_coord(socket, 0, 0, msr_misc_enable, &msr_val);
        if (ret)
        {
            variorum_error_handler("Error reading turbo MSR", VARIORUM_ERROR_MSR_READ, getenv("HOSTNAME"), __FILE__, __FUNCTION__, __LINE__);
            return ret;
        }

        /// Does the msr value have the turbo disable bit off?
        if (msr_val != (msr_val | mask))
        {
            uint64_t write_val = msr_val | mask;
            //fprintf(stdout, "Socket %i: Turning Turbo off\n", socket);
            //fprintf(stdout, "Value trying to be written to MSR %i: 0x%lx\n", msr_misc_enable, write_val);
            ret = write_msr_by_coord(socket, 0, 0, msr_misc_enable, write_val);
        }
        if (ret)
        {
            variorum_error_handler("Error writing turbo MSR", VARIORUM_ERROR_MSR_WRITE, getenv("HOSTNAME"), __FILE__, __FUNCTION__, __LINE__);
            return ret;
        }
        else
        {
            fprintf(stdout, "Turbo disabled on socket %d\n", socket);
        }
    }
	return ret;
}

int dump_turbo_status(FILE *writedest, off_t msr_misc_enable, unsigned int turbo_mode_disable_bit)
{
    int ret = 0;
    int socket;
    int nsockets;
    uint64_t mask = 0;
    uint64_t msr_val = 0;

    variorum_set_topology(&nsockets, NULL, NULL);
    mask |= 1LL << turbo_mode_disable_bit;

    for (socket = 0; socket < nsockets; socket++)
    {
        ret = read_msr_by_coord(socket, 0, 0, msr_misc_enable, &msr_val);
        if (ret)
        {
            variorum_error_handler("Could not read MSR_MISC_ENABLE", VARIORUM_ERROR_MSR_READ, getenv("HOSTNAME"), __FILE__, __FUNCTION__, __LINE__);
            return ret;
        }
        if (msr_val == (msr_val | mask))
        {
            fprintf(writedest, "Turbo disabled on socket %d.\n", socket);
        }
        else
        {
            fprintf(writedest, "Turbo enabled on socket %d.\n", socket);
        }
    }
    return ret;
}

///// For core level
//int set_turbo_on_core(const unsigned socket, const unsigned core, off_t msr_misc_enable, unsigned int turbo_mode_disable_bit)
//{
//	int ret = 0;
//	uint64_t mask = 0;
//	uint64_t msr_val = 0;
//	/// Creates mask for turbo disable bit according to the architecture offset given.
//	mask |= 1LL << turbo_mode_disable_bit;
//	ret = read_msr_by_coord(socket, 0, 0, msr_misc_enable, &msr_val);
//
//	/// Does the msr value have the Turbo disable bit on?
//	if (msr_val == (msr_val | mask))
//	{
//		uint64_t write_val = msr_val & ~mask;
//		//fprintf(stdout, "Socket %i: Turning Turbo on\n", socket);
//		//fprintf(stdout, "Value trying to be written to MSR %i: 0x%lx\n", msr_misc_enable, write_val);
//		ret += write_msr_by_coord(socket, 0, 0, msr_misc_enable, write_val);
//	}
//	if (ret)
//	{
//		fprintf(stderr, "Socket %i: Error Reading/Writing Turbo MSR\n", socket);
//	}
//	else
//	{
//		fprintf(stdout, "Socket %i: Turbo Enabled\n", socket);
//	}
//	return ret;
//}
//
//int set_turbo_off_core(const unsigned socket, const unsigned core, off_t msr_misc_enable, unsigned int turbo_mode_disable_bit)
//{
//	int ret = 0;
//	uint64_t mask = 0;
//	uint64_t msr_val = 0;
//    /// Creates mask for turbo disable bit according to the architecture offset
//    /// given.
//	//fprintf(stdout, "MSR Misc Enable value: %lx\n", msr_misc_enable);
//	mask |= 1LL << turbo_mode_disable_bit;
//	ret = read_msr_by_coord(socket, 0, 0, msr_misc_enable, &msr_val);
//
//	/// Does the msr value have the Turbo disable bit off?
//	if (msr_val != (msr_val | mask))
//	{
//		uint64_t write_val = msr_val | mask;
//		//fprintf(stdout, "Socket %i: Turning Turbo off\n", socket);
//		//fprintf(stdout, "Value trying to be written to MSR %i: 0x%lx\n", msr_misc_enable, write_val);
//		ret += write_msr_by_coord(socket, 0, 0, msr_misc_enable, write_val);
//	}
//	if (ret)
//	{
//		fprintf(stderr, "Socket %i: Error Reading/Writing Turbo MSR\n", socket);
//	}
//	else
//	{
//		fprintf(stdout, "Socket %i: Turbo Disabled\n", socket);
//	}
//	return ret;
//}
//
//int get_turbo_status_core(const unsigned socket, const unsigned core, off_t msr_misc_enable, unsigned int turbo_mode_disable_bit)
//{
//    /// Function returns:
//    /// 1 if Turbo is enabled
//    /// 0 if Turbo is DISabled
//    /// -1 if there was an error
//    int ret = 0;
//    uint64_t mask = 0;
//    uint64_t msr_val = 0;
//
//    mask |= 1LL << turbo_mode_disable_bit;
//    ret = read_msr_by_coord(socket, core, 0, msr_misc_enable, &msr_val);
//    if (ret)
//    {
//        fprintf(stderr, "Socket %d, Core %d: ERROR! Could not read msr 0x%lx", socket, core, msr_misc_enable);
//        return -1;
//    }
//    if (msr_val == (msr_val | mask))
//    {
//        //fprintf(stdout, "Socket %i, Core %d: Turbo is DISabled", socket, core);
//        return 0;
//    }
//    else
//    {
//        //fprintf(stdout, "Socket %i, Core %d: Turbo is enabled", socket, core);
//        return 1;
//    }
//}
