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
 * Version: 1.3
 * 
 * Description:
 *   Professional EPROM data reader and hex editor for DOS systems.
 *   Reads ASCII space-separated hex data from COM1 serial port and
 *   provides a full-featured hex editor with disassembly capabilities
 *   for 6800, 8080, and 8086 microprocessors.
 * 
 * Features:
 *   - COM1 serial communication with configurable baud rates
 *   - 64KB buffer capacity (farmalloc - far heap, bypasses 64KB segment limit)
 *   - Full hex/ASCII editing with visual highlighting
 *   - File load/save operations (binary, Intel HEX, listings)
 *   - Upload to EPROM programmer via COM1
 *   - Disassembler for 6800, 8080, and 8086 CPUs
 *   - Print to LPT1 support
 *   - Command-line file loading
 *   - ALT-key command help menu
 * 
 * Compiler: Turbo C/C++ 1.0 or later
 * Platform: MS-DOS / DOSBox
 * 
 * Change log:
 *   v1.3 - Fixed premature EOF: timeout now counts only dead-idle iterations
 *          (delay(5) every 500 iters => ~5 s silence required to declare done)
 *          instead of a raw loop counter that fired too early on large ROMs.
 *        - Added SendInitString(): user-configurable COM1 init string sent
 *          before read begins; supports any string or single character (e.g.
 *          "R", "READ", etc.).  Press ENTER with no input to skip.
 *   v1.2 - farmalloc() used for DataBuffer to support 64KB chips (27512 etc.)
 *          without hitting the 64KB near-data-segment limit.
 * 
 * Copyright (c) 2025 Mickey W. Lawless
 * All Rights Reserved
 * ========================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <alloc.h>
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
#define MAX_BUFFER_SIZE 65536UL  /* 64KB buffer - requires farmalloc */
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
unsigned char far *DataBuffer = NULL;  /* far heap - bypasses 64KB segment limit */
unsigned long BufferSize = 0;
unsigned long CursorPos = 0;
unsigned long TopLine = 0;
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
void SendInitString(void);
void UploadToEPROM(void);
void LoadFileIntoBuffer(void);
int ShowStartupMenu(void);
void HexEditor(void);
void Disassemble(void);
void DisassembleMenu(void);
void SaveDisassembly(int cpuType);
int Disasm6800(unsigned int addr, char *output);
int Disasm8080(unsigned int addr, char *output);
int Disasm8086(unsigned int addr, char *output);

/* Initialize COM1 port */
void InitCOM1(unsigned int baud) {
    unsigned int divisor;
    divisor = (unsigned int)(115200L / baud);
    outportb(COM1_IER, 0x00);
    outportb(COM1_LCR, 0x80);
    outportb(COM1_DATA, divisor & 0xFF);
    outportb(COM1_IER, (divisor >> 8) & 0xFF);
    outportb(COM1_LCR, 0x03);
    outportb(COM1_IIR, 0x00);
    delay(10);
    outportb(COM1_IIR, 0x07);
    delay(10);
    outportb(COM1_MCR, 0x0B);
    while (COM1DataReady()) {
        ReadCOM1();
        delay(1);
    }
}

int COM1DataReady(void) {
    return (inportb(COM1_LSR) & 0x01);
}

unsigned char ReadCOM1(void) {
    while (!COM1DataReady());
    return inportb(COM1_DATA);
}

void ReadDataFromCOM1(void) {
    char hexStr[3];
    int hexPos;
    int ch;
    unsigned int value;
    int linePos;
    unsigned long expectedSize;
    unsigned long lastPrintSize;
    long idleCount;
    char sizeBuf[16];
    int si, sch;

    /* Ask for expected ROM size so we stop exactly on byte count,
       not on a fragile timeout. Press ENTER to use timeout instead. */
    clrscr();
    textcolor(LIGHTCYAN);
    printf("Enter expected ROM size in bytes (e.g. 4096 for 2732)\n");
    printf("or press ENTER to use timeout: ");
    textcolor(LIGHTGRAY);
    si = 0;
    while (1) {
        sch = getch();
        if (sch == 13) { sizeBuf[si] = '\0'; break; }
        else if (sch == 8 && si > 0) { si--; printf("\b \b"); }
        else if (sch >= '0' && sch <= '9' && si < 15) {
            sizeBuf[si++] = (char)sch;
            putchar(sch);
        }
    }
    printf("\n");

    if (si > 0)
        expectedSize = (unsigned long)atol(sizeBuf);
    else
        expectedSize = 0;  /* 0 = unknown, use timeout */

    if (expectedSize > 0) {
        textcolor(LIGHTGREEN);
        printf("Will read exactly %lu bytes.\n\n", expectedSize);
    } else {
        textcolor(YELLOW);
        printf("No size given - timeout detection active.\n\n");
    }
    textcolor(LIGHTGRAY);

    hexStr[0] = 0; hexStr[1] = 0; hexStr[2] = 0;
    hexPos      = 0;
    linePos     = 0;
    lastPrintSize = 0;
    idleCount   = 0;
    BufferSize  = 0;

    printf("Reading from COM1 (9600 baud)\n");
    printf("Press ESC to abort\n");
    printf("========================================================================\n");

    /* Auto-detecting line parser.
       Handles both formats transparently:
         Listing: AAAA  DD DD DD DD ...  (Bytek/GTEK)
         Raw hex: DD DD DD DD ...        (plain space-separated)

       Detection: if the first token on a line is exactly 4 hex chars
       followed by 2+ spaces, it is an address - skip it.
       Otherwise treat all hex pairs as data.

       lineState:
         0 = start of line, collecting first token to decide format
         1 = first token was address - skip trailing spaces then read data
         2 = reading data bytes
    */
    {
        int lineState = 0;
        int tokenLen = 0;        /* length of first token on line */
        int spaceCount = 0;      /* spaces seen after first token */

    while (BufferSize < MAX_BUFFER_SIZE) {

        /* --- drain every byte currently in the UART --- */
        while (inportb(COM1_LSR) & 0x01) {
            ch = inportb(COM1_DATA);
            idleCount = 0;

            /* End of line - reset state */
            if (ch == '\r' || ch == '\n') {
                lineState = 0;
                tokenLen = 0;
                spaceCount = 0;
                hexPos = 0;
                continue;
            }

            if (lineState == 0) {
                /* Collecting first token on line to detect address vs data */
                int isHex = ((ch >= '0' && ch <= '9') ||
                             (ch >= 'A' && ch <= 'F') ||
                             (ch >= 'a' && ch <= 'f'));

                if (isHex && tokenLen < 7) {
                    tokenLen++;
                    /* Also feed into hexStr in case this turns out to be data */
                    if (hexPos < 2)
                        hexStr[hexPos++] = (char)ch;
                } else if (ch == ' ' || ch == '\t') {
                    /* Space ends the first token */
                    spaceCount++;
                    if (tokenLen == 4 && spaceCount >= 2) {
                        /* 4 hex chars + 2 spaces = address field, skip it */
                        lineState = 1;
                        hexPos = 0;
                    } else if (tokenLen == 2 && spaceCount >= 1) {
                        /* 2 hex chars + space = this was a data byte, store it */
                        if (hexPos == 2) {
                            unsigned char hi = (unsigned char)hexStr[0];
                            unsigned char lo = (unsigned char)hexStr[1];
                            hi = (hi >= 'a') ? (hi-'a'+10) :
                                 (hi >= 'A') ? (hi-'A'+10) : (hi-'0');
                            lo = (lo >= 'a') ? (lo-'a'+10) :
                                 (lo >= 'A') ? (lo-'A'+10) : (lo-'0');
                            DataBuffer[BufferSize++] = (hi << 4) | lo;
                            if (expectedSize > 0 && BufferSize >= expectedSize)
                                break;
                        }
                        hexPos = 0;
                        lineState = 2;
                    } else if (tokenLen > 0 && tokenLen != 4) {
                        /* odd length token - treat as data line */
                        lineState = 2;
                    }
                    /* if tokenLen==4 but only 1 space so far, keep waiting */
                } else if (!isHex) {
                    /* non-hex char on line start - skip to data mode */
                    lineState = 2;
                    hexPos = 0;
                }
                continue;
            }

            if (lineState == 1) {
                /* Address was detected and skipped - now read data bytes */
                /* Fall through to lineState 2 handling below */
                lineState = 2;
            }

            if (lineState == 2) {
                /* Reading data hex pairs */
                if ((ch >= '0' && ch <= '9') ||
                    (ch >= 'A' && ch <= 'F') ||
                    (ch >= 'a' && ch <= 'f')) {
                    hexStr[hexPos++] = (char)ch;
                    if (hexPos == 2) {
                        unsigned char hi = (unsigned char)hexStr[0];
                        unsigned char lo = (unsigned char)hexStr[1];
                        hi = (hi >= 'a') ? (hi-'a'+10) :
                             (hi >= 'A') ? (hi-'A'+10) : (hi-'0');
                        lo = (lo >= 'a') ? (lo-'a'+10) :
                             (lo >= 'A') ? (lo-'A'+10) : (lo-'0');
                        DataBuffer[BufferSize++] = (hi << 4) | lo;
                        hexPos = 0;
                        if (expectedSize > 0 && BufferSize >= expectedSize)
                            break;
                    }
                } else if (ch == ' ' || ch == '\t') {
                    hexPos = 0;
                }
            }
        } /* end inner UART drain while */

        /* --- stop immediately if we have all expected bytes --- */
        if (expectedSize > 0 && BufferSize >= expectedSize) {
            printf("\n");
            textcolor(LIGHTGREEN);
            printf("Received all %lu bytes - done.\n", BufferSize);
            textcolor(LIGHTGRAY);
            break;
        }

        /* --- screen update every 64 bytes --- */
        if (BufferSize >= lastPrintSize + 64) {
            unsigned long i;
            for (i = lastPrintSize; i < BufferSize; i++) {
                printf("%02X ", DataBuffer[i]);
                linePos++;
                if (linePos >= 16) { printf("\n"); linePos = 0; }
            }
            lastPrintSize = BufferSize;
        }

        /* --- keyboard check --- */
        if (kbhit()) {
            if (getch() == 27) {
                printf("\n");
                textcolor(LIGHTRED);
                printf("Aborted by user.\n");
                textcolor(LIGHTGRAY);
                break;
            }
        }

        /* --- idle tracking --- */
        if (!(inportb(COM1_LSR) & 0x01)) {
            idleCount++;
            if (idleCount % 200 == 0) delay(1);

            if (BufferSize < 64 && idleCount > 36000L && idleCount % 36000L == 0) {
                textcolor(YELLOW);
                printf("Waiting for data from COM1...\n");
                textcolor(LIGHTGRAY);
            }

            /* timeout only after meaningful data received */
            {
                unsigned long threshold;
                if (expectedSize > 0)
                    threshold = expectedSize / 2;
                else
                    threshold = 64UL;
                if (BufferSize >= threshold && idleCount > 180000L) {
                    printf("\n");
                    textcolor(LIGHTGREEN);
                    printf("Transmission complete. Total bytes: %lu\n", BufferSize);
                    textcolor(LIGHTGRAY);
                    break;
                }
            }
        }

    } /* end outer while */
    } /* end lineState block */

    /* flush any remaining bytes to screen */
    if (BufferSize > lastPrintSize) {
        unsigned long i;
        for (i = lastPrintSize; i < BufferSize; i++) {
            printf("%02X ", DataBuffer[i]);
            linePos++;
            if (linePos >= 16) { printf("\n"); linePos = 0; }
        }
        printf("\n");
    }

    if (BufferSize >= MAX_BUFFER_SIZE) {
        textcolor(YELLOW);
        printf("Buffer full - reading stopped.\n");
        textcolor(LIGHTGRAY);
    }

    printf("========================================================================\n");
    textcolor(LIGHTGREEN);
    printf("Total bytes read: %lu\n", BufferSize);
    textcolor(LIGHTGRAY);
    printf("Press any key to view hex editor...\n");
    getch();
}

char NibbleToHex(unsigned char nibble) {
    return (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
}

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
        if (ch == 27) return;
        else if (ch == 13 && i > 0) { filename[i] = '\0'; break; }
        else if (ch == 8 && i > 0) { i--; printf("\b \b"); }
        else if (ch >= 32 && ch < 127 && i < 79) { filename[i++] = ch; putchar(ch); }
    }

    fp = fopen(filename, "wb");
    if (fp == NULL) {
        gotoxy(1, yPos); clreol();
        textcolor(LIGHTRED);
        printf("ERROR: Cannot create file! Press any key...");
        textcolor(LIGHTGRAY);
        getch(); return;
    }

    /* DataBuffer is on the far heap - fwrite() may not handle far pointers
       correctly in Turbo C. Write through a near buffer in 512-byte chunks. */
    {
        unsigned char nearBuf[512];
        unsigned long written = 0;
        unsigned int chunk, j;
        while (written < BufferSize) {
            chunk = (BufferSize - written > 512UL) ? 512 : (unsigned int)(BufferSize - written);
            for (j = 0; j < chunk; j++)
                nearBuf[j] = DataBuffer[written + j];
            fwrite(nearBuf, 1, chunk, fp);
            written += chunk;
        }
    }

    fclose(fp);
    gotoxy(1, yPos); clreol();
    textcolor(LIGHTGREEN);
    printf("File saved successfully! (%lu bytes) Press any key...", BufferSize);
    textcolor(LIGHTGRAY);
    getch();
    Modified = 0;
}

void SaveBufferAsHex(void) {
    char filename[80];
    FILE *fp;
    unsigned int i;
    int yPos = 23;

    gotoxy(1, yPos);
    textcolor(LIGHTGRAY);
    textbackground(BLACK);
    clreol();
    printf("Enter .hex filename to save (or ESC to cancel): ");

    i = 0;
    while (1) {
        int ch = getch();
        if (ch == 27) return;
        else if (ch == 13 && i > 0) { filename[i] = '\0'; break; }
        else if (ch == 8 && i > 0) { i--; printf("\b \b"); }
        else if (ch >= 32 && ch < 127 && i < 79) { filename[i++] = ch; putchar(ch); }
    }

    fp = fopen(filename, "w");
    if (fp == NULL) {
        gotoxy(1, yPos); clreol();
        textcolor(LIGHTRED);
        printf("ERROR: Cannot create file! Press any key...");
        textcolor(LIGHTGRAY);
        getch(); return;
    }

    {
        unsigned long laddr = 0;
        unsigned int bline, cs, j;
        unsigned char b;
        while (laddr < BufferSize) {
            bline = (BufferSize - laddr >= 16) ? 16 : (unsigned int)(BufferSize - laddr);
            cs = bline + ((laddr >> 8) & 0xFF) + (laddr & 0xFF);
            fprintf(fp, ":%02X%04lX00", bline, laddr);
            for (j = 0; j < bline; j++) {
                b = DataBuffer[laddr + j];
                fprintf(fp, "%02X", b);
                cs += b;
            }
            cs = (~cs + 1) & 0xFF;
            fprintf(fp, "%02X\n", cs);
            laddr += bline;
        }
    }
    fprintf(fp, ":00000001FF\n");
    fclose(fp);

    gotoxy(1, yPos); clreol();
    textcolor(LIGHTGREEN);
    printf("Intel HEX file saved! (%lu bytes) Press any key...", BufferSize);
    textcolor(LIGHTGRAY);
    getch();
}

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
        if (ch == 27) return;
        else if (ch == 13 && i > 0) { filename[i] = '\0'; break; }
        else if (ch == 8 && i > 0) { i--; printf("\b \b"); }
        else if (ch >= 32 && ch < 127 && i < 79) { filename[i++] = ch; putchar(ch); }
    }

    fp = fopen(filename, "w");
    if (fp == NULL) {
        gotoxy(1, yPos); clreol();
        textcolor(LIGHTRED);
        printf("ERROR: Cannot create file! Press any key...");
        textcolor(LIGHTGRAY);
        getch(); return;
    }

    fprintf(fp, "EPROM Data Listing\n");
    fprintf(fp, "==================\n\n");
    fprintf(fp, "Total bytes: %lu\n\n", BufferSize);
    fprintf(fp, "Offset   00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F  ASCII\n");
    fprintf(fp, "------------------------------------------------------------------------\n");

    for (addr = 0; addr < BufferSize; addr += BYTES_PER_LINE) {
        fprintf(fp, "%04X:    ", addr);
        for (col = 0; col < BYTES_PER_LINE && addr + col < BufferSize; col++) {
            byte = DataBuffer[addr + col];
            fprintf(fp, "%02X ", byte);
        }
        for (; col < BYTES_PER_LINE; col++) fprintf(fp, "   ");
        fprintf(fp, " ");
        for (col = 0; col < BYTES_PER_LINE && addr + col < BufferSize; col++) {
            byte = DataBuffer[addr + col];
            fprintf(fp, "%c", (byte >= 32 && byte < 127) ? byte : '.');
        }
        fprintf(fp, "\n");
    }

    fclose(fp);
    gotoxy(1, yPos); clreol();
    textcolor(LIGHTGREEN);
    printf("Listing saved! (%lu bytes) Press any key...", BufferSize);
    textcolor(LIGHTGRAY);
    getch();
}

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
        gotoxy(1, yPos); clreol();
        textcolor(LIGHTRED);
        printf("ERROR: Cannot access printer (LPT1)! Press any key...");
        textcolor(LIGHTGRAY);
        getch(); return;
    }

    fprintf(fp, "EPROM Data Listing\n");
    fprintf(fp, "==================\n\n");
    fprintf(fp, "Total bytes: %lu\n\n", BufferSize);
    fprintf(fp, "Offset   00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F  ASCII\n");
    fprintf(fp, "------------------------------------------------------------------------\n");

    for (addr = 0; addr < BufferSize; addr += BYTES_PER_LINE) {
        if (kbhit() && getch() == 27) {
            fclose(fp);
            gotoxy(1, yPos); clreol();
            textcolor(LIGHTRED);
            printf("Printing cancelled! Press any key...");
            textcolor(LIGHTGRAY);
            getch(); return;
        }
        fprintf(fp, "%04X:    ", addr);
        for (col = 0; col < BYTES_PER_LINE && addr + col < BufferSize; col++) {
            byte = DataBuffer[addr + col];
            fprintf(fp, "%02X ", byte);
        }
        for (; col < BYTES_PER_LINE; col++) fprintf(fp, "   ");
        fprintf(fp, " ");
        for (col = 0; col < BYTES_PER_LINE && addr + col < BufferSize; col++) {
            byte = DataBuffer[addr + col];
            fprintf(fp, "%c", (byte >= 32 && byte < 127) ? byte : '.');
        }
        fprintf(fp, "\n");
    }

    fprintf(fp, "\f");
    fclose(fp);
    gotoxy(1, yPos); clreol();
    textcolor(LIGHTGREEN);
    printf("Printing complete! Press any key...");
    textcolor(LIGHTGRAY);
    getch();
}

void EditByteHex(void) {
    char hexStr[3];
    int hexPos = 0;
    unsigned int value;
    int ch;
    int yPos = 23;

    hexStr[0] = 0; hexStr[1] = 0; hexStr[2] = 0;

    gotoxy(1, yPos);
    textcolor(YELLOW);
    textbackground(BLACK);
    clreol();
    printf("HEX EDIT - Enter 2 hex digits (or ESC): ");

    while (hexPos < 2) {
        ch = getch();
        if (ch == 27) return;
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
    if (CursorPos < BufferSize - 1) CursorPos++;
}

void EditByteASCII(void) {
    int ch;
    int yPos = 23;

    gotoxy(1, yPos);
    textcolor(YELLOW);
    textbackground(BLACK);
    clreol();
    printf("ASCII EDIT - Enter character (or ESC): ");

    ch = getch();
    if (ch == 27) return;
    else if (ch >= 0 && ch < 256) {
        DataBuffer[CursorPos] = (unsigned char)ch;
        Modified = 1;
        putchar(ch);
        delay(200);
        if (CursorPos < BufferSize - 1) CursorPos++;
    }
}

void DisplayHexEditor(void) {
    int row, col;
    unsigned int addr;
    unsigned char byte;
    int maxLines = 19;
    const char *modeStr;
    int cursorCol;

    _setcursortype(_NOCURSOR);

    gotoxy(1, 1);
    textcolor(WHITE);
    textbackground(BLUE);

    modeStr = (CursorArea == AREA_HEX) ? "HEX" : "ASCII";

    cprintf(" EPROM Editor v1.3 - Mickey W. Lawless [%s MODE]%s", modeStr, Modified ? " *MOD*" : "     ");
    cprintf("%-21s", "");

    textcolor(LIGHTGRAY);
    textbackground(BLACK);
    gotoxy(1, 2);

    cprintf("Offset   00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F  ASCII\r\n");
    cprintf("------------------------------------------------------------------------\r\n");

    for (row = 0; row < maxLines && (TopLine + row) * BYTES_PER_LINE < BufferSize; row++) {
        addr = (TopLine + row) * BYTES_PER_LINE;
        gotoxy(1, 4 + row);
        cprintf("%04X:    ", addr);

        for (col = 0; col < BYTES_PER_LINE && addr + col < BufferSize; col++) {
            byte = DataBuffer[addr + col];
            if (addr + col == CursorPos) { textcolor(BLACK); textbackground(LIGHTCYAN); }
            cprintf("%c%c ", NibbleToHex(byte >> 4), NibbleToHex(byte & 0x0F));
            if (addr + col == CursorPos) { textcolor(LIGHTGRAY); textbackground(BLACK); }
        }
        for (; col < BYTES_PER_LINE; col++) cprintf("   ");
        cprintf(" ");

        for (col = 0; col < BYTES_PER_LINE && addr + col < BufferSize; col++) {
            byte = DataBuffer[addr + col];
            if (addr + col == CursorPos) { textcolor(BLACK); textbackground(LIGHTCYAN); }
            cprintf("%c", (byte >= 32 && byte < 127) ? byte : '.');
            if (addr + col == CursorPos) { textcolor(LIGHTGRAY); textbackground(BLACK); }
        }
        cprintf("%-10s", "");
    }
    for (; row < maxLines; row++) { gotoxy(1, 4 + row); cprintf("%-79s", ""); }

    if (ShowCommandHelp) {
        gotoxy(1, 23);
        textcolor(WHITE);
        textbackground(BLUE);
        cprintf(" ENTER)Edit U)pload S)ave H)ex L)ist P)rint O)pen D)isasm ESC)exit%-2s", "");
    } else {
        gotoxy(1, 23);
        textcolor(LIGHTGRAY);
        textbackground(BLACK);
        cprintf("%-80s", "");
    }

    gotoxy(1, 24);
    textcolor(WHITE);
    textbackground(BLUE);

    if (BufferSize > 0 && CursorPos < BufferSize) {
        char statusBuf[81];
        sprintf(statusBuf, " Offset:%04lX Value:%02X (%d) Area:%s CPU:%s Size:%lu",
                CursorPos,
                DataBuffer[CursorPos],
                DataBuffer[CursorPos],
                (CursorArea == AREA_HEX) ? "HEX" : "ASCII",
                (CPUType == CPU_6800) ? "6800" : ((CPUType == CPU_8080) ? "8080" : "8086"),
                BufferSize);
        cprintf("%-80s", statusBuf);
    } else {
        cprintf(" Empty Buffer%-67s", "");
    }

    textcolor(LIGHTGRAY);
    textbackground(BLACK);
    _setcursortype(_NORMALCURSOR);
}

void HexEditor(void) {
    int key;
    int maxLines = 19;
    int cursorCol;

    DisplayHexEditor();

    while (1) {
        if (bioskey(2) & 0x08) {
            if (!ShowCommandHelp) { ShowCommandHelp = 1; DisplayHexEditor(); }
        } else {
            if (ShowCommandHelp) { ShowCommandHelp = 0; DisplayHexEditor(); }
        }

        if (kbhit()) {
            key = getch();

            if (key == 0 || key == 224) {
                key = getch();
                switch (key) {
                    case 72: case 56:
                        if (CursorPos >= BYTES_PER_LINE) {
                            CursorPos -= BYTES_PER_LINE;
                            if (CursorPos < TopLine * BYTES_PER_LINE) TopLine--;
                            DisplayHexEditor();
                        }
                        break;
                    case 80: case 50:
                        if (CursorPos + BYTES_PER_LINE < BufferSize) {
                            CursorPos += BYTES_PER_LINE;
                            if (CursorPos >= (TopLine + maxLines) * BYTES_PER_LINE) TopLine++;
                            CursorArea = AREA_HEX;
                            DisplayHexEditor();
                        }
                        break;
                    case 75: case 52:
                        cursorCol = CursorPos % BYTES_PER_LINE;
                        if (CursorArea == AREA_ASCII && cursorCol == 0) {
                            CursorArea = AREA_HEX;
                            CursorPos = (CursorPos / BYTES_PER_LINE) * BYTES_PER_LINE + (BYTES_PER_LINE - 1);
                            DisplayHexEditor();
                        } else if (CursorArea == AREA_ASCII && cursorCol > 0) {
                            CursorPos--; DisplayHexEditor();
                        } else if (CursorArea == AREA_HEX && CursorPos > 0) {
                            CursorPos--;
                            if (CursorPos < TopLine * BYTES_PER_LINE) TopLine--;
                            DisplayHexEditor();
                        }
                        break;
                    case 77: case 54:
                        cursorCol = CursorPos % BYTES_PER_LINE;
                        if (CursorArea == AREA_HEX && cursorCol == BYTES_PER_LINE - 1) {
                            CursorArea = AREA_ASCII;
                            CursorPos = (CursorPos / BYTES_PER_LINE) * BYTES_PER_LINE;
                            DisplayHexEditor();
                        } else if (CursorArea == AREA_HEX && cursorCol < BYTES_PER_LINE - 1) {
                            CursorPos++; DisplayHexEditor();
                        } else if (CursorArea == AREA_ASCII && cursorCol == BYTES_PER_LINE - 1) {
                            if (CursorPos + 1 < BufferSize) {
                                CursorPos++; CursorArea = AREA_HEX;
                                if (CursorPos >= (TopLine + maxLines) * BYTES_PER_LINE) TopLine++;
                                DisplayHexEditor();
                            }
                        } else if (CursorArea == AREA_ASCII && cursorCol < BYTES_PER_LINE - 1) {
                            CursorPos++; DisplayHexEditor();
                        }
                        break;
                    case 73:
                        if (TopLine > 0) {
                            TopLine -= maxLines;
                            if ((int)TopLine < 0) TopLine = 0;
                            CursorPos = TopLine * BYTES_PER_LINE;
                            CursorArea = AREA_HEX;
                            DisplayHexEditor();
                        }
                        break;
                    case 81:
                        if ((TopLine + maxLines) * BYTES_PER_LINE < BufferSize) {
                            TopLine += maxLines;
                            CursorPos = TopLine * BYTES_PER_LINE;
                            CursorArea = AREA_HEX;
                            DisplayHexEditor();
                        }
                        break;
                }
            } else if (key == 27) {
                if (Modified) {
                    int ch_esc;
                    gotoxy(1, 23);
                    textcolor(YELLOW);
                    textbackground(BLACK);
                    clreol();
                    printf("Buffer modified! Save before exit? (Y/N): ");
                    ch_esc = getch();
                    if (ch_esc == 'Y' || ch_esc == 'y') SaveBufferToFile();
                }
                break;
            } else if (key == 'd' || key == 'D') {
                DisassembleMenu(); DisplayHexEditor();
            } else if (key == 13) {
                if (CursorArea == AREA_HEX) EditByteHex();
                else EditByteASCII();
                DisplayHexEditor();
            } else if (key == 's' || key == 'S') {
                SaveBufferToFile(); DisplayHexEditor();
            } else if (key == 'h' || key == 'H') {
                SaveBufferAsHex(); DisplayHexEditor();
            } else if (key == 'l' || key == 'L') {
                SaveListing(); DisplayHexEditor();
            } else if (key == 'p' || key == 'P') {
                PrintBuffer(); DisplayHexEditor();
            } else if (key == 'u' || key == 'U') {
                UploadToEPROM(); DisplayHexEditor();
            } else if (key == 'o' || key == 'O') {
                LoadFileIntoBuffer();
                if (BufferSize > 0) { CursorPos = 0; TopLine = 0; CursorArea = AREA_HEX; }
                DisplayHexEditor();
            }
        }
    }
}

/* Main program */
int main(int argc, char *argv[]) {
    int menuChoice;
    FILE *fp;

    /* Allocate buffer from far heap - bypasses 64KB near-data-segment limit.
       This is required when MAX_BUFFER_SIZE >= 32768.                         */
    DataBuffer = (unsigned char far *)farmalloc((unsigned long)MAX_BUFFER_SIZE);
    if (DataBuffer == NULL) {
        textcolor(LIGHTRED);
        printf("ERROR: farmalloc failed - not enough far heap memory!\n");
        printf("Cannot allocate %lu byte buffer.\n", (unsigned long)MAX_BUFFER_SIZE);
        textcolor(LIGHTGRAY);
        printf("Press any key to exit...\n");
        getch();
        return 1;
    }

    /* Check for command line filename argument */
    if (argc > 1 && argv[1] != NULL && argv[1][0] != '\0') {
        char cmdFilename[80];
        strncpy(cmdFilename, argv[1], 79);
        cmdFilename[79] = '\0';

        textmode(C80);
        clrscr();
        textcolor(LIGHTCYAN);
        printf("========================================================================\n");
        printf("               EPROM Data Reader & Hex Editor v1.3\n");
        printf("                   Developer: Mickey W. Lawless\n");
        printf("                    Completed: December 21, 2025\n");
        printf("========================================================================\n\n");
        textcolor(LIGHTGRAY);

        printf("Loading file: ");
        puts(cmdFilename);

        fp = fopen(cmdFilename, "rb");
        if (fp == NULL) {
            textcolor(LIGHTRED);
            printf("\nERROR: Cannot open file: ");
            puts(cmdFilename);
            textcolor(LIGHTGRAY);
            printf("Press any key to exit...\n");
            getch();
            farfree(DataBuffer);
            return 1;
        }

        /* fread() does not handle far pointers reliably in Turbo C.
           Read through a near buffer in 512-byte chunks.           */
        {
            unsigned char nearBuf[512];
            int bytesRead;
            unsigned int j;
            BufferSize = 0;
            while (BufferSize < MAX_BUFFER_SIZE) {
                unsigned int chunk = (MAX_BUFFER_SIZE - BufferSize > 512UL)
                                     ? 512 : (unsigned int)(MAX_BUFFER_SIZE - BufferSize);
                bytesRead = fread(nearBuf, 1, chunk, fp);
                if (bytesRead <= 0) break;
                for (j = 0; j < (unsigned int)bytesRead; j++)
                    DataBuffer[BufferSize + j] = nearBuf[j];
                BufferSize += (unsigned int)bytesRead;
            }
        }
        fclose(fp);

        if (BufferSize == 0) {
            textcolor(LIGHTRED);
            printf("\nERROR: File is empty or could not be read.\n");
            textcolor(LIGHTGRAY);
            printf("Press any key to exit...\n");
            getch();
            farfree(DataBuffer);
            return 1;
        }

        textcolor(LIGHTGREEN);
        printf("\nLoaded %lu bytes from '%s'\n", BufferSize, argv[1]);
        printf("Press any key to enter hex editor...");
        getch();

        CursorPos = 0; TopLine = 0; EditMode = MODE_VIEW;
        CursorArea = AREA_HEX; Modified = 0;

        HexEditor();
        farfree(DataBuffer);
        return 0;
    }

    menuChoice = ShowStartupMenu();

    if (menuChoice == 4) {
        textcolor(LIGHTGRAY);
        printf("\nExiting...\n");
        farfree(DataBuffer);
        return 0;
    }

    if (menuChoice == 1) {
        printf("\nInitializing COM1 at 9600 baud...\n");
        InitCOM1(9600);
        printf("Ready!\n\n");
        SendInitString();
        printf("Press any key to start reading from COM1...\n");
        getch();
        ReadDataFromCOM1();
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
            farfree(DataBuffer);
            return 1;
        }
    } else if (menuChoice == 2) {
        LoadFileIntoBuffer();
        if (BufferSize == 0) {
            textcolor(LIGHTRED);
            printf("\n\nNo file loaded. Exiting...\n");
            textcolor(LIGHTGRAY);
            farfree(DataBuffer);
            return 1;
        }
    } else if (menuChoice == 3) {
        BufferSize = 0;
        textcolor(LIGHTGREEN);
        printf("\nEntering empty editor...\n");
        printf("Use 'L' key in editor to load a file.\n");
        printf("Press any key to continue...");
        getch();
    }

    if (menuChoice == 3) {
        DataBuffer[0] = 0x00;
        BufferSize = 1;
    }

    CursorPos = 0; TopLine = 0; EditMode = MODE_VIEW;
    Modified = 0; CursorArea = AREA_HEX;

    HexEditor();

    farfree(DataBuffer);

    textmode(C80);
    clrscr();
    textcolor(LIGHTGREEN);
    printf("EPROM Reader terminated.\n");
    textcolor(LIGHTGRAY);

    return 0;
}

void WriteCOM1(unsigned char byte) {
    while ((inportb(COM1_LSR) & 0x20) == 0);
    outportb(COM1_DATA, byte);
}

/*
 * SendInitString - prompt the user for an initialization string to send
 * over COM1 before reading begins.  The user may press ENTER with no input
 * to send nothing (skip), or type a custom string / single character.
 * A carriage-return is appended automatically after the string.
 */
void SendInitString(void) {
    char initStr[81];
    int i = 0, ch;
    unsigned int k;

    textcolor(YELLOW);
    printf("Enter initialization string to send over COM1\n");
    printf("(press ENTER with no input to skip, or type a string e.g. R or READ): ");
    textcolor(LIGHTGRAY);

    while (1) {
        ch = getch();
        if (ch == 27) { printf("\n"); return; }          /* ESC = skip */
        else if (ch == 13) { initStr[i] = '\0'; break; } /* ENTER = done */
        else if (ch == 8 && i > 0) { i--; printf("\b \b"); }
        else if (ch >= 32 && ch < 127 && i < 80) { initStr[i++] = (char)ch; putchar(ch); }
    }
    printf("\n");

    if (i == 0) {
        textcolor(LIGHTGRAY);
        printf("No init string sent.\n");
        return;
    }

    textcolor(LIGHTCYAN);
    printf("Sending init string: \"%s\"\n", initStr);
    textcolor(LIGHTGRAY);

    for (k = 0; k < (unsigned int)i; k++) {
        WriteCOM1((unsigned char)initStr[k]);
        delay(5);
    }
    WriteCOM1('\r');   /* terminate with CR */
    delay(50);         /* brief pause for device to respond */
}

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
        if (ch == 27) return;
        else if (ch == 13 && i > 0) { filename[i] = '\0'; break; }
        else if (ch == 8 && i > 0) { i--; printf("\b \b"); }
        else if (ch >= 32 && ch < 127 && i < 79) { filename[i++] = ch; putchar(ch); }
    }

    fp = fopen(filename, "rb");
    if (fp == NULL) {
        gotoxy(1, yPos); clreol();
        textcolor(LIGHTRED);
        printf("ERROR: Cannot open file! Press any key...");
        textcolor(LIGHTGRAY);
        getch(); return;
    }

    gotoxy(1, yPos); clreol();
    printf("Generate upload listing? (Y/N): ");
    ch = getch();

    if (ch == 'Y' || ch == 'y') {
        gotoxy(1, yPos); clreol();
        printf("Enter listing filename: ");
        i = 0;
        while (1) {
            ch = getch();
            if (ch == 27) { listFp = NULL; break; }
            else if (ch == 13 && i > 0) {
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
            } else if (ch == 8 && i > 0) { i--; printf("\b \b"); }
            else if (ch >= 32 && ch < 127 && i < 79) { listFilename[i++] = ch; putchar(ch); }
        }
    } else { listFp = NULL; }

    fseek(fp, 0, SEEK_SET);
    i = 0;
    while (i < 100 && !feof(fp)) {
        ch = fgetc(fp);
        if (ch == EOF) break;
        if (!((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F') ||
              (ch >= 'a' && ch <= 'f') || ch == ' ' || ch == '\r' ||
              ch == '\n' || ch == '\t')) { isBinary = 1; break; }
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
        while (!feof(fp) && addr < 0xFFFFU) {
            if (kbhit() && getch() == 27) {
                textcolor(LIGHTRED); printf("\nUpload cancelled by user!\n"); textcolor(LIGHTGRAY);
                fclose(fp); if (listFp) fclose(listFp); delay(1000); return;
            }
            if (fread(&byte, 1, 1, fp) == 1) {
                WriteCOM1(NibbleToHex(byte >> 4));
                WriteCOM1(NibbleToHex(byte & 0x0F));
                WriteCOM1(' ');
                printf("%02X ", byte);
                if ((addr + 1) % 16 == 0) printf("\n");
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
                DataBuffer[addr] = byte;
                addr++;
                delay(10);
            }
        }
    } else {
        char hexStr[3];
        int hexPos = 0;
        unsigned int value;
        hexStr[0] = 0; hexStr[1] = 0; hexStr[2] = 0;

        while (!feof(fp) && addr < 0xFFFFU) {
            if (kbhit() && getch() == 27) {
                textcolor(LIGHTRED); printf("\nUpload cancelled by user!\n"); textcolor(LIGHTGRAY);
                fclose(fp); if (listFp) fclose(listFp); delay(1000); return;
            }
            ch = fgetc(fp);
            if (ch == EOF) break;
            WriteCOM1((unsigned char)ch);
            if ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f')) {
                hexStr[hexPos++] = ch;
                if (hexPos == 2) {
                    hexStr[2] = '\0';
                    sscanf(hexStr, "%x", &value);
                    byte = (unsigned char)value;
                    hexPos = 0;
                    printf("%02X ", byte);
                    if ((addr + 1) % 16 == 0) printf("\n");
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
                    DataBuffer[addr] = byte;
                    addr++;
                }
            } else if (ch == ' ' || ch == '\r' || ch == '\n' || ch == '\t') {
                hexPos = 0;
            }
            delay(5);
        }
    }

    if (listFp) {
        if (addr > 0 && addr % BYTES_PER_LINE != 0) {
            for (col = addr % BYTES_PER_LINE; col < BYTES_PER_LINE; col++) fprintf(listFp, "   ");
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
    if (listFp) printf("Listing saved to: %s\n", listFilename);
    textcolor(LIGHTGRAY);
    printf("Press any key to continue...");
    getch();
}

void LoadFileIntoBuffer(void) {
    char filename[80];
    FILE *fp;
    unsigned char byte;
    int i, ch;

    textcolor(LIGHTCYAN);
    printf("\nEnter filename to load (or ESC to cancel): ");
    textcolor(LIGHTGRAY);

    i = 0;
    while (1) {
        ch = getch();
        if (ch == 27) { BufferSize = 0; return; }
        else if (ch == 13 && i > 0) { filename[i] = '\0'; break; }
        else if (ch == 8 && i > 0) { i--; printf("\b \b"); }
        else if (ch >= 32 && ch < 127 && i < 79) { filename[i++] = ch; putchar(ch); }
    }

    fp = fopen(filename, "rb");
    if (fp == NULL) {
        textcolor(LIGHTRED);
        printf("\n\nERROR: Cannot open file '%s'!\n", filename);
        textcolor(LIGHTGRAY);
        printf("Press any key to continue...");
        getch();
        BufferSize = 0; return;
    }

    BufferSize = 0;
    textcolor(LIGHTGREEN);
    printf("\n\nLoading file...\n");

    while (BufferSize < MAX_BUFFER_SIZE && fread(&byte, 1, 1, fp) == 1) {
        DataBuffer[BufferSize++] = byte;
        if ((BufferSize % 256) == 0) printf("\rBytes loaded: %lu", BufferSize);
    }

    fclose(fp);
    textcolor(LIGHTGREEN);
    printf("\n\nFile loaded successfully! %lu bytes read.\n", BufferSize);
    textcolor(LIGHTGRAY);
    printf("Press any key to enter editor...");
    getch();
}

int ShowStartupMenu(void) {
    int ch;

    textmode(C80);
    clrscr();

    textcolor(LIGHTCYAN);
    printf("========================================================================\n");
    printf("               EPROM Data Reader & Hex Editor v1.3\n");
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
    printf("Enter choice (1-4): ");

    while (1) {
        ch = getch();
        if (ch >= '1' && ch <= '4') { printf("%c\n", ch); return ch - '0'; }
        else if (ch == 27) return 4;
    }
}

void Disassemble(void) {
    char outbuf[80];
    char filename[80];
    char cpuname[10];
    int i, addr;
    FILE *fp;
    int yPos = 23;

    if (CPUType == CPU_6800) strcpy(cpuname, "6800");
    else if (CPUType == CPU_8080) strcpy(cpuname, "8080");
    else strcpy(cpuname, "8086");

    sprintf(filename, "DISASM_%s.ASM", cpuname);

    gotoxy(1, yPos);
    textcolor(LIGHTGRAY);
    textbackground(BLACK);
    clreol();
    printf("Enter start address (hex) or press ENTER for 0000: ");

    i = 0; addr = 0; outbuf[0] = '\0';

    while (1) {
        int ch = getch();
        if (ch == 27) return;
        else if (ch == 13) {
            if (i > 0) { outbuf[i] = '\0'; sscanf(outbuf, "%X", &addr); }
            break;
        } else if (ch == 8 && i > 0) { i--; printf("\b \b"); }
        else if (ch >= '0' && ch <= '9' && i < 79) { outbuf[i++] = ch; putchar(ch); }
        else if (((ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f')) && i < 79) { outbuf[i++] = ch; putchar(ch); }
    }

    fp = fopen(filename, "w");
    if (!fp) {
        clrscr();
        textcolor(LIGHTRED);
        printf("ERROR: Cannot create file %s\n", filename);
        textcolor(LIGHTGRAY);
        printf("Press any key to continue...");
        getch(); return;
    }

    clrscr();
    textcolor(YELLOW);
    printf("Disassembling from address %04X...\n", addr);
    printf("Saving to: %s\n", filename);
    textcolor(LIGHTGRAY);
    printf("Press ESC to exit disassembler.\n");
    printf("========================================================================\n");

    fprintf(fp, "; Disassembly Output\n");
    fprintf(fp, "; CPU Type: %s\n", cpuname);
    fprintf(fp, "; Start Address: %04X\n", addr);
    fprintf(fp, "; Developer: Mickey W. Lawless\n");
    fprintf(fp, "; Generated: %s\n", __DATE__);
    fprintf(fp, ";\n");
    fprintf(fp, "; ========================================================================\n\n");

    while (addr < BufferSize) {
        int len;
        if (kbhit() && getch() == 27) {
            printf("\n");
            textcolor(LIGHTRED); printf("Disassembly cancelled by user.\n"); textcolor(LIGHTGRAY);
            fclose(fp); delay(1000); return;
        }
        if (CPUType == CPU_6800) len = Disasm6800(addr, outbuf);
        else if (CPUType == CPU_8080) len = Disasm8080(addr, outbuf);
        else len = Disasm8086(addr, outbuf);

        if (len > 0) {
            printf("%04X: %s\n", addr, outbuf);
            fprintf(fp, "%04X: %s\n", addr, outbuf);
            addr += len;
        } else {
            printf("%04X: DB %02X\n", addr, DataBuffer[addr]);
            fprintf(fp, "%04X: DB %02X\n", addr, DataBuffer[addr]);
            addr++;
        }
    }

    fclose(fp);
    printf("\n");
    textcolor(LIGHTGREEN);
    printf("Disassembly complete! Saved to: %s\n", filename);
    textcolor(LIGHTGRAY);
    printf("Press any key to continue...");
    getch();
}

void SaveDisassembly(int cpuType) {
    FILE *fp;
    char filename[80];
    char asmLine[120];
    unsigned int addr = 0;
    int len, lineCount = 0;
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
        getch(); return;
    }

    switch (cpuType) {
        case CPU_6800: cpuName = "Motorola 6800"; break;
        case CPU_8080: cpuName = "Intel 8080"; break;
        case CPU_8086: cpuName = "Intel 8086"; break;
        default: cpuName = "Unknown"; break;
    }

    fprintf(fp, "; Disassembly generated by EPROM Reader\n");
    fprintf(fp, "; CPU: %s\n", cpuName);
    fprintf(fp, "; Buffer size: %lu bytes\n", BufferSize);
    fprintf(fp, ";\n\n");

    textcolor(LIGHTGREEN);
    printf("\nDisassembling...\n");

    while (addr < BufferSize) {
        int i;
        switch (cpuType) {
            case CPU_6800: len = Disasm6800(addr, asmLine); break;
            case CPU_8080: len = Disasm8080(addr, asmLine); break;
            case CPU_8086: len = Disasm8086(addr, asmLine); break;
            default: len = 1; sprintf(asmLine, "DB $%02X", DataBuffer[addr]); break;
        }
        fprintf(fp, "%04X: ", addr);
        for (i = 0; i < len && addr + i < BufferSize; i++) fprintf(fp, "%02X ", DataBuffer[addr + i]);
        for (; i < 4; i++) fprintf(fp, "   ");
        fprintf(fp, "  %s\n", asmLine);
        addr += len;
        lineCount++;
        if (lineCount % 100 == 0) printf("  %u bytes processed...\n", addr);
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
        if (ch >= '1' && ch <= '3') { printf("%c\n", ch); CPUType = ch - '0'; break; }
        else if (ch == 27) return;
    }
    Disassemble();
}

/* ======================================================================== */
/* Disassemblers - identical logic to v1.1, no changes needed here          */
/* ======================================================================== */

int Disasm6800(unsigned int addr, char *output) {
    unsigned char opcode, operand1, operand2;
    int len = 1;
    if (addr >= BufferSize) { sprintf(output, "FCB $??"); return 1; }
    opcode   = DataBuffer[addr];
    operand1 = (addr + 1 < BufferSize) ? DataBuffer[addr+1] : 0;
    operand2 = (addr + 2 < BufferSize) ? DataBuffer[addr+2] : 0;
    switch (opcode) {
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
        case 0x20: sprintf(output, "BRA $%04X", (addr+2+(signed char)operand1)&0xFFFF); len=2; break;
        case 0x21: sprintf(output, "BRN $%04X", (addr+2+(signed char)operand1)&0xFFFF); len=2; break;
        case 0x22: sprintf(output, "BHI $%04X", (addr+2+(signed char)operand1)&0xFFFF); len=2; break;
        case 0x23: sprintf(output, "BLS $%04X", (addr+2+(signed char)operand1)&0xFFFF); len=2; break;
        case 0x24: sprintf(output, "BCC $%04X", (addr+2+(signed char)operand1)&0xFFFF); len=2; break;
        case 0x25: sprintf(output, "BCS $%04X", (addr+2+(signed char)operand1)&0xFFFF); len=2; break;
        case 0x26: sprintf(output, "BNE $%04X", (addr+2+(signed char)operand1)&0xFFFF); len=2; break;
        case 0x27: sprintf(output, "BEQ $%04X", (addr+2+(signed char)operand1)&0xFFFF); len=2; break;
        case 0x28: sprintf(output, "BVC $%04X", (addr+2+(signed char)operand1)&0xFFFF); len=2; break;
        case 0x29: sprintf(output, "BVS $%04X", (addr+2+(signed char)operand1)&0xFFFF); len=2; break;
        case 0x2A: sprintf(output, "BPL $%04X", (addr+2+(signed char)operand1)&0xFFFF); len=2; break;
        case 0x2B: sprintf(output, "BMI $%04X", (addr+2+(signed char)operand1)&0xFFFF); len=2; break;
        case 0x2C: sprintf(output, "BGE $%04X", (addr+2+(signed char)operand1)&0xFFFF); len=2; break;
        case 0x2D: sprintf(output, "BLT $%04X", (addr+2+(signed char)operand1)&0xFFFF); len=2; break;
        case 0x2E: sprintf(output, "BGT $%04X", (addr+2+(signed char)operand1)&0xFFFF); len=2; break;
        case 0x2F: sprintf(output, "BLE $%04X", (addr+2+(signed char)operand1)&0xFFFF); len=2; break;
        case 0x8D: sprintf(output, "BSR $%04X", (addr+2+(signed char)operand1)&0xFFFF); len=2; break;
        case 0x9D: sprintf(output, "JSR $%02X", operand1); len=2; break;
        case 0xAD: sprintf(output, "JSR %u,X", operand1); len=2; break;
        case 0xBD: sprintf(output, "JSR $%04X", (operand1<<8)|operand2); len=3; break;
        case 0x6E: sprintf(output, "JMP %u,X", operand1); len=2; break;
        case 0x7E: sprintf(output, "JMP $%04X", (operand1<<8)|operand2); len=3; break;
        case 0x86: sprintf(output, "LDAA #$%02X", operand1); len=2; break;
        case 0x96: sprintf(output, "LDAA $%02X", operand1); len=2; break;
        case 0xA6: sprintf(output, "LDAA %u,X", operand1); len=2; break;
        case 0xB6: sprintf(output, "LDAA $%04X", (operand1<<8)|operand2); len=3; break;
        case 0xC6: sprintf(output, "LDAB #$%02X", operand1); len=2; break;
        case 0xD6: sprintf(output, "LDAB $%02X", operand1); len=2; break;
        case 0xE6: sprintf(output, "LDAB %u,X", operand1); len=2; break;
        case 0xF6: sprintf(output, "LDAB $%04X", (operand1<<8)|operand2); len=3; break;
        case 0x97: sprintf(output, "STAA $%02X", operand1); len=2; break;
        case 0xA7: sprintf(output, "STAA %u,X", operand1); len=2; break;
        case 0xB7: sprintf(output, "STAA $%04X", (operand1<<8)|operand2); len=3; break;
        case 0xD7: sprintf(output, "STAB $%02X", operand1); len=2; break;
        case 0xE7: sprintf(output, "STAB %u,X", operand1); len=2; break;
        case 0xF7: sprintf(output, "STAB $%04X", (operand1<<8)|operand2); len=3; break;
        case 0x8B: sprintf(output, "ADDA #$%02X", operand1); len=2; break;
        case 0x9B: sprintf(output, "ADDA $%02X", operand1); len=2; break;
        case 0xAB: sprintf(output, "ADDA %u,X", operand1); len=2; break;
        case 0xBB: sprintf(output, "ADDA $%04X", (operand1<<8)|operand2); len=3; break;
        case 0xCB: sprintf(output, "ADDB #$%02X", operand1); len=2; break;
        case 0xDB: sprintf(output, "ADDB $%02X", operand1); len=2; break;
        case 0xEB: sprintf(output, "ADDB %u,X", operand1); len=2; break;
        case 0xFB: sprintf(output, "ADDB $%04X", (operand1<<8)|operand2); len=3; break;
        case 0x80: sprintf(output, "SUBA #$%02X", operand1); len=2; break;
        case 0x90: sprintf(output, "SUBA $%02X", operand1); len=2; break;
        case 0xA0: sprintf(output, "SUBA %u,X", operand1); len=2; break;
        case 0xB0: sprintf(output, "SUBA $%04X", (operand1<<8)|operand2); len=3; break;
        case 0xC0: sprintf(output, "SUBB #$%02X", operand1); len=2; break;
        case 0xD0: sprintf(output, "SUBB $%02X", operand1); len=2; break;
        case 0xE0: sprintf(output, "SUBB %u,X", operand1); len=2; break;
        case 0xF0: sprintf(output, "SUBB $%04X", (operand1<<8)|operand2); len=3; break;
        case 0x81: sprintf(output, "CMPA #$%02X", operand1); len=2; break;
        case 0x91: sprintf(output, "CMPA $%02X", operand1); len=2; break;
        case 0xA1: sprintf(output, "CMPA %u,X", operand1); len=2; break;
        case 0xB1: sprintf(output, "CMPA $%04X", (operand1<<8)|operand2); len=3; break;
        case 0xC1: sprintf(output, "CMPB #$%02X", operand1); len=2; break;
        case 0xD1: sprintf(output, "CMPB $%02X", operand1); len=2; break;
        case 0xE1: sprintf(output, "CMPB %u,X", operand1); len=2; break;
        case 0xF1: sprintf(output, "CMPB $%04X", (operand1<<8)|operand2); len=3; break;
        case 0x84: sprintf(output, "ANDA #$%02X", operand1); len=2; break;
        case 0x94: sprintf(output, "ANDA $%02X", operand1); len=2; break;
        case 0xA4: sprintf(output, "ANDA %u,X", operand1); len=2; break;
        case 0xB4: sprintf(output, "ANDA $%04X", (operand1<<8)|operand2); len=3; break;
        case 0xC4: sprintf(output, "ANDB #$%02X", operand1); len=2; break;
        case 0xD4: sprintf(output, "ANDB $%02X", operand1); len=2; break;
        case 0xE4: sprintf(output, "ANDB %u,X", operand1); len=2; break;
        case 0xF4: sprintf(output, "ANDB $%04X", (operand1<<8)|operand2); len=3; break;
        case 0x8A: sprintf(output, "ORAA #$%02X", operand1); len=2; break;
        case 0x9A: sprintf(output, "ORAA $%02X", operand1); len=2; break;
        case 0xAA: sprintf(output, "ORAA %u,X", operand1); len=2; break;
        case 0xBA: sprintf(output, "ORAA $%04X", (operand1<<8)|operand2); len=3; break;
        case 0xCA: sprintf(output, "ORAB #$%02X", operand1); len=2; break;
        case 0xDA: sprintf(output, "ORAB $%02X", operand1); len=2; break;
        case 0xEA: sprintf(output, "ORAB %u,X", operand1); len=2; break;
        case 0xFA: sprintf(output, "ORAB $%04X", (operand1<<8)|operand2); len=3; break;
        case 0x88: sprintf(output, "EORA #$%02X", operand1); len=2; break;
        case 0x98: sprintf(output, "EORA $%02X", operand1); len=2; break;
        case 0xA8: sprintf(output, "EORA %u,X", operand1); len=2; break;
        case 0xB8: sprintf(output, "EORA $%04X", (operand1<<8)|operand2); len=3; break;
        case 0xC8: sprintf(output, "EORB #$%02X", operand1); len=2; break;
        case 0xD8: sprintf(output, "EORB $%02X", operand1); len=2; break;
        case 0xE8: sprintf(output, "EORB %u,X", operand1); len=2; break;
        case 0xF8: sprintf(output, "EORB $%04X", (operand1<<8)|operand2); len=3; break;
        case 0x85: sprintf(output, "BITA #$%02X", operand1); len=2; break;
        case 0x95: sprintf(output, "BITA $%02X", operand1); len=2; break;
        case 0xA5: sprintf(output, "BITA %u,X", operand1); len=2; break;
        case 0xB5: sprintf(output, "BITA $%04X", (operand1<<8)|operand2); len=3; break;
        case 0xC5: sprintf(output, "BITB #$%02X", operand1); len=2; break;
        case 0xD5: sprintf(output, "BITB $%02X", operand1); len=2; break;
        case 0xE5: sprintf(output, "BITB %u,X", operand1); len=2; break;
        case 0xF5: sprintf(output, "BITB $%04X", (operand1<<8)|operand2); len=3; break;
        case 0x8E: sprintf(output, "LDS #$%04X", (operand1<<8)|operand2); len=3; break;
        case 0x9E: sprintf(output, "LDS $%02X", operand1); len=2; break;
        case 0xAE: sprintf(output, "LDS %u,X", operand1); len=2; break;
        case 0xBE: sprintf(output, "LDS $%04X", (operand1<<8)|operand2); len=3; break;
        case 0x9F: sprintf(output, "STS $%02X", operand1); len=2; break;
        case 0xAF: sprintf(output, "STS %u,X", operand1); len=2; break;
        case 0xBF: sprintf(output, "STS $%04X", (operand1<<8)|operand2); len=3; break;
        case 0xCE: sprintf(output, "LDX #$%04X", (operand1<<8)|operand2); len=3; break;
        case 0xDE: sprintf(output, "LDX $%02X", operand1); len=2; break;
        case 0xEE: sprintf(output, "LDX %u,X", operand1); len=2; break;
        case 0xFE: sprintf(output, "LDX $%04X", (operand1<<8)|operand2); len=3; break;
        case 0xDF: sprintf(output, "STX $%02X", operand1); len=2; break;
        case 0xEF: sprintf(output, "STX %u,X", operand1); len=2; break;
        case 0xFF: sprintf(output, "STX $%04X", (operand1<<8)|operand2); len=3; break;
        case 0x8C: sprintf(output, "CPX #$%04X", (operand1<<8)|operand2); len=3; break;
        case 0x9C: sprintf(output, "CPX $%02X", operand1); len=2; break;
        case 0xAC: sprintf(output, "CPX %u,X", operand1); len=2; break;
        case 0xBC: sprintf(output, "CPX $%04X", (operand1<<8)|operand2); len=3; break;
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
        case 0x70: sprintf(output, "NEG $%04X", (operand1<<8)|operand2); len=3; break;
        case 0x73: sprintf(output, "COM $%04X", (operand1<<8)|operand2); len=3; break;
        case 0x74: sprintf(output, "LSR $%04X", (operand1<<8)|operand2); len=3; break;
        case 0x76: sprintf(output, "ROR $%04X", (operand1<<8)|operand2); len=3; break;
        case 0x77: sprintf(output, "ASR $%04X", (operand1<<8)|operand2); len=3; break;
        case 0x78: sprintf(output, "ASL $%04X", (operand1<<8)|operand2); len=3; break;
        case 0x79: sprintf(output, "ROL $%04X", (operand1<<8)|operand2); len=3; break;
        case 0x7A: sprintf(output, "DEC $%04X", (operand1<<8)|operand2); len=3; break;
        case 0x7C: sprintf(output, "INC $%04X", (operand1<<8)|operand2); len=3; break;
        case 0x7D: sprintf(output, "TST $%04X", (operand1<<8)|operand2); len=3; break;
        case 0x7F: sprintf(output, "CLR $%04X", (operand1<<8)|operand2); len=3; break;
        case 0x89: sprintf(output, "ADCA #$%02X", operand1); len=2; break;
        case 0x99: sprintf(output, "ADCA $%02X", operand1); len=2; break;
        case 0xA9: sprintf(output, "ADCA %u,X", operand1); len=2; break;
        case 0xB9: sprintf(output, "ADCA $%04X", (operand1<<8)|operand2); len=3; break;
        case 0xC9: sprintf(output, "ADCB #$%02X", operand1); len=2; break;
        case 0xD9: sprintf(output, "ADCB $%02X", operand1); len=2; break;
        case 0xE9: sprintf(output, "ADCB %u,X", operand1); len=2; break;
        case 0xF9: sprintf(output, "ADCB $%04X", (operand1<<8)|operand2); len=3; break;
        case 0x82: sprintf(output, "SBCA #$%02X", operand1); len=2; break;
        case 0x92: sprintf(output, "SBCA $%02X", operand1); len=2; break;
        case 0xA2: sprintf(output, "SBCA %u,X", operand1); len=2; break;
        case 0xB2: sprintf(output, "SBCA $%04X", (operand1<<8)|operand2); len=3; break;
        case 0xC2: sprintf(output, "SBCB #$%02X", operand1); len=2; break;
        case 0xD2: sprintf(output, "SBCB $%02X", operand1); len=2; break;
        case 0xE2: sprintf(output, "SBCB %u,X", operand1); len=2; break;
        case 0xF2: sprintf(output, "SBCB $%04X", (operand1<<8)|operand2); len=3; break;
        default: sprintf(output, "FCB $%02X", opcode); break;
    }
    return len;
}

int Disasm8080(unsigned int addr, char *output) {
    unsigned char opcode, byte1, byte2;
    unsigned int targetAddr;
    int len = 1;
    if (addr >= BufferSize) { sprintf(output, "???"); return 1; }
    opcode = DataBuffer[addr];
    byte1  = (addr+1 < BufferSize) ? DataBuffer[addr+1] : 0;
    byte2  = (addr+2 < BufferSize) ? DataBuffer[addr+2] : 0;
    switch (opcode) {
        case 0x00: sprintf(output, "NOP"); break;
        case 0x76: sprintf(output, "HLT"); break;
        case 0xFB: sprintf(output, "EI"); break;
        case 0xF3: sprintf(output, "DI"); break;
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
        case 0x06: sprintf(output, "MVI B,%02XH", byte1); len=2; break;
        case 0x0E: sprintf(output, "MVI C,%02XH", byte1); len=2; break;
        case 0x16: sprintf(output, "MVI D,%02XH", byte1); len=2; break;
        case 0x1E: sprintf(output, "MVI E,%02XH", byte1); len=2; break;
        case 0x26: sprintf(output, "MVI H,%02XH", byte1); len=2; break;
        case 0x2E: sprintf(output, "MVI L,%02XH", byte1); len=2; break;
        case 0x36: sprintf(output, "MVI M,%02XH", byte1); len=2; break;
        case 0x3E: sprintf(output, "MVI A,%02XH", byte1); len=2; break;
        case 0x01: targetAddr=byte1|(byte2<<8); sprintf(output, "LXI B,%04XH", targetAddr); len=3; break;
        case 0x11: targetAddr=byte1|(byte2<<8); sprintf(output, "LXI D,%04XH", targetAddr); len=3; break;
        case 0x21: targetAddr=byte1|(byte2<<8); sprintf(output, "LXI H,%04XH", targetAddr); len=3; break;
        case 0x31: targetAddr=byte1|(byte2<<8); sprintf(output, "LXI SP,%04XH", targetAddr); len=3; break;
        case 0x3A: targetAddr=byte1|(byte2<<8); sprintf(output, "LDA %04XH", targetAddr); len=3; break;
        case 0x32: targetAddr=byte1|(byte2<<8); sprintf(output, "STA %04XH", targetAddr); len=3; break;
        case 0x2A: targetAddr=byte1|(byte2<<8); sprintf(output, "LHLD %04XH", targetAddr); len=3; break;
        case 0x22: targetAddr=byte1|(byte2<<8); sprintf(output, "SHLD %04XH", targetAddr); len=3; break;
        case 0x0A: sprintf(output, "LDAX B"); break;
        case 0x1A: sprintf(output, "LDAX D"); break;
        case 0x02: sprintf(output, "STAX B"); break;
        case 0x12: sprintf(output, "STAX D"); break;
        case 0xEB: sprintf(output, "XCHG"); break;
        case 0xE3: sprintf(output, "XTHL"); break;
        case 0xF9: sprintf(output, "SPHL"); break;
        case 0xE9: sprintf(output, "PCHL"); break;
        case 0x80: sprintf(output, "ADD B"); break;
        case 0x81: sprintf(output, "ADD C"); break;
        case 0x82: sprintf(output, "ADD D"); break;
        case 0x83: sprintf(output, "ADD E"); break;
        case 0x84: sprintf(output, "ADD H"); break;
        case 0x85: sprintf(output, "ADD L"); break;
        case 0x86: sprintf(output, "ADD M"); break;
        case 0x87: sprintf(output, "ADD A"); break;
        case 0xC6: sprintf(output, "ADI %02XH", byte1); len=2; break;
        case 0x88: sprintf(output, "ADC B"); break;
        case 0x89: sprintf(output, "ADC C"); break;
        case 0x8A: sprintf(output, "ADC D"); break;
        case 0x8B: sprintf(output, "ADC E"); break;
        case 0x8C: sprintf(output, "ADC H"); break;
        case 0x8D: sprintf(output, "ADC L"); break;
        case 0x8E: sprintf(output, "ADC M"); break;
        case 0x8F: sprintf(output, "ADC A"); break;
        case 0xCE: sprintf(output, "ACI %02XH", byte1); len=2; break;
        case 0x90: sprintf(output, "SUB B"); break;
        case 0x91: sprintf(output, "SUB C"); break;
        case 0x92: sprintf(output, "SUB D"); break;
        case 0x93: sprintf(output, "SUB E"); break;
        case 0x94: sprintf(output, "SUB H"); break;
        case 0x95: sprintf(output, "SUB L"); break;
        case 0x96: sprintf(output, "SUB M"); break;
        case 0x97: sprintf(output, "SUB A"); break;
        case 0xD6: sprintf(output, "SUI %02XH", byte1); len=2; break;
        case 0x98: sprintf(output, "SBB B"); break;
        case 0x99: sprintf(output, "SBB C"); break;
        case 0x9A: sprintf(output, "SBB D"); break;
        case 0x9B: sprintf(output, "SBB E"); break;
        case 0x9C: sprintf(output, "SBB H"); break;
        case 0x9D: sprintf(output, "SBB L"); break;
        case 0x9E: sprintf(output, "SBB M"); break;
        case 0x9F: sprintf(output, "SBB A"); break;
        case 0xDE: sprintf(output, "SBI %02XH", byte1); len=2; break;
        case 0xA0: sprintf(output, "ANA B"); break;
        case 0xA1: sprintf(output, "ANA C"); break;
        case 0xA2: sprintf(output, "ANA D"); break;
        case 0xA3: sprintf(output, "ANA E"); break;
        case 0xA4: sprintf(output, "ANA H"); break;
        case 0xA5: sprintf(output, "ANA L"); break;
        case 0xA6: sprintf(output, "ANA M"); break;
        case 0xA7: sprintf(output, "ANA A"); break;
        case 0xE6: sprintf(output, "ANI %02XH", byte1); len=2; break;
        case 0xA8: sprintf(output, "XRA B"); break;
        case 0xA9: sprintf(output, "XRA C"); break;
        case 0xAA: sprintf(output, "XRA D"); break;
        case 0xAB: sprintf(output, "XRA E"); break;
        case 0xAC: sprintf(output, "XRA H"); break;
        case 0xAD: sprintf(output, "XRA L"); break;
        case 0xAE: sprintf(output, "XRA M"); break;
        case 0xAF: sprintf(output, "XRA A"); break;
        case 0xEE: sprintf(output, "XRI %02XH", byte1); len=2; break;
        case 0xB0: sprintf(output, "ORA B"); break;
        case 0xB1: sprintf(output, "ORA C"); break;
        case 0xB2: sprintf(output, "ORA D"); break;
        case 0xB3: sprintf(output, "ORA E"); break;
        case 0xB4: sprintf(output, "ORA H"); break;
        case 0xB5: sprintf(output, "ORA L"); break;
        case 0xB6: sprintf(output, "ORA M"); break;
        case 0xB7: sprintf(output, "ORA A"); break;
        case 0xF6: sprintf(output, "ORI %02XH", byte1); len=2; break;
        case 0xB8: sprintf(output, "CMP B"); break;
        case 0xB9: sprintf(output, "CMP C"); break;
        case 0xBA: sprintf(output, "CMP D"); break;
        case 0xBB: sprintf(output, "CMP E"); break;
        case 0xBC: sprintf(output, "CMP H"); break;
        case 0xBD: sprintf(output, "CMP L"); break;
        case 0xBE: sprintf(output, "CMP M"); break;
        case 0xBF: sprintf(output, "CMP A"); break;
        case 0xFE: sprintf(output, "CPI %02XH", byte1); len=2; break;
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
        case 0x03: sprintf(output, "INX B"); break;
        case 0x13: sprintf(output, "INX D"); break;
        case 0x23: sprintf(output, "INX H"); break;
        case 0x33: sprintf(output, "INX SP"); break;
        case 0x0B: sprintf(output, "DCX B"); break;
        case 0x1B: sprintf(output, "DCX D"); break;
        case 0x2B: sprintf(output, "DCX H"); break;
        case 0x3B: sprintf(output, "DCX SP"); break;
        case 0x09: sprintf(output, "DAD B"); break;
        case 0x19: sprintf(output, "DAD D"); break;
        case 0x29: sprintf(output, "DAD H"); break;
        case 0x39: sprintf(output, "DAD SP"); break;
        case 0x07: sprintf(output, "RLC"); break;
        case 0x0F: sprintf(output, "RRC"); break;
        case 0x17: sprintf(output, "RAL"); break;
        case 0x1F: sprintf(output, "RAR"); break;
        case 0x2F: sprintf(output, "CMA"); break;
        case 0x3F: sprintf(output, "CMC"); break;
        case 0x37: sprintf(output, "STC"); break;
        case 0x27: sprintf(output, "DAA"); break;
        case 0xC3: targetAddr=byte1|(byte2<<8); sprintf(output, "JMP %04XH", targetAddr); len=3; break;
        case 0xC2: targetAddr=byte1|(byte2<<8); sprintf(output, "JNZ %04XH", targetAddr); len=3; break;
        case 0xCA: targetAddr=byte1|(byte2<<8); sprintf(output, "JZ %04XH", targetAddr); len=3; break;
        case 0xD2: targetAddr=byte1|(byte2<<8); sprintf(output, "JNC %04XH", targetAddr); len=3; break;
        case 0xDA: targetAddr=byte1|(byte2<<8); sprintf(output, "JC %04XH", targetAddr); len=3; break;
        case 0xE2: targetAddr=byte1|(byte2<<8); sprintf(output, "JPO %04XH", targetAddr); len=3; break;
        case 0xEA: targetAddr=byte1|(byte2<<8); sprintf(output, "JPE %04XH", targetAddr); len=3; break;
        case 0xF2: targetAddr=byte1|(byte2<<8); sprintf(output, "JP %04XH", targetAddr); len=3; break;
        case 0xFA: targetAddr=byte1|(byte2<<8); sprintf(output, "JM %04XH", targetAddr); len=3; break;
        case 0xCD: targetAddr=byte1|(byte2<<8); sprintf(output, "CALL %04XH", targetAddr); len=3; break;
        case 0xC4: targetAddr=byte1|(byte2<<8); sprintf(output, "CNZ %04XH", targetAddr); len=3; break;
        case 0xCC: targetAddr=byte1|(byte2<<8); sprintf(output, "CZ %04XH", targetAddr); len=3; break;
        case 0xD4: targetAddr=byte1|(byte2<<8); sprintf(output, "CNC %04XH", targetAddr); len=3; break;
        case 0xDC: targetAddr=byte1|(byte2<<8); sprintf(output, "CC %04XH", targetAddr); len=3; break;
        case 0xE4: targetAddr=byte1|(byte2<<8); sprintf(output, "CPO %04XH", targetAddr); len=3; break;
        case 0xEC: targetAddr=byte1|(byte2<<8); sprintf(output, "CPE %04XH", targetAddr); len=3; break;
        case 0xF4: targetAddr=byte1|(byte2<<8); sprintf(output, "CP %04XH", targetAddr); len=3; break;
        case 0xFC: targetAddr=byte1|(byte2<<8); sprintf(output, "CM %04XH", targetAddr); len=3; break;
        case 0xC9: sprintf(output, "RET"); break;
        case 0xC0: sprintf(output, "RNZ"); break;
        case 0xC8: sprintf(output, "RZ"); break;
        case 0xD0: sprintf(output, "RNC"); break;
        case 0xD8: sprintf(output, "RC"); break;
        case 0xE0: sprintf(output, "RPO"); break;
        case 0xE8: sprintf(output, "RPE"); break;
        case 0xF0: sprintf(output, "RP"); break;
        case 0xF8: sprintf(output, "RM"); break;
        case 0xC7: sprintf(output, "RST 0"); break;
        case 0xCF: sprintf(output, "RST 1"); break;
        case 0xD7: sprintf(output, "RST 2"); break;
        case 0xDF: sprintf(output, "RST 3"); break;
        case 0xE7: sprintf(output, "RST 4"); break;
        case 0xEF: sprintf(output, "RST 5"); break;
        case 0xF7: sprintf(output, "RST 6"); break;
        case 0xFF: sprintf(output, "RST 7"); break;
        case 0xC5: sprintf(output, "PUSH B"); break;
        case 0xD5: sprintf(output, "PUSH D"); break;
        case 0xE5: sprintf(output, "PUSH H"); break;
        case 0xF5: sprintf(output, "PUSH PSW"); break;
        case 0xC1: sprintf(output, "POP B"); break;
        case 0xD1: sprintf(output, "POP D"); break;
        case 0xE1: sprintf(output, "POP H"); break;
        case 0xF1: sprintf(output, "POP PSW"); break;
        case 0xD3: sprintf(output, "OUT %02XH", byte1); len=2; break;
        case 0xDB: sprintf(output, "IN %02XH", byte1); len=2; break;
        case 0x20: sprintf(output, "RIM"); break;
        case 0x30: sprintf(output, "SIM"); break;
        default: sprintf(output, "DB %02XH", opcode); break;
    }
    return len;
}

int Disasm8086(unsigned int addr, char *output) {
    unsigned char opcode, byte1, byte2;
    unsigned int targetAddr;
    int len = 1;
    signed short offset;
    if (addr >= BufferSize) { sprintf(output, "???"); return 1; }
    opcode = DataBuffer[addr];
    byte1  = (addr+1 < BufferSize) ? DataBuffer[addr+1] : 0;
    byte2  = (addr+2 < BufferSize) ? DataBuffer[addr+2] : 0;
    switch (opcode) {
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
        case 0xB0: sprintf(output, "MOV AL,%02Xh", byte1); len=2; break;
        case 0xB1: sprintf(output, "MOV CL,%02Xh", byte1); len=2; break;
        case 0xB2: sprintf(output, "MOV DL,%02Xh", byte1); len=2; break;
        case 0xB3: sprintf(output, "MOV BL,%02Xh", byte1); len=2; break;
        case 0xB4: sprintf(output, "MOV AH,%02Xh", byte1); len=2; break;
        case 0xB5: sprintf(output, "MOV CH,%02Xh", byte1); len=2; break;
        case 0xB6: sprintf(output, "MOV DH,%02Xh", byte1); len=2; break;
        case 0xB7: sprintf(output, "MOV BH,%02Xh", byte1); len=2; break;
        case 0xB8: targetAddr=byte1|(byte2<<8); sprintf(output, "MOV AX,%04Xh", targetAddr); len=3; break;
        case 0xB9: targetAddr=byte1|(byte2<<8); sprintf(output, "MOV CX,%04Xh", targetAddr); len=3; break;
        case 0xBA: targetAddr=byte1|(byte2<<8); sprintf(output, "MOV DX,%04Xh", targetAddr); len=3; break;
        case 0xBB: targetAddr=byte1|(byte2<<8); sprintf(output, "MOV BX,%04Xh", targetAddr); len=3; break;
        case 0xBC: targetAddr=byte1|(byte2<<8); sprintf(output, "MOV SP,%04Xh", targetAddr); len=3; break;
        case 0xBD: targetAddr=byte1|(byte2<<8); sprintf(output, "MOV BP,%04Xh", targetAddr); len=3; break;
        case 0xBE: targetAddr=byte1|(byte2<<8); sprintf(output, "MOV SI,%04Xh", targetAddr); len=3; break;
        case 0xBF: targetAddr=byte1|(byte2<<8); sprintf(output, "MOV DI,%04Xh", targetAddr); len=3; break;
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
        case 0x91: sprintf(output, "XCHG AX,CX"); break;
        case 0x92: sprintf(output, "XCHG AX,DX"); break;
        case 0x93: sprintf(output, "XCHG AX,BX"); break;
        case 0x94: sprintf(output, "XCHG AX,SP"); break;
        case 0x95: sprintf(output, "XCHG AX,BP"); break;
        case 0x96: sprintf(output, "XCHG AX,SI"); break;
        case 0x97: sprintf(output, "XCHG AX,DI"); break;
        case 0x04: sprintf(output, "ADD AL,%02Xh", byte1); len=2; break;
        case 0x0C: sprintf(output, "OR AL,%02Xh", byte1); len=2; break;
        case 0x14: sprintf(output, "ADC AL,%02Xh", byte1); len=2; break;
        case 0x1C: sprintf(output, "SBB AL,%02Xh", byte1); len=2; break;
        case 0x24: sprintf(output, "AND AL,%02Xh", byte1); len=2; break;
        case 0x2C: sprintf(output, "SUB AL,%02Xh", byte1); len=2; break;
        case 0x34: sprintf(output, "XOR AL,%02Xh", byte1); len=2; break;
        case 0x3C: sprintf(output, "CMP AL,%02Xh", byte1); len=2; break;
        case 0x05: targetAddr=byte1|(byte2<<8); sprintf(output, "ADD AX,%04Xh", targetAddr); len=3; break;
        case 0x0D: targetAddr=byte1|(byte2<<8); sprintf(output, "OR AX,%04Xh", targetAddr); len=3; break;
        case 0x15: targetAddr=byte1|(byte2<<8); sprintf(output, "ADC AX,%04Xh", targetAddr); len=3; break;
        case 0x1D: targetAddr=byte1|(byte2<<8); sprintf(output, "SBB AX,%04Xh", targetAddr); len=3; break;
        case 0x25: targetAddr=byte1|(byte2<<8); sprintf(output, "AND AX,%04Xh", targetAddr); len=3; break;
        case 0x2D: targetAddr=byte1|(byte2<<8); sprintf(output, "SUB AX,%04Xh", targetAddr); len=3; break;
        case 0x35: targetAddr=byte1|(byte2<<8); sprintf(output, "XOR AX,%04Xh", targetAddr); len=3; break;
        case 0x3D: targetAddr=byte1|(byte2<<8); sprintf(output, "CMP AX,%04Xh", targetAddr); len=3; break;
        case 0x70: targetAddr=(addr+2+(signed char)byte1)&0xFFFF; sprintf(output, "JO %04Xh", targetAddr); len=2; break;
        case 0x71: targetAddr=(addr+2+(signed char)byte1)&0xFFFF; sprintf(output, "JNO %04Xh", targetAddr); len=2; break;
        case 0x72: targetAddr=(addr+2+(signed char)byte1)&0xFFFF; sprintf(output, "JC %04Xh", targetAddr); len=2; break;
        case 0x73: targetAddr=(addr+2+(signed char)byte1)&0xFFFF; sprintf(output, "JNC %04Xh", targetAddr); len=2; break;
        case 0x74: targetAddr=(addr+2+(signed char)byte1)&0xFFFF; sprintf(output, "JZ %04Xh", targetAddr); len=2; break;
        case 0x75: targetAddr=(addr+2+(signed char)byte1)&0xFFFF; sprintf(output, "JNZ %04Xh", targetAddr); len=2; break;
        case 0x76: targetAddr=(addr+2+(signed char)byte1)&0xFFFF; sprintf(output, "JBE %04Xh", targetAddr); len=2; break;
        case 0x77: targetAddr=(addr+2+(signed char)byte1)&0xFFFF; sprintf(output, "JA %04Xh", targetAddr); len=2; break;
        case 0x78: targetAddr=(addr+2+(signed char)byte1)&0xFFFF; sprintf(output, "JS %04Xh", targetAddr); len=2; break;
        case 0x79: targetAddr=(addr+2+(signed char)byte1)&0xFFFF; sprintf(output, "JNS %04Xh", targetAddr); len=2; break;
        case 0x7A: targetAddr=(addr+2+(signed char)byte1)&0xFFFF; sprintf(output, "JPE %04Xh", targetAddr); len=2; break;
        case 0x7B: targetAddr=(addr+2+(signed char)byte1)&0xFFFF; sprintf(output, "JPO %04Xh", targetAddr); len=2; break;
        case 0x7C: targetAddr=(addr+2+(signed char)byte1)&0xFFFF; sprintf(output, "JL %04Xh", targetAddr); len=2; break;
        case 0x7D: targetAddr=(addr+2+(signed char)byte1)&0xFFFF; sprintf(output, "JGE %04Xh", targetAddr); len=2; break;
        case 0x7E: targetAddr=(addr+2+(signed char)byte1)&0xFFFF; sprintf(output, "JLE %04Xh", targetAddr); len=2; break;
        case 0x7F: targetAddr=(addr+2+(signed char)byte1)&0xFFFF; sprintf(output, "JG %04Xh", targetAddr); len=2; break;
        case 0xE0: targetAddr=(addr+2+(signed char)byte1)&0xFFFF; sprintf(output, "LOOPNE %04Xh", targetAddr); len=2; break;
        case 0xE1: targetAddr=(addr+2+(signed char)byte1)&0xFFFF; sprintf(output, "LOOPE %04Xh", targetAddr); len=2; break;
        case 0xE2: targetAddr=(addr+2+(signed char)byte1)&0xFFFF; sprintf(output, "LOOP %04Xh", targetAddr); len=2; break;
        case 0xE3: targetAddr=(addr+2+(signed char)byte1)&0xFFFF; sprintf(output, "JCXZ %04Xh", targetAddr); len=2; break;
        case 0xE8: offset=(short)(byte1|(byte2<<8)); targetAddr=(addr+3+offset)&0xFFFF; sprintf(output, "CALL %04Xh", targetAddr); len=3; break;
        case 0xE9: offset=(short)(byte1|(byte2<<8)); targetAddr=(addr+3+offset)&0xFFFF; sprintf(output, "JMP %04Xh", targetAddr); len=3; break;
        case 0xEA: targetAddr=byte1|(byte2<<8); sprintf(output, "JMP FAR %04Xh", targetAddr); len=5; break;
        case 0xEB: targetAddr=(addr+2+(signed char)byte1)&0xFFFF; sprintf(output, "JMP SHORT %04Xh", targetAddr); len=2; break;
        case 0xC3: sprintf(output, "RET"); break;
        case 0xCB: sprintf(output, "RETF"); break;
        case 0xC2: targetAddr=byte1|(byte2<<8); sprintf(output, "RET %04Xh", targetAddr); len=3; break;
        case 0xCA: targetAddr=byte1|(byte2<<8); sprintf(output, "RETF %04Xh", targetAddr); len=3; break;
        case 0xCC: sprintf(output, "INT 3"); break;
        case 0xCD: sprintf(output, "INT %02Xh", byte1); len=2; break;
        case 0xCE: sprintf(output, "INTO"); break;
        case 0xCF: sprintf(output, "IRET"); break;
        case 0xE4: sprintf(output, "IN AL,%02Xh", byte1); len=2; break;
        case 0xE5: sprintf(output, "IN AX,%02Xh", byte1); len=2; break;
        case 0xE6: sprintf(output, "OUT %02Xh,AL", byte1); len=2; break;
        case 0xE7: sprintf(output, "OUT %02Xh,AX", byte1); len=2; break;
        case 0xEC: sprintf(output, "IN AL,DX"); break;
        case 0xED: sprintf(output, "IN AX,DX"); break;
        case 0xEE: sprintf(output, "OUT DX,AL"); break;
        case 0xEF: sprintf(output, "OUT DX,AX"); break;
        case 0xA0: targetAddr=byte1|(byte2<<8); sprintf(output, "MOV AL,[%04Xh]", targetAddr); len=3; break;
        case 0xA1: targetAddr=byte1|(byte2<<8); sprintf(output, "MOV AX,[%04Xh]", targetAddr); len=3; break;
        case 0xA2: targetAddr=byte1|(byte2<<8); sprintf(output, "MOV [%04Xh],AL", targetAddr); len=3; break;
        case 0xA3: targetAddr=byte1|(byte2<<8); sprintf(output, "MOV [%04Xh],AX", targetAddr); len=3; break;
        case 0xA8: sprintf(output, "TEST AL,%02Xh", byte1); len=2; break;
        case 0xA9: targetAddr=byte1|(byte2<<8); sprintf(output, "TEST AX,%04Xh", targetAddr); len=3; break;
        case 0x06: sprintf(output, "PUSH ES"); break;
        case 0x0E: sprintf(output, "PUSH CS"); break;
        case 0x16: sprintf(output, "PUSH SS"); break;
        case 0x1E: sprintf(output, "PUSH DS"); break;
        case 0x07: sprintf(output, "POP ES"); break;
        case 0x1F: sprintf(output, "POP DS"); break;
        default: sprintf(output, "DB %02Xh", opcode); break;
    }
    return len;
}
