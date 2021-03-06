#include <kernel/mm/virtualPageManager.h>
#include <kernel/sched/hpet.h>
#include <kernel/acpi/madt.h>
#include <kernel/int/apic.h>
#include <lib/asmUtils.h>
#include <lib/output.h>

namespace apic {

uint32_t lapicRead(uint16_t offset) {
    return *(volatile uint32_t*)(madtInfo.lapicAddr + HIGH_VMA + offset);
}

void lapicWrite(uint16_t offset, uint32_t data) {
    *(volatile uint32_t*)(madtInfo.lapicAddr + HIGH_VMA + offset) = data;
}

void sendIPI(uint8_t ap, uint32_t ipi) {
    lapicWrite(LAPIC_ICRH, (ap << 24));
    lapicWrite(LAPIC_ICRL, ipi);
}

uint32_t ioapicRead(uint64_t base, uint32_t reg) {
    *(volatile uint32_t*)(base + 16 + HIGH_VMA) = reg;
    return *(volatile uint32_t*)(base + 18 + HIGH_VMA);
}

void ioapicWrite(uint64_t base, uint32_t reg, uint32_t data) {
    *(volatile uint32_t*)(base + HIGH_VMA) = reg;
    *(volatile uint32_t*)(base + 16 + HIGH_VMA) = data;
}

uint32_t getMaxGSIs(uint64_t ioapicBase) {
    uint32_t data = ioapicRead(ioapicBase, 1) >> 16; // Read register 1
    return data & ~(1 << 7);
}

uint64_t writeRedirectionTable(uint32_t gsi, uint64_t data, uint64_t ioapicIndex) {
    uint32_t reg = ((gsi - madtInfo.madtEntry1[ioapicIndex].gsiBase) * 2) + 16;
    ioapicWrite(madtInfo.madtEntry1[ioapicIndex].ioapicAddr, reg, (uint32_t)data);
    ioapicWrite(madtInfo.madtEntry1[ioapicIndex].ioapicAddr, reg + 1, (uint32_t)(data >> 32));
    return 1;
}

uint64_t readRedirectionTable(uint32_t gsi, uint64_t ioapicIndex) {
    uint32_t reg = ((gsi - madtInfo.madtEntry1[ioapicIndex].gsiBase) * 2) + 16;
    uint64_t data = (uint64_t)ioapicRead(madtInfo.madtEntry1[ioapicIndex].ioapicAddr, reg);
    return data | ((uint64_t)(ioapicRead(madtInfo.madtEntry1[ioapicIndex].ioapicAddr, reg + 1)) << 32);
}

void maskGSI(uint32_t gsi) {
    uint64_t redirectionTable = readRedirectionTable(gsi, 0);
    if(redirectionTable == ERROR) {
        cout + "[APIC]" << "Bad redirection table : unable to mask GSI " << gsi << "\n";
        return;
    }

    writeRedirectionTable(gsi, redirectionTable | (1 << 16), 0); // mask it
}

void unmaskGSI(uint32_t gsi) {
    uint64_t redirectionTable = readRedirectionTable(gsi, 0);
    if(redirectionTable == ERROR) {
        cout + "[APIC]" << "Bad redirection table : unable to mask GSI " << gsi << "\n";
        return;
    }

    writeRedirectionTable(gsi, redirectionTable & (1 << 16), 0); // unmask it
    writeRedirectionTable(gsi, gsi + 32, 0); // map it again
}

void lapicTimerInit(uint64_t ticksPerMS) { 
    ticksPerMS -= 10; // make up for ksleep 10

    lapicWrite(LAPIC_TIMER_DIVIDE_CONF, 0x3);
    ksleep(10);
    lapicWrite(LAPIC_TIMER_INITAL_COUNT, 0xffffffff);

    ksleep(ticksPerMS);

    uint32_t ticks = 0xffffffff - lapicRead(LAPIC_TIMER_CURRENT_COUNT);

    lapicWrite(LAPIC_TIMER_LVT, 32 | 0x20000);
    lapicWrite(LAPIC_TIMER_DIVIDE_CONF, 0x3);
    lapicWrite(LAPIC_TIMER_INITAL_COUNT, ticks);
}

void init() {
    // remap the pic
    outb(0x20, 0x11);
    outb(0xa0, 0x11);
    outb(0x21, 0x20);
    outb(0xa1, 0x28);
    outb(0x21, 0x4);
    outb(0xa1, 0x2);
    outb(0x21, 0x1);
    outb(0xa1, 0x1);
    outb(0x21, 0x0);
    outb(0xa1, 0x0);

    // disable the pic
    outb(0xa1, 0xff); 
    outb(0x21, 0xff); 

    cout + "[APIC]" << "Detected core count " << madtInfo.madtEntry0Count << "\n";

    for(uint64_t i = 0; i < madtInfo.madtEntry1Count; i++) {
        for(uint64_t j = madtInfo.madtEntry1[i].gsiBase; j < getMaxGSIs(madtInfo.madtEntry1[i].ioapicAddr); j++) { 
            maskGSI(j);
        }
    }

    for(int i = 0; i < 16; i++) 
        writeRedirectionTable(i, i + 32, 0);

    maskGSI(2);

    lapicWrite(LAPIC_SINT, lapicRead(LAPIC_SINT) | 0x1ff); // enavle spurious interrupts

    asm volatile("mov %0, %%cr8" :: "r"((uint64_t)0)); // set the TPR and also sti
}

}
