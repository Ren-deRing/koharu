.code16

.text
.global _start

_start:
                 # initialize segments
	xor %ax, %ax # ax is 0x00
	mov %ax, %ds # data segment
	mov %ax, %ss # stack segment
    mov %ax, %es

    mov $0x7bf0, %sp # stack pointer
                     # stack grows downwards, so it is a 16B aligned address below 0x7c00.

    mov %dl, drive # save him!

    mov $hello, %si # what to print
    call _print
    call _print.newline

_load:
    mov $0x00, %ax
    mov %ax, %es     # segment is 0..
    mov $0x8000, %bx # and pointer is 8000.

    mov $0x02, %ah  # read sectors
    mov $0x20, %al  # how many?
    mov $0x00, %ch  # where?   (cylinder)
    mov $0x02, %cl  # where??  (sector)
    mov $0x00, %dh  # where??? (head)
    mov (drive), %dl # drive

    int $0x13 # read!

    jc _error.disk # if carry flag set (error), jump to error

_jmp.stage2:
    ljmp $0, $0x8000 # see ya! (ljmp instead of jmp, because ljmp resets CS)

_error.disk:
    mov $disk_error, %si # what to print (disk error msg)
    call _print
    call _print.newline

    jmp . # hlt (panic)

_print:
    pusha # save all registers

    cld            # clear direction flag
    mov $0x0E, %ah # teletype output mode
    mov $0x00, %bh # page number = 0
    mov $0x0F, %bl # black bg, white text

_print.loop:
    lodsb           # load one byte of characters into al
    cmp $0, %al     # if text ends
    je _print.exit  # exit loop
    int $0x10       # print!
    jmp _print.loop # next character

_print.exit:
    popa # restore all registers
    ret

_print.newline:
    pusha

    mov $0x0E, %ah
    mov $0x0D, %al # CR (Carriage Return, cursor reset)
    int $0x10

    mov $0x0E, %ah
    mov $0x0A, %al # LF (Line Feed, newline)
    int $0x10

    popa
    ret

drive: # dl's safe house
    .byte 0

hello:
    .ascii "shimoe koharu" # kawaii
    .byte 0

disk_error:
    .ascii "disk read error!"
    .byte 0

.org 500         # fill 0 until addr reaches 500
_shared_print:   # absolute addr: 0x7c00 + 500
    jmp _print
_shared_newline: # + 3
    jmp _print.newline

.fill 510 - (. - _start), 1, 0 # (510) - (curr addr) - (start addr) = how much of the 510 Bytes remains
                               # or .org 510
.word 0xAA55                   # 2 Bytes for this