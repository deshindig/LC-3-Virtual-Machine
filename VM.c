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

/* condition register flags */
const unsigned int FLAG_N = 0x1 << 2;
const unsigned int FLAG_Z = 0x1 << 1;
const unsigned int FLAG_P = 0x1 << 0;

uint16_t mem[UINT16_MAX];
uint16_t reg[8];    // 8 general purpose registers, R0 to R7
uint16_t pc;        // Program Counter register
uint16_t ir;        // Instruction Register
uint16_t cond;      // Condition register

uint16_t mem_read(uint16_t address) {
    return mem[address];
}

void mem_write(uint16_t address, uint16_t word) {
    mem[address] = word;
}

void set_cc(uint16_t result) {
    if (result == 0) {
        cond = FLAG_Z;
    } else {
        cond = result & 0x8000 == 0? FLAG_P: FLAG_N;
    }
}

uint16_t sext(uint16_t value, int bitlen) {
    return 0;
}

int main(int argc, char *argv[]) {
    const ORIG = 0x3000;
    pc = ORIG;

    while (1) {
        ir = mem_read(pc++);
        uint16_t opcode = ir >> 12;

        switch (opcode) {
            case OPCODE_ADD:
                uint16_t sr1 = (ir >> 6) & 0x7;
                uint16_t imm_mode = (ir >> 5) & 0x1;
                uint16_t result;
                if (imm_mode == 1) {
                    uint16_t imm = 0x001f;
                    result = reg[sr1] + sext(imm, 5);
                } else {
                    uint16_t sr2 = ir & 0x0007;
                    result = reg[sr1] + reg[sr2];
                }
                uint16_t dr = (ir >> 9) & 0x7;
                reg[dr] = result;
                set_cc(result);
                break;
            case OPCODE_AND:
                uint16_t sr1 = (ir >> 6) & 0x7;
                uint16_t imm_mode = (ir >> 5) & 0x1;
                uint16_t result;
                if (imm_mode == 1) {
                    uint16_t imm = 0x001f;
                    result = reg[sr1] & sext(imm, 5);
                } else {
                    uint16_t sr2 = ir & 0x7;
                    result = reg[sr1] & reg[sr2];
                }
                uint16_t dr = (ir >> 9) & 0x7;
                reg[dr] = result;
                set_cc(result);
                break;
            case OPCODE_NOT:
                uint16_t sr = (ir >> 6) & 0x7;
                uint16_t dr = (ir >> 9) & 0x7;
                uint16_t result = ~reg[sr];
                reg[dr] = result;
                set_cc(result);
                break;
            case OPCODE_BR:
                uint16_t nzp = (ir >> 9) & 0x7;
                if (nzp != 0 && nzp == cond) {
                    uint16_t pc_offset = sext(ir & 0x01ff, 9);
                    pc += pc_offset;
                }
                break;
            case OPCODE_JMP:
                uint16_t base_r = (ir >> 6) & 0x7;
                pc = reg[base_r];
                break;
            case OPCODE_JSR:
                reg[7] = pc;
                uint16_t imm_mode = (ir >> 11) & 0x1;
                if (imm_mode == 1) {
                    uint16_t pc_offset = sext(ir & 0x07ff, 9);
                    pc += pc_offset;
                } else {
                    uint16_t base_r = (ir >> 6) & 0x7;
                    pc = reg[base_r];
                }
                break;
            case OPCODE_LD:
                uint16_t dr = (ir >> 9) & 0x7;
                uint16_t pc_offset = sext(ir & 0x01ff, 9);
                uint16_t result = mem_read(pc + pc_offset);
                reg[dr] = result;
                set_cc(result);
                break;
            case OPCODE_LDI:
                uint16_t dr = (ir >> 9) & 0x7;
                uint16_t pc_offset = sext(ir & 0x01ff, 9);
                uint16_t result = mem_read(mem_read(pc + pc_offset));
                reg[dr] = result;
                set_cc(result);
                break;
            case OPCODE_LDR:
                uint16_t base_r = (ir >> 6) & 0x7;
                uint16_t dr = (ir >> 9) & 0x7;
                uint16_t pc_offset = sext(ir & 0x003f, 6);
                uint16_t result = mem_read(reg[base_r] + pc_offset);
                reg[dr] = result;
                set_cc(result);
                break;
            case OPCODE_LEA:
                uint16_t dr = (ir >> 9) & 0x7;
                uint16_t pc_offset = sext(ir & 0x01ff, 9);
                uint16_t result = pc + pc_offset;
                reg[dr] = result;
                set_cc(result);
                break;
            case OPCODE_ST:
                uint16_t sr = (ir >> 9) & 0x7;
                uint16_t pc_offset = sext(ir & 0x01ff, 9);
                uint16_t write_to = pc + pc_offset;
                mem_write(write_to, reg[sr]);
                break;
            case OPCODE_STI:
                uint16_t sr = (ir >> 9) & 0x7;
                uint16_t pc_offset = sext(ir & 0x01ff, 9);
                mem_write(mem_read(pc + pc_offset), reg[sr]);
                break;
            case OPCODE_STR:
                uint16_t base_r = (ir >> 6) & 0x7;
                uint16_t sr = (ir >> 9) & 0x7;
                uint16_t pc_offset = sext(ir & 0x003f, 6);
                mem_write(reg[base_r] + pc_offset, reg[sr]);
                break;
            case OPCODE_TRAP:
                uint16_t trapvect = ir & 0x00ff;
                reg[7] = pc;
                pc = mem_read(trapvect);
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