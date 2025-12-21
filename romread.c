/*
 * EPROM Data Reader with Hex Editor
 * Turbo C Compatible
 * Reads ASCII space-separated hex from COM1
 * Text mode version for better DOSBox compatibility
 */

/*
 * ========================================================================
 * EPROM Data Reader with Hex Editor
 * ========================================================================
 * 
 * Developer: Mickey W. Lawless
 * Date Completed: December 21, 2025
 * Version: 1.1
 * 
 * Description:
 *   Professional EPROM data reader and hex editor for DOS systems.
 *   Reads ASCII space-separated hex data from COM1 serial port and
 *   provides a full-featured hex editor with disassembly capabilities
 *   for 6800, 8080, and 8086 microprocessors.
 * 
 * Features:
 *   - COM1 serial communication with configurable baud rates
 *   - 16KB buffer capacity
 *   - Full hex/ASCII editing with visual highlighting
 *   - File load/save operations (binary, Intel HEX, listings)
 *   - Upload to EPROM programmer via COM1
 *   - Disassembler for 6800, 8080, and 8086 CPUs
 *   - Print to LPT1 support
 *   - Command-line file loading
 *   - ALT-key command help menu
 * 
 * Compiler: Turbo C/C++ 1.0 or later
 * Platform: MS-DOS / DOSBox;
 * 
 * Copyright (c) 2025 Mickey W. Lawless
 * All Rights Reserved
 * ========================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <dos.h>
#include <string.h>

/* COM1 Port Definitions */
#define COM1_DATA     0x3F8
#define COM1_IER      0x3F9
#define COM1_LCR      0x3FB
#define COM1_LSR      0x3FD
#define COM1_MCR      0x3FC
#define COM1_IIR      0x3FA

/* Buffer Configuration */
#define MAX_BUFFER_SIZE 16384  /* 16KB buffer */
#define BYTES_PER_LINE 16

/* Edit Mode Definitions */
#define MODE_VIEW     0
#define MODE_EDIT_HEX 1
#define MODE_EDIT_ASCII 2

/* Cursor Area Definitions */
#define AREA_HEX   0
#define AREA_ASCII 1

/* Disassembler CPU type constants */
#define CPU_6800  1
#define CPU_8080  2
#define CPU_8086  3

/* Global Variables */
unsigned char DataBuffer[MAX_BUFFER_SIZE];
unsigned int BufferSize = 0;
unsigned int CursorPos = 0;
unsigned int TopLine = 0;
int EditMode = MODE_VIEW;  /* Track current edit mode */
int Modified = 0;          /* Track if buffer was modified */
int CursorArea = AREA_HEX; /* Track if cursor is in hex or ASCII area */
int CPUType = CPU_6800;     /* Track the current CPU type for disassembly */
/* Added global variable to track ALT key state for menu display */
int ShowCommandHelp = 0;  /* Show command help when ALT is held */

/* Forward Declarations */
void InitCOM1(unsigned int baud);
int COM1DataReady(void);
unsigned char ReadCOM1(void);
void ReadDataFromCOM1(void);
char NibbleToHex(unsigned char nibble);
void SaveBufferToFile(void);
void SaveBufferAsHex(void);
void SaveListing(void);
void PrintBuffer(void);
void EditByteHex(void);
void EditByteASCII(void);
void DisplayHexEditor(void);
void WriteCOM1(unsigned char byte);
void UploadToEPROM(void);
void LoadFileIntoBuffer(void);
int ShowStartupMenu(void);
void HexEditor(void); /* Forward declaration for the updated HexEditor function */
void Disassemble(void);
void DisassembleMenu(void);
/* Disassemble6800 and Disassemble8080 and Disassemble8086 function prototypes are now defined below. */
void SaveDisassembly(int cpuType);

/* Initialize COM1 port */
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
    
    /* Clear FIFOs first before enabling */
    outportb(COM1_IIR, 0x00);
    delay(10);
    
    /* Enable FIFO, clear both FIFOs, 1-byte trigger (more reliable for real hardware) */
    outportb(COM1_IIR, 0x07);
    delay(10);
    
    /* Enable DTR, RTS, OUT2 */
    outportb(COM1_MCR, 0x0B);
    
    /* Flush any pending data from previous session */
    while (COM1DataReady()) {
        ReadCOM1();
        delay(1);
    }
}

/* Check if data is available on COM1 */
int COM1DataReady(void) {
    return (inportb(COM1_LSR) & 0x01);
}

/* Read a byte from COM1 */
unsigned char ReadCOM1(void) {
    while (!COM1DataReady());
    return inportb(COM1_DATA);
}

/* Read hex data from COM1 into buffer */
void ReadDataFromCOM1(void) {
    char hexStr[3];
    int hexPos = 0;
    unsigned char ch;
    unsigned int value;
    long timeout = 0;
    int linePos = 0;
    int bytesThisSecond = 0;  /* Track throughput for debugging */
    long lastByteTime = 0;
    
    hexStr[0] = 0;
    hexStr[1] = 0;
    hexStr[2] = 0;
    
    BufferSize = 0;
    
    clrscr();
    textcolor(YELLOW);
    printf("Reading from COM1 (9600 baud)\n");
    textcolor(LIGHTGRAY);
    printf("Format: ASCII space-separated hex (e.g. 48 65 6C 6C 6F)\n");
    printf("Press ESC to stop reading\n");
    printf("========================================================================\n");
    
    while (BufferSize < MAX_BUFFER_SIZE) {
        if (kbhit()) {
            if (getch() == 27) {
                printf("\n");
                textcolor(LIGHTRED);
                printf("Reading stopped by user.\n");
                textcolor(LIGHTGRAY);
                break;
            }
        }
        
        if (COM1DataReady()) {
            ch = ReadCOM1();
            timeout = 0; /* Reset timeout on any data received */
            lastByteTime = 0;
            
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
                    
                    /* Display formatted output - 16 bytes per line */
                    printf("%02X ", (unsigned char)value);
                    linePos++;
                    bytesThisSecond++;
                    
                    if (linePos >= 16) {
                        printf("\n");
                        linePos = 0;
                    }
                    
                    /* Small delay after each byte for stability on real hardware */
                    delay(2);
                }
            }
            else if (ch == ' ' || ch == '\r' || ch == '\n' || ch == '\t') {
                hexPos = 0;
            }
        } else {
            /* Longer timeout with proper delays for real hardware reliability */
            timeout++;
            lastByteTime++;
            
            /* Check timeout every 500 iterations with small delay */
            if (timeout % 500 == 0) {
                delay(5); /* 5ms delay */
            }
            
            /* Much longer timeout: 100000 * 5ms = ~0.5 seconds of no data */
            /* This gives real hardware more time to complete transmission */
            if (timeout > 100000L && BufferSize > 0) {
                printf("\n");
                textcolor(LIGHTGREEN);
                printf("Transmission complete. Total bytes: %u\n", BufferSize);
                textcolor(LIGHTGRAY);
                break;
            }
            
            /* Show waiting status periodically if no data coming */
            if (BufferSize == 0 && timeout > 50000L && (timeout % 50000L) == 0) {
                textcolor(YELLOW);
                printf("Waiting for data from COM1...\n");
                textcolor(LIGHTGRAY);
            }
        }
    }
    
    /* Check if buffer is full */
    if (BufferSize >= MAX_BUFFER_SIZE) {
        printf("\n");
        textcolor(YELLOW);
        printf("Buffer full - reading stopped.\n");
        textcolor(LIGHTGRAY);
    }
    
    printf("========================================================================\n");
    textcolor(LIGHTGREEN);
    printf("Total bytes read: %u\n", BufferSize);
    textcolor(LIGHTGRAY);
    printf("Press any key to view hex editor...\n");
    getch();
}

/* Convert nibble to hex character */
char NibbleToHex(unsigned char nibble) {
    return (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
}

/* Save buffer to file */
void SaveBufferToFile(void) {
    char filename[80];
    FILE *fp;
    unsigned int i;
    int yPos = 23;
    
    gotoxy(1, yPos);
    textcolor(LIGHTGRAY);
    textbackground(BLACK);
    clreol();
    printf("Enter filename to save (or ESC to cancel): ");
    
    i = 0;
    while (1) {
        int ch = getch();
        
        if (ch == 27) {  /* ESC */
            return;
        }
        else if (ch == 13 && i > 0) {  /* ENTER */
            filename[i] = '\0';
            break;
        }
        else if (ch == 8 && i > 0) {  /* BACKSPACE */
            i--;
            printf("\b \b");
        }
        else if (ch >= 32 && ch < 127 && i < 79) {
            filename[i++] = ch;
            putchar(ch);
        }
    }
    
    fp = fopen(filename, "wb");
    if (fp == NULL) {
        gotoxy(1, yPos);
        clreol();
        textcolor(LIGHTRED);
        printf("ERROR: Cannot create file! Press any key...");
        textcolor(LIGHTGRAY);
        getch();
        return;
    }
    
    fwrite(DataBuffer, 1, BufferSize, fp);
    fclose(fp);
    
    gotoxy(1, yPos);
    clreol();
    textcolor(LIGHTGREEN);
    printf("File saved successfully! (%u bytes) Press any key...", BufferSize);
    textcolor(LIGHTGRAY);
    getch();
    
    Modified = 0;
}

/* Added function to save buffer in Intel HEX format */
void SaveBufferAsHex(void) {
    char filename[80];
    FILE *fp;
    unsigned int i, addr, checksum, bytesInLine;
    int yPos = 23;
    
    gotoxy(1, yPos);
    textcolor(LIGHTGRAY);
    textbackground(BLACK);
    clreol();
    printf("Enter .hex filename to save (or ESC to cancel): ");
    
    i = 0;
    while (1) {
        int ch = getch();
        
        if (ch == 27) {  /* ESC */
            return;
        }
        else if (ch == 13 && i > 0) {  /* ENTER */
            filename[i] = '\0';
            break;
        }
        else if (ch == 8 && i > 0) {  /* BACKSPACE */
            i--;
            printf("\b \b");
        }
        else if (ch >= 32 && ch < 127 && i < 79) {
            filename[i++] = ch;
            putchar(ch);
        }
    }
    
    fp = fopen(filename, "w");
    if (fp == NULL) {
        gotoxy(1, yPos);
        clreol();
        textcolor(LIGHTRED);
        printf("ERROR: Cannot create file! Press any key...");
        textcolor(LIGHTGRAY);
        getch();
        return;
    }
    
    /* Write Intel HEX format */
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
    
    /* Write end-of-file record */
    fprintf(fp, ":00000001FF\n");
    fclose(fp);
    
    gotoxy(1, yPos);
    clreol();
    textcolor(LIGHTGREEN);
    printf("Intel HEX file saved! (%u bytes) Press any key...", BufferSize);
    textcolor(LIGHTGRAY);
    getch();
}

/* Added function to save formatted listing */
void SaveListing(void) {
    char filename[80];
    FILE *fp;
    unsigned int addr, col;
    unsigned char byte;
    int yPos = 23;
    int i;
    
    gotoxy(1, yPos);
    textcolor(LIGHTGRAY);
    textbackground(BLACK);
    clreol();
    printf("Enter listing filename (or ESC to cancel): ");
    
    i = 0;
    while (1) {
        int ch = getch();
        
        if (ch == 27) {  /* ESC */
            return;
        }
        else if (ch == 13 && i > 0) {  /* ENTER */
            filename[i] = '\0';
            break;
        }
        else if (ch == 8 && i > 0) {  /* BACKSPACE */
            i--;
            printf("\b \b");
        }
        else if (ch >= 32 && ch < 127 && i < 79) {
            filename[i++] = ch;
            putchar(ch);
        }
    }
    
    fp = fopen(filename, "w");
    if (fp == NULL) {
        gotoxy(1, yPos);
        clreol();
        textcolor(LIGHTRED);
        printf("ERROR: Cannot create file! Press any key...");
        textcolor(LIGHTGRAY);
        getch();
        return;
    }
    
    /* Write formatted listing header */
    fprintf(fp, "EPROM Data Listing\n");
    fprintf(fp, "==================\n\n");
    fprintf(fp, "Total bytes: %u\n\n", BufferSize);
    fprintf(fp, "Offset   00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F  ASCII\n");
    fprintf(fp, "------------------------------------------------------------------------\n");
    
    /* Write hex data */
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
    
    gotoxy(1, yPos);
    clreol();
    textcolor(LIGHTGREEN);
    printf("Listing saved! (%u bytes) Press any key...", BufferSize);
    textcolor(LIGHTGRAY);
    getch();
}

/* Added function to print buffer to LPT1 */
void PrintBuffer(void) {
    FILE *fp;
    unsigned int addr, col;
    unsigned char byte;
    int yPos = 23;
    
    gotoxy(1, yPos);
    textcolor(YELLOW);
    textbackground(BLACK);
    clreol();
    printf("Printing to LPT1... Press ESC to cancel...");
    
    fp = fopen("LPT1", "w");
    if (fp == NULL) {
        gotoxy(1, yPos);
        clreol();
        textcolor(LIGHTRED);
        printf("ERROR: Cannot access printer (LPT1)! Press any key...");
        textcolor(LIGHTGRAY);
        getch();
        return;
    }
    
    /* Print header */
    fprintf(fp, "EPROM Data Listing\n");
    fprintf(fp, "==================\n\n");
    fprintf(fp, "Total bytes: %u\n\n", BufferSize);
    fprintf(fp, "Offset   00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F  ASCII\n");
    fprintf(fp, "------------------------------------------------------------------------\n");
    
    /* Print hex data */
    for (addr = 0; addr < BufferSize; addr += BYTES_PER_LINE) {
        if (kbhit() && getch() == 27) {
            fclose(fp);
            gotoxy(1, yPos);
            clreol();
            textcolor(LIGHTRED);
            printf("Printing cancelled! Press any key...");
            textcolor(LIGHTGRAY);
            getch();
            return;
        }
        
        fprintf(fp, "%04X:    ", addr);
        
        /* Hex bytes */
        for (col = 0; col < BYTES_PER_LINE && addr + col < BufferSize; col++) {
            byte = DataBuffer[addr + col];
            fprintf(fp, "%02X ", byte);
        }
        
        /* Padding */
        for (; col < BYTES_PER_LINE; col++) {
            fprintf(fp, "   ");
        }
        
        fprintf(fp, " ");
        
        /* ASCII */
        for (col = 0; col < BYTES_PER_LINE && addr + col < BufferSize; col++) {
            byte = DataBuffer[addr + col];
            fprintf(fp, "%c", (byte >= 32 && byte < 127) ? byte : '.');
        }
        
        fprintf(fp, "\n");
    }
    
    /* Form feed */
    fprintf(fp, "\f");
    fclose(fp);
    
    gotoxy(1, yPos);
    clreol();
    textcolor(LIGHTGREEN);
    printf("Printing complete! Press any key...");
    textcolor(LIGHTGRAY);
    getch();
}

/* Edit byte in hex mode */
void EditByteHex(void) {
    char hexStr[3];
    int hexPos = 0;
    unsigned int value;
    int ch;
    int yPos = 23;
    
    hexStr[0] = 0;
    hexStr[1] = 0;
    hexStr[2] = 0;
    
    gotoxy(1, yPos);
    textcolor(YELLOW);
    textbackground(BLACK);
    clreol();
    printf("HEX EDIT - Enter 2 hex digits (or ESC): ");
    
    while (hexPos < 2) {
        ch = getch();
        
        if (ch == 27) {  /* ESC */
            return;
        }
        else if ((ch >= '0' && ch <= '9') || 
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
    
    if (CursorPos < BufferSize - 1) {
        CursorPos++;
    }
}

/* Edit byte in ASCII mode */
void EditByteASCII(void) {
    int ch;
    int yPos = 23;
    
    gotoxy(1, yPos);
    textcolor(YELLOW);
    textbackground(BLACK);
    clreol();
    printf("ASCII EDIT - Enter character (or ESC): ");
    
    ch = getch();
    
    if (ch == 27) {  /* ESC */
        return;
    }
    else if (ch >= 0 && ch < 256) {
        DataBuffer[CursorPos] = (unsigned char)ch;
        Modified = 1;
        putchar(ch);
        delay(200);
        
        if (CursorPos < BufferSize - 1) {
            CursorPos++;
        }
    }
}

/* Display hex editor screen */
void DisplayHexEditor(void) {
    int row, col;
    unsigned int addr;
    unsigned char byte;
    int maxLines = 21;  /* Increased from 20 to 21 to show more data */
    const char *modeStr;
    int cursorCol;
    
    /* Hide cursor during screen refresh to eliminate visible cursor movement */
    _setcursortype(_NOCURSOR);
    
    /* Draw header */
    gotoxy(1, 1);
    textcolor(WHITE);
    textbackground(BLUE);
    
    if (CursorArea == AREA_HEX) {
        modeStr = "HEX";
    } else {
        modeStr = "ASCII";
    }
    
    cprintf(" EPROM Editor v1.1 - Mickey W. Lawless [%s MODE]%s", modeStr, Modified ? " *MOD*" : "     ");
    cprintf("%-21s", "");  /* Pad to fill 80 columns */
    
    textcolor(LIGHTGRAY);
    textbackground(BLACK);
    gotoxy(1, 2);
    
    /* Column headers */
    cprintf("Offset   00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F  ASCII\r\n");
    cprintf("------------------------------------------------------------------------\r\n");
    
    /* Draw hex data */
    for (row = 0; row < maxLines && (TopLine + row) * BYTES_PER_LINE < BufferSize; row++) {
        addr = (TopLine + row) * BYTES_PER_LINE;
        
        gotoxy(1, 4 + row);
        
        /* Draw offset */
        cprintf("%04X:    ", addr);
        
        /* Draw hex bytes */
        for (col = 0; col < BYTES_PER_LINE && addr + col < BufferSize; col++) {
            byte = DataBuffer[addr + col];
            
            /* Apply reverse video highlight for selected byte (works for both HEX and ASCII mode) */
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
        
        /* Padding for incomplete lines */
        for (; col < BYTES_PER_LINE; col++) {
            cprintf("   ");
        }
        
        cprintf(" ");
        
        /* Draw ASCII representation */
        for (col = 0; col < BYTES_PER_LINE && addr + col < BufferSize; col++) {
            byte = DataBuffer[addr + col];
            
            /* Apply reverse video highlight for selected byte (works for both HEX and ASCII mode) */
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
        
        cprintf("%-10s", "");
    }
    
    for (; row < maxLines; row++) {
        gotoxy(1, 4 + row);
        cprintf("%-79s", "");
    }
    
    /* Command help bar only shown when ALT key is held */
    if (ShowCommandHelp) {
        gotoxy(1, 23);
        textcolor(WHITE);
        textbackground(BLUE);
        cprintf(" ENTER)Edit U)pload S)ave H)ex L)ist P)rint O)pen D)isasm ESC)exit%-2s", "");
    } else {
        /* Clear line 23 when command help is hidden */
        gotoxy(1, 23);
        textcolor(LIGHTGRAY);
        textbackground(BLACK);
        cprintf("%-80s", "");
    }
    
    /* Display current byte offset, value, and area in status bar - ALWAYS visible */
    gotoxy(1, 24);
    textcolor(WHITE);
    textbackground(BLUE);
    
    if (BufferSize > 0 && CursorPos < BufferSize) {
        char statusBuf[81];
        sprintf(statusBuf, " Offset:%04X Value:%02X (%d) Area:%s CPU:%s Size:%d",
                CursorPos,
                DataBuffer[CursorPos],
                DataBuffer[CursorPos],
                (CursorArea == AREA_HEX) ? "HEX" : "ASCII",
                (CPUType == CPU_6800) ? "6800" : ((CPUType == CPU_8080) ? "8080" : "8086"),
                BufferSize);
        cprintf("%-80s", statusBuf);  /* Pad to exactly 80 columns */
    } else {
        cprintf(" Empty Buffer%-67s", "");  /* Pad to exactly 80 columns */
    }
    
    textcolor(LIGHTGRAY);
    textbackground(BLACK);
    
    /* Restore cursor after screen refresh is complete */
    _setcursortype(_NORMALCURSOR);
}

/* Updated HexEditor function incorporating changes */
void HexEditor(void) {
    int key;
    int maxLines = 21;  /* Increased from 20 to 21 to match display */
    int cursorCol;
    
    DisplayHexEditor();
    
    while (1) {
        /* Check if ALT key is pressed using bioskey() */
        if (bioskey(2) & 0x08) {  /* 0x08 = ALT key mask */
            if (!ShowCommandHelp) {
                ShowCommandHelp = 1;
                DisplayHexEditor();
            }
        } else {
            if (ShowCommandHelp) {
                ShowCommandHelp = 0;
                DisplayHexEditor();
            }
        }
        
        if (kbhit()) { /* Check for key press only if kbhit() is true */
            key = getch();
            
            if (key == 0 || key == 224) {  /* Extended key prefix */
                key = getch();
                
                switch (key) {
                    case 72:  /* Up arrow */
                    case 56:  /* Numpad 8 */
                        if (CursorPos >= BYTES_PER_LINE) {
                            CursorPos -= BYTES_PER_LINE;
                            if (CursorPos < TopLine * BYTES_PER_LINE) {
                                TopLine--;
                            }
                            DisplayHexEditor();
                        }
                        break;
                        
                    case 80:  /* Down arrow */
                    case 50:  /* Numpad 2 */
                        if (CursorPos + BYTES_PER_LINE < BufferSize) {
                            CursorPos += BYTES_PER_LINE;
                            if (CursorPos >= (TopLine + maxLines) * BYTES_PER_LINE) {
                                TopLine++;
                            }
                            /* Down arrow always returns to HEX mode at start of line */
                            CursorArea = AREA_HEX;
                            DisplayHexEditor();
                        }
                        break;
                        
                    /* Left arrow - move backward in current area */
                    case 75:  /* Left arrow */
                    case 52:  /* Numpad 4 */
                        cursorCol = CursorPos % BYTES_PER_LINE;
                        
                        /* If in ASCII area and at start of line, go back to end of HEX line */
                        if (CursorArea == AREA_ASCII && cursorCol == 0) {
                            CursorArea = AREA_HEX;
                            CursorPos = (CursorPos / BYTES_PER_LINE) * BYTES_PER_LINE + (BYTES_PER_LINE - 1);
                            DisplayHexEditor();
                        }
                        /* If in ASCII area, move left within ASCII */
                        else if (CursorArea == AREA_ASCII && cursorCol > 0) {
                            CursorPos--;
                            DisplayHexEditor();
                        }
                        /* If in HEX area, move to previous byte */
                        else if (CursorArea == AREA_HEX && CursorPos > 0) {
                            CursorPos--;
                            if (CursorPos < TopLine * BYTES_PER_LINE) {
                                TopLine--;
                            }
                            DisplayHexEditor();
                        }
                        break;
                        
                    /* Right arrow - stay in HEX until end of line, then switch to ASCII */
                    case 77:  /* Right arrow */
                    case 54:  /* Numpad 6 */
                        cursorCol = CursorPos % BYTES_PER_LINE;
                        
                        /* If in HEX area and at end of line (col 15), switch to ASCII at start */
                        if (CursorArea == AREA_HEX && cursorCol == BYTES_PER_LINE - 1) {
                            CursorArea = AREA_ASCII;
                            CursorPos = (CursorPos / BYTES_PER_LINE) * BYTES_PER_LINE;
                            DisplayHexEditor();
                        }
                        /* If in HEX area and not at end, move right within HEX */
                        else if (CursorArea == AREA_HEX && cursorCol < BYTES_PER_LINE - 1) {
                            CursorPos++;
                            DisplayHexEditor();
                        }
                        /* If in ASCII area and at end of line, go to next line HEX start */
                        else if (CursorArea == AREA_ASCII && cursorCol == BYTES_PER_LINE - 1) {
                            if (CursorPos + 1 < BufferSize) {
                                CursorPos++;
                                CursorArea = AREA_HEX;
                                if (CursorPos >= (TopLine + maxLines) * BYTES_PER_LINE) {
                                    TopLine++;
                                }
                                DisplayHexEditor();
                            }
                        }
                        /* If in ASCII area, move right within ASCII */
                        else if (CursorArea == AREA_ASCII && cursorCol < BYTES_PER_LINE - 1) {
                            CursorPos++;
                            DisplayHexEditor();
                        }
                        break;
                        
                    case 73:  /* Page Up */
                        if (TopLine > 0) {
                            TopLine -= maxLines;
                            if ((int)TopLine < 0) TopLine = 0;
                            CursorPos = TopLine * BYTES_PER_LINE;
                            CursorArea = AREA_HEX;
                            DisplayHexEditor();
                        }
                        break;
                        
                    case 81:  /* Page Down */
                        if ((TopLine + maxLines) * BYTES_PER_LINE < BufferSize) {
                            TopLine += maxLines;
                            CursorPos = TopLine * BYTES_PER_LINE;
                            CursorArea = AREA_HEX;
                            DisplayHexEditor();
                        }
                        break;
                }
            }
            else if (key == 27) {  /* ESC - Exit */
                if (Modified) {
                    int ch_esc;
                    gotoxy(1, 23);
                    textcolor(YELLOW);
                    textbackground(BLACK);
                    clreol();
                    printf("Buffer modified! Save before exit? (Y/N): ");
                    ch_esc = getch();
                    if (ch_esc == 'Y' || ch_esc == 'y') {
                        SaveBufferToFile();
                    }
                }
                break; /* Exit the while loop */
            }
            /* Add 'D' key for disassembler menu */
            else if (key == 'd' || key == 'D') {
                DisassembleMenu();
                DisplayHexEditor();
            } else if (key == 13) {  /* ENTER - Edit current byte */
                if (CursorArea == AREA_HEX) {
                    EditByteHex();
                } else {
                    EditByteASCII();
                }
                DisplayHexEditor();
            }
            else if (key == 's' || key == 'S') {  /* Save file */
                SaveBufferToFile();
                DisplayHexEditor();
            }
            else if (key == 'h' || key == 'H') {  /* Save as Intel HEX */
                SaveBufferAsHex();
                DisplayHexEditor();
            }
            else if (key == 'l' || key == 'L') {  /* Save listing */
                SaveListing();
                DisplayHexEditor();
            }
            else if (key == 'p' || key == 'P') {  /* Print to LPT1 */
                PrintBuffer();
                DisplayHexEditor();
            }
            else if (key == 'u' || key == 'U') {  /* Upload file to EPROM */
                UploadToEPROM();
                DisplayHexEditor();
            }
            else if (key == 'o' || key == 'O') {  /* Open/Load file */
                LoadFileIntoBuffer();
                if (BufferSize > 0) {
                    CursorPos = 0;
                    TopLine = 0;
                    CursorArea = AREA_HEX;
                }
                DisplayHexEditor();
            }
        }
    }
}

/* Main program */
int main(int argc, char *argv[]) {
    int key;
    int cursorCol;
    int menuChoice;
    FILE *fp;
    
    /* Check for command line filename argument */
    if (argc > 1) {
        /* File specified on command line - load it directly */
        textmode(C80);
        clrscr();
        textcolor(LIGHTCYAN);
        printf("========================================================================\n");
        printf("               EPROM Data Reader & Hex Editor v1.1\n");
        printf("                   Developer: Mickey W. Lawless\n");
        printf("                    Completed: December 21, 2025\n");
        printf("========================================================================\n\n");
        textcolor(LIGHTGRAY);
        
        printf("Loading file: %s\n", argv[1]);
        
        /* Try to open the file */
        fp = fopen(argv[1], "rb");
        if (fp == NULL) {
            textcolor(LIGHTRED);
            printf("\nERROR: Cannot open file '%s'\n", argv[1]);
            textcolor(LIGHTGRAY);
            printf("Press any key to exit...\n");
            getch();
            return 1;
        }
        
        /* Read file into buffer */
        BufferSize = 0;
        while (BufferSize < MAX_BUFFER_SIZE && !feof(fp)) {
            int bytesRead = fread(&DataBuffer[BufferSize], 1, 
                                  MAX_BUFFER_SIZE - BufferSize, fp);
            if (bytesRead > 0) {
                BufferSize += bytesRead;
            } else {
                break;
            }
        }
        fclose(fp);
        
        if (BufferSize == 0) {
            textcolor(LIGHTRED);
            printf("\nERROR: File is empty or could not be read.\n");
            textcolor(LIGHTGRAY);
            printf("Press any key to exit...\n");
            getch();
            return 1;
        }
        
        textcolor(LIGHTGREEN);
        printf("\nLoaded %d bytes from '%s'\n", BufferSize, argv[1]);
        printf("Press any key to enter hex editor...");
        getch();
        
        /* Initialize editor state and skip menu */
        CursorPos = 0;
        TopLine = 0;
        EditMode = MODE_VIEW;
        CursorArea = AREA_HEX; /* Ensure CursorArea is initialized */
        Modified = 0;
        
        /* Enter hex editor */
        HexEditor();
        return 0;
    }
    
    /* No command line args - show normal startup menu */
    /* Show startup menu instead of automatic COM1 read */
    menuChoice = ShowStartupMenu();
    
    if (menuChoice == 4) {
        /* Exit */
        textcolor(LIGHTGRAY);
        printf("\nExiting...\n");
        return 0;
    }
    
    /* Handle menu choice */
    if (menuChoice == 1) {
        /* Read from COM1 */
        printf("\nInitializing COM1 at 9600 baud...\n");
        InitCOM1(9600);
        
        printf("Ready!\n\n");
        printf("Press any key to start reading from COM1...\n");
        getch();
        
        /* Read data from COM1 */
        ReadDataFromCOM1();
        
        /* Handle no data case */
        if (BufferSize == 0) {
            textcolor(LIGHTRED);
            printf("\nERROR: No data received from COM1!\n\n");
            textcolor(LIGHTGRAY);
            printf("Check:\n");
            printf("  - COM1 connection and cable\n");
            printf("  - Baud rate (should be 9600)\n");
            printf("  - Data format: ASCII space-separated hex\n");
            printf("  - Device is transmitting\n\n");
            printf("Press any key to exit...\n");
            getch();
            return 1;
        }
    }
    else if (menuChoice == 2) {
        /* Load file into buffer */
        LoadFileIntoBuffer();
        
        if (BufferSize == 0) {
            textcolor(LIGHTRED);
            printf("\n\nNo file loaded. Exiting...\n");
            textcolor(LIGHTGRAY);
            return 1;
        }
    }
    else if (menuChoice == 3) {
        /* Enter empty editor */
        BufferSize = 0;
        textcolor(LIGHTGREEN);
        printf("\nEntering empty editor...\n");
        printf("Use 'L' key in editor to load a file.\n");
        printf("Press any key to continue...");
        getch();
    }
    
    /* Added option to load file in empty editor mode */
    if (menuChoice == 3) {
        /* Start with 1 byte in buffer so editor works */
        DataBuffer[0] = 0x00;
        BufferSize = 1;
    }
    
    /* Initialize editor state */
    CursorPos = 0;
    TopLine = 0;
    EditMode = MODE_VIEW;
    Modified = 0;
    CursorArea = AREA_HEX;
    
    /* Call the updated HexEditor function */
    HexEditor();
    
    /* Restore text mode and exit */
    textmode(C80);
    clrscr();
    textcolor(LIGHTGREEN);
    printf("EPROM Reader terminated.\n");
    textcolor(LIGHTGRAY);
    
    return 0;
}

/* Added function to write a byte to COM1 */
void WriteCOM1(unsigned char byte) {
    /* Wait for transmit buffer to be empty */
    while ((inportb(COM1_LSR) & 0x20) == 0);
    outportb(COM1_DATA, byte);
}

/* Added function to upload file to EPROM programmer */
void UploadToEPROM(void) {
    char filename[80];
    FILE *fp;
    unsigned char byte;
    unsigned int addr = 0;
    int i, yPos = 23;
    int isBinary = 0;
    char listFilename[80];
    FILE *listFp;
    unsigned int col;
    int ch;
    
    gotoxy(1, yPos);
    textcolor(LIGHTGRAY);
    textbackground(BLACK);
    clreol();
    printf("Enter filename to upload (or ESC to cancel): ");
    
    i = 0;
    while (1) {
        ch = getch();
        
        if (ch == 27) {  /* ESC */
            return;
        }
        else if (ch == 13 && i > 0) {  /* ENTER */
            filename[i] = '\0';
            break;
        }
        else if (ch == 8 && i > 0) {  /* BACKSPACE */
            i--;
            printf("\b \b");
        }
        else if (ch >= 32 && ch < 127 && i < 79) {
            filename[i++] = ch;
            putchar(ch);
        }
    }
    
    /* Open file */
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        gotoxy(1, yPos);
        clreol();
        textcolor(LIGHTRED);
        printf("ERROR: Cannot open file! Press any key...");
        textcolor(LIGHTGRAY);
        getch();
        return;
    }
    
    /* Ask for listing file */
    gotoxy(1, yPos);
    clreol();
    printf("Generate upload listing? (Y/N): ");
    ch = getch();
    
    if (ch == 'Y' || ch == 'y') {
        gotoxy(1, yPos);
        clreol();
        printf("Enter listing filename: ");
        
        i = 0;
        while (1) {
            ch = getch();
            
            if (ch == 27) {  /* ESC - skip listing */
                listFp = NULL;
                break;
            }
            else if (ch == 13 && i > 0) {  /* ENTER */
                listFilename[i] = '\0';
                listFp = fopen(listFilename, "w");
                if (listFp != NULL) {
                    fprintf(listFp, "EPROM Upload Listing\n");
                    fprintf(listFp, "====================\n");
                    fprintf(listFp, "Source file: %s\n\n", filename);
                    fprintf(listFp, "Offset   00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F  ASCII\n");
                    fprintf(listFp, "------------------------------------------------------------------------\n");
                }
                break;
            }
            else if (ch == 8 && i > 0) {  /* BACKSPACE */
                i--;
                printf("\b \b");
            }
            else if (ch >= 32 && ch < 127 && i < 79) {
                listFilename[i++] = ch;
                putchar(ch);
            }
        }
    } else {
        listFp = NULL;
    }
    
    /* Determine if file is binary or ASCII hex */
    fseek(fp, 0, SEEK_SET);
    i = 0;
    while (i < 100 && !feof(fp)) {
        ch = fgetc(fp);
        if (ch == EOF) break;
        
        /* Check if it's ASCII hex format (only hex digits, spaces, newlines) */
        if (!((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F') || 
              (ch >= 'a' && ch <= 'f') || ch == ' ' || ch == '\r' || 
              ch == '\n' || ch == '\t')) {
            isBinary = 1;
            break;
        }
        i++;
    }
    
    fseek(fp, 0, SEEK_SET);
    
    clrscr();
    textcolor(YELLOW);
    printf("Uploading to EPROM programmer via COM1...\n");
    textcolor(LIGHTGRAY);
    printf("File: %s\n", filename);
    printf("Format: %s\n", isBinary ? "BINARY (converting to ASCII hex)" : "ASCII HEX");
    printf("Press ESC to cancel\n");
    printf("========================================================================\n");
    
    addr = 0;
    
    if (isBinary) {
        /* Binary file - convert to ASCII hex */
        while (!feof(fp) && addr < MAX_BUFFER_SIZE) {
            if (kbhit() && getch() == 27) {
                textcolor(LIGHTRED);
                printf("\nUpload cancelled by user!\n");
                textcolor(LIGHTGRAY);
                fclose(fp);
                if (listFp) fclose(listFp);
                delay(1000);
                return;
            }
            
            if (fread(&byte, 1, 1, fp) == 1) {
                /* Send as ASCII hex with space */
                WriteCOM1(NibbleToHex(byte >> 4));
                WriteCOM1(NibbleToHex(byte & 0x0F));
                WriteCOM1(' ');
                
                /* Display progress */
                printf("%02X ", byte);
                if ((addr + 1) % 16 == 0) {
                    printf("\n");
                }
                
                /* Write to listing */
                if (listFp) {
                    if (addr % BYTES_PER_LINE == 0) {
                        if (addr > 0) {
                            /* Write ASCII representation for previous line */
                            fprintf(listFp, " ");
                            for (col = 0; col < BYTES_PER_LINE; col++) {
                                unsigned char b = DataBuffer[addr - BYTES_PER_LINE + col];
                                fprintf(listFp, "%c", (b >= 32 && b < 127) ? b : '.');
                            }
                            fprintf(listFp, "\n");
                        }
                        fprintf(listFp, "%04X:    ", addr);
                    }
                    fprintf(listFp, "%02X ", byte);
                }
                
                /* Store in buffer for listing */
                if (addr < MAX_BUFFER_SIZE) {
                    DataBuffer[addr] = byte;
                }
                
                addr++;
                delay(10);  /* Small delay to prevent buffer overflow */
            }
        }
    } else {
        /* ASCII hex file - send as-is */
        char hexStr[3];
        int hexPos = 0;
        unsigned int value;
        
        hexStr[0] = 0;
        hexStr[1] = 0;
        hexStr[2] = 0;
        
        while (!feof(fp) && addr < MAX_BUFFER_SIZE) {
            if (kbhit() && getch() == 27) {
                textcolor(LIGHTRED);
                printf("\nUpload cancelled by user!\n");
                textcolor(LIGHTGRAY);
                fclose(fp);
                if (listFp) fclose(listFp);
                delay(1000);
                return;
            }
            
            ch = fgetc(fp);
            if (ch == EOF) break;
            
            /* Forward to COM1 */
            WriteCOM1((unsigned char)ch);
            
            /* Parse for listing */
            if ((ch >= '0' && ch <= '9') || 
                (ch >= 'A' && ch <= 'F') || 
                (ch >= 'a' && ch <= 'f')) {
                hexStr[hexPos++] = ch;
                
                if (hexPos == 2) {
                    hexStr[2] = '\0';
                    sscanf(hexStr, "%x", &value);
                    byte = (unsigned char)value;
                    hexPos = 0;
                    
                    /* Display progress */
                    printf("%02X ", byte);
                    if ((addr + 1) % 16 == 0) {
                        printf("\n");
                    }
                    
                    /* Write to listing */
                    if (listFp) {
                        if (addr % BYTES_PER_LINE == 0) {
                            if (addr > 0) {
                                fprintf(listFp, " ");
                                for (col = 0; col < BYTES_PER_LINE; col++) {
                                    unsigned char b = DataBuffer[addr - BYTES_PER_LINE + col];
                                    fprintf(listFp, "%c", (b >= 32 && b < 127) ? b : '.');
                                }
                                fprintf(listFp, "\n");
                            }
                            fprintf(listFp, "%04X:    ", addr);
                        }
                        fprintf(listFp, "%02X ", byte);
                    }
                    
                    /* Store in buffer */
                    if (addr < MAX_BUFFER_SIZE) {
                        DataBuffer[addr] = byte;
                    }
                    
                    addr++;
                }
            } else if (ch == ' ' || ch == '\r' || ch == '\n' || ch == '\t') {
                hexPos = 0;
            }
            
            delay(5);
        }
    }
    
    /* Finish listing */
    if (listFp) {
        if (addr > 0 && addr % BYTES_PER_LINE != 0) {
            /* Pad incomplete line */
            for (col = addr % BYTES_PER_LINE; col < BYTES_PER_LINE; col++) {
                fprintf(listFp, "   ");
            }
            fprintf(listFp, " ");
            for (col = 0; col < addr % BYTES_PER_LINE; col++) {
                unsigned char b = DataBuffer[addr - (addr % BYTES_PER_LINE) + col];
                fprintf(listFp, "%c", (b >= 32 && b < 127) ? b : '.');
            }
            fprintf(listFp, "\n");
        }
        fprintf(listFp, "\nTotal bytes uploaded: %u\n", addr);
        fclose(listFp);
    }
    
    fclose(fp);
    
    printf("\n========================================================================\n");
    textcolor(LIGHTGREEN);
    printf("Upload complete! %u bytes sent to EPROM programmer.\n", addr);
    if (listFp) {
        printf("Listing saved to: %s\n", listFilename);
    }
    textcolor(LIGHTGRAY);
    printf("Press any key to continue...");
    getch();
}

/* Added function to load file into buffer */
void LoadFileIntoBuffer(void) {
    char filename[80];
    FILE *fp;
    unsigned char byte;
    int i;
    int ch;
    
    textcolor(LIGHTCYAN);
    printf("\nEnter filename to load (or ESC to cancel): ");
    textcolor(LIGHTGRAY);
    
    i = 0;
    while (1) {
        ch = getch();
        
        if (ch == 27) {  /* ESC */
            BufferSize = 0;
            return;
        }
        else if (ch == 13 && i > 0) {  /* ENTER */
            filename[i] = '\0';
            break;
        }
        else if (ch == 8 && i > 0) {  /* BACKSPACE */
            i--;
            printf("\b \b");
        }
        else if (ch >= 32 && ch < 127 && i < 79) {
            filename[i++] = ch;
            putchar(ch);
        }
    }
    
    /* Open file */
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        textcolor(LIGHTRED);
        printf("\n\nERROR: Cannot open file '%s'!\n", filename);
        textcolor(LIGHTGRAY);
        printf("Press any key to continue...");
        getch();
        BufferSize = 0;
        return;
    }
    
    /* Read file into buffer */
    BufferSize = 0;
    textcolor(LIGHTGREEN);
    printf("\n\nLoading file...\n");
    
    while (BufferSize < MAX_BUFFER_SIZE && fread(&byte, 1, 1, fp) == 1) {
        DataBuffer[BufferSize++] = byte;
        
        /* Show progress every 256 bytes */
        if ((BufferSize % 256) == 0) {
            printf("\rBytes loaded: %u", BufferSize);
        }
    }
    
    fclose(fp);
    
    textcolor(LIGHTGREEN);
    printf("\n\nFile loaded successfully! %u bytes read.\n", BufferSize);
    textcolor(LIGHTGRAY);
    printf("Press any key to enter editor...");
    getch();
}

/* Added startup menu function */
int ShowStartupMenu(void) {
    int ch;
    
    textmode(C80);
    clrscr();
    
    textcolor(LIGHTCYAN);
    printf("========================================================================\n");
    printf("               EPROM Data Reader & Hex Editor v1.1\n");
    printf("                   Developer: Mickey W. Lawless\n");
    printf("                    Completed: December 21, 2025\n");
    printf("========================================================================\n\n");
    
    textcolor(YELLOW);
    printf("Main Menu:\n\n");
    textcolor(LIGHTGRAY);
    printf("  1. Read data from COM1 (EPROM Programmer)\n");
    printf("  2. Load file into buffer\n");
    printf("  3. Enter hex editor (empty buffer)\n");
    printf("  4. Exit\n\n");
    
    textcolor(LIGHTGRAY);
    printf("Enter choice (1-4): ");
    
    while (1) {
        ch = getch();
        
        if (ch >= '1' && ch <= '4') {
            printf("%c\n", ch);
            return ch - '0';
        }
        else if (ch == 27) {  /* ESC - treat as exit */
            return 4;
        }
    }
}

/* Added disassembler function */
void Disassemble(void) {
    char outbuf[80];
    char filename[80];
    char cpuname[10];
    int i, addr;
    FILE *fp;
    int yPos = 23;
    
    /* Generate filename based on CPU type */
    if (CPUType == CPU_6800) {
        strcpy(cpuname, "6800");
    } else if (CPUType == CPU_8080) {
        strcpy(cpuname, "8080");
    } else {
        strcpy(cpuname, "8086");
    }
    
    sprintf(filename, "DISASM_%s.ASM", cpuname);
    
    gotoxy(1, yPos);
    textcolor(LIGHTGRAY);
    textbackground(BLACK);
    clreol();
    printf("Enter start address (hex) or press ENTER for 0000: ");
    
    i = 0;
    addr = 0;
    outbuf[0] = '\0';
    
    while (1) {
        int ch = getch();
        
        if (ch == 27) {  /* ESC */
            return;
        }
        else if (ch == 13) {  /* ENTER */
            if (i > 0) {
                outbuf[i] = '\0';
                sscanf(outbuf, "%X", &addr);
            }
            break;
        }
        else if (ch == 8 && i > 0) {  /* BACKSPACE */
            i--;
            printf("\b \b");
        }
        else if (ch >= '0' && ch <= '9' && i < 79) {
            outbuf[i++] = ch;
            putchar(ch);
        }
        else if (((ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f')) && i < 79) {
            outbuf[i++] = ch;
            putchar(ch);
        }
    }
    
    /* Open file for writing disassembly */
    fp = fopen(filename, "w");
    if (!fp) {
        clrscr();
        textcolor(LIGHTRED);
        printf("ERROR: Cannot create file %s\n", filename);
        textcolor(LIGHTGRAY);
        printf("Press any key to continue...");
        getch();
        return;
    }
    
    clrscr();
    textcolor(YELLOW);
    printf("Disassembling from address %04X...\n", addr);
    printf("Saving to: %s\n", filename);
    textcolor(LIGHTGRAY);
    printf("Press ESC to exit disassembler.\n");
    printf("========================================================================\n");
    
    /* Write header to file */
    fprintf(fp, "; Disassembly Output\n");
    fprintf(fp, "; CPU Type: %s\n", cpuname);
    fprintf(fp, "; Start Address: %04X\n", addr);
    fprintf(fp, "; Developer: Mickey W. Lawless\n");
    fprintf(fp, "; Generated: %s\n", __DATE__);
    fprintf(fp, ";\n");
    fprintf(fp, "; ========================================================================\n\n");
    
    while (addr < BufferSize) {
        if (kbhit() && getch() == 27) {
            printf("\n");
            textcolor(LIGHTRED);
            printf("Disassembly cancelled by user.\n");
            textcolor(LIGHTGRAY);
            /* Close file before returning */
            fclose(fp);
            delay(1000);
            return;
        }
        
        if (CPUType == CPU_6800) {
            int len = Disasm6800(addr, outbuf);
            if (len > 0) {
                printf("%04X: %s\n", addr, outbuf);
                fprintf(fp, "%04X: %s\n", addr, outbuf);  /* Write to file */
                addr += len;
            } else {
                printf("%04X: DB %02X\n", addr, DataBuffer[addr]);
                fprintf(fp, "%04X: DB %02X\n", addr, DataBuffer[addr]);  /* Write to file */
                addr++;
            }
        } else if (CPUType == CPU_8080) {
            int len = Disasm8080(addr, outbuf);
            if (len > 0) {
                printf("%04X: %s\n", addr, outbuf);
                fprintf(fp, "%04X: %s\n", addr, outbuf);  /* Write to file */
                addr += len;
            } else {
                printf("%04X: DB %02X\n", addr, DataBuffer[addr]);
                fprintf(fp, "%04X: DB %02X\n", addr, DataBuffer[addr]);  /* Write to file */
                addr++;
            }
        } else if (CPUType == CPU_8086) {
            int len = Disasm8086(addr, outbuf);
            if (len > 0) {
                printf("%04X: %s\n", addr, outbuf);
                fprintf(fp, "%04X: %s\n", addr, outbuf);  /* Write to file */
                addr += len;
            } else {
                printf("%04X: DB %02X\n", addr, DataBuffer[addr]);
                fprintf(fp, "%04X: DB %02X\n", addr, DataBuffer[addr]);  /* Write to file */
                addr++;
            }
        }
    }
    
    /* Close file and show completion message */
    fclose(fp);
    
    printf("\n");
    textcolor(LIGHTGREEN);
    printf("Disassembly complete! Saved to: %s\n", filename);
    textcolor(LIGHTGRAY);
    printf("Press any key to continue...");
    getch();
}

/* Save disassembly to file */
void SaveDisassembly(int cpuType) {
    FILE *fp;
    char filename[80];
    char asmLine[120];
    unsigned int addr = 0;
    int len;
    int lineCount = 0;
    const char *cpuName;
    
    textcolor(LIGHTCYAN);
    printf("\nEnter filename for disassembly listing: ");
    textcolor(WHITE);
    scanf("%s", filename);
    
    fp = fopen(filename, "w");
    if (!fp) {
        textcolor(LIGHTRED);
        printf("\n\nERROR: Cannot create file '%s'\n", filename);
        textcolor(LIGHTGRAY);
        printf("Press any key to continue...");
        getch();
        return;
    }
    
    /* Determine CPU name */
    switch (cpuType) {
        case CPU_6800: cpuName = "Motorola 6800"; break;
        case CPU_8080: cpuName = "Intel 8080"; break;
        case CPU_8086: cpuName = "Intel 8086"; break;
        default: cpuName = "Unknown"; break;
    }
    
    /* Write header */
    fprintf(fp, "; Disassembly generated by EPROM Reader\n");
    fprintf(fp, "; CPU: %s\n", cpuName);
    fprintf(fp, "; Buffer size: %u bytes\n", BufferSize);
    fprintf(fp, ";\n\n");
    
    textcolor(LIGHTGREEN);
    printf("\nDisassembling...\n");
    
    /* Disassemble buffer */
    while (addr < BufferSize) {
        /* Call appropriate disassembler */
        switch (cpuType) {
            case CPU_6800:
                len = Disasm6800(addr, asmLine);
                break;
            case CPU_8080:
                len = Disasm8080(addr, asmLine);
                break;
            case CPU_8086:
                len = Disasm8086(addr, asmLine);
                break;
            default:
                len = 1;
                sprintf(asmLine, "DB $%02X", DataBuffer[addr]);
                break;
        }
        
        /* Write line with address and hex bytes */
        fprintf(fp, "%04X: ", addr);
        
        /* Write hex bytes */
        {
            int i;
            for (i = 0; i < len && addr + i < BufferSize; i++) {
                fprintf(fp, "%02X ", DataBuffer[addr + i]);
            }
            for (; i < 4; i++) {
                fprintf(fp, "   ");
            }
        }
        
        fprintf(fp, "  %s\n", asmLine);
        
        addr += len;
        lineCount++;
        
        /* Show progress every 100 lines */
        if (lineCount % 100 == 0) {
            printf("  %u bytes processed...\n", addr);
        }
    }
    
    fclose(fp);
    
    textcolor(LIGHTGREEN);
    printf("\nDisassembly saved to '%s'\n", filename);
    printf("%u lines written.\n", lineCount);
    textcolor(LIGHTGRAY);
    printf("\nPress any key to continue...");
    getch();
}

void DisassembleMenu(void) {
    int ch;
    
    clrscr();
    textcolor(YELLOW);
    printf("====================================\n");
    printf("  Disassembler\n");
    printf("====================================\n\n");
    textcolor(LIGHTGRAY);
    
    printf("Select CPU type:\n\n");
    textcolor(LIGHTCYAN);
    printf("  1. Motorola 6800\n");
    printf("  2. Intel 8080\n");
    printf("  3. Intel 8086\n");
    printf("  ESC. Cancel\n\n");
    textcolor(LIGHTGRAY);
    printf("Enter choice (1-3): ");
    
    while (1) {
        ch = getch();
        
        if (ch >= '1' && ch <= '3') {
            printf("%c\n", ch);
            CPUType = ch - '0'; /* Correctly assigned to global CPUType */
            break;
        }
        else if (ch == 27) {
            return;
        }
    }
    
    Disassemble();
}

/* Disassemble one instruction for Motorola 6800 */
int Disasm6800(unsigned int addr, char *output) {
    unsigned char opcode;
    unsigned char operand1, operand2;
    int len = 1;
    
    if (addr >= BufferSize) {
        sprintf(output, "FCB $??");
        return 1;
    }
    
    opcode = DataBuffer[addr];
    operand1 = (addr + 1 < BufferSize) ? DataBuffer[addr+1] : 0;
    operand2 = (addr + 2 < BufferSize) ? DataBuffer[addr+2] : 0;
    
    /* Complete Motorola 6800 instruction set */
    switch (opcode) {
        /* Inherent addressing mode - no operands */
        case 0x01: sprintf(output, "NOP"); break;
        case 0x06: sprintf(output, "TAP"); break;
        case 0x07: sprintf(output, "TPA"); break;
        case 0x08: sprintf(output, "INX"); break;
        case 0x09: sprintf(output, "DEX"); break;
        case 0x0A: sprintf(output, "CLV"); break;
        case 0x0B: sprintf(output, "SEV"); break;
        case 0x0C: sprintf(output, "CLC"); break;
        case 0x0D: sprintf(output, "SEC"); break;
        case 0x0E: sprintf(output, "CLI"); break;
        case 0x0F: sprintf(output, "SEI"); break;
        case 0x10: sprintf(output, "SBA"); break;
        case 0x11: sprintf(output, "CBA"); break;
        case 0x16: sprintf(output, "TAB"); break;
        case 0x17: sprintf(output, "TBA"); break;
        case 0x19: sprintf(output, "DAA"); break;
        case 0x1B: sprintf(output, "ABA"); break;
        case 0x30: sprintf(output, "TSX"); break;
        case 0x31: sprintf(output, "INS"); break;
        case 0x32: sprintf(output, "PULA"); break;
        case 0x33: sprintf(output, "PULB"); break;
        case 0x34: sprintf(output, "DES"); break;
        case 0x35: sprintf(output, "TXS"); break;
        case 0x36: sprintf(output, "PSHA"); break;
        case 0x37: sprintf(output, "PSHB"); break;
        case 0x39: sprintf(output, "RTS"); break;
        case 0x3B: sprintf(output, "RTI"); break;
        case 0x3E: sprintf(output, "WAI"); break;
        case 0x3F: sprintf(output, "SWI"); break;
        case 0x40: sprintf(output, "NEGA"); break;
        case 0x43: sprintf(output, "COMA"); break;
        case 0x44: sprintf(output, "LSRA"); break;
        case 0x46: sprintf(output, "RORA"); break;
        case 0x47: sprintf(output, "ASRA"); break;
        case 0x48: sprintf(output, "ASLA"); break;
        case 0x49: sprintf(output, "ROLA"); break;
        case 0x4A: sprintf(output, "DECA"); break;
        case 0x4C: sprintf(output, "INCA"); break;
        case 0x4D: sprintf(output, "TSTA"); break;
        case 0x4F: sprintf(output, "CLRA"); break;
        case 0x50: sprintf(output, "NEGB"); break;
        case 0x53: sprintf(output, "COMB"); break;
        case 0x54: sprintf(output, "LSRB"); break;
        case 0x56: sprintf(output, "RORB"); break;
        case 0x57: sprintf(output, "ASRB"); break;
        case 0x58: sprintf(output, "ASLB"); break;
        case 0x59: sprintf(output, "ROLB"); break;
        case 0x5A: sprintf(output, "DECB"); break;
        case 0x5C: sprintf(output, "INCB"); break;
        case 0x5D: sprintf(output, "TSTB"); break;
        case 0x5F: sprintf(output, "CLRB"); break;
        
        /* Relative branches (2 bytes) */
        case 0x20: sprintf(output, "BRA $%04X", (addr + 2 + (signed char)operand1) & 0xFFFF); len=2; break;
        case 0x21: sprintf(output, "BRN $%04X", (addr + 2 + (signed char)operand1) & 0xFFFF); len=2; break;
        case 0x22: sprintf(output, "BHI $%04X", (addr + 2 + (signed char)operand1) & 0xFFFF); len=2; break;
        case 0x23: sprintf(output, "BLS $%04X", (addr + 2 + (signed char)operand1) & 0xFFFF); len=2; break;
        case 0x24: sprintf(output, "BCC $%04X", (addr + 2 + (signed char)operand1) & 0xFFFF); len=2; break;
        case 0x25: sprintf(output, "BCS $%04X", (addr + 2 + (signed char)operand1) & 0xFFFF); len=2; break;
        case 0x26: sprintf(output, "BNE $%04X", (addr + 2 + (signed char)operand1) & 0xFFFF); len=2; break;
        case 0x27: sprintf(output, "BEQ $%04X", (addr + 2 + (signed char)operand1) & 0xFFFF); len=2; break;
        case 0x28: sprintf(output, "BVC $%04X", (addr + 2 + (signed char)operand1) & 0xFFFF); len=2; break;
        case 0x29: sprintf(output, "BVS $%04X", (addr + 2 + (signed char)operand1) & 0xFFFF); len=2; break;
        case 0x2A: sprintf(output, "BPL $%04X", (addr + 2 + (signed char)operand1) & 0xFFFF); len=2; break;
        case 0x2B: sprintf(output, "BMI $%04X", (addr + 2 + (signed char)operand1) & 0xFFFF); len=2; break;
        case 0x2C: sprintf(output, "BGE $%04X", (addr + 2 + (signed char)operand1) & 0xFFFF); len=2; break;
        case 0x2D: sprintf(output, "BLT $%04X", (addr + 2 + (signed char)operand1) & 0xFFFF); len=2; break;
        case 0x2E: sprintf(output, "BGT $%04X", (addr + 2 + (signed char)operand1) & 0xFFFF); len=2; break;
        case 0x2F: sprintf(output, "BLE $%04X", (addr + 2 + (signed char)operand1) & 0xFFFF); len=2; break;
        
        /* JSR and JMP */
        case 0x8D: sprintf(output, "BSR $%04X", (addr + 2 + (signed char)operand1) & 0xFFFF); len=2; break;
        case 0x9D: sprintf(output, "JSR $%02X", operand1); len=2; break;
        case 0xAD: sprintf(output, "JSR %u,X", operand1); len=2; break; /* Fixed hex constant - removed space between A and D */
        case 0xBD: sprintf(output, "JSR $%04X", (operand1 << 8) | operand2); len=3; break; /* Extended addressing */
        
        case 0x6E: sprintf(output, "JMP %u,X", operand1); len=2; break; /* Direct addressing with index register X */
        case 0x7E: sprintf(output, "JMP $%04X", (operand1 << 8) | operand2); len=3; break; /* Extended addressing */
        
        /* LDAA - Load Accumulator A */
        case 0x86: sprintf(output, "LDAA #$%02X", operand1); len=2; break; // Immediate
        case 0x96: sprintf(output, "LDAA $%02X", operand1); len=2; break; // Direct
        case 0xA6: sprintf(output, "LDAA %u,X", operand1); len=2; break; // Indexed, zero offset
        case 0xB6: sprintf(output, "LDAA $%04X", (operand1 << 8) | operand2); len=3; break; // Extended
        
        /* LDAB - Load Accumulator B */
        case 0xC6: sprintf(output, "LDAB #$%02X", operand1); len=2; break; // Immediate
        case 0xD6: sprintf(output, "LDAB $%02X", operand1); len=2; break; // Direct
        case 0xE6: sprintf(output, "LDAB %u,X", operand1); len=2; break; // Indexed, zero offset
        case 0xF6: sprintf(output, "LDAB $%04X", (operand1 << 8) | operand2); len=3; break; // Extended
        
        /* STAA - Store Accumulator A */
        case 0x97: sprintf(output, "STAA $%02X", operand1); len=2; break; // Direct
        case 0xA7: sprintf(output, "STAA %u,X", operand1); len=2; break; // Indexed, zero offset
        case 0xB7: sprintf(output, "STAA $%04X", (operand1 << 8) | operand2); len=3; break; // Extended
        
        /* STAB - Store Accumulator B */
        case 0xD7: sprintf(output, "STAB $%02X", operand1); len=2; break; // Direct
        case 0xE7: sprintf(output, "STAB %u,X", operand1); len=2; break; // Indexed, zero offset
        case 0xF7: sprintf(output, "STAB $%04X", (operand1 << 8) | operand2); len=3; break; // Extended
        
        /* ADDA - Add to Accumulator A */
        case 0x8B: sprintf(output, "ADDA #$%02X", operand1); len=2; break; // Immediate
        case 0x9B: sprintf(output, "ADDA $%02X", operand1); len=2; break; // Direct
        case 0xAB: sprintf(output, "ADDA %u,X", operand1); len=2; break; // Indexed, zero offset
        case 0xBB: sprintf(output, "ADDA $%04X", (operand1 << 8) | operand2); len=3; break; // Extended
        
        /* ADDB - Add to Accumulator B */
        case 0xCB: sprintf(output, "ADDB #$%02X", operand1); len=2; break; // Immediate
        case 0xDB: sprintf(output, "ADDB $%02X", operand1); len=2; break; // Direct
        case 0xEB: sprintf(output, "ADDB %u,X", operand1); len=2; break; // Indexed, zero offset
        case 0xFB: sprintf(output, "ADDB $%04X", (operand1 << 8) | operand2); len=3; break; // Extended
        
        /* SUBA - Subtract from Accumulator A */
        case 0x80: sprintf(output, "SUBA #$%02X", operand1); len=2; break; // Immediate
        case 0x90: sprintf(output, "SUBA $%02X", operand1); len=2; break; // Direct
        case 0xA0: sprintf(output, "SUBA %u,X", operand1); len=2; break; // Indexed, zero offset
        case 0xB0: sprintf(output, "SUBA $%04X", (operand1 << 8) | operand2); len=3; break; // Extended
        
        /* SUBB - Subtract from Accumulator B */
        case 0xC0: sprintf(output, "SUBB #$%02X", operand1); len=2; break; // Immediate
        case 0xD0: sprintf(output, "SUBB $%02X", operand1); len=2; break; // Direct
        case 0xE0: sprintf(output, "SUBB %u,X", operand1); len=2; break; // Indexed, zero offset
        case 0xF0: sprintf(output, "SUBB $%04X", (operand1 << 8) | operand2); len=3; break; // Extended
        
        /* CMPA - Compare Accumulator A */
        case 0x81: sprintf(output, "CMPA #$%02X", operand1); len=2; break; // Immediate
        case 0x91: sprintf(output, "CMPA $%02X", operand1); len=2; break; // Direct
        case 0xA1: sprintf(output, "CMPA %u,X", operand1); len=2; break; // Indexed, zero offset
        case 0xB1: sprintf(output, "CMPA $%04X", (operand1 << 8) | operand2); len=3; break; // Extended
        
        /* CMPB - Compare Accumulator B */
        case 0xC1: sprintf(output, "CMPB #$%02X", operand1); len=2; break; // Immediate
        case 0xD1: sprintf(output, "CMPB $%02X", operand1); len=2; break; // Direct
        case 0xE1: sprintf(output, "CMPB %u,X", operand1); len=2; break; // Indexed, zero offset
        case 0xF1: sprintf(output, "CMPB $%04X", (operand1 << 8) | operand2); len=3; break; // Extended
        
        /* ANDA - AND Accumulator A */
        case 0x84: sprintf(output, "ANDA #$%02X", operand1); len=2; break; // Immediate
        case 0x94: sprintf(output, "ANDA $%02X", operand1); len=2; break; // Direct
        case 0xA4: sprintf(output, "ANDA %u,X", operand1); len=2; break; // Indexed, zero offset
        case 0xB4: sprintf(output, "ANDA $%04X", (operand1 << 8) | operand2); len=3; break; // Extended
        
        /* ANDB - AND Accumulator B */
        case 0xC4: sprintf(output, "ANDB #$%02X", operand1); len=2; break; // Immediate
        case 0xD4: sprintf(output, "ANDB $%02X", operand1); len=2; break; // Direct
        case 0xE4: sprintf(output, "ANDB %u,X", operand1); len=2; break; // Indexed, zero offset
        case 0xF4: sprintf(output, "ANDB $%04X", (operand1 << 8) | operand2); len=3; break; // Extended
        
        /* ORAA - OR Accumulator A */
        case 0x8A: sprintf(output, "ORAA #$%02X", operand1); len=2; break; // Immediate
        case 0x9A: sprintf(output, "ORAA $%02X", operand1); len=2; break; // Direct
        case 0xAA: sprintf(output, "ORAA %u,X", operand1); len=2; break; // Indexed, zero offset
        case 0xBA: sprintf(output, "ORAA $%04X", (operand1 << 8) | operand2); len=3; break; // Extended
        
        /* ORAB - OR Accumulator B */
        case 0xCA: sprintf(output, "ORAB #$%02X", operand1); len=2; break; // Immediate
        case 0xDA: sprintf(output, "ORAB $%02X", operand1); len=2; break; // Direct
        case 0xEA: sprintf(output, "ORAB %u,X", operand1); len=2; break; // Indexed, zero offset
        case 0xFA: sprintf(output, "ORAB $%04X", (operand1 << 8) | operand2); len=3; break; // Extended
        
        /* EORA - XOR Accumulator A */
        case 0x88: sprintf(output, "EORA #$%02X", operand1); len=2; break; // Immediate
        case 0x98: sprintf(output, "EORA $%02X", operand1); len=2; break; // Direct
        case 0xA8: sprintf(output, "EORA %u,X", operand1); len=2; break; // Indexed, zero offset
        case 0xB8: sprintf(output, "EORA $%04X", (operand1 << 8) | operand2); len=3; break; // Extended
        
        /* EORB - XOR Accumulator B */
        case 0xC8: sprintf(output, "EORB #$%02X", operand1); len=2; break; // Immediate
        case 0xD8: sprintf(output, "EORB $%02X", operand1); len=2; break; // Direct
        case 0xE8: sprintf(output, "EORB %u,X", operand1); len=2; break; // Indexed, zero offset
        case 0xF8: sprintf(output, "EORB $%04X", (operand1 << 8) | operand2); len=3; break; // Extended
        
        /* BITA - Bit Test Accumulator A */
        case 0x85: sprintf(output, "BITA #$%02X", operand1); len=2; break; // Immediate
        case 0x95: sprintf(output, "BITA $%02X", operand1); len=2; break; // Direct
        case 0xA5: sprintf(output, "BITA %u,X", operand1); len=2; break; // Indexed, zero offset
        case 0xB5: sprintf(output, "BITA $%04X", (operand1 << 8) | operand2); len=3; break; // Extended
        
        /* BITB - Bit Test Accumulator B */
        case 0xC5: sprintf(output, "BITB #$%02X", operand1); len=2; break; // Immediate
        case 0xD5: sprintf(output, "BITB $%02X", operand1); len=2; break; // Direct
        case 0xE5: sprintf(output, "BITB %u,X", operand1); len=2; break; // Indexed, zero offset
        case 0xF5: sprintf(output, "BITB $%04X", (operand1 << 8) | operand2); len=3; break; // Extended
        
        /* LDS - Load Stack Pointer */
        case 0x8E: sprintf(output, "LDS #$%04X", (operand1 << 8) | operand2); len=3; break; // Immediate
        case 0x9E: sprintf(output, "LDS $%02X", operand1); len=2; break; // Direct
        case 0xAE: sprintf(output, "LDS %u,X", operand1); len=2; break; // Indexed, zero offset
        case 0xBE: sprintf(output, "LDS $%04X", (operand1 << 8) | operand2); len=3; break; // Extended
        
        /* STS - Store Stack Pointer */
        case 0x9F: sprintf(output, "STS $%02X", operand1); len=2; break; // Direct
        case 0xAF: sprintf(output, "STS %u,X", operand1); len=2; break; // Indexed, zero offset
        case 0xBF: sprintf(output, "STS $%04X", (operand1 << 8) | operand2); len=3; break; // Extended
        
        /* LDX - Load Index Register */
        case 0xCE: sprintf(output, "LDX #$%04X", (operand1 << 8) | operand2); len=3; break; // Immediate
        case 0xDE: sprintf(output, "LDX $%02X", operand1); len=2; break; // Direct
        case 0xEE: sprintf(output, "LDX %u,X", operand1); len=2; break; // Indexed, zero offset
        case 0xFE: sprintf(output, "LDX $%04X", (operand1 << 8) | operand2); len=3; break; // Extended
        
        /* STX - Store Index Register */
        case 0xDF: sprintf(output, "STX $%02X", operand1); len=2; break; // Direct
        case 0xEF: sprintf(output, "STX %u,X", operand1); len=2; break; // Indexed, zero offset
        case 0xFF: sprintf(output, "STX $%04X", (operand1 << 8) | operand2); len=3; break; // Extended
        
        /* CPX - Compare Index Register */
        case 0x8C: sprintf(output, "CPX #$%04X", (operand1 << 8) | operand2); len=3; break; // Immediate
        case 0x9C: sprintf(output, "CPX $%02X", operand1); len=2; break; // Direct
        case 0xAC: sprintf(output, "CPX %u,X", operand1); len=2; break; // Indexed, zero offset
        case 0xBC: sprintf(output, "CPX $%04X", (operand1 << 8) | operand2); len=3; break; // Extended
        
        /* Memory operations - Direct and Indexed */
        case 0x60: sprintf(output, "NEG %u,X", operand1); len=2; break;
        case 0x63: sprintf(output, "COM %u,X", operand1); len=2; break;
        case 0x64: sprintf(output, "LSR %u,X", operand1); len=2; break;
        case 0x66: sprintf(output, "ROR %u,X", operand1); len=2; break;
        case 0x67: sprintf(output, "ASR %u,X", operand1); len=2; break;
        case 0x68: sprintf(output, "ASL %u,X", operand1); len=2; break;
        case 0x69: sprintf(output, "ROL %u,X", operand1); len=2; break;
        case 0x6A: sprintf(output, "DEC %u,X", operand1); len=2; break;
        case 0x6C: sprintf(output, "INC %u,X", operand1); len=2; break;
        case 0x6D: sprintf(output, "TST %u,X", operand1); len=2; break;
        case 0x6F: sprintf(output, "CLR %u,X", operand1); len=2; break;
        
        case 0x70: sprintf(output, "NEG $%04X", (operand1 << 8) | operand2); len=3; break;
        case 0x73: sprintf(output, "COM $%04X", (operand1 << 8) | operand2); len=3; break;
        case 0x74: sprintf(output, "LSR $%04X", (operand1 << 8) | operand2); len=3; break;
        case 0x76: sprintf(output, "ROR $%04X", (operand1 << 8) | operand2); len=3; break;
        case 0x77: sprintf(output, "ASR $%04X", (operand1 << 8) | operand2); len=3; break;
        case 0x78: sprintf(output, "ASL $%04X", (operand1 << 8) | operand2); len=3; break;
        case 0x79: sprintf(output, "ROL $%04X", (operand1 << 8) | operand2); len=3; break;
        case 0x7A: sprintf(output, "DEC $%04X", (operand1 << 8) | operand2); len=3; break;
        case 0x7C: sprintf(output, "INC $%04X", (operand1 << 8) | operand2); len=3; break;
        case 0x7D: sprintf(output, "TST $%04X", (operand1 << 8) | operand2); len=3; break;
        case 0x7F: sprintf(output, "CLR $%04X", (operand1 << 8) | operand2); len=3; break;
        
        case 0x89: sprintf(output, "ADCA #$%02X", operand1); len=2; break;
        case 0x99: sprintf(output, "ADCA $%02X", operand1); len=2; break;
        case 0xA9: sprintf(output, "ADCA %u,X", operand1); len=2; break;
        case 0xB9: sprintf(output, "ADCA $%04X", (operand1 << 8) | operand2); len=3; break;
        
        case 0xC9: sprintf(output, "ADCB #$%02X", operand1); len=2; break;
        case 0xD9: sprintf(output, "ADCB $%02X", operand1); len=2; break;
        case 0xE9: sprintf(output, "ADCB %u,X", operand1); len=2; break;
        case 0xF9: sprintf(output, "ADCB $%04X", (operand1 << 8) | operand2); len=3; break;
        
        case 0x82: sprintf(output, "SBCA #$%02X", operand1); len=2; break;
        case 0x92: sprintf(output, "SBCA $%02X", operand1); len=2; break;
        case 0xA2: sprintf(output, "SBCA %u,X", operand1); len=2; break;
        case 0xB2: sprintf(output, "SBCA $%04X", (operand1 << 8) | operand2); len=3; break;
        
        case 0xC2: sprintf(output, "SBCB #$%02X", operand1); len=2; break;
        case 0xD2: sprintf(output, "SBCB $%02X", operand1); len=2; break;
        case 0xE2: sprintf(output, "SBCB %u,X", operand1); len=2; break;
        case 0xF2: sprintf(output, "SBCB $%04X", (operand1 << 8) | operand2); len=3; break;
        
        /* Undefined/illegal opcodes - use FCB */
        default: 
            sprintf(output, "FCB $%02X", opcode); 
            break;
    }
    
    return len;
}

/* Disassemble one instruction for Intel 8080 */
int Disasm8080(unsigned int addr, char *output) {
    unsigned char opcode, byte1, byte2;
    unsigned int targetAddr;
    int len = 1;
    
    if (addr >= BufferSize) {
        sprintf(output, "???");
        return 1;
    }
    
    opcode = DataBuffer[addr];
    byte1 = (addr + 1 < BufferSize) ? DataBuffer[addr+1] : 0;
    byte2 = (addr + 2 < BufferSize) ? DataBuffer[addr+2] : 0;
    
    /* Complete Intel 8080 instruction set - all 244 opcodes */
    switch (opcode) {
        /* NOP and control */
        case 0x00: sprintf(output, "NOP"); break;
        case 0x76: sprintf(output, "HLT"); break;
        case 0xFB: sprintf(output, "EI"); break;
        case 0xF3: sprintf(output, "DI"); break;
        
        /* MOV r,r instructions (64 opcodes) */
        case 0x40: sprintf(output, "MOV B,B"); break;
        case 0x41: sprintf(output, "MOV B,C"); break;
        case 0x42: sprintf(output, "MOV B,D"); break;
        case 0x43: sprintf(output, "MOV B,E"); break;
        case 0x44: sprintf(output, "MOV B,H"); break;
        case 0x45: sprintf(output, "MOV B,L"); break;
        case 0x46: sprintf(output, "MOV B,M"); break;
        case 0x47: sprintf(output, "MOV B,A"); break;
        
        case 0x48: sprintf(output, "MOV C,B"); break;
        case 0x49: sprintf(output, "MOV C,C"); break;
        case 0x4A: sprintf(output, "MOV C,D"); break;
        case 0x4B: sprintf(output, "MOV C,E"); break;
        case 0x4C: sprintf(output, "MOV C,H"); break;
        case 0x4D: sprintf(output, "MOV C,L"); break;
        case 0x4E: sprintf(output, "MOV C,M"); break;
        case 0x4F: sprintf(output, "MOV C,A"); break;
        
        case 0x50: sprintf(output, "MOV D,B"); break;
        case 0x51: sprintf(output, "MOV D,C"); break;
        case 0x52: sprintf(output, "MOV D,D"); break;
        case 0x53: sprintf(output, "MOV D,E"); break;
        case 0x54: sprintf(output, "MOV D,H"); break;
        case 0x55: sprintf(output, "MOV D,L"); break;
        case 0x56: sprintf(output, "MOV D,M"); break;
        case 0x57: sprintf(output, "MOV D,A"); break;
        
        case 0x58: sprintf(output, "MOV E,B"); break;
        case 0x59: sprintf(output, "MOV E,C"); break;
        case 0x5A: sprintf(output, "MOV E,D"); break;
        case 0x5B: sprintf(output, "MOV E,E"); break;
        case 0x5C: sprintf(output, "MOV E,H"); break;
        case 0x5D: sprintf(output, "MOV E,L"); break;
        case 0x5E: sprintf(output, "MOV E,M"); break;
        case 0x5F: sprintf(output, "MOV E,A"); break;
        
        case 0x60: sprintf(output, "MOV H,B"); break;
        case 0x61: sprintf(output, "MOV H,C"); break;
        case 0x62: sprintf(output, "MOV H,D"); break;
        case 0x63: sprintf(output, "MOV H,E"); break;
        case 0x64: sprintf(output, "MOV H,H"); break;
        case 0x65: sprintf(output, "MOV H,L"); break;
        case 0x66: sprintf(output, "MOV H,M"); break;
        case 0x67: sprintf(output, "MOV H,A"); break;
        
        case 0x68: sprintf(output, "MOV L,B"); break;
        case 0x69: sprintf(output, "MOV L,C"); break;
        case 0x6A: sprintf(output, "MOV L,D"); break;
        case 0x6B: sprintf(output, "MOV L,E"); break;
        case 0x6C: sprintf(output, "MOV L,H"); break;
        case 0x6D: sprintf(output, "MOV L,L"); break;
        case 0x6E: sprintf(output, "MOV L,M"); break;
        case 0x6F: sprintf(output, "MOV L,A"); break;
        
        case 0x70: sprintf(output, "MOV M,B"); break;
        case 0x71: sprintf(output, "MOV M,C"); break;
        case 0x72: sprintf(output, "MOV M,D"); break;
        case 0x73: sprintf(output, "MOV M,E"); break;
        case 0x74: sprintf(output, "MOV M,H"); break;
        case 0x75: sprintf(output, "MOV M,L"); break;
        case 0x77: sprintf(output, "MOV M,A"); break;
        
        case 0x78: sprintf(output, "MOV A,B"); break;
        case 0x79: sprintf(output, "MOV A,C"); break;
        case 0x7A: sprintf(output, "MOV A,D"); break;
        case 0x7B: sprintf(output, "MOV A,E"); break;
        case 0x7C: sprintf(output, "MOV A,H"); break;
        case 0x7D: sprintf(output, "MOV A,L"); break;
        case 0x7E: sprintf(output, "MOV A,M"); break;
        case 0x7F: sprintf(output, "MOV A,A"); break;
        
        /* MVI (Move Immediate) */
        case 0x06: sprintf(output, "MVI B,%02XH", byte1); len=2; break;
        case 0x0E: sprintf(output, "MVI C,%02XH", byte1); len=2; break;
        case 0x16: sprintf(output, "MVI D,%02XH", byte1); len=2; break;
        case 0x1E: sprintf(output, "MVI E,%02XH", byte1); len=2; break;
        case 0x26: sprintf(output, "MVI H,%02XH", byte1); len=2; break;
        case 0x2E: sprintf(output, "MVI L,%02XH", byte1); len=2; break;
        case 0x36: sprintf(output, "MVI M,%02XH", byte1); len=2; break;
        case 0x3E: sprintf(output, "MVI A,%02XH", byte1); len=2; break;
        
        /* LXI (Load register pair immediate) */
        case 0x01: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "LXI B,%04XH", targetAddr); 
            len=3; break;
        case 0x11: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "LXI D,%04XH", targetAddr); 
            len=3; break;
        case 0x21: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "LXI H,%04XH", targetAddr); 
            len=3; break;
        case 0x31: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "LXI SP,%04XH", targetAddr); 
            len=3; break;
        
        /* LDA/STA (Load/Store Accumulator) */
        case 0x3A: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "LDA %04XH", targetAddr); 
            len=3; break;
        case 0x32: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "STA %04XH", targetAddr); 
            len=3; break;
        
        /* LHLD/SHLD (Load/Store H&L) */
        case 0x2A: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "LHLD %04XH", targetAddr); 
            len=3; break;
        case 0x22: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "SHLD %04XH", targetAddr); 
            len=3; break;
        
        /* LDAX/STAX (Load/Store A indirect) */
        case 0x0A: sprintf(output, "LDAX B"); break;
        case 0x1A: sprintf(output, "LDAX D"); break;
        case 0x02: sprintf(output, "STAX B"); break;
        case 0x12: sprintf(output, "STAX D"); break;
        
        /* XCHG, XTHL, SPHL, PCHL */
        case 0xEB: sprintf(output, "XCHG"); break;
        case 0xE3: sprintf(output, "XTHL"); break;
        case 0xF9: sprintf(output, "SPHL"); break;
        case 0xE9: sprintf(output, "PCHL"); break;
        
        /* ADD instructions */
        case 0x80: sprintf(output, "ADD B"); break;
        case 0x81: sprintf(output, "ADD C"); break;
        case 0x82: sprintf(output, "ADD D"); break;
        case 0x83: sprintf(output, "ADD E"); break;
        case 0x84: sprintf(output, "ADD H"); break;
        case 0x85: sprintf(output, "ADD L"); break;
        case 0x86: sprintf(output, "ADD M"); break;
        case 0x87: sprintf(output, "ADD A"); break;
        case 0xC6: sprintf(output, "ADI %02XH", byte1); len=2; break;
        
        /* ADC (Add with Carry) */
        case 0x88: sprintf(output, "ADC B"); break;
        case 0x89: sprintf(output, "ADC C"); break;
        case 0x8A: sprintf(output, "ADC D"); break;
        case 0x8B: sprintf(output, "ADC E"); break;
        case 0x8C: sprintf(output, "ADC H"); break;
        case 0x8D: sprintf(output, "ADC L"); break;
        case 0x8E: sprintf(output, "ADC M"); break;
        case 0x8F: sprintf(output, "ADC A"); break;
        case 0xCE: sprintf(output, "ACI %02XH", byte1); len=2; break;
        
        /* SUB (Subtract) */
        case 0x90: sprintf(output, "SUB B"); break;
        case 0x91: sprintf(output, "SUB C"); break;
        case 0x92: sprintf(output, "SUB D"); break;
        case 0x93: sprintf(output, "SUB E"); break;
        case 0x94: sprintf(output, "SUB H"); break;
        case 0x95: sprintf(output, "SUB L"); break;
        case 0x96: sprintf(output, "SUB M"); break;
        case 0x97: sprintf(output, "SUB A"); break;
        case 0xD6: sprintf(output, "SUI %02XH", byte1); len=2; break;
        
        /* SBB (Subtract with Borrow) */
        case 0x98: sprintf(output, "SBB B"); break;
        case 0x99: sprintf(output, "SBB C"); break;
        case 0x9A: sprintf(output, "SBB D"); break;
        case 0x9B: sprintf(output, "SBB E"); break;
        case 0x9C: sprintf(output, "SBB H"); break;
        case 0x9D: sprintf(output, "SBB L"); break;
        case 0x9E: sprintf(output, "SBB M"); break;
        case 0x9F: sprintf(output, "SBB A"); break;
        case 0xDE: sprintf(output, "SBI %02XH", byte1); len=2; break;
        
        /* ANA (AND) */
        case 0xA0: sprintf(output, "ANA B"); break;
        case 0xA1: sprintf(output, "ANA C"); break;
        case 0xA2: sprintf(output, "ANA D"); break;
        case 0xA3: sprintf(output, "ANA E"); break;
        case 0xA4: sprintf(output, "ANA H"); break;
        case 0xA5: sprintf(output, "ANA L"); break;
        case 0xA6: sprintf(output, "ANA M"); break;
        case 0xA7: sprintf(output, "ANA A"); break;
        case 0xE6: sprintf(output, "ANI %02XH", byte1); len=2; break;
        
        /* XRA (XOR) */
        case 0xA8: sprintf(output, "XRA B"); break;
        case 0xA9: sprintf(output, "XRA C"); break;
        case 0xAA: sprintf(output, "XRA D"); break;
        case 0xAB: sprintf(output, "XRA E"); break;
        case 0xAC: sprintf(output, "XRA H"); break;
        case 0xAD: sprintf(output, "XRA L"); break;
        case 0xAE: sprintf(output, "XRA M"); break;
        case 0xAF: sprintf(output, "XRA A"); break;
        case 0xEE: sprintf(output, "XRI %02XH", byte1); len=2; break;
        
        /* ORA (OR) */
        case 0xB0: sprintf(output, "ORA B"); break;
        case 0xB1: sprintf(output, "ORA C"); break;
        case 0xB2: sprintf(output, "ORA D"); break;
        case 0xB3: sprintf(output, "ORA E"); break;
        case 0xB4: sprintf(output, "ORA H"); break;
        case 0xB5: sprintf(output, "ORA L"); break;
        case 0xB6: sprintf(output, "ORA M"); break;
        case 0xB7: sprintf(output, "ORA A"); break;
        case 0xF6: sprintf(output, "ORI %02XH", byte1); len=2; break;
        
        /* CMP (Compare) */
        case 0xB8: sprintf(output, "CMP B"); break;
        case 0xB9: sprintf(output, "CMP C"); break;
        case 0xBA: sprintf(output, "CMP D"); break;
        case 0xBB: sprintf(output, "CMP E"); break;
        case 0xBC: sprintf(output, "CMP H"); break;
        case 0xBD: sprintf(output, "CMP L"); break;
        case 0xBE: sprintf(output, "CMP M"); break;
        case 0xBF: sprintf(output, "CMP A"); break;
        case 0xFE: sprintf(output, "CPI %02XH", byte1); len=2; break;
        
        /* INR/DCR (Increment/Decrement register) */
        case 0x04: sprintf(output, "INR B"); break;
        case 0x0C: sprintf(output, "INR C"); break;
        case 0x14: sprintf(output, "INR D"); break;
        case 0x1C: sprintf(output, "INR E"); break;
        case 0x24: sprintf(output, "INR H"); break;
        case 0x2C: sprintf(output, "INR L"); break;
        case 0x34: sprintf(output, "INR M"); break;
        case 0x3C: sprintf(output, "INR A"); break;
        
        case 0x05: sprintf(output, "DCR B"); break;
        case 0x0D: sprintf(output, "DCR C"); break;
        case 0x15: sprintf(output, "DCR D"); break;
        case 0x1D: sprintf(output, "DCR E"); break;
        case 0x25: sprintf(output, "DCR H"); break;
        case 0x2D: sprintf(output, "DCR L"); break;
        case 0x35: sprintf(output, "DCR M"); break;
        case 0x3D: sprintf(output, "DCR A"); break;
        
        /* INX/DCX (Increment/Decrement register pair) */
        case 0x03: sprintf(output, "INX B"); break;
        case 0x13: sprintf(output, "INX D"); break;
        case 0x23: sprintf(output, "INX H"); break;
        case 0x33: sprintf(output, "INX SP"); break;
        
        case 0x0B: sprintf(output, "DCX B"); break;
        case 0x1B: sprintf(output, "DCX D"); break;
        case 0x2B: sprintf(output, "DCX H"); break;
        case 0x3B: sprintf(output, "DCX SP"); break;
        
        /* DAD (Add register pair to HL) */
        case 0x09: sprintf(output, "DAD B"); break;
        case 0x19: sprintf(output, "DAD D"); break;
        case 0x29: sprintf(output, "DAD H"); break;
        case 0x39: sprintf(output, "DAD SP"); break;
        
        /* Rotate instructions */
        case 0x07: sprintf(output, "RLC"); break;
        case 0x0F: sprintf(output, "RRC"); break;
        case 0x17: sprintf(output, "RAL"); break;
        case 0x1F: sprintf(output, "RAR"); break;
        
        /* Complement and carry */
        case 0x2F: sprintf(output, "CMA"); break;
        case 0x3F: sprintf(output, "CMC"); break;
        case 0x37: sprintf(output, "STC"); break;
        case 0x27: sprintf(output, "DAA"); break;
        
        /* JMP instructions */
        case 0xC3: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "JMP %04XH", targetAddr); 
            len=3; break;
        case 0xC2: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "JNZ %04XH", targetAddr); 
            len=3; break;
        case 0xCA: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "JZ %04XH", targetAddr); 
            len=3; break;
        case 0xD2: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "JNC %04XH", targetAddr); 
            len=3; break;
        case 0xDA: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "JC %04XH", targetAddr); 
            len=3; break;
        case 0xE2: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "JPO %04XH", targetAddr); 
            len=3; break;
        case 0xEA: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "JPE %04XH", targetAddr); 
            len=3; break;
        case 0xF2: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "JP %04XH", targetAddr); 
            len=3; break;
        case 0xFA: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "JM %04XH", targetAddr); 
            len=3; break;
        
        /* CALL instructions */
        case 0xCD: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "CALL %04XH", targetAddr); 
            len=3; break;
        case 0xC4: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "CNZ %04XH", targetAddr); 
            len=3; break;
        case 0xCC: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "CZ %04XH", targetAddr); 
            len=3; break;
        case 0xD4: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "CNC %04XH", targetAddr); 
            len=3; break;
        case 0xDC: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "CC %04XH", targetAddr); 
            len=3; break;
        case 0xE4: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "CPO %04XH", targetAddr); 
            len=3; break;
        case 0xEC: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "CPE %04XH", targetAddr); 
            len=3; break;
        case 0xF4: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "CP %04XH", targetAddr); 
            len=3; break;
        case 0xFC: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "CM %04XH", targetAddr); 
            len=3; break;
        
        /* RET instructions */
        case 0xC9: sprintf(output, "RET"); break;
        case 0xC0: sprintf(output, "RNZ"); break;
        case 0xC8: sprintf(output, "RZ"); break;
        case 0xD0: sprintf(output, "RNC"); break;
        case 0xD8: sprintf(output, "RC"); break;
        case 0xE0: sprintf(output, "RPO"); break;
        case 0xE8: sprintf(output, "RPE"); break;
        case 0xF0: sprintf(output, "RP"); break;
        case 0xF8: sprintf(output, "RM"); break;
        
        /* RST (Restart) */
        case 0xC7: sprintf(output, "RST 0"); break;
        case 0xCF: sprintf(output, "RST 1"); break;
        case 0xD7: sprintf(output, "RST 2"); break;
        case 0xDF: sprintf(output, "RST 3"); break;
        case 0xE7: sprintf(output, "RST 4"); break;
        case 0xEF: sprintf(output, "RST 5"); break;
        case 0xF7: sprintf(output, "RST 6"); break;
        case 0xFF: sprintf(output, "RST 7"); break;
        
        /* PUSH/POP */
        case 0xC5: sprintf(output, "PUSH B"); break;
        case 0xD5: sprintf(output, "PUSH D"); break;
        case 0xE5: sprintf(output, "PUSH H"); break;
        case 0xF5: sprintf(output, "PUSH PSW"); break;
        
        case 0xC1: sprintf(output, "POP B"); break;
        case 0xD1: sprintf(output, "POP D"); break;
        case 0xE1: sprintf(output, "POP H"); break;
        case 0xF1: sprintf(output, "POP PSW"); break;
        
        /* I/O */
        case 0xD3: sprintf(output, "OUT %02XH", byte1); len=2; break;
        case 0xDB: sprintf(output, "IN %02XH", byte1); len=2; break;
        
        /* RIM/SIM (8085 only, but included for completeness) */
        case 0x20: sprintf(output, "RIM"); break;
        case 0x30: sprintf(output, "SIM"); break;
        
        /* Undefined/illegal opcodes treated as data */
        default: sprintf(output, "DB %02XH", opcode); break;
    }
    
    return len;
}

/* Disassemble one instruction for Intel 8086 */
int Disasm8086(unsigned int addr, char *output) {
    unsigned char opcode, byte1, byte2, modrm;
    unsigned int targetAddr;
    int len = 1;
    signed short offset;
    
    if (addr >= BufferSize) {
        sprintf(output, "???");
        return 1;
    }
    
    opcode = DataBuffer[addr];
    byte1 = (addr + 1 < BufferSize) ? DataBuffer[addr+1] : 0;
    byte2 = (addr + 2 < BufferSize) ? DataBuffer[addr+2] : 0;
    
    /* Intel 8086 instruction set - major opcodes and common instructions */
    switch (opcode) {
        /* Single-byte instructions */
        case 0x90: sprintf(output, "NOP"); break;
        case 0xF4: sprintf(output, "HLT"); break;
        case 0xFA: sprintf(output, "CLI"); break;
        case 0xFB: sprintf(output, "STI"); break;
        case 0xF5: sprintf(output, "CMC"); break;
        case 0xF8: sprintf(output, "CLC"); break;
        case 0xF9: sprintf(output, "STC"); break;
        case 0xFC: sprintf(output, "CLD"); break;
        case 0xFD: sprintf(output, "STD"); break;
        case 0x9E: sprintf(output, "SAHF"); break;
        case 0x9F: sprintf(output, "LAHF"); break;
        case 0x98: sprintf(output, "CBW"); break;
        case 0x99: sprintf(output, "CWD"); break;
        case 0x9B: sprintf(output, "WAIT"); break;
        case 0xF0: sprintf(output, "LOCK"); break;
        case 0x9C: sprintf(output, "PUSHF"); break;
        case 0x9D: sprintf(output, "POPF"); break;
        case 0xA4: sprintf(output, "MOVSB"); break;
        case 0xA5: sprintf(output, "MOVSW"); break;
        case 0xA6: sprintf(output, "CMPSB"); break;
        case 0xA7: sprintf(output, "CMPSW"); break;
        case 0xAA: sprintf(output, "STOSB"); break;
        case 0xAB: sprintf(output, "STOSW"); break;
        case 0xAC: sprintf(output, "LODSB"); break;
        case 0xAD: sprintf(output, "LODSW"); break;
        case 0xAE: sprintf(output, "SCASB"); break;
        case 0xAF: sprintf(output, "SCASW"); break;
        case 0x27: sprintf(output, "DAA"); break;
        case 0x2F: sprintf(output, "DAS"); break;
        case 0x37: sprintf(output, "AAA"); break;
        case 0x3F: sprintf(output, "AAS"); break;
        case 0xD4: sprintf(output, "AAM"); len=2; break;
        case 0xD5: sprintf(output, "AAD"); len=2; break;
        case 0xD7: sprintf(output, "XLAT"); break;
        
        /* MOV immediate to register (8-bit) */
        case 0xB0: sprintf(output, "MOV AL,%02Xh", byte1); len=2; break;
        case 0xB1: sprintf(output, "MOV CL,%02Xh", byte1); len=2; break;
        case 0xB2: sprintf(output, "MOV DL,%02Xh", byte1); len=2; break;
        case 0xB3: sprintf(output, "MOV BL,%02Xh", byte1); len=2; break;
        case 0xB4: sprintf(output, "MOV AH,%02Xh", byte1); len=2; break;
        case 0xB5: sprintf(output, "MOV CH,%02Xh", byte1); len=2; break;
        case 0xB6: sprintf(output, "MOV DH,%02Xh", byte1); len=2; break;
        case 0xB7: sprintf(output, "MOV BH,%02Xh", byte1); len=2; break;
        
        /* MOV immediate to register (16-bit) */
        case 0xB8: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "MOV AX,%04Xh", targetAddr); 
            len=3; break;
        case 0xB9: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "MOV CX,%04Xh", targetAddr); 
            len=3; break;
        case 0xBA: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "MOV DX,%04Xh", targetAddr); 
            len=3; break;
        case 0xBB: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "MOV BX,%04Xh", targetAddr); 
            len=3; break;
        case 0xBC: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "MOV SP,%04Xh", targetAddr); 
            len=3; break;
        case 0xBD: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "MOV BP,%04Xh", targetAddr); 
            len=3; break;
        case 0xBE: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "MOV SI,%04Xh", targetAddr); 
            len=3; break;
        case 0xBF: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "MOV DI,%04Xh", targetAddr); 
            len=3; break;
        
        /* PUSH/POP registers */
        case 0x50: sprintf(output, "PUSH AX"); break;
        case 0x51: sprintf(output, "PUSH CX"); break;
        case 0x52: sprintf(output, "PUSH DX"); break;
        case 0x53: sprintf(output, "PUSH BX"); break;
        case 0x54: sprintf(output, "PUSH SP"); break;
        case 0x55: sprintf(output, "PUSH BP"); break;
        case 0x56: sprintf(output, "PUSH SI"); break;
        case 0x57: sprintf(output, "PUSH DI"); break;
        
        case 0x58: sprintf(output, "POP AX"); break;
        case 0x59: sprintf(output, "POP CX"); break;
        case 0x5A: sprintf(output, "POP DX"); break;
        case 0x5B: sprintf(output, "POP BX"); break;
        case 0x5C: sprintf(output, "POP SP"); break;
        case 0x5D: sprintf(output, "POP BP"); break;
        case 0x5E: sprintf(output, "POP SI"); break;
        case 0x5F: sprintf(output, "POP DI"); break;
        
        /* INC/DEC 16-bit registers */
        case 0x40: sprintf(output, "INC AX"); break;
        case 0x41: sprintf(output, "INC CX"); break;
        case 0x42: sprintf(output, "INC DX"); break;
        case 0x43: sprintf(output, "INC BX"); break;
        case 0x44: sprintf(output, "INC SP"); break;
        case 0x45: sprintf(output, "INC BP"); break;
        case 0x46: sprintf(output, "INC SI"); break;
        case 0x47: sprintf(output, "INC DI"); break;
        
        case 0x48: sprintf(output, "DEC AX"); break;
        case 0x49: sprintf(output, "DEC CX"); break;
        case 0x4A: sprintf(output, "DEC DX"); break;
        case 0x4B: sprintf(output, "DEC BX"); break;
        case 0x4C: sprintf(output, "DEC SP"); break;
        case 0x4D: sprintf(output, "DEC BP"); break;
        case 0x4E: sprintf(output, "DEC SI"); break;
        case 0x4F: sprintf(output, "DEC DI"); break;
        
        /* XCHG with AX */
        case 0x91: sprintf(output, "XCHG AX,CX"); break;
        case 0x92: sprintf(output, "XCHG AX,DX"); break;
        case 0x93: sprintf(output, "XCHG AX,BX"); break;
        case 0x94: sprintf(output, "XCHG AX,SP"); break;
        case 0x95: sprintf(output, "XCHG AX,BP"); break;
        case 0x96: sprintf(output, "XCHG AX,SI"); break;
        case 0x97: sprintf(output, "XCHG AX,DI"); break;
        
        /* Arithmetic immediate to AL */
        case 0x04: sprintf(output, "ADD AL,%02Xh", byte1); len=2; break;
        case 0x0C: sprintf(output, "OR AL,%02Xh", byte1); len=2; break;
        case 0x14: sprintf(output, "ADC AL,%02Xh", byte1); len=2; break;
        case 0x1C: sprintf(output, "SBB AL,%02Xh", byte1); len=2; break;
        case 0x24: sprintf(output, "AND AL,%02Xh", byte1); len=2; break;
        case 0x2C: sprintf(output, "SUB AL,%02Xh", byte1); len=2; break;
        case 0x34: sprintf(output, "XOR AL,%02Xh", byte1); len=2; break;
        case 0x3C: sprintf(output, "CMP AL,%02Xh", byte1); len=2; break;
        
        /* Arithmetic immediate to AX */
        case 0x05: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "ADD AX,%04Xh", targetAddr); 
            len=3; break;
        case 0x0D: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "OR AX,%04Xh", targetAddr); 
            len=3; break;
        case 0x15: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "ADC AX,%04Xh", targetAddr); 
            len=3; break;
        case 0x1D: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "SBB AX,%04Xh", targetAddr); 
            len=3; break;
        case 0x25: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "AND AX,%04Xh", targetAddr); 
            len=3; break;
        case 0x2D: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "SUB AX,%04Xh", targetAddr); 
            len=3; break;
        case 0x35: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "XOR AX,%04Xh", targetAddr); 
            len=3; break;
        case 0x3D: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "CMP AX,%04Xh", targetAddr); 
            len=3; break;
        
        /* Short jumps (relative) */
        case 0x70: 
            targetAddr = (addr + 2 + (signed char)byte1) & 0xFFFF;
            sprintf(output, "JO %04Xh", targetAddr); 
            len=2; break;
        case 0x71: 
            targetAddr = (addr + 2 + (signed char)byte1) & 0xFFFF;
            sprintf(output, "JNO %04Xh", targetAddr); 
            len=2; break;
        case 0x72: 
            targetAddr = (addr + 2 + (signed char)byte1) & 0xFFFF;
            sprintf(output, "JC %04Xh", targetAddr); 
            len=2; break;
        case 0x73: 
            targetAddr = (addr + 2 + (signed char)byte1) & 0xFFFF;
            sprintf(output, "JNC %04Xh", targetAddr); 
            len=2; break;
        case 0x74: 
            targetAddr = (addr + 2 + (signed char)byte1) & 0xFFFF;
            sprintf(output, "JZ %04Xh", targetAddr); 
            len=2; break;
        case 0x75: 
            targetAddr = (addr + 2 + (signed char)byte1) & 0xFFFF;
            sprintf(output, "JNZ %04Xh", targetAddr); 
            len=2; break;
        case 0x76: 
            targetAddr = (addr + 2 + (signed char)byte1) & 0xFFFF;
            sprintf(output, "JBE %04Xh", targetAddr); 
            len=2; break;
        case 0x77: 
            targetAddr = (addr + 2 + (signed char)byte1) & 0xFFFF;
            sprintf(output, "JA %04Xh", targetAddr); 
            len=2; break;
        case 0x78: 
            targetAddr = (addr + 2 + (signed char)byte1) & 0xFFFF;
            sprintf(output, "JS %04Xh", targetAddr); 
            len=2; break;
        case 0x79: 
            targetAddr = (addr + 2 + (signed char)byte1) & 0xFFFF;
            sprintf(output, "JNS %04Xh", targetAddr); 
            len=2; break;
        case 0x7A: 
            targetAddr = (addr + 2 + (signed char)byte1) & 0xFFFF;
            sprintf(output, "JPE %04Xh", targetAddr); 
            len=2; break;
        case 0x7B: 
            targetAddr = (addr + 2 + (signed char)byte1) & 0xFFFF;
            sprintf(output, "JPO %04Xh", targetAddr); 
            len=2; break;
        case 0x7C: 
            targetAddr = (addr + 2 + (signed char)byte1) & 0xFFFF;
            sprintf(output, "JL %04Xh", targetAddr); 
            len=2; break;
        case 0x7D: 
            targetAddr = (addr + 2 + (signed char)byte1) & 0xFFFF;
            sprintf(output, "JGE %04Xh", targetAddr); 
            len=2; break;
        case 0x7E: 
            targetAddr = (addr + 2 + (signed char)byte1) & 0xFFFF;
            sprintf(output, "JLE %04Xh", targetAddr); 
            len=2; break;
        case 0x7F: 
            targetAddr = (addr + 2 + (signed char)byte1) & 0xFFFF;
            sprintf(output, "JG %04Xh", targetAddr); 
            len=2; break;
        
        /* Loop instructions */
        case 0xE0: 
            targetAddr = (addr + 2 + (signed char)byte1) & 0xFFFF;
            sprintf(output, "LOOPNE %04Xh", targetAddr); 
            len=2; break;
        case 0xE1: 
            targetAddr = (addr + 2 + (signed char)byte1) & 0xFFFF;
            sprintf(output, "LOOPE %04Xh", targetAddr); 
            len=2; break;
        case 0xE2: 
            targetAddr = (addr + 2 + (signed char)byte1) & 0xFFFF;
            sprintf(output, "LOOP %04Xh", targetAddr); 
            len=2; break;
        case 0xE3: 
            targetAddr = (addr + 2 + (signed char)byte1) & 0xFFFF;
            sprintf(output, "JCXZ %04Xh", targetAddr); 
            len=2; break;
        
        /* JMP/CALL */
        case 0xE8: 
            offset = (short)(byte1 | (byte2 << 8));
            targetAddr = (addr + 3 + offset) & 0xFFFF;
            sprintf(output, "CALL %04Xh", targetAddr); 
            len=3; break;
        case 0xE9: 
            offset = (short)(byte1 | (byte2 << 8));
            targetAddr = (addr + 3 + offset) & 0xFFFF;
            sprintf(output, "JMP %04Xh", targetAddr); 
            len=3; break;
        case 0xEA: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "JMP FAR %04Xh", targetAddr); 
            len=5; break;
        case 0xEB: 
            targetAddr = (addr + 2 + (signed char)byte1) & 0xFFFF;
            sprintf(output, "JMP SHORT %04Xh", targetAddr); 
            len=2; break;
        
        /* RET */
        case 0xC3: sprintf(output, "RET"); break;
        case 0xCB: sprintf(output, "RETF"); break;
        case 0xC2: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "RET %04Xh", targetAddr); 
            len=3; break;
        case 0xCA: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "RETF %04Xh", targetAddr); 
            len=3; break;
        
        /* INT */
        case 0xCC: sprintf(output, "INT 3"); break;
        case 0xCD: sprintf(output, "INT %02Xh", byte1); len=2; break;
        case 0xCE: sprintf(output, "INTO"); break;
        case 0xCF: sprintf(output, "IRET"); break;
        
        /* I/O */
        case 0xE4: sprintf(output, "IN AL,%02Xh", byte1); len=2; break;
        case 0xE5: sprintf(output, "IN AX,%02Xh", byte1); len=2; break;
        case 0xE6: sprintf(output, "OUT %02Xh,AL", byte1); len=2; break;
        case 0xE7: sprintf(output, "OUT %02Xh,AX", byte1); len=2; break;
        case 0xEC: sprintf(output, "IN AL,DX"); break;
        case 0xED: sprintf(output, "IN AX,DX"); break;
        case 0xEE: sprintf(output, "OUT DX,AL"); break;
        case 0xEF: sprintf(output, "OUT DX,AX"); break;
        
        /* Memory/Accumulator direct */
        case 0xA0: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "MOV AL,[%04Xh]", targetAddr); 
            len=3; break;
        case 0xA1: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "MOV AX,[%04Xh]", targetAddr); 
            len=3; break;
        case 0xA2: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "MOV [%04Xh],AL", targetAddr); 
            len=3; break;
        case 0xA3: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "MOV [%04Xh],AX", targetAddr); 
            len=3; break;
        
        /* TEST */
        case 0xA8: sprintf(output, "TEST AL,%02Xh", byte1); len=2; break;
        case 0xA9: 
            targetAddr = byte1 | (byte2 << 8);
            sprintf(output, "TEST AX,%04Xh", targetAddr); 
            len=3; break;
        
        /* Segment register PUSH/POP */
        case 0x06: sprintf(output, "PUSH ES"); break;
        case 0x0E: sprintf(output, "PUSH CS"); break;
        case 0x16: sprintf(output, "PUSH SS"); break;
        case 0x1E: sprintf(output, "PUSH DS"); break;
        case 0x07: sprintf(output, "POP ES"); break;
        case 0x1F: sprintf(output, "POP DS"); break;
        
        /* Undefined opcodes */
        default: 
            sprintf(output, "DB %02Xh", opcode); 
            break;
    }
    
    return len;
}
