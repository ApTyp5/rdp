
#ifndef RDP_DGRAM_H
#define RDP_DGRAM_H

#define CHUNK_SIZE      1300
struct dgram {
	uint32_t packet_num;
	uint32_t message_size;
	char chunk[CHUNK_SIZE];
};


#endif //RDP_DGRAM_H
