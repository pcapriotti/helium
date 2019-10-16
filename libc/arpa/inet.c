#include "arpa/inet.h"

uint32_t htonl(uint32_t x) { return ntohl(x); }
uint16_t htons(uint16_t x) { return ntohs(x); }
uint32_t ntohl(uint32_t x) { return __builtin_bswap32(x); }
uint16_t ntohs(uint16_t x) { return __builtin_bswap16(x); }
