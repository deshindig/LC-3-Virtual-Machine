#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/* Opcodes */
const unsigned int OPCODE_BR = 0x0;
const unsigned int OPCODE_ADD = 0x1;
const unsigned int OPCODE_LD = 0x2;
const unsigned int OPCODE_ST = 0x3;
const unsigned int OPCODE_JSR = 0x4;
const unsigned int OPCODE_AND = 0x5;
const unsigned int OPCODE_LDR = 0x6;
const unsigned int OPCODE_STR = 0x7;
const unsigned int OPCODE_RTI = 0x8;
const unsigned int OPCODE_NOT = 0x9;
const unsigned int OPCODE_LDI = 0xa;
const unsigned int OPCODE_STI = 0xb;
const unsigned int OPCODE_JMP = 0xc;
const unsigned int OPCODE_RES = 0xd;
const unsigned int OPCODE_LEA = 0xe;
const unsigned int OPCODE_TRAP = 0xf;

/* Condition register flags */
const unsigned int FLAG_N = 0x1 << 2;
const unsigned int FLAG_Z = 0x1 << 1;
const unsigned int FLAG_P = 0x1 << 0;

typedef struct lc3_state {
    uint16_t mem[UINT16_MAX];
    uint16_t reg[8];    // 8 general purpose registers, R0 to R7
    uint16_t pc;        // Program Counter register
    uint16_t ir;        // Instruction Register
    uint16_t cond;      // Condition register
} lc3_state;

uint16_t mem_read(uint16_t address, lc3_state *state) {
    return state->mem[address];
}

void mem_write(uint16_t address, uint16_t word, lc3_state *state) {
    state->mem[address] = word;
}

void set_cc(uint16_t result, lc3_state *state) {
    if (result == 0) {
        state->cond = FLAG_Z;
    } else {
        state->cond = result & 0x8000 == 0? FLAG_P: FLAG_N;
    }
}

uint16_t sext16(uint16_t value, int bitlen) {
    return 0;
}

int main(int argc, char *argv[]) {
    lc3_state *state = malloc(sizeof(lc3_state));
    const ORIG = 0x3000;
    state->pc = ORIG;

    while (1) {
        state->ir = mem_read(state->pc++, state);
        uint16_t opcode = state->ir >> 12;

        switch (opcode) {
            case OPCODE_ADD:
                uint16_t sr1 = (state->ir >> 6) & 0x7;
                uint16_t imm_mode = (state->ir >> 5) & 0x1;
                uint16_t result;
                if (imm_mode == 1) {
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
            case OPCODE_AND:
                uint16_t sr1 = (state->ir >> 6) & 0x7;
                uint16_t imm_mode = (state->ir >> 5) & 0x1;
                uint16_t result;
                if (imm_mode == 1) {
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
            case OPCODE_NOT:
                uint16_t sr = (state->ir >> 6) & 0x7;
                uint16_t dr = (state->ir >> 9) & 0x7;
                uint16_t result = ~state->reg[sr];
                state->reg[dr] = result;
                set_cc(result, state);
                break;
            case OPCODE_BR:
                uint16_t nzp = (state->ir >> 9) & 0x7;
                if (nzp != 0 && nzp == state->cond) {
                    uint16_t pc_offset = sext16(state->ir & 0x01ff, 9);
                    state->pc += pc_offset;
                }
                break;
            case OPCODE_JMP:
                uint16_t base_r = (state->ir >> 6) & 0x7;
                state->pc = state->reg[base_r];
                break;
            case OPCODE_JSR:
                state->reg[7] = state->pc;
                uint16_t imm_mode = (state->ir >> 11) & 0x1;
                if (imm_mode == 1) {
                    uint16_t pc_offset = sext16(state->ir & 0x07ff, 9);
                    state->pc += pc_offset;
                } else {
                    uint16_t base_r = (state->ir >> 6) & 0x7;
                    state->pc = state->reg[base_r];
                }
                break;
            case OPCODE_LD:
                uint16_t dr = (state->ir >> 9) & 0x7;
                uint16_t pc_offset = sext16(state->ir & 0x01ff, 9);
                uint16_t result = mem_read(state->pc + pc_offset, state);
                state->reg[dr] = result;
                set_cc(result, state);
                break;
            case OPCODE_LDI:
                uint16_t dr = (state->ir >> 9) & 0x7;
                uint16_t pc_offset = sext16(state->ir & 0x01ff, 9);
                uint16_t result = mem_read(state->pc + pc_offset, state);
                result = mem_read(result, state);
                state->reg[dr] = result;
                set_cc(result, state);
                break;
            case OPCODE_LDR:
                uint16_t base_r = (state->ir >> 6) & 0x7;
                uint16_t dr = (state->ir >> 9) & 0x7;
                uint16_t offset = sext16(state->ir & 0x003f, 6);
                uint16_t result = mem_read(state->reg[base_r] + offset, state);
                state->reg[dr] = result;
                set_cc(result, state);
                break;
            case OPCODE_LEA:
                uint16_t dr = (state->ir >> 9) & 0x7;
                uint16_t pc_offset = sext16(state->ir & 0x01ff, 9);
                uint16_t result = state->pc + pc_offset;
                state->reg[dr] = result;
                set_cc(result, state);
                break;
            case OPCODE_ST:
                uint16_t sr = (state->ir >> 9) & 0x7;
                uint16_t pc_offset = sext16(state->ir & 0x01ff, 9);
                uint16_t write_to = state->pc + pc_offset;
                mem_write(write_to, state->reg[sr], state);
                break;
            case OPCODE_STI:
                uint16_t sr = (state->ir >> 9) & 0x7;
                uint16_t pc_offset = sext16(state->ir & 0x01ff, 9);
                uint16_t write_to = mem_read(state->pc + pc_offset, state);
                mem_write(write_to, state->reg[sr], state);
                break;
            case OPCODE_STR:
                uint16_t base_r = (state->ir >> 6) & 0x7;
                uint16_t sr = (state->ir >> 9) & 0x7;
                uint16_t offset = sext16(state->ir & 0x003f, 6);
                uint16_t write_to = state->reg[base_r] + offset;
                mem_write(write_to, state->reg[sr], state);
                break;
            case OPCODE_TRAP:
                uint16_t trapvect = state->ir & 0x00ff;
                state->reg[7] = state->pc;
                state->pc = mem_read(trapvect, state);
                break;
            case OPCODE_RES:
                printf("bad opcode");
                abort();
                break;
            case OPCODE_RTI:
                printf("bad opcode");
                abort();
                break;
            default:
                printf("bad opcode");
                abort();
                break;
        }
    }
}