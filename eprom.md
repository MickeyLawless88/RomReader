# EPROM Data Reader & Hex Editor v1.1
## Comprehensive Technical Documentation

**Developer:** Mickey W. Lawless  
**Date Completed:** December 21, 2025  
**Version:** 1.1  
**Compiler:** Turbo C/C++ 1.0 or later  
**Platform:** MS-DOS / DOSBox

---

## Table of Contents

1. [Overview](#overview)
2. [Features](#features)
3. [System Requirements](#system-requirements)
4. [Hardware Configuration](#hardware-configuration)
5. [Architecture & Design](#architecture--design)
6. [Core Components](#core-components)
7. [User Interface](#user-interface)
8. [Serial Communication](#serial-communication)
9. [Hex Editor Functions](#hex-editor-functions)
10. [Disassembler System](#disassembler-system)
11. [File Operations](#file-operations)
12. [Customization Guide](#customization-guide)
13. [Compilation Instructions](#compilation-instructions)
14. [Usage Guide](#usage-guide)
15. [Troubleshooting](#troubleshooting)

---

## Overview

The EPROM Data Reader is a professional-grade DOS application designed for reading, editing, and analyzing data from EPROM programmers via serial communication. The program provides a full-featured hex editor with integrated disassemblers for three classic microprocessor architectures: Motorola 6800, Intel 8080, and Intel 8086.

### Design Philosophy

This application was designed with several key principles:

- **Hardware Compatibility:** Works reliably on both real DOS hardware and DOSBox emulation
- **Professional Interface:** Text-based UI with color-coded information and intuitive navigation
- **Comprehensive Toolset:** Combines data acquisition, editing, and analysis in one program
- **Minimal Dependencies:** Uses only standard Turbo C libraries for maximum compatibility

---

## Features

### Data Acquisition
- **Serial Communication:** Reads ASCII space-separated hexadecimal data from COM1
- **Configurable Baud Rate:** Default 9600 baud with easy modification
- **Real-time Display:** Shows incoming data as it's received
- **Timeout Handling:** Automatic detection of transmission completion
- **Large Buffer:** 16KB capacity for typical EPROM sizes (2732, 2764, 27128, 27256)

### Hex Editor
- **Dual View:** Simultaneous hex and ASCII display
- **Visual Highlighting:** Reverse video cursor for easy navigation
- **Dual Edit Modes:** Edit in either hex or ASCII representation
- **Smart Navigation:** Arrow keys, Page Up/Down support
- **Status Bar:** Real-time display of cursor position, byte value, and settings
- **Context Help:** ALT key reveals command menu

### File Operations
- **Multiple Formats:** Binary, Intel HEX, and formatted listings
- **File Upload:** Send data back to EPROM programmer via COM1
- **Command-line Loading:** Specify file on startup for quick access
- **Printer Support:** Direct printing to LPT1

### Disassembly
- **Three CPU Types:** 6800, 8080, and 8086 architectures
- **Complete Instruction Sets:** All standard opcodes decoded
- **File Output:** Save disassembly to .ASM files
- **Formatted Listings:** Address, hex bytes, and mnemonics

---

## System Requirements

### Minimum Hardware
- IBM PC/XT/AT compatible computer
- 640KB RAM (typical DOS system)
- COM1 serial port
- CGA/EGA/VGA compatible display

### Software
- MS-DOS 3.0 or later
- DOSBox 0.74 or later (for emulation)

### Development
- Turbo C/C++ 1.0 or later
- Knowledge of C programming for customization

---

## Hardware Configuration

### COM1 Serial Port Settings

The program initializes COM1 with these parameters:

```c
/* COM1 Port Definitions */
#define COM1_DATA     0x3F8  /* Data register */
#define COM1_IER      0x3F9  /* Interrupt enable */
#define COM1_LCR      0x3FB  /* Line control */
#define COM1_LSR      0x3FD  /* Line status */
#define COM1_MCR      0x3FC  /* Modem control */
#define COM1_IIR      0x3FA  /* Interrupt ID/FIFO control */
```

**Default Configuration:**
- **Baud Rate:** 9600 (configurable via `InitCOM1()` parameter)
- **Data Bits:** 8
- **Parity:** None
- **Stop Bits:** 1
- **Flow Control:** RTS/DTR enabled

### Changing Baud Rate

To modify the baud rate, locate this line in `main()`:

```c
InitCOM1(9600);
```

Common baud rates: 300, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200

**Example:** For 19200 baud:
```c
InitCOM1(19200);
```

### COM Port Address Modification

To use COM2 instead of COM1, change these definitions:

```c
/* COM2 Port Definitions */
#define COM1_DATA     0x2F8  /* Was 0x3F8 */
#define COM1_IER      0x2F9  /* Was 0x3F9 */
#define COM1_LCR      0x2FB  /* Was 0x3FB */
#define COM1_LSR      0x2FD  /* Was 0x3FD */
#define COM1_MCR      0x2FC  /* Was 0x3FC */
#define COM1_IIR      0x2FA  /* Was 0x3FA */
```

---

## Architecture & Design

### Memory Model

The program uses a simple flat buffer architecture:

```c
/* Buffer Configuration */
#define MAX_BUFFER_SIZE 16384  /* 16KB buffer */
#define BYTES_PER_LINE 16      /* Display format */

/* Global Variables */
unsigned char DataBuffer[MAX_BUFFER_SIZE];
unsigned int BufferSize = 0;
unsigned int CursorPos = 0;
unsigned int TopLine = 0;
```

**Customizing Buffer Size:**

For larger EPROMs (27512 = 64KB), change:
```c
#define MAX_BUFFER_SIZE 65536  /* 64KB buffer */
```

Note: Turbo C's memory model may require compilation with large or huge model for buffers over 64KB.

### Display Layout

```
┌─────────────────────────────────────────────────────────────────────┐
│ EPROM Editor v1.1 - Mickey W. Lawless [HEX MODE]                   │ Line 1: Header
├─────────────────────────────────────────────────────────────────────┤
│ Offset   00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F  ASCII    │ Line 2: Column Headers
├─────────────────────────────────────────────────────────────────────┤
│ 0000:    48 65 6C 6C 6F 20 57 6F 72 6C 64 21 00 00 00 00  Hello World!.... │ Lines 4-24: Data
│ 0010:    54 68 69 73 20 69 73 20 61 20 74 65 73 74 00 00  This is a test.. │
│ ...                                                                  │
├─────────────────────────────────────────────────────────────────────┤
│ (Line 23: Command help - shown when ALT key held)                   │
├─────────────────────────────────────────────────────────────────────┤
│ Offset:0000 Value:48 (72) Area:HEX CPU:6800 Size:256               │ Line 24: Status Bar
└─────────────────────────────────────────────────────────────────────┘
```

**Customizing Display:**

Lines per screen can be adjusted:
```c
int maxLines = 21;  /* Increased from 20 to show more data */
```

Change to 18 for more compact display or 24 for maximum density.

---

## Core Components

### 1. Serial Communication System

#### InitCOM1() - Port Initialization

```c
void InitCOM1(unsigned int baud) {
    unsigned int divisor;
    
    /* Calculate divisor for baud rate */
    divisor = (unsigned int)(115200L / baud);
    
    /* Disable interrupts */
    outportb(COM1_IER, 0x00);
    
    /* Set DLAB to access divisor */
    outportb(COM1_LCR, 0x80);
    
    /* Set divisor (low/high byte) */
    outportb(COM1_DATA, divisor & 0xFF);
    outportb(COM1_IER, (divisor >> 8) & 0xFF);
    
    /* 8 bits, no parity, 1 stop bit */
    outportb(COM1_LCR, 0x03);
    
    /* Enable FIFO, clear both FIFOs, 1-byte trigger */
    outportb(COM1_IIR, 0x07);
    
    /* Enable DTR, RTS, OUT2 */
    outportb(COM1_MCR, 0x0B);
    
    /* Flush any pending data */
    while (COM1DataReady()) {
        ReadCOM1();
        delay(1);
    }
}
```

**Key Features:**
- Automatic baud rate divisor calculation
- FIFO buffer configuration for 16550A UARTs
- Hardware handshaking enabled
- Pre-flush to prevent stale data

**Customization Points:**

For 7-bit data with even parity:
```c
outportb(COM1_LCR, 0x1A);  /* 7E1: 7 bits, even parity, 1 stop */
```

For different FIFO trigger levels:
```c
outportb(COM1_IIR, 0x47);  /* 4-byte trigger instead of 1-byte */
```

#### ReadDataFromCOM1() - Data Reception

```c
void ReadDataFromCOM1(void) {
    char hexStr[3];
    int hexPos = 0;
    unsigned char ch;
    unsigned int value;
    long timeout = 0;
    
    hexStr[0] = 0;
    hexStr[1] = 0;
    hexStr[2] = 0;
    
    BufferSize = 0;
    
    while (BufferSize < MAX_BUFFER_SIZE) {
        if (kbhit()) {
            if (getch() == 27) {  /* ESC to stop */
                break;
            }
        }
        
        if (COM1DataReady()) {
            ch = ReadCOM1();
            timeout = 0;
            
            /* Parse hex digit */
            if ((ch >= '0' && ch <= '9') || 
                (ch >= 'A' && ch <= 'F') || 
                (ch >= 'a' && ch <= 'f')) {
                hexStr[hexPos++] = ch;
                
                if (hexPos == 2) {
                    hexStr[2] = '\0';
                    sscanf(hexStr, "%x", &value);
                    DataBuffer[BufferSize++] = (unsigned char)value;
                    hexPos = 0;
                    
                    /* Display and small delay */
                    printf("%02X ", (unsigned char)value);
                    delay(2);
                }
            }
        } else {
            timeout++;
            
            /* Timeout check with delays */
            if (timeout % 500 == 0) {
                delay(5);
            }
            
            /* Exit after 0.5 seconds of no data */
            if (timeout > 100000L && BufferSize > 0) {
                break;
            }
        }
    }
}
```

**Data Format Expected:**
```
48 65 6C 6C 6F 20 57 6F 72 6C 64
```
(Space-separated hexadecimal bytes)

**Customization: Binary Data Reception**

To receive raw binary instead of ASCII hex:

```c
void ReadBinaryFromCOM1(void) {
    unsigned char ch;
    long timeout = 0;
    
    BufferSize = 0;
    
    while (BufferSize < MAX_BUFFER_SIZE) {
        if (kbhit() && getch() == 27) break;
        
        if (COM1DataReady()) {
            ch = ReadCOM1();
            DataBuffer[BufferSize++] = ch;
            timeout = 0;
            
            /* Display progress */
            if ((BufferSize % 16) == 0) {
                printf(".");
            }
        } else {
            timeout++;
            if (timeout > 100000L && BufferSize > 0) break;
        }
    }
}
```

### 2. Hex Editor Core

#### DisplayHexEditor() - Screen Rendering

```c
void DisplayHexEditor(void) {
    int row, col;
    unsigned int addr;
    unsigned char byte;
    int maxLines = 21;
    
    /* Hide cursor during refresh */
    _setcursortype(_NOCURSOR);
    
    /* Draw header */
    gotoxy(1, 1);
    textcolor(WHITE);
    textbackground(BLUE);
    cprintf(" EPROM Editor v1.1 - Mickey W. Lawless [%s MODE]%s", 
            (CursorArea == AREA_HEX) ? "HEX" : "ASCII",
            Modified ? " *MOD*" : "     ");
    
    /* Draw data rows */
    for (row = 0; row < maxLines; row++) {
        addr = (TopLine + row) * BYTES_PER_LINE;
        
        if (addr >= BufferSize) break;
        
        gotoxy(1, 4 + row);
        cprintf("%04X:    ", addr);
        
        /* Hex bytes with highlighting */
        for (col = 0; col < BYTES_PER_LINE && addr + col < BufferSize; col++) {
            byte = DataBuffer[addr + col];
            
            /* Highlight cursor position */
            if (addr + col == CursorPos) {
                textcolor(BLACK);
                textbackground(LIGHTCYAN);
            }
            
            cprintf("%c%c ", NibbleToHex(byte >> 4), NibbleToHex(byte & 0x0F));
            
            if (addr + col == CursorPos) {
                textcolor(LIGHTGRAY);
                textbackground(BLACK);
            }
        }
        
        /* ASCII representation */
        cprintf(" ");
        for (col = 0; col < BYTES_PER_LINE && addr + col < BufferSize; col++) {
            byte = DataBuffer[addr + col];
            
            if (addr + col == CursorPos) {
                textcolor(BLACK);
                textbackground(LIGHTCYAN);
            }
            
            cprintf("%c", (byte >= 32 && byte < 127) ? byte : '.');
            
            if (addr + col == CursorPos) {
                textcolor(LIGHTGRAY);
                textbackground(BLACK);
            }
        }
    }
    
    /* Status bar */
    gotoxy(1, 24);
    textcolor(WHITE);
    textbackground(BLUE);
    cprintf(" Offset:%04X Value:%02X (%d) Area:%s CPU:%s Size:%d",
            CursorPos,
            DataBuffer[CursorPos],
            DataBuffer[CursorPos],
            (CursorArea == AREA_HEX) ? "HEX" : "ASCII",
            (CPUType == CPU_6800) ? "6800" : ((CPUType == CPU_8080) ? "8080" : "8086"),
            BufferSize);
    
    _setcursortype(_NORMALCURSOR);
}
```

**Color Customization:**

Change highlight colors:
```c
/* Current: Black on cyan */
textcolor(BLACK);
textbackground(LIGHTCYAN);

/* Alternative: White on blue */
textcolor(WHITE);
textbackground(BLUE);

/* Alternative: Yellow on red */
textcolor(YELLOW);
textbackground(RED);
```

Available colors: BLACK, BLUE, GREEN, CYAN, RED, MAGENTA, BROWN, LIGHTGRAY, DARKGRAY, LIGHTBLUE, LIGHTGREEN, LIGHTCYAN, LIGHTRED, LIGHTMAGENTA, YELLOW, WHITE

#### Navigation System

```c
/* Arrow key handling in HexEditor() */
case 72:  /* Up arrow */
    if (CursorPos >= BYTES_PER_LINE) {
        CursorPos -= BYTES_PER_LINE;
        if (CursorPos < TopLine * BYTES_PER_LINE) {
            TopLine--;
        }
        DisplayHexEditor();
    }
    break;

case 80:  /* Down arrow */
    if (CursorPos + BYTES_PER_LINE < BufferSize) {
        CursorPos += BYTES_PER_LINE;
        if (CursorPos >= (TopLine + maxLines) * BYTES_PER_LINE) {
            TopLine++;
        }
        CursorArea = AREA_HEX;  /* Return to HEX area */
        DisplayHexEditor();
    }
    break;
```

**Navigation Behavior:**
- **Up/Down:** Move by full lines (16 bytes)
- **Left/Right:** Move within hex area; right arrow switches to ASCII at end of line
- **Page Up/Down:** Jump by screen (21 lines)
- **Scrolling:** Automatic when cursor moves off screen

**Customizing Navigation:**

Add Home/End key support:
```c
case 71:  /* Home key */
    CursorPos = 0;
    TopLine = 0;
    CursorArea = AREA_HEX;
    DisplayHexEditor();
    break;

case 79:  /* End key */
    CursorPos = BufferSize - 1;
    TopLine = (CursorPos / BYTES_PER_LINE) - maxLines + 1;
    if (TopLine < 0) TopLine = 0;
    CursorArea = AREA_HEX;
    DisplayHexEditor();
    break;
```

### 3. Edit Functions

#### EditByteHex() - Hexadecimal Editing

```c
void EditByteHex(void) {
    char hexStr[3];
    int hexPos = 0;
    unsigned int value;
    int ch;
    
    hexStr[0] = 0;
    hexStr[1] = 0;
    hexStr[2] = 0;
    
    gotoxy(1, 23);
    textcolor(YELLOW);
    textbackground(BLACK);
    clreol();
    printf("HEX EDIT - Enter 2 hex digits (or ESC): ");
    
    while (hexPos < 2) {
        ch = getch();
        
        if (ch == 27) return;  /* ESC cancels */
        
        if ((ch >= '0' && ch <= '9') || 
            (ch >= 'A' && ch <= 'F') || 
            (ch >= 'a' && ch <= 'f')) {
            hexStr[hexPos++] = ch;
            putchar(ch);
        }
    }
    
    hexStr[2] = '\0';
    sscanf(hexStr, "%x", &value);
    DataBuffer[CursorPos] = (unsigned char)value;
    Modified = 1;
    
    /* Auto-advance to next byte */
    if (CursorPos < BufferSize - 1) {
        CursorPos++;
    }
}
```

**Features:**
- Accepts hex digits 0-9, A-F (case insensitive)
- ESC cancels without changes
- Auto-advances after edit
- Sets Modified flag for save prompt

**Customization: Decimal Input Mode**

Add decimal editing option:
```c
void EditByteDecimal(void) {
    char decStr[4];
    int decPos = 0;
    unsigned int value;
    int ch;
    
    gotoxy(1, 23);
    printf("DEC EDIT - Enter decimal value 0-255 (or ESC): ");
    
    decStr[0] = 0;
    while (decPos < 3) {
        ch = getch();
        
        if (ch == 27) return;
        if (ch == 13) break;  /* ENTER confirms */
        
        if (ch >= '0' && ch <= '9') {
            decStr[decPos++] = ch;
            putchar(ch);
        }
        if (ch == 8 && decPos > 0) {  /* Backspace */
            decPos--;
            printf("\b \b");
        }
    }
    
    decStr[decPos] = '\0';
    value = atoi(decStr);
    
    if (value <= 255) {
        DataBuffer[CursorPos] = (unsigned char)value;
        Modified = 1;
        if (CursorPos < BufferSize - 1) CursorPos++;
    }
}
```

---

## Disassembler System

### Architecture Overview

The disassembler system supports three CPU architectures:

```c
/* CPU type constants */
#define CPU_6800  1
#define CPU_8080  2
#define CPU_8086  3

/* Global CPU selection */
int CPUType = CPU_6800;
```

### Motorola 6800 Disassembler

#### Sample Instructions

```c
int Disasm6800(unsigned int addr, char *output) {
    unsigned char opcode, operand1, operand2;
    int len = 1;
    
    opcode = DataBuffer[addr];
    operand1 = (addr + 1 < BufferSize) ? DataBuffer[addr+1] : 0;
    operand2 = (addr + 2 < BufferSize) ? DataBuffer[addr+2] : 0;
    
    switch (opcode) {
        /* Inherent addressing - no operands */
        case 0x01: sprintf(output, "NOP"); break;
        case 0x08: sprintf(output, "INX"); break;
        case 0x09: sprintf(output, "DEX"); break;
        
        /* Immediate addressing - 2 bytes */
        case 0x86: 
            sprintf(output, "LDAA #$%02X", operand1); 
            len=2; 
            break;
        case 0xC6: 
            sprintf(output, "LDAB #$%02X", operand1); 
            len=2; 
            break;
        
        /* Extended addressing - 3 bytes */
        case 0xB6: 
            sprintf(output, "LDAA $%04X", (operand1 << 8) | operand2); 
            len=3; 
            break;
        
        /* Relative branches - 2 bytes */
        case 0x20: 
            sprintf(output, "BRA $%04X", 
                    (addr + 2 + (signed char)operand1) & 0xFFFF); 
            len=2; 
            break;
        case 0x27: 
            sprintf(output, "BEQ $%04X", 
                    (addr + 2 + (signed char)operand1) & 0xFFFF); 
            len=2; 
            break;
        
        /* Indexed addressing */
        case 0xA6: 
            sprintf(output, "LDAA %u,X", operand1); 
            len=2; 
            break;
        
        default: 
            sprintf(output, "FCB $%02X", opcode); 
            break;
    }
    
    return len;
}
```

**Adding Custom Mnemonics:**

For undocumented or custom opcodes:
```c
case 0x12: 
    sprintf(output, "CUSTOM_OP1 $%02X", operand1); 
    len=2; 
    break;
```

### Intel 8080 Disassembler

#### Key Features

```c
int Disasm8080(unsigned int addr, char *output) {
    unsigned char opcode, byte1, byte2;
    int len = 1;
    
    opcode = DataBuffer[addr];
    byte1 = (addr + 1 < BufferSize) ? DataBuffer[addr+1] : 0;
    byte2 = (addr + 2 < BufferSize) ? DataBuffer[addr+2] : 0;
    
    switch (opcode) {
        /* MOV register to register (64 opcodes) */
        case 0x40: sprintf(output, "MOV B,B"); break;
        case 0x41: sprintf(output, "MOV B,C"); break;
        /* ... all 64 combinations ... */
        case 0x7F: sprintf(output, "MOV A,A"); break;
        
        /* MVI - Move immediate */
        case 0x06: sprintf(output, "MVI B,%02XH", byte1); len=2; break;
        case 0x0E: sprintf(output, "MVI C,%02XH", byte1); len=2; break;
        
        /* LXI - Load register pair immediate */
        case 0x01: 
            sprintf(output, "LXI B,%04XH", byte1 | (byte2 << 8)); 
            len=3; 
            break;
        
        /* Arithmetic */
        case 0x80: sprintf(output, "ADD B"); break;
        case 0x88: sprintf(output, "ADC B"); break;
        case 0x90: sprintf(output, "SUB B"); break;
        
        /* Jump and Call */
        case 0xC3: 
            sprintf(output, "JMP %04XH", byte1 | (byte2 << 8)); 
            len=3; 
            break;
        case 0xCD: 
            sprintf(output, "CALL %04XH", byte1 | (byte2 << 8)); 
            len=3; 
            break;
        
        default: 
            sprintf(output, "DB %02XH", opcode); 
            break;
    }
    
    return len;
}
```

**8080 Addressing Modes:**
1. **Implied:** `NOP`, `HLT`
2. **Register:** `MOV A,B`
3. **Immediate:** `MVI A,05H`
4. **Direct:** `LDA 1234H`
5. **Register Indirect:** `LDAX B`

### Intel 8086 Disassembler

#### Complex Instruction Decoding

```c
int Disasm8086(unsigned int addr, char *output) {
    unsigned char opcode, byte1, byte2;
    int len = 1;
    signed short offset;
    
    opcode = DataBuffer[addr];
    byte1 = (addr + 1 < BufferSize) ? DataBuffer[addr+1] : 0;
    byte2 = (addr + 2 < BufferSize) ? DataBuffer[addr+2] : 0;
    
    switch (opcode) {
        /* Single-byte instructions */
        case 0x90: sprintf(output, "NOP"); break;
        case 0xF4: sprintf(output, "HLT"); break;
        
        /* Register moves with immediate */
        case 0xB0: sprintf(output, "MOV AL,%02Xh", byte1); len=2; break;
        case 0xB8: 
            sprintf(output, "MOV AX,%04Xh", byte1 | (byte2 << 8)); 
            len=3; 
            break;
        
        /* Conditional jumps with relative addressing */
        case 0x74: 
            sprintf(output, "JZ %04Xh", 
                    (addr + 2 + (signed char)byte1) & 0xFFFF); 
            len=2; 
            break;
        
        /* Near call with relative offset */
        case 0xE8: 
            offset = (short)(byte1 | (byte2 << 8));
            sprintf(output, "CALL %04Xh", 
                    (addr + 3 + offset) & 0xFFFF); 
            len=3; 
            break;
        
        /* Interrupt */
        case 0xCD: 
            sprintf(output, "INT %02Xh", byte1); 
            len=2; 
            break;
        
        default: 
            sprintf(output, "DB %02Xh", opcode); 
            break;
    }
    
    return len;
}
```

**Note on ModR/M Decoding:**

For full 8086 support, ModR/M byte decoding would expand the disassembler significantly:

```c
/* Example ModR/M decoder (not included in current version) */
void DecodeModRM(unsigned char modrm, char *output) {
    int mod = (modrm >> 6) & 0x03;
    int reg = (modrm >> 3) & 0x07;
    int rm = modrm & 0x07;
    
    /* Decode based on mod and rm fields */
    /* This adds significant complexity */
}
```

### Disassembly Output

#### File Format

```
; Disassembly Output
; CPU Type: 6800
; Start Address: 0000
; Developer: Mickey W. Lawless
; Generated: Dec 21 2025
;
; ========================================================================

0000: LDAA #$00
0002: STAA $0100
0005: INX
0006: BRA $0000
```

---

## File Operations

### Save Formats

#### 1. Binary Format (Raw)

```c
void SaveBufferToFile(void) {
    char filename[80];
    FILE *fp;
    
    /* Get filename from user */
    /* ... input handling ... */
    
    fp = fopen(filename, "wb");
    if (fp == NULL) {
        printf("ERROR: Cannot create file!");
        return;
    }
    
    fwrite(DataBuffer, 1, BufferSize, fp);
    fclose(fp);
    
    Modified = 0;  /* Clear modified flag */
}
```

#### 2. Intel HEX Format

```c
void SaveBufferAsHex(void) {
    FILE *fp;
    unsigned int addr, checksum, bytesInLine;
    
    /* ... filename input ... */
    
    fp = fopen(filename, "w");
    
    addr = 0;
    while (addr < BufferSize) {
        bytesInLine = (BufferSize - addr >= 16) ? 16 : (BufferSize - addr);
        checksum = bytesInLine + ((addr >> 8) & 0xFF) + (addr & 0xFF);
        
        fprintf(fp, ":%02X%04X00", bytesInLine, addr);
        
        for (i = 0; i < bytesInLine; i++) {
            fprintf(fp, "%02X", DataBuffer[addr + i]);
            checksum += DataBuffer[addr + i];
        }
        
        checksum = (~checksum + 1) & 0xFF;
        fprintf(fp, "%02X\n", checksum);
        
        addr += bytesInLine;
    }
    
    /* End-of-file record */
    fprintf(fp, ":00000001FF\n");
    fclose(fp);
}
```

**Intel HEX Format Structure:**
```
:10000000214601360121470136007EFE09D2194097  <- Data record
:00000001FF                                    <- End record
```

Format: `:LLAAAATTDD...DDCC`
- `LL` = Byte count (hex)
- `AAAA` = Address (hex)
- `TT` = Record type (00=data, 01=EOF)
- `DD` = Data bytes
- `CC` = Checksum

#### 3. Formatted Listing

```c
void SaveListing(void) {
    FILE *fp;
    unsigned int addr, col;
    unsigned char byte;
    
    fp = fopen(filename, "w");
    
    /* Header */
    fprintf(fp, "EPROM Data Listing\n");
    fprintf(fp, "==================\n\n");
    fprintf(fp, "Total bytes: %u\n\n", BufferSize);
    fprintf(fp, "Offset   00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F  ASCII\n");
    fprintf(fp, "------------------------------------------------------------------------\n");
    
    /* Data with hex and ASCII */
    for (addr = 0; addr < BufferSize; addr += BYTES_PER_LINE) {
        fprintf(fp, "%04X:    ", addr);
        
        /* Hex bytes */
        for (col = 0; col < BYTES_PER_LINE && addr + col < BufferSize; col++) {
            byte = DataBuffer[addr + col];
            fprintf(fp, "%02X ", byte);
        }
        
        /* Padding for incomplete lines */
        for (; col < BYTES_PER_LINE; col++) {
            fprintf(fp, "   ");
        }
        
        fprintf(fp, " ");
        
        /* ASCII representation */
        for (col = 0; col < BYTES_PER_LINE && addr + col < BufferSize; col++) {
            byte = DataBuffer[addr + col];
            fprintf(fp, "%c", (byte >= 32 && byte < 127) ? byte : '.');
        }
        
        fprintf(fp, "\n");
    }
    
    fclose(fp);
}
```

### Upload to Programmer

```c
void UploadToEPROM(void) {
    FILE *fp;
    unsigned char byte;
    unsigned int addr = 0;
    int isBinary = 0;
    
    /* Open source file */
    fp = fopen(filename, "rb");
    
    /* Detect file format (binary vs ASCII hex) */
    /* ... format detection code ... */
    
    if (isBinary) {
        /* Send binary as ASCII hex */
        while (!feof(fp) && addr < MAX_BUFFER_SIZE) {
            if (fread(&byte, 1, 1, fp) == 1) {
                /* Convert to ASCII hex with space separator */
                WriteCOM1(NibbleToHex(byte >> 4));
                WriteCOM1(NibbleToHex(byte & 0x0F));
                WriteCOM1(' ');
                
                printf("%02X ", byte);
                addr++;
                delay(10);  /* Throttle for programmer */
            }
        }
    } else {
        /* Forward ASCII hex as-is */
        while (!feof(fp)) {
            ch = fgetc(fp);
            WriteCOM1((unsigned char)ch);
            delay(5);
        }
    }
    
    fclose(fp);
}
```

**WriteCOM1() Implementation:**
```c
void WriteCOM1(unsigned char byte) {
    /* Wait for transmit buffer empty */
    while ((inportb(COM1_LSR) & 0x20) == 0);
    outportb(COM1_DATA, byte);
}
```

---

## Customization Guide

### 1. Adding New Keyboard Commands

Add to HexEditor() switch statement:

```c
/* In HexEditor() main loop */
else if (key == 'f' || key == 'F') {  /* Find byte */
    FindByte();
    DisplayHexEditor();
}
```

Implement the function:
```c
void FindByte(void) {
    char hexStr[3];
    unsigned char searchByte;
    unsigned int startPos, i;
    
    gotoxy(1, 23);
    printf("Find byte (hex): ");
    
    /* Get hex input */
    /* ... input code ... */
    
    sscanf(hexStr, "%x", &searchByte);
    
    startPos = CursorPos + 1;
    for (i = startPos; i < BufferSize; i++) {
        if (DataBuffer[i] == searchByte) {
            CursorPos = i;
            TopLine = i / BYTES_PER_LINE;
            return;
        }
    }
    
    /* Wrap around */
    for (i = 0; i < startPos; i++) {
        if (DataBuffer[i] == searchByte) {
            CursorPos = i;
            TopLine = i / BYTES_PER_LINE;
            return;
        }
    }
    
    printf("Not found!");
    getch();
}
```

### 2. Checksum Calculator

Add to menu and implement:

```c
void CalculateChecksum(void) {
    unsigned int sum = 0;
    unsigned char xor_sum = 0;
    unsigned int i;
    
    for (i = 0; i < BufferSize; i++) {
        sum += DataBuffer[i];
        xor_sum ^= DataBuffer[i];
    }
    
    clrscr();
    printf("Checksum Results\n");
    printf("================\n\n");
    printf("8-bit sum:     %02X\n", sum & 0xFF);
    printf("16-bit sum:    %04X\n", sum);
    printf("XOR checksum:  %02X\n", xor_sum);
    printf("Two's complement: %02X\n", (~sum + 1) & 0xFF);
    printf("\nPress any key...");
    getch();
}
```

### 3. Data Fill Function

```c
void FillRange(void) {
    unsigned int startAddr, endAddr, i;
    unsigned char fillByte;
    char input[10];
    
    gotoxy(1, 23);
    printf("Start address (hex): ");
    scanf("%s", input);
    sscanf(input, "%x", &startAddr);
    
    printf("End address (hex): ");
    scanf("%s", input);
    sscanf(input, "%x", &endAddr);
    
    printf("Fill byte (hex): ");
    scanf("%s", input);
    sscanf(input, "%x", &fillByte);
    
    if (startAddr >= BufferSize || endAddr >= BufferSize || startAddr > endAddr) {
        printf("Invalid range!");
        getch();
        return;
    }
    
    for (i = startAddr; i <= endAddr; i++) {
        DataBuffer[i] = fillByte;
    }
    
    Modified = 1;
    printf("Range filled!");
    getch();
}
```

### 4. Copy/Paste Block

```c
#define MAX_CLIPBOARD 1024
unsigned char Clipboard[MAX_CLIPBOARD];
unsigned int ClipboardSize = 0;

void CopyBlock(void) {
    unsigned int startAddr, endAddr, size, i;
    
    /* Get range from user */
    /* ... */
    
    size = endAddr - startAddr + 1;
    if (size > MAX_CLIPBOARD) {
        printf("Block too large!");
        return;
    }
    
    for (i = 0; i < size; i++) {
        Clipboard[i] = DataBuffer[startAddr + i];
    }
    ClipboardSize = size;
    
    printf("Copied %u bytes", size);
}

void PasteBlock(void) {
    unsigned int i;
    
    if (ClipboardSize == 0) {
        printf("Clipboard empty!");
        return;
    }
    
    if (CursorPos + ClipboardSize > BufferSize) {
        printf("Not enough space!");
        return;
    }
    
    for (i = 0; i < ClipboardSize; i++) {
        DataBuffer[CursorPos + i] = Clipboard[i];
    }
    
    Modified = 1;
    printf("Pasted %u bytes", ClipboardSize);
}
```

### 5. Export to C Header File

```c
void ExportToCHeader(void) {
    FILE *fp;
    char filename[80];
    char arrayName[40];
    unsigned int i;
    
    printf("Enter header filename: ");
    scanf("%s", filename);
    
    printf("Enter array name: ");
    scanf("%s", arrayName);
    
    fp = fopen(filename, "w");
    
    fprintf(fp, "/* Auto-generated EPROM data */\n");
    fprintf(fp, "/* Generated: %s */\n\n", __DATE__);
    fprintf(fp, "const unsigned char %s[%u] = {\n", arrayName, BufferSize);
    
    for (i = 0; i < BufferSize; i++) {
        if (i % 16 == 0) fprintf(fp, "    ");
        
        fprintf(fp, "0x%02X", DataBuffer[i]);
        
        if (i < BufferSize - 1) fprintf(fp, ",");
        if ((i + 1) % 16 == 0) fprintf(fp, "\n");
        else if (i < BufferSize - 1) fprintf(fp, " ");
    }
    
    fprintf(fp, "\n};\n");
    fclose(fp);
    
    printf("Exported to %s", filename);
}
```

### 6. ASCII String Search

```c
void SearchString(void) {
    char searchStr[80];
    int searchLen, i, j, match;
    
    gotoxy(1, 23);
    printf("Search string: ");
    scanf("%s", searchStr);
    
    searchLen = strlen(searchStr);
    
    for (i = 0; i <= BufferSize - searchLen; i++) {
        match = 1;
        for (j = 0; j < searchLen; j++) {
            if (DataBuffer[i + j] != searchStr[j]) {
                match = 0;
                break;
            }
        }
        
        if (match) {
            CursorPos = i;
            TopLine = i / BYTES_PER_LINE;
            printf("Found at %04X", i);
            getch();
            return;
        }
    }
    
    printf("Not found!");
    getch();
}
```

### 7. Statistics Display

```c
void ShowStatistics(void) {
    unsigned int histogram[256] = {0};
    unsigned char mostCommon = 0;
    unsigned int maxCount = 0;
    unsigned int i;
    double entropy = 0.0;
    
    /* Build histogram */
    for (i = 0; i < BufferSize; i++) {
        histogram[DataBuffer[i]]++;
    }
    
    /* Find most common byte */
    for (i = 0; i < 256; i++) {
        if (histogram[i] > maxCount) {
            maxCount = histogram[i];
            mostCommon = i;
        }
    }
    
    /* Calculate entropy (simplified) */
    for (i = 0; i < 256; i++) {
        if (histogram[i] > 0) {
            double p = (double)histogram[i] / BufferSize;
            entropy -= p * log(p) / log(2.0);
        }
    }
    
    clrscr();
    printf("Buffer Statistics\n");
    printf("=================\n\n");
    printf("Size:           %u bytes\n", BufferSize);
    printf("Most common:    0x%02X (%u occurrences)\n", mostCommon, maxCount);
    printf("Entropy:        %.2f bits/byte\n", entropy);
    printf("Unique bytes:   ");
    
    int unique = 0;
    for (i = 0; i < 256; i++) {
        if (histogram[i] > 0) unique++;
    }
    printf("%d\n", unique);
    
    printf("\nPress any key...");
    getch();
}
```

---

## Compilation Instructions

### Using Turbo C IDE

1. **Create Project:**
   ```
   File → New → Project
   Name: ROMREAD.PRJ
   ```

2. **Add Source:**
   ```
   Add romread.c to project
   ```

3. **Set Options:**
   ```
   Options → Compiler → Model: Small
   Options → Compiler → Optimization: Speed
   ```

4. **Compile:**
   ```
   Compile → Make
   ```

### Command Line Compilation

```batch
TCC -ms -G -O -Z romread.c
```

**Flags:**
- `-ms` = Small memory model
- `-G` = Optimize for speed
- `-O` = Enable optimization
- `-Z` = Register optimization

### For Larger Buffer (>64KB)

```batch
TCC -mh -G -O romread.c
```

**Flags:**
- `-mh` = Huge memory model (required for large arrays)

### DOSBox Configuration

Add to dosbox.conf:

```ini
[serial]
serial1=directserial realport:COM1

[cpu]
core=auto
cputype=auto
cycles=max

[dos]
ems=true
umb=true
```

---

## Usage Guide

### Quick Start

1. **Basic Operation:**
   ```
   ROMREAD.EXE
   ```
   - Select option 1 to read from COM1
   - Or option 2 to load existing file
   - Or option 3 to start with empty buffer

2. **Load File Directly:**
   ```
   ROMREAD.EXE EPROM.BIN
   ```

3. **Navigation in Editor:**
   - Arrow keys: Move cursor
   - Page Up/Down: Scroll page
   - ENTER: Edit current byte
   - ESC: Exit editor

4. **Commands (Hold ALT to see menu):**
   - S: Save file
   - H: Save as Intel HEX
   - L: Save formatted listing
   - P: Print to LPT1
   - U: Upload to programmer
   - O: Open/load file
   - D: Disassemble
   - ESC: Exit

### Typical Workflows

#### Workflow 1: Read and Save EPROM

```
1. Connect EPROM programmer to COM1
2. Run ROMREAD.EXE
3. Select option 1 (Read from COM1)
4. Start transfer from programmer
5. Wait for completion
6. Press S to save
7. Enter filename
```

#### Workflow 2: Edit and Upload

```
1. Run ROMREAD.EXE EPROM.BIN
2. Navigate to bytes to edit
3. Press ENTER, enter new values
4. Press U to upload
5. Enter filename (or same file)
6. Confirm upload
```

#### Workflow 3: Disassembly

```
1. Load binary file
2. Press D for disassembler menu
3. Select CPU type (1=6800, 2=8080, 3=8086)
4. Enter start address (or ENTER for 0000)
5. View disassembly on screen
6. Output saved to DISASM_xxxx.ASM
```

---

## Troubleshooting

### Problem: No Data Received from COM1

**Symptoms:**
- "Waiting for data..." message repeats
- Timeout occurs with 0 bytes

**Solutions:**

1. **Check COM port:**
   ```c
   /* Try COM2 instead */
   #define COM1_DATA 0x2F8
   ```

2. **Check baud rate:**
   ```c
   InitCOM1(9600);  /* Try 4800, 19200, etc. */
   ```

3. **Increase timeout:**
   ```c
   if (timeout > 100000L && BufferSize > 0)  /* Increase to 200000L */
   ```

4. **Disable FIFO:**
   ```c
   outportb(COM1_IIR, 0x00);  /* Instead of 0x07 */
   ```

### Problem: Garbled Data Reception

**Symptoms:**
- Random characters displayed
- Incorrect byte values

**Solutions:**

1. **Check format:**
   - Ensure data is ASCII hex: "48 65 6C"
   - Not binary or Intel HEX format

2. **Verify parity:**
   ```c
   outportb(COM1_LCR, 0x1A);  /* Try even parity */
   ```

3. **Add delays:**
   ```c
   delay(5);  /* Increase from 2 */
   ```

### Problem: Cursor Not Visible

**Solution:**
```c
/* Force cursor on */
_setcursortype(_NORMALCURSOR);
```

### Problem: Colors Wrong in DOSBox

**Solution:**
Add to dosbox.conf:
```ini
[render]
scaler=normal2x
output=surface
```

### Problem: Editor Crashes on Large Files

**Solutions:**

1. **Use huge memory model:**
   ```batch
   TCC -mh romread.c
   ```

2. **Reduce buffer size:**
   ```c
   #define MAX_BUFFER_SIZE 8192
   ```

3. **Add bounds checking:**
   ```c
   if (CursorPos >= BufferSize) {
       CursorPos = BufferSize - 1;
   }
   ```

---

## Advanced Topics

### Creating Custom CPU Disassemblers

Template for new CPU:

```c
#define CPU_Z80 4

int DisasmZ80(unsigned int addr, char *output) {
    unsigned char opcode, byte1, byte2;
    int len = 1;
    
    if (addr >= BufferSize) {
        sprintf(output, "???");
        return 1;
    }
    
    opcode = DataBuffer[addr];
    byte1 = (addr + 1 < BufferSize) ? DataBuffer[addr+1] : 0;
    byte2 = (addr + 2 < BufferSize) ? DataBuffer[addr+2] : 0;
    
    /* Handle CB prefix */
    if (opcode == 0xCB) {
        opcode = byte1;
        switch (opcode) {
            case 0x00: sprintf(output, "RLC B"); len=2; break;
            case 0x10: sprintf(output, "RL B"); len=2; break;
            /* ... */
            default: sprintf(output, "DB CBh,%02Xh", opcode); len=2; break;
        }
        return len;
    }
    
    /* Handle ED prefix */
    if (opcode == 0xED) {
        opcode = byte1;
        switch (opcode) {
            case 0x42: sprintf(output, "SBC HL,BC"); len=2; break;
            case 0x4B: 
                sprintf(output, "LD BC,(%04Xh)", byte1 | (byte2 << 8)); 
                len=4; 
                break;
            /* ... */
            default: sprintf(output, "DB EDh,%02Xh", opcode); len=2; break;
        }
        return len;
    }
    
    /* Standard Z80 opcodes */
    switch (opcode) {
        case 0x00: sprintf(output, "NOP"); break;
        case 0x06: sprintf(output, "LD B,%02Xh", byte1); len=2; break;
        /* ... */
        default: sprintf(output, "DB %02Xh", opcode); break;
    }
    
    return len;
}
```

Then add to DisassembleMenu():
```c
printf("  4. Zilog Z80\n");
/* ... */
case '4':
    CPUType = CPU_Z80;
    break;
```

### Implementing ModR/M Decoding for 8086

```c
void Decode8086ModRM(unsigned int addr, unsigned char modrm, 
                     char *operand, int *len) {
    int mod = (modrm >> 6) & 0x03;
    int reg = (modrm >> 3) & 0x07;
    int rm = modrm & 0x07;
    unsigned char disp_low, disp_high;
    
    /* Register names */
    const char *reg8[] = {"AL", "CL", "DL", "BL", "AH", "CH", "DH", "BH"};
    const char *reg16[] = {"AX", "CX", "DX", "BX", "SP", "BP", "SI", "DI"};
    
    *len = 1;  /* ModR/M byte */
    
    if (mod == 0x03) {
        /* Register-to-register */
        sprintf(operand, "%s", reg8[rm]);
    }
    else if (mod == 0x00 && rm == 0x06) {
        /* Direct addressing */
        disp_low = DataBuffer[addr + *len];
        disp_high = DataBuffer[addr + *len + 1];
        *len += 2;
        sprintf(operand, "[%04Xh]", disp_low | (disp_high << 8));
    }
    else {
        /* Memory addressing */
        const char *ea[] = {"BX+SI", "BX+DI", "BP+SI", "BP+DI", 
                           "SI", "DI", "BP", "BX"};
        
        if (mod == 0x00) {
            sprintf(operand, "[%s]", ea[rm]);
        }
        else if (mod == 0x01) {
            disp_low = DataBuffer[addr + *len];
            *len += 1;
            sprintf(operand, "[%s+%02Xh]", ea[rm], disp_low);
        }
        else if (mod == 0x02) {
            disp_low = DataBuffer[addr + *len];
            disp_high = DataBuffer[addr + *len + 1];
            *len += 2;
            sprintf(operand, "[%s+%04Xh]", ea[rm], 
                    disp_low | (disp_high << 8));
        }
    }
}
```

### Binary File Format Handlers

```c
typedef struct {
    char signature[4];
    unsigned int loadAddress;
    unsigned int entryPoint;
    unsigned int dataSize;
} BinaryHeader;

int LoadBinaryWithHeader(const char *filename) {
    FILE *fp;
    BinaryHeader header;
    
    fp = fopen(filename, "rb");
    if (!fp) return 0;
    
    /* Read header */
    fread(&header, sizeof(BinaryHeader), 1, fp);
    
    /* Verify signature */
    if (strncmp(header.signature, "EPROM", 4) != 0) {
        fclose(fp);
        return 0;
    }
    
    /* Read data */
    BufferSize = header.dataSize;
    if (BufferSize > MAX_BUFFER_SIZE) {
        BufferSize = MAX_BUFFER_SIZE;
    }
    
    fread(DataBuffer, 1, BufferSize, fp);
    fclose(fp);
    
    printf("Load address: %04X\n", header.loadAddress);
    printf("Entry point:  %04X\n", header.entryPoint);
    
    return 1;
}
```

---

## Performance Optimization

### 1. Faster Screen Updates

```c
/* Use direct video memory access */
#define VIDEO_SEGMENT 0xB800  /* Color text mode */

void FastPutChar(int x, int y, char ch, int attr) {
    char far *video = (char far *)MK_FP(VIDEO_SEGMENT, 0);
    int offset = ((y - 1) * 80 + (x - 1)) * 2;
    video[offset] = ch;
    video[offset + 1] = attr;
}
```

### 2. Buffered Display

```c
char DisplayBuffer[4000];  /* 80x25 * 2 */

void UpdateDisplayBuffer(void) {
    /* Build entire screen in memory */
    /* Then copy to video memory in one operation */
    char far *video = (char far *)MK_FP(0xB800, 0);
    memcpy(video, DisplayBuffer, 4000);
}
```

### 3. Optimized Hex Conversion

```c
/* Lookup table faster than calculation */
const char hexChars[] = "0123456789ABCDEF";

char NibbleToHex(unsigned char nibble) {
    return hexChars[nibble & 0x0F];
}
```

---

## Conclusion

This EPROM Data Reader represents a complete solution for vintage computer enthusiasts and embedded systems developers working with EPROM data. The modular design allows easy customization and extension for specific needs.

**Key Strengths:**
- Reliable serial communication with real hardware
- Professional user interface
- Comprehensive file format support
- Multiple CPU architecture support
- Extensive customization options

**Suggested Enhancements:**
- Add Motorola S-record format support
- Implement binary search in large files
- Add CRC32 checksum calculation
- Support for multiple EPROM programmers
- Batch file processing

For questions or contributions, contact Mickey W. Lawless.

---

**Document Version:** 1.1  
**Last Updated:** December 21, 2025  
**License:** All Rights Reserved  
**Platform:** MS-DOS / Turbo C