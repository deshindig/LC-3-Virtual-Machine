#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
/* unix */
#include <unistd.h>
#include <fcntl.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/termios.h>
#include <sys/mman.h>

/* Opcodes */
enum {
    OP_BR = 0x0,
    OP_ADD = 0x1,
    OP_LD = 0x2,
    OP_ST = 0x3,
    OP_JSR = 0x4,
    OP_AND = 0x5,
    OP_LDR = 0x6,
    OP_STR = 0x7,
    OP_RTI = 0x8,
    OP_NOT = 0x9,
    OP_LDI = 0xa,
    OP_STI = 0xb,
    OP_JMP = 0xc,
    OP_RES = 0xd,
    OP_LEA = 0xe,
    OP_TRAP = 0xf,
};

/* Trapcodes */
enum {
    TRAP_GETC = 0x20,
    TRAP_OUT = 0x21,
    TRAP_PUTS = 0x22,
    TRAP_IN = 0x23,
    TRAP_PUTSP = 0x24,
    TRAP_HALT = 0x25,
};

/* Condition register flags */
enum {
    FLAG_N = 0x1 << 2,
    FLAG_Z = 0x1 << 1,
    FLAG_P = 0x1 << 0,
};

/* Memory mapped registers */
enum {
    MR_KBSR = 0xfe00, // keyboard status
    MR_KBDR = 0xfe02, // keyboard data
};

typedef struct lc3_state {
    uint16_t mem[UINT16_MAX];
    uint16_t reg[8];    // 8 general purpose registers, R0 to R7
    uint16_t pc;        // Program Counter register
    uint16_t ir;        // Instruction Register
    uint16_t cond;      // Condition register
} lc3_state;

uint16_t sext16(uint16_t x, int bitlen) {
    if ((x >> (bitlen - 1)) & 1) {
        x |= (0xFFFF << bitlen);
    }
    return x;
}

uint16_t swap16(uint16_t x) {
    return (x << 8) | (x >> 8);
}

void set_cc(uint16_t result, lc3_state *state) {
    if (result == 0) {
        state->cond = FLAG_Z;
    } else if (result >> 15) {
        state->cond = FLAG_N;
    } else {
        state->cond = FLAG_P;
    }
}

void read_image_file(FILE* file, lc3_state *state) {
    /* the origin tells us where in memory to place the image */
    uint16_t origin;
    fread(&origin, sizeof(origin), 1, file);
    origin = swap16(origin);

    /* we know the maximum file size so we only need one fread */
    uint16_t max_read = UINT16_MAX - origin;
    uint16_t* ptr = state->mem + origin;
    size_t read = fread(ptr, sizeof(uint16_t), max_read, file);

    /* swap to little endian */
    while (read-- > 0) {
        *ptr = swap16(*ptr);
        ++ptr;
    }
}

int read_image(const char* image_path, lc3_state *state) {
    FILE* file = fopen(image_path, "rb");
    if (!file) {
        return 0;
    };
    read_image_file(file, state);
    fclose(file);
    return 1;
}

uint16_t check_key() {
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    return select(1, &readfds, NULL, NULL, &timeout) != 0;
}

uint16_t mem_read(uint16_t address, lc3_state *state) {
    if (address == MR_KBSR) {
        if (check_key()) {
            state->mem[MR_KBSR] = (0x1 << 15);
            state->mem[MR_KBDR] = getchar();
        }
        else {
            state->mem[MR_KBSR] = 0;
        }
    }
    return state->mem[address];
}

void mem_write(uint16_t address, uint16_t word, lc3_state *state) {
    state->mem[address] = word;
}

struct termios original_tio;

void disable_input_buffering() {
    tcgetattr(STDIN_FILENO, &original_tio);
    struct termios new_tio = original_tio;
    new_tio.c_lflag &= ~ICANON & ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
}

void restore_input_buffering() {
    tcsetattr(STDIN_FILENO, TCSANOW, &original_tio);
}

void handle_interrupt(int signal) {
    restore_input_buffering();
    printf("\n");
    exit(-2);
}

int main(int argc, char *argv[]) {
    lc3_state *state = malloc(sizeof(lc3_state));
    if (argc < 2) {
        /* show usage string */
        printf("lc3-vm [image_file1] ...\n");
        exit(2);
    }

    for (int i = 1; i < argc; ++i) {
        if (!read_image(argv[i], state)) {
            printf("failed to load image: %s\n", argv[i]);
            exit(1);
        }
    }

    signal(SIGINT, handle_interrupt);
    disable_input_buffering();
    enum {
        ORIG = 0x3000,
    };
    state->pc = ORIG;

    int run_flag = 1;
    while (run_flag) {
        state->ir = mem_read(state->pc++, state);
        uint16_t opcode = state->ir >> 12;

        switch (opcode) {
            case OP_ADD:
            {
                uint16_t sr1 = (state->ir >> 6) & 0x7;
                uint16_t imm_mode = (state->ir >> 5) & 0x1;
                uint16_t result;
                if (imm_mode) {
                    uint16_t imm = 0x001f;
                    result = state->reg[sr1] + sext16(imm, 5);
                } else {
                    uint16_t sr2 = state->ir & 0x0007;
                    result = state->reg[sr1] + state->reg[sr2];
                }
                uint16_t dr = (state->ir >> 9) & 0x7;
                state->reg[dr] = result;
                set_cc(result, state);
                break;
            }
            case OP_AND:
            {
                uint16_t sr1 = (state->ir >> 6) & 0x7;
                uint16_t imm_mode = (state->ir >> 5) & 0x1;
                uint16_t result;
                if (imm_mode) {
                    uint16_t imm = 0x001f;
                    result = state->reg[sr1] & sext16(imm, 5);
                } else {
                    uint16_t sr2 = state->ir & 0x7;
                    result = state->reg[sr1] & state->reg[sr2];
                }
                uint16_t dr = (state->ir >> 9) & 0x7;
                state->reg[dr] = result;
                set_cc(result, state);
                break;
            }
            case OP_NOT:
            {
                uint16_t sr = (state->ir >> 6) & 0x7;
                uint16_t dr = (state->ir >> 9) & 0x7;
                uint16_t result = ~(state->reg[sr]);
                state->reg[dr] = result;
                set_cc(result, state);
                break;
            }
            case OP_BR:
            {
                uint16_t nzp = (state->ir >> 9) & 0x7;
                if (nzp & state->cond) {
                    uint16_t pc_offset = sext16(state->ir & 0x01ff, 9);
                    state->pc += pc_offset;
                }
                break;
            }
            case OP_JMP:
            {
                uint16_t base_r = (state->ir >> 6) & 0x7;
                state->pc = state->reg[base_r];
                break;
            }
            case OP_JSR:
            {
                state->reg[7] = state->pc;
                uint16_t imm_mode = (state->ir >> 11) & 0x1;
                if (imm_mode) {
                    uint16_t pc_offset = sext16(state->ir & 0x07ff, 9);
                    state->pc += pc_offset;
                } else {
                    uint16_t base_r = (state->ir >> 6) & 0x7;
                    state->pc = state->reg[base_r];
                }
                break;
            }
            case OP_LD:
            {
                uint16_t dr = (state->ir >> 9) & 0x7;
                uint16_t pc_offset = sext16(state->ir & 0x01ff, 9);
                uint16_t result = mem_read(state->pc + pc_offset, state);
                state->reg[dr] = result;
                set_cc(result, state);
                break;
            }
            case OP_LDI:
            {
                uint16_t dr = (state->ir >> 9) & 0x7;
                uint16_t pc_offset = sext16(state->ir & 0x01ff, 9);
                uint16_t result = mem_read(state->pc + pc_offset, state);
                result = mem_read(result, state);
                state->reg[dr] = result;
                set_cc(result, state);
                break;
            }
            case OP_LDR:
            {
                uint16_t base_r = (state->ir >> 6) & 0x7;
                uint16_t dr = (state->ir >> 9) & 0x7;
                uint16_t offset = sext16(state->ir & 0x003f, 6);
                uint16_t result = mem_read(state->reg[base_r] + offset, state);
                state->reg[dr] = result;
                set_cc(result, state);
                break;
            }
            case OP_LEA:
            {
                uint16_t dr = (state->ir >> 9) & 0x7;
                uint16_t pc_offset = sext16(state->ir & 0x01ff, 9);
                uint16_t result = state->pc + pc_offset;
                state->reg[dr] = result;
                set_cc(result, state);
                break;
            }
            case OP_ST:
            {
                uint16_t sr = (state->ir >> 9) & 0x7;
                uint16_t pc_offset = sext16(state->ir & 0x01ff, 9);
                uint16_t write_to = state->pc + pc_offset;
                mem_write(write_to, state->reg[sr], state);
                break;
            }
            case OP_STI:
            {
                uint16_t sr = (state->ir >> 9) & 0x7;
                uint16_t pc_offset = sext16(state->ir & 0x01ff, 9);
                uint16_t write_to = mem_read(state->pc + pc_offset, state);
                mem_write(write_to, state->reg[sr], state);
                break;
            }
            case OP_STR:
            {
                uint16_t base_r = (state->ir >> 6) & 0x7;
                uint16_t sr = (state->ir >> 9) & 0x7;
                uint16_t offset = sext16(state->ir & 0x003f, 6);
                uint16_t write_to = state->reg[base_r] + offset;
                mem_write(write_to, state->reg[sr], state);
                break;
            }
            case OP_TRAP:
            {
                // uint16_t trapvect = state->ir & 0x00ff;
                // state->reg[7] = state->pc;
                // state->pc = mem_read(trapvect, state);
                uint16_t trapcode = state->ir & 0x00ff;
                switch (trapcode) {
                    case TRAP_GETC:
                    {
                        state->reg[0] = (uint16_t) getchar();
                        break;
                    }
                    case TRAP_OUT:
                    {
                        putc((char) state->reg[0], stdout);
                        fflush(stdout);
                        break;
                    }
                    case TRAP_PUTS:
                    {
                        uint16_t *ptr = state->mem + state->reg[0];

                        while (*ptr) {
                            putc((char) *ptr, stdout);
                            ++ptr;
                        }

                        fflush(stdout);
                        break;
                    }
                    case TRAP_IN:
                    {
                        printf("Enter a character: ");
                        char c = getchar();
                        putc(c, stdout);
                        state->reg[0] = (uint16_t) c;
                        break;
                    }
                    case TRAP_PUTSP:
                    {
                        uint16_t *ptr = state->mem + state->reg[0];

                        while (*ptr) {
                            char char1 = (*ptr) & 0xFF;
                            putc(char1, stdout);
                            char char2 = (*ptr) >> 8;
                            if (char2) putc(char2, stdout);
                            ++ptr;
                        }

                        fflush(stdout);
                        break;
                    }
                    case TRAP_HALT:
                    {
                        puts("HALT");
                        fflush(stdout);
                        run_flag = 0;
                        break;
                    }
                }
                break;
            }
            case OP_RES:
            case OP_RTI:
            default:
                printf("bad opcode");
                abort();
                break;
        }
    }

    free(state);
    restore_input_buffering();
}