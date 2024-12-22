[BITS 16]
[ORG 0x7C00]               ; Adresse où le BIOS charge le bootloader

start:
    ; Afficher un message de démarrage
    mov si, boot_msg
print_char:
    lodsb                   ; Charger un caractère
    or al, al               ; Vérifier la fin de la chaîne (al = 0 ?)
    jz load_kernel          ; Si fin, passer au chargement du noyau
    mov ah, 0x0E            ; Fonction BIOS pour afficher un caractère
    int 0x10                ; Interruption BIOS pour afficher
    jmp print_char          ; Boucler pour le prochain caractère

load_kernel:
    ; Charger le noyau à l'adresse 0x1000:0x0000
    mov ax, 0x1000          ; Segment où le noyau sera chargé
    mov es, ax              ; Charger le segment dans ES
    xor bx, bx              ; Début du chargement dans ES:BX
    mov ah, 0x02            ; Fonction BIOS pour lire un secteur
    mov al, 5               ; Nombre de secteurs à lire
    mov ch, 0               ; Cylindre 0
    mov cl, 2               ; Secteur 2 (le noyau commence ici)
    mov dh, 0               ; Tête 0
    int 0x13                ; Appeler le BIOS pour lire
    jc error                ; Si erreur, afficher un message et boucler

    ; Désactiver les interruptions et passer en mode protégé
    cli
    lgdt [gdt_descriptor]   ; Charger la GDT
    mov eax, cr0
    or eax, 0x1             ; Activer le bit PE (Protection Enable)
    mov cr0, eax
    jmp 0x08:protected_mode ; Saut long vers le mode protégé

[BITS 32]
protected_mode:
    ; Initialiser les segments en mode protégé
    mov ax, 0x10            ; Segment de données
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x9C00         ; Initialiser la pile (exemple)

    ; Passer le contrôle au noyau
    jmp 0x1000:0x0000       ; Appeler le noyau chargé à 0x1000:0x0000

error:
    ; Message d'erreur en cas d'échec
    mov si, error_msg
print_error:
    lodsb
    or al, al
    jz halt                 ; Si fin, boucler indéfiniment
    mov ah, 0x0E
    int 0x10
    jmp print_error

halt:
    cli
    hlt                     ; Boucle infinie pour arrêter le CPU

; Données
boot_msg db "Booting Kernel...", 0
error_msg db "Disk read error!", 0

; GDT : Définir une table descripteur global
gdt:
    dq 0x0000000000000000   ; Descripteur NULL
    dq 0x00CF9A000000FFFF   ; Descripteur Code Segment (base=0, limite=4GB, exécutable)
    dq 0x00CF92000000FFFF   ; Descripteur Data Segment (base=0, limite=4GB)

gdt_descriptor:
    dw gdt_end - gdt - 1    ; Taille de la GDT
    dd gdt                  ; Adresse de la GDT
gdt_end:

; Remplir le secteur jusqu'à 512 octets
times 510-($-$$) db 0
dw 0xAA55                   ; Signature de boot
