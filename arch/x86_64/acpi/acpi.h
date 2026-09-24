#ifndef NULLOS_ACPI_H
#define NULLOS_ACPI_H
#include <stdint.h>
#include <stdbool.h>
void AcpiInitialize(void);
bool AcpiReady(void);
uint64_t AcpiXsdt(void);
uint64_t AcpiFindTable(const char Sig[4]);
#endif
