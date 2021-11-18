#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#define OPCODE_HLT 0x00
#define OPCODE_SET_RES_ADDR 0x01
#define OPCODE_LOAD_RNW 0x02
#define OPCODE_LOAD_DNW 0x03
#define OPCODE_BEGIN_PROC 0x04
#define OPCODE_SET_IMG_NUM 0x05
#define OPCODE_NONE 0xFF

bool is_digit(char c) {
  return (c >= 0x30 && c <= 0x39);
}

bool is_hex_digit(char c) {
  return (c >= 0x30 && c <= 0x39) ||
         (c >= 0x41 && c <= 0x5A) ||
         (c >= 0x61 && c <= 0x7A);
}

uint8_t hex_value(char c) {
  if (c < 0x40) {
    return c - 0x30;
  } else if (c < 0x5B) {
    return c - 0x37;
  }
  return c - 0x57;
}

static int line_num = 0;

static FILE *in_file = NULL;
static FILE *out_file = NULL;

static uint32_t instructions[8192];
static uint32_t instruction_count = 0;

void error(char *message) {
  printf("Error on line %d: %s", line_num, message);
  fclose(in_file);
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    printf("Expected an output and input file\n");
    exit(EXIT_FAILURE);
  }
  in_file = fopen(argv[1], "r");
  if (in_file == NULL) {
    printf("Could not open input file\n");
    exit(EXIT_FAILURE);
  }
  char *line = NULL;
  size_t len = 0;
  ssize_t read;
  while ((read = getline(&line, &len, in_file)) != -1) {
    line_num++;
    char *line_suffix = line;
    uint8_t opcode = OPCODE_NONE;
    if (strncmp(line, "HLT", 3) == 0) {
      opcode = OPCODE_HLT;
      line_suffix += 3;
    } else if (strncmp(line, "SET_RESULT_ADDR", 15) == 0) {
      opcode = OPCODE_SET_RES_ADDR;
      line_suffix += 15;
    } else if (strncmp(line, "LOAD_RNW", 8) == 0) {
      opcode = OPCODE_LOAD_RNW;
      line_suffix += 8;
    } else if (strncmp(line, "LOAD_DNW", 8) == 0) {
      opcode = OPCODE_LOAD_DNW;
      line_suffix += 8;
    } else if (strncmp(line, "BEGIN_PROC", 10) == 0) {
      opcode = OPCODE_BEGIN_PROC;
      line_suffix += 10;
    } else if (strncmp(line, "SET_IMG_NUM", 11) == 0) {
      opcode = OPCODE_SET_IMG_NUM;
      line_suffix += 11;
    }
    uint32_t value = 0;
    uint32_t instruction = 0;
    switch (opcode) {
      case OPCODE_SET_RES_ADDR:
      case OPCODE_BEGIN_PROC:
      case OPCODE_LOAD_RNW:
      case OPCODE_LOAD_DNW:
        if (strncmp(line_suffix, " 0x", 3) == 0) {
          line_suffix += 3;
          for (int i = 0; i < 8; i++) {
            if (is_hex_digit(*line_suffix)) {
              if (i == 7) {
                error("DRAM Address longer than 28 bits");
              }
              value <<= 4;
              value |= hex_value(*(line_suffix++));
            } else if (i == 0) {
              error("Invalid DRAM address");
            } else {
              break;
            }
          }
        } else {
          error("Expected DRAM address");
        }
        instruction = (((uint32_t)opcode) << 28) | value;
        break;

      case OPCODE_SET_IMG_NUM:
        if (*(line_suffix++) == ' ') {
          while (is_digit(*line_suffix)) {
            value *= 10;
            value += *(line_suffix++) - 0x30;
            if (value > 65535) {
              error("Integer value greater than 16 bits");
            }
          }
        } else {
          error("Expected integer value");
        }
        instruction = (((uint32_t)opcode) << 28) | value;
        break;
    }
    while (*line_suffix != '\n') {
      if (*line_suffix == '#') {
        break;
      } else if (*line_suffix == ' ' || *line_suffix == '\t') {
        line_suffix++;
      } else {
        error("Unexpected character");
      }
    }
    if (opcode != OPCODE_NONE) {
      instructions[instruction_count++] = instruction;
    }
  }
  if (instruction_count == 0) {
    error("Expected at least one instruction");
  }
  out_file = fopen(argv[2], "wb+");
  if (out_file == NULL) {
    printf("Could not open output file\n");
    fclose(in_file);
    exit(EXIT_FAILURE);
  }
  for (int i = 0; i < instruction_count; i++) {
    uint8_t bytes[4];
    for (int j = 0; j < 4; j++) {
      bytes[j] = (uint8_t)(instructions[i] >> ((3 - j) * 8));
    }
    fwrite(&bytes[0], sizeof(uint8_t), 4, out_file);
  }
  fclose(in_file);
  fclose(out_file);
  return 0;
}
