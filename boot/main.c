#include <stdint.h>

void loader_entry() {
    volatile uint16_t* video_mem = (volatile uint16_t *)0xB8000; // VGA Text Buffer
    uint16_t color = 0x0F; // black bg & white text

    char* msg = "koharu is kami-sama";

    for (int i = 0; msg[i] != '\0'; i++) { // loop until reach null terminator
        video_mem[i] = (color << 8) | (uint16_t)msg[i]; // VGA Text: (BG / TEXT / ASCII), so shifts. (0x000F -> 0x0F00)
    }

    for (;;);
}