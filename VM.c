#include <stdlib.h>
#include <stdint.h>

enum opcodes {
    OP_BR = 0,
    OP_ADD,
    OP_LD,
    OP_ST,
    OP_JSR,
    OP_AND,
    OP_LDR,
    OP_STR,
    OP_RTI,
    OP_NOT,
    OP_LDI,
    OP_STI,
    OP_JMP,
    OP_RES,
    OP_LEA,
    OP_TRAP,
};

const FLAG_N = 0x1 << 2;
const FLAG_Z = 0x1 << 1;
const FLAG_P = 0x1 << 0;

uint16_t memory[UINT16_MAX];
uint16_t reg[8];    // 8 general purpose registers, R0 to R7
uint16_t PC;        // Program Counter register
uint16_t IR;        // Instruction Register
uint16_t COND;      // Condition register

uint16_t mem_read(uint16_t address) {
    uint16_t word = memory[address];
    return word;
}

int mem_write(uint16_t address, uint16_t word) {
    memory[address] = word;
    return 1;
}

int set_cc(uint16_t result) {
    if (result == 0) {
        COND = FLAG_Z;
    } else {
        COND = result & 0x8000 == 0? FLAG_P: FLAG_N;
    }
}

int main(int argc, char *argv[]) {
    const ORIG = 0x3000;
    PC = ORIG;

    while (1) {
        IR = mem_read(PC++);
        uint16_t opcode = IR >> 12;

        switch (opcode) {
            case OP_ADD:
                uint16_t SR1 = IR & 0x01c0 >> 6;
                uint16_t operand1 = reg[SR1];
                uint16_t mode = (opcode & 0x0020) >> 5;
                uint16_t operand2;
                if (mode == 0) {
                    uint16_t SR2 = IR & 0x0007;
                    operand2 = reg[SR2];
                } else {
                    operand2 = IR & 0x001f;
                    struct {signed int x: 5} s;
                    operand2 = s.x = operand2; // sign extension
                }
                uint16_t DR = IR & 0x0e00 >> 9;
                uint16_t result = operand1 + operand2;
                reg[DR] = result;
                set_cc(result);
                break;
            case OP_AND:
                uint16_t SR1 = IR & 0x01c0 >> 6;
                uint16_t operand1 = reg[SR1];
                uint16_t mode = (opcode & 0x0020) >> 5;
                uint16_t operand2;
                if (mode == 0) {
                    uint16_t SR2 = IR & 0x0007;
                    operand2 = reg[SR2];
                } else {
                    operand2 = IR & 0x001f;
                    struct {signed int x: 5} s;
                    operand2 = s.x = operand2; // sign extension
                }
                uint16_t DR = IR & 0x0e00 >> 9;
                uint16_t result = operand1 & operand2;
                reg[DR] = result;
                set_cc(result);
                break;
            case OP_NOT:
                uint16_t SR = IR & 0x01c0 >> 6;
                uint16_t DR = IR & 0x0e00 >> 9;
                uint16_t operand = reg[SR];
                uint16_t result = ~operand;
                reg[DR] = result;
                set_cc(result);
                break;
            case OP_BR:
                uint16_t nzp = IR & 0x0e00 >> 9;
                if (nzp == FLAG_N && COND == FLAG_N
                        || nzp == FLAG_Z && COND == FLAG_Z
                            || nzp == FLAG_P && COND == FLAG_P) {
                    uint16_t PCoffset = IR & 0x01ff;
                    struct {signed int x: 9} s;
                    PCoffset = s.x = operand2; // sign extension
                }
                break;
            case OP_JMP:
                uint16_t BR = IR & 0x01c0 >> 6;
                PC = reg[BR];
                break;
            case OP_JSR:
                break;
            case OP_LD:
                break;
            case OP_LDI:
                break;
            case OP_LDR:
                break;
            case OP_LEA:
                break;
            case OP_ST:
                break;
            case OP_STI:
                break;
            case OP_STR:
                break;
            case OP_TRAP:
                break;
            case OP_RES:
            case OP_RTI:
            default:
                break;
        }
    }
}