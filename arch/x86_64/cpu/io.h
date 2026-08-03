#ifndef IO_H
#define IO_H

#include <stdint.h>

uint8_t Inb(uint16_t port);
void Outb(uint16_t port, uint8_t value);
uint16_t Inw(uint16_t Port);
void Outw(uint16_t Port, uint16_t Value);
uint32_t Ind(uint16_t Port);
void Outd(uint16_t Port, uint32_t Value);

#endif
