/*
 * thrd-partition.c
 *
 *  Created on: 23 May 2016
 *  Author: Lukas Zimmermann <lzimmer1@stud.hs-offenburg.de>
 *
 *  Thread Network Partitions.
 */

#include "emb6.h"
#include "thread_conf.h"
#include "bsp.h"
#include "thrd-partition.h"
#include "rip.h"
#include "tlv.h"

#include "thrd-router-id.h"

#define DEBUG DEBUG_PRINT
#include "uip-debug.h"	// For debugging terminal output.

/*==============================================================================
                          LOCAL VARIABLE DECLARATIONS
 =============================================================================*/

/*==============================================================================
                               LOCAL FUNCTION PROTOTYPES
 =============================================================================*/

/*==============================================================================
                                    LOCAL FUNCTIONS
 =============================================================================*/

/* --------------------------------------------------------------------------- */

/*==============================================================================
                                     API FUNCTIONS
 =============================================================================*/

/**
 * Starting a new Partition.
 */
void
thrd_partition_start(void)
{
	if ( (thrd_dev.type == THRD_DEV_TYPE_ROUTER) || (thrd_dev.type == THRD_DEV_TYPE_REED) ) {

		PRINTF("thrd_partition_start: Starting new Thread Partition.\n");
		thrd_leader_init();		// Initialize itself as the leader.

		thrd_partition.Partition_ID = (bsp_getrand(0) << 16) | (bsp_getrand(0));
		thrd_partition.VN_version = (uint8_t) bsp_getrand(0);
		thrd_partition.VN_stable_version = (uint8_t) bsp_getrand(0);
		thrd_partition.ID_sequence_number = (uint8_t) bsp_getrand(0);
		thrd_partition.Partition_weight = 64;

		thrd_print_partition_data();
	}
}

/* --------------------------------------------------------------------------- */

/**
 * Process a given Leader Data TLV and compare its content with the available
 * leader data.
 * @param id_sequence_number The corresponding ID Sequence Number (Route64 TLV).
 * @param leader_tlv The Leader Data TLV.
 * @return 0, if the partition topology changes (the Leader Data TLV is invalid).
 *         1, if the partition data is valid (this should trigger the procession
 *         of a Route64 TLV).
 */
uint8_t
thrd_partition_process(uint8_t id_sequence_number, tlv_leader_t *leader_tlv)
{
	if ( leader_tlv != NULL ) {

		// Compare partition values.
		if ( leader_tlv->weight > thrd_partition.Partition_weight ) {
			attach:
			// TODO Try to attach to other partition.
			PRINTF("thrd_partition_process: Try to attach to other partition!\n");
			return 0;
		} else if ( leader_tlv->weight == thrd_partition.Partition_weight ) {
			if ( leader_tlv->partition_id > thrd_partition.Partition_ID ) {
				goto attach;
			} else if ( leader_tlv->partition_id == thrd_partition.Partition_ID ) {
				if ( (leader_tlv->data_version > thrd_partition.VN_version)
						&& (leader_tlv->stable_data_version > thrd_partition.VN_stable_version)
						&& (id_sequence_number > thrd_partition.ID_sequence_number)) {
					// Inconsistency detected -> Start a new Partition.
					thrd_partition_start();
					return 0;
				}
			}
		}
		return 1;
	}
	PRINTF("thrd_partition_process: Invalid Leader Data TLV.\n");
	return 0;
}

/* --------------------------------------------------------------------------- */

/**
 * Remove all information related to the previous partition.
 * Thread Spec.: Resetting Network Partition Data.
 */
void
thrd_partition_empty(void)
{
	// TODO
}

/*
 ********************************************************************************
 *                                DEBUG FUNCTIONS
 ********************************************************************************
 */

void
thrd_print_partition_data()
{
	PRINTF(ANSI_COLOR_CYAN "|================================== THREAD PARTITION ===================================|" ANSI_COLOR_RESET "\n\r");;
	PRINTF("| Partition_ID | VN_version | VN_stable_version | ID_sequence_number | Partition_weight |\n");
	PRINTF("-----------------------------------------------------------------------------------------\n\r");
	PRINTF("| " ANSI_COLOR_YELLOW "%12lu" ANSI_COLOR_RESET
			" | " ANSI_COLOR_YELLOW "%10d" ANSI_COLOR_RESET
			" | " ANSI_COLOR_YELLOW "%17d" ANSI_COLOR_RESET
			" | " ANSI_COLOR_YELLOW "%18d" ANSI_COLOR_RESET
			" | " ANSI_COLOR_YELLOW "%16d" ANSI_COLOR_RESET
			" |\n", thrd_partition.Partition_ID, thrd_partition.VN_version, thrd_partition.VN_stable_version, thrd_partition.ID_sequence_number, thrd_partition.Partition_weight);
	PRINTF(ANSI_COLOR_CYAN "=========================================================================================" ANSI_COLOR_RESET "\n\r");
}

/* --------------------------------------------------------------------------- */