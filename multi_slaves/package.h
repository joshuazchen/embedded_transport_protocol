/* ==========================================================================
 * package.c: Embedded Transport Protocol (Single master with Multiple Salves)
 *
 * function:  1. For single master with multiple slaves, half-duplex. The master
 *               ask initiatively, and the slaves ack passively.
 *            2. Can ensure data integrity by checksum algorithm.
 *            3. The master can resend automatically, with a feedback of
 *               resend times.
 *            4. The shared data buffer can save space and time.
 *            5. Caches sent data for resend.
 *            6. Support variable-length data part.
 *            7. Application can define their own data structure.
 *            8. Statistics for every sent and received package.
 * ======================================================================== */

#ifndef _PACKAGE_H
#define _PACKAGE_H

#include <time.h>
#include <stdbool.h>

// Define CPU type
#define X86
//#define AVR

// Definitions of basic type
#if defined X86
	// PC computer
	typedef unsigned char  U8;
	typedef unsigned short U16;
	typedef unsigned int   U32;
#elif defined AVR
	// AVR MCU
	typedef unsigned char U8;
	typedef unsigned int  U16;
	typedef unsigned long U32;
#endif

// Function type of callback function for sending bytes
typedef void (*send_bytes_func)(U8* buf, U16 count);

// Get local time
#define LOCAL_TIME() (clock())

// Maximum buffer size
#define MAX_BUF_SIZE 100
// Maxinum size of data part
#define MAX_DATA_LEN (MAX_BUF_SIZE - sizeof(struct pack_header))

// Premble
#define PACK_PREMBLE '-'
// Start code
#define PACK_START   '>'

// The length for checksum computing before 'data' in struct pack_header
#define CHECKSUM_HEAD_LEN 6

// Package header
struct pack_header {
	U8 premble[3]; // Premble
	U8 start;      // Start code
	U16 chksum;    // Checksum that computed from 'dest' to the tail of 'data'
	U8 dest;       // destination address
	U8 src;        // source address
	U16 seqno;     // Sequence number
	U16 len;       // Length of data part
	U8 data[];     // Data part
};

// Type of sent package
enum pack_send_type_list {
	PACK_SEND_NEW,        // New package
	PACK_SEND_RETRY,      // Resending package

	PACK_SEND_TYPE_TOTAL, // Total type of sent package
};

// Type of received package
enum pack_recv_type_list {
	PACK_RECV_NEW,         // New package
	PACK_RECV_RETRY,       // Resending package
	PACK_RECV_PREMBLE_ERR, // Package with wrong premble
	PACK_RECV_START_ERR,   // Package with wrong start code
	PACK_RECV_DEST_ERR,    // Package with wrong destination address
	PACK_RECV_SRC_ERR,     // Package with wrong source address
	PACK_RECV_SEQNO_ERR,   // Package with wrong seqno
	PACK_RECV_LEN_ERR,     // Package with wrong data length
	PACK_RECV_CHKSUM_ERR,  // Package with wrong checksum

	PACK_RECV_TYPE_TOTAL,  // Total type of received package
};

// Statistics for sent and received packages
struct pack_count {
	U32 send_pack_count[PACK_SEND_TYPE_TOTAL]; // Statistics for sent packages
	U32 recv_pack_count[PACK_RECV_TYPE_TOTAL]; // Statistics for received packages
};

// ============================ Global Variables ============================
// Receiving buffer for lower layer to store received data
extern U8 recv_buf[MAX_BUF_SIZE];
// Sending data address for application to store its sending data
extern void* send_data;
// Receiving data address for application to read its receiving data
extern const void* recv_data;

// =========================== Interface Functions ==========================
// Master initialize protocol
void master_init_pack(U8 my_addr, U32 max_ack_delay, send_bytes_func func);
// Slave initialize protocol
void slave_init_pack(U8 my_addr, U8 master_add, send_bytes_func func);
// Master send package
void master_send_pack(U8 dest_addr, U16 data_len);
// Slave send package
void slave_send_pack(U16 data_len);
// Check validity of the received package
enum pack_recv_type_list check_pack(void);
// When ack timeout, master will resend the last package and return the resend times
U16 master_check_ack_delay(void);
// Get the last slave address that master sent package
U8 get_master_send_addr_last(void);
// Get statistics for sent and received package
struct pack_count* get_pack_count_info(void);


#endif
