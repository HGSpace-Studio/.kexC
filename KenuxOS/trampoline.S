; trampoline.S - 32-bit Multiboot trampoline for 64-bit Kenux Kernel
; nasm -f bin trampoline.S -o trampoline.bin
; QEMU: qemu-system-x86_64 -kernel trampoline.bin -device loader,file=kernel.elf

MB_MAGIC    equ 0x1BADB002
MB_FLAGS    equ 0x00010000          ; AOUT_KLUDGE: use header fields for loading
MB_CHECKSUM equ -(MB_MAGIC + MB_FLAGS)

LOAD_ADDR   equ 0x100000            ; QEMU loads Multiboot kernel at 1MB
STACK_TOP   equ 0x90000             ; Stack grows down from 576KB (safe below 1MB)
KERNEL_ENTRY equ 0x20000C           ; Skip 12-byte Multiboot header (identity mapped at 0x200000)

org LOAD_ADDR

bits 32

; ---- Multiboot header (AOUT_KLUDGE format, 32 bytes) ----
; Must be within first 8192 bytes, 32-bit aligned
align 4
mb_header:
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECKSUM
    dd mb_header            ; header_addr: address of this header
    dd LOAD_ADDR            ; load_addr: start loading file here
    dd file_end             ; load_end_addr: stop loading here
    dd file_end             ; bss_end_addr: no separate BSS
    dd start                ; entry_addr: jump here after loading

start:
    mov esp, STACK_TOP
    cli

    ; ==== Debug: output 'T' to COM1 ====
    mov dx, 0x3F8
    mov al, 'T'
    out dx, al

    ; ==== Page table setup ====
    ; PML4 at 0x80000 (4KB = 512 entries * 8 bytes)
    mov edi, 0x80000
    xor eax, eax
    mov ecx, 1024          ; 1024 dwords = 4096 bytes
    rep stosd

    ; PML4[0] -> PDPT_id at 0x81000 (identity map low 512GB)
    mov dword [0x80000], 0x81000 | 3    ; present + writable

    ; PML4[511] -> PDPT_hi at 0x82000 (high-half map)
    mov dword [0x80000 + 511*8], 0x82000 | 3

    ; PDPT_id[0] -> PD_id at 0x83000 (identity map first 1GB)
    mov edi, 0x81000
    xor eax, eax
    mov ecx, 1024
    rep stosd
    mov dword [0x81000], 0x83000 | 3

    ; PD[0..511] -> 512 x 2MB huge pages (identity map 0-1GB)
    mov edi, 0x83000
    xor eax, eax
    mov ecx, 1024
    rep stosd
    mov edi, 0x83000
    mov eax, 0x83          ; phys 0x0 | present(1) + writable(2) + PS(0x80)
    mov ecx, 512
.set_pd0:
    mov [edi], eax
    add eax, 0x200000      ; next 2MB physical address
    add edi, 8             ; next PD entry (8 bytes)
    loop .set_pd0

    ; PD_3 @ 0x84000 -> 512 x 2MB huge pages (identity map 3GB-4GB for MMIO)
    mov edi, 0x84000
    xor eax, eax
    mov ecx, 1024
    rep stosd
    mov edi, 0x84000
    mov eax, 0xC0000000 | 0x83   ; phys 0xC0000000 | P+W+PS
    mov ecx, 512
.set_pd3:
    mov [edi], eax
    add eax, 0x200000
    add edi, 8
    loop .set_pd3
    mov dword [0x81000 + 3*8], 0x84000 | 3   ; PDPT_id[3] -> PD_3

    ; PDPT_hi[510] -> PD_hi = 0x83000 (reuse same PD for high-half)
    ; PML4[511] covers 0xFFFFFF8000000000-0xFFFFFFFFFFFFFFFF
    ; PDPT[510] covers 0xFFFFFFFF80000000-0xFFFFFFFFBFFFFFFF
    mov edi, 0x82000
    xor eax, eax
    mov ecx, 1024
    rep stosd
    mov dword [0x82000 + 510*8], 0x83000 | 3

    ; ==== Enable PAE and SSE ====
    mov eax, cr4
    or eax, (1 << 5) | (1 << 9) | (1 << 10)  ; PAE | OSFXSR | OSXMMEXCPT
    mov cr4, eax

    ; ==== Clear CR0.EM (bit 2) and set CR0.MP (bit 1) for SSE/x87 ====
    mov eax, cr0
    and eax, ~(1 << 2)       ; clear EM
    or eax, (1 << 1)         ; set MP
    mov cr0, eax

    ; ==== Load CR3 (PML4 base) ====
    mov eax, 0x80000
    mov cr3, eax

    ; ==== Set LME in EFER MSR ====
    mov ecx, 0xC0000080      ; EFER MSR number
    rdmsr
    or eax, 1 << 8           ; LME bit (EFER.LME)
    wrmsr

    ; ==== Enable paging (CR0.PG) ====
    mov eax, cr0
    or eax, 1 << 31          ; PG bit
    mov cr0, eax

    ; ==== Load 64-bit GDT ====
    lgdt [gdt64_pointer]

    ; ==== Far jump to 64-bit code segment (selector 0x08) ====
    jmp 0x08:long_mode_start

; ==== 64-bit GDT ====
align 8
gdt64:
    dq 0                                        ; null descriptor (index 0)
    dq (1<<43) | (1<<44) | (1<<47) | (1<<53)    ; 64-bit code segment (index 1, selector 0x08)
gdt64_pointer:
    dw $ - gdt64 - 1                             ; GDT limit
    dd gdt64                                     ; GDT base (32-bit, zero-extended in 64-bit)

; ==== 64-bit entry point ====
bits 64
long_mode_start:
    ; ==== Set up stack first (needed for call instructions) ====
    mov rsp, STACK_TOP

    ; ==== Debug: output 'L' to COM1 ====
    mov dx, 0x3F8
    mov al, 'L'
    out dx, al

    ; ==== Verify kernel is loaded at physical 0x200000 ====
    ; First byte should be 0x02 (Multiboot magic 0x1BADB002 in little-endian)
    mov rsi, 0x200000
    mov al, [rsi]
    cmp al, 0x02
    jne .no_kernel
    mov al, 'K'
    out dx, al
    jmp .do_jump
.no_kernel:
    ; Output 'X' then the actual byte value as 2 hex digits
    mov al, 'X'
    out dx, al
    mov al, [rsi]        ; re-read byte
    mov cl, al
    shr al, 4            ; high nibble
    add al, '0'
    cmp al, '9'
    jbe .hi_ok
    add al, 7            ; 'A' - '9' - 1 = 7
.hi_ok:
    out dx, al
    mov al, cl
    and al, 0x0F         ; low nibble
    add al, '0'
    cmp al, '9'
    jbe .lo_ok
    add al, 7
.lo_ok:
    out dx, al
    hlt
    jmp $                ; halt forever

.do_jump:
    mov rax, KERNEL_ENTRY
    jmp rax

file_end:
