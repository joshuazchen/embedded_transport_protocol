#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <windows.h>
#include "package.h"

// ========================= Test Program for Master ========================
// Warning value of master resend times
#define MASTER_MAX_RETRY_TIMES 2

// Define the file names for communication
#define FILE_FOR_SEND "master_send.txt"
#define FILE_FOR_RECV "master_recv.txt"

// If get a interrupt signal
bool get_signal_interrupt;

// Application's data structure
struct pack_data {
	U8 cmd;
	U8 cmd_data[];
};

// Callback function for sending bytes, simulated by write data to file
void send_bytes(U8* buf, U16 count)
{
	int i;
	FILE* fd = fopen(FILE_FOR_SEND, "w+");

	for (i = 0; i < count; i++) {
		fputc(buf[i], fd);
	}
	fclose(fd);
}

// Check if received a package, copy it to receiving buffer,
// simulated by read from file
bool pack_recv(void)
{
	bool ret = false;
	int c;
	U8* buf = recv_buf;
	FILE* fd = fopen(FILE_FOR_RECV, "r+");

	// Check if file opened successfully
	if (fd == NULL) {
		return false;
	}

	do {
		// See if a package is arrived by checking the premble
		if (fgetc(fd) != PACK_PREMBLE) {
			break;
		}
		if (fgetc(fd) != PACK_PREMBLE) {
			break;
		}
		if (fgetc(fd) != PACK_PREMBLE) {
			break;
		}
		if (fgetc(fd) != PACK_START) {
			break;
		}

		// Reset the pointer of the file stream for reading the whole data
		// For simplicity, read all data
		fseek(fd, 0, SEEK_SET);
		while ((c = fgetc(fd)) != EOF) {
			*buf++ = c;
		}
		// The file is only read by master, empty it after read
		fclose(fd);
		fd = fopen(FILE_FOR_RECV, "w");
		ret = true;
	} while (0);

	fclose(fd);

	return ret;
}

// Print statistics for sent and received package
void print_pack_count_info()
{
	struct pack_count* pack_count_info = get_pack_count_info();

	// Mark the flag that a interrupt signal is got
	get_signal_interrupt = true;

	printf("PACK_SEND_NEW:         %u\n", pack_count_info->send_pack_count[PACK_SEND_NEW]);
	printf("PACK_SEND_RETRY:       %u\n", pack_count_info->send_pack_count[PACK_SEND_RETRY]);
	putchar('\n');
	printf("PACK_RECV_NEW:         %u\n", pack_count_info->recv_pack_count[PACK_RECV_NEW]);
	printf("PACK_RECV_RETRY:       %u\n", pack_count_info->recv_pack_count[PACK_RECV_RETRY]);
	printf("PACK_RECV_PREMBLE_ERR: %u\n", pack_count_info->recv_pack_count[PACK_RECV_PREMBLE_ERR]);
	printf("PACK_RECV_START_ERR:   %u\n", pack_count_info->recv_pack_count[PACK_RECV_START_ERR]);
	printf("PACK_RECV_SEQNO_ERR:   %u\n", pack_count_info->recv_pack_count[PACK_RECV_SEQNO_ERR]);
	printf("PACK_RECV_LEN_ERR:     %u\n", pack_count_info->recv_pack_count[PACK_RECV_LEN_ERR]);
	printf("PACK_RECV_CHKSUM_ERR:  %u\n", pack_count_info->recv_pack_count[PACK_RECV_CHKSUM_ERR]);

	getchar();
}

// Test program
int main(void)
{
	// Mapping the sending data address with application's data structure
	struct pack_data* data_send = (struct pack_data*)send_data;
	struct pack_data* data_recv = (struct pack_data*)recv_data;
	enum pack_recv_type_list check_result;

	// Command data
	U8 data[] = {'F'};

	// Initialize the files for communication
	FILE* fd;
	fd = fopen(FILE_FOR_SEND, "w");
	fclose(fd);
	fd = fopen(FILE_FOR_RECV, "w");
	fclose(fd);

	// Initialize protocol
	init_pack(true, 6000, send_bytes);

	// Bind a callback function to handle the interrupt signal
	// Press key 'Ctrl + C' will send a interrupt signal to the program
	signal(SIGINT, print_pack_count_info);

	// Master send package first
	data_send->cmd = 'E';
	memcpy(data_send->cmd_data, data, sizeof(data));
	send_pack(sizeof(data)+sizeof(struct pack_data));

	// The loop will be broken when a interrupt signal received
	while (!get_signal_interrupt) {
		// When ack timeout, master will resend the last package and return
		// the resend times, if the warning value is reached, show messages
		if (master_check_ack_delay() > MASTER_MAX_RETRY_TIMES) {
			printf("The slave seems offline.\n");
		}

		// See if a package is arrived
		if (pack_recv()) {
			// Check validity of the received package
			check_result = check_pack();
			if (check_result == PACK_RECV_NEW) {
				// Print the package
				printf("<Master Recv> seqno: %d, len: %d, cmd: %c, data: %c\n",
				((struct pack_header*)recv_buf)->seqno,
				((struct pack_header*)recv_buf)->len,
				data_recv->cmd,
				data_recv->cmd_data[0]);
			}
			// Master send package to salve
			data_send->cmd = 'E';
			memcpy(data_send->cmd_data, data, sizeof(data));
			send_pack(sizeof(data)+sizeof(struct pack_data));
		}

		Sleep(2000);
	}

	return 0;
}
