#include <stddef.h>

void kernel_main() {
    volatile char *video_memory = (char *)0xb8000; // Adresse de la mémoire vidéo
    const char *message = "Hello, Kernel!";
    const char COLOR = 0x07;  // Blanc sur noir

    for (int i = 0; message[i] != '\0'; i++) {
        video_memory[i * 2] = message[i];       // Caractère
        video_memory[i * 2 + 1] = COLOR;       // Couleur
    }

    while (1);  // Boucle infinie pour garder le kernel actif
}
