/* ==========================================================================
 * package.c: Embedded Transport Protocol (Single master with Single Salve)
 *
 * function:  1. For single master with single slave, half-duplex. The master
 *               ask initiatively, and the slave ack passively.
 *            2. Can ensure data integrity by checksum algorithm.
 *            3. The master can resend automatically, with a feedback of
 *               resend times.
 *            4. The shared data buffer can save space and time.
 *            5. Caches sent data for resend.
 *            6. Support variable-length data part.
 *            7. Application can define their own data structure.
 *            8. Statistics for every sent and received package.
 * ======================================================================== */

#include <stdio.h>
#include <string.h>
#include "package.h"

// ============================ Static Variables ============================
static U8 send_buf[MAX_BUF_SIZE];  // Sending buffer
static bool flag_is_master;        // If the machine is master
static U32 master_max_ack_delay;   // The max wait time that master waiting for ack
static send_bytes_func send_bytes; // Callback function for sending bytes

static U16 slave_recv_seqno_last;  // The last seqno that slave received
static U16 master_send_seqno_last; // The last seqno that master sent
static bool flag_master_need_ack;  // If master is waiting for ack
static U32 master_send_time_last;  // The last point-in-time that master sent package
static U16 master_retry_times;     // The resend times of master
static struct pack_count pack_count_info; // Statistics for sent and received packages

// ============================ Global Variables ============================
// Receiving buffer for lower layer to store received data
U8 recv_buf[MAX_BUF_SIZE];

// Sending data address for application to store its sending data
void* send_data = ((struct pack_header*)send_buf)->data;
// Receiving data address for application to read its receiving data
const void* recv_data = ((struct pack_header*)recv_buf)->data;


// =========================== Interface Functions ==========================
// Compute Checksum for 'count' bytes beginning at location 'addr'
static U16 checksum(const U8* addr, U16 count)
{
	register U32 sum = 0;

	// Calculate the sum as 16-bit digital
	while (count > 1) {
		sum += *(U16*)addr++;
		count -= 2;
	}

	// Deal with odd-numbered situation
	if (count > 0) {
		sum += *(U8*)addr;
	}

	// Add the high bit overflow to the low 16-bit
	while (sum >> 16) {
		sum = (sum & 0xFFFF) + (sum >> 16);
	}

	// return the one's complement
	return (U16)(~sum);
}

// Initialize variables
static void init_data(void)
{
	memset(send_buf, 0, sizeof(send_buf));
	flag_is_master = false;
	master_max_ack_delay = 0;
	send_bytes = NULL;

	slave_recv_seqno_last = 0;
	master_send_seqno_last = 0;
	flag_master_need_ack = false;
	master_send_time_last = 0;
	master_retry_times = 0;
	memset(&pack_count_info, 0, sizeof(pack_count_info));

	memset(recv_buf, 0, sizeof(recv_buf));
	send_data = ((struct pack_header*)send_buf)->data;
	recv_data = ((struct pack_header*)recv_buf)->data;
}

// Initialize protocol
void init_pack(bool is_master, U32 max_ack_delay, send_bytes_func func)
{
	// Initialize variables
	init_data();
	// Config the protocol parameters with the given values
	flag_is_master = is_master;
	master_max_ack_delay = max_ack_delay;
	send_bytes = func;
}

// Original send package function
static void _send_pack(U16 data_len, bool is_new_pack)
{
	// Mapping the sending buffer with struct pack_header
	struct pack_header* pack = (struct pack_header*)send_buf;

	// See if it is a new package
	if (is_new_pack) {
		// Fill data in accordance with the package structure
		pack->premble[0] = PACK_PREMBLE;
		pack->premble[1] = PACK_PREMBLE;
		pack->premble[2] = PACK_PREMBLE;
		pack->start = PACK_START;
		// Master's seqno will incremente by 1, but slave just take the last
		// received seqno for this sending
		if (flag_is_master) {
			pack->seqno++;
			if (pack->seqno == 0) {
				pack->seqno = 1;
			}
		} else {
			pack->seqno = slave_recv_seqno_last;
		}
		pack->len = data_len;
		pack->chksum = checksum((const U8*)&pack->seqno, pack->len + CHECKSUM_HEAD_LEN);

		// Count the new sending package
		pack_count_info.send_pack_count[PACK_SEND_NEW]++;
	} else {
		// Count the resending package
		pack_count_info.send_pack_count[PACK_SEND_RETRY]++;
	}

	// Send package
	send_bytes(send_buf, sizeof(struct pack_header) + pack->len);

	// Master must check if ack is timeout
	if (flag_is_master) {
		// Mark the master is waiting for ack
		flag_master_need_ack = true;
		// Record the last point-in-time that master sent package
		master_send_time_last = LOCAL_TIME();
		// Record the last seqno that master sent
		master_send_seqno_last = pack->seqno;
	}
}

// Send package
void send_pack(U16 data_len)
{
	_send_pack(data_len, true);
}

// Resend the last package
static void resend_pack(void)
{
	_send_pack(0, false);
}

// Check validity of the received package
enum pack_recv_type_list check_pack(void)
{
	enum pack_recv_type_list ret = PACK_RECV_NEW;
	struct pack_header* pack = (struct pack_header*)recv_buf;

	do {
		// Check the premble
		if (pack->premble[0] != PACK_PREMBLE
		|| pack->premble[1] != PACK_PREMBLE
		|| pack->premble[2] != PACK_PREMBLE) {
			// Count the premble error package
			pack_count_info.recv_pack_count[PACK_RECV_PREMBLE_ERR]++;
			ret = PACK_RECV_PREMBLE_ERR;
			break;
		}

		// Check the start code
		if (pack->start != PACK_START) {
			// Count the start code error package
			pack_count_info.recv_pack_count[PACK_RECV_START_ERR]++;
			ret = PACK_RECV_START_ERR;
			break;
		}

		if (flag_is_master) {
			// Master check if the seqno is same as the last sent
			if (pack->seqno != master_send_seqno_last) {
				// Count the seqno error package
				pack_count_info.recv_pack_count[PACK_RECV_SEQNO_ERR]++;
				ret = PACK_RECV_SEQNO_ERR;
				break;
			}
		}

		// Check the data length
		if ((pack->len < 1) || (pack->len > MAX_DATA_LEN)) {
			// Count the data length error package
			pack_count_info.recv_pack_count[PACK_RECV_LEN_ERR]++;
			ret = PACK_RECV_LEN_ERR;
			break;
		}

		// Check the checksum
		if (pack->chksum != checksum((const U8*)&pack->seqno, pack->len + CHECKSUM_HEAD_LEN)) {
			// Count the checksum error package
			pack_count_info.recv_pack_count[PACK_RECV_CHKSUM_ERR]++;
			ret = PACK_RECV_CHKSUM_ERR;
			break;
		}

		// If the seqno is same as the last received, slave will resend the last package
		if (!flag_is_master) {
			// Check the seqno
			if (pack->seqno == slave_recv_seqno_last) {
				// Count the resend package that slave received
				pack_count_info.recv_pack_count[PACK_RECV_RETRY]++;
				// Resend the last package
				resend_pack();
				ret = PACK_RECV_RETRY;
				break;
			}
		}

		// Count the new package received
		pack_count_info.recv_pack_count[PACK_RECV_NEW]++;

		if (flag_is_master) {
			// Ack package has received, clear the mark that master is waiting for ack
			flag_master_need_ack = false;
			// Set the resend times of master to zero
			master_retry_times = 0;
		} else {
			// Slave record the last seqno that received
			slave_recv_seqno_last = pack->seqno;
		}
	} while (0);

	return ret;
}

// When ack timeout, master will resend the last package and return the resend times
U16 master_check_ack_delay(void)
{
	// Check if master is waiting for ack
	if (flag_master_need_ack) {
		// Check if ack timeout
		if ((LOCAL_TIME() - master_send_time_last) > master_max_ack_delay) {
			// Increment the resend times of master by 1
			master_retry_times++;
			// Resend the last sent package
			resend_pack();
		}
	}

	return master_retry_times;
}

// Get statistics for sent and received package
struct pack_count* get_pack_count_info(void)
{
	return &pack_count_info;
}
