#include <port.h>
#include <shitio.h>
#include <interrupt.h>
#include <keyboard.h>
#include <paging.h>
#include <process.h>
#include <stdint.h>

extern void load_gdt(void) asm("load_gdt");

using namespace standardout;
using namespace MM;

extern "C" void kernel_main(void)
{
    load_gdt();
    initalize(VGA_BLUE, VGA_LIGHT_GREY);
    t_print("\nKernel Debug");
    k_print("Starting crepOS\n");
    page_setup();
    idt_init();
    page_frame_init(0xF42400); //Reserves ~ 16mb

    asm volatile("sti");

    /* multi-block allocation test */

    uint32_t *root_ptr = (uint32_t*)malloc(0x2001);
    free(root_ptr, 0x2001);

    start_counter(1, 0, 0x6);

    k_print("-------------------------------------------\n");

    k_print("> ");
    startInput();

    for(;;);
}
