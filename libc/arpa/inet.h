#ifndef ARPA_INET_H
#define ARPA_INET_H

#include <stdint.h>

uint32_t htonl(uint32_t x);
uint16_t htons(uint16_t x);
uint32_t ntohl(uint32_t x);
uint16_t ntohs(uint16_t x);

#endif /* ARPA_INET_H */
