#ifndef NULLOS_PCI_H
#define NULLOS_PCI_H
#include <stdint.h>
void PciInitialize(void);
uint32_t PciConfigRead32(uint8_t bus,uint8_t dev,uint8_t fn,uint8_t off);
void PciConfigWrite32(uint8_t bus,uint8_t dev,uint8_t fn,uint8_t off,uint32_t value);
void PciConfigWrite16(uint8_t bus,uint8_t dev,uint8_t fn,uint8_t off,uint16_t value);
uint16_t PciVendor(uint8_t bus,uint8_t dev,uint8_t fn);
#endif
