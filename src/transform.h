#pragma once

#include "base.h"

static const uint8_t font8x16[95][16] =
{
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, //0x20, ' '
   {0x00, 0x00, 0x18, 0x3C, 0x3C, 0x3C, 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00}, //0x21, '!'
   {0x00, 0x66, 0x66, 0x66, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, //0x22, '"'
   {0x00, 0x00, 0x00, 0x6C, 0x6C, 0xFE, 0x6C, 0x6C, 0x6C, 0xFE, 0x6C, 0x6C, 0x00, 0x00, 0x00, 0x00}, //0x23, '#'
   {0x18, 0x18, 0x7C, 0xC6, 0xC2, 0xC0, 0x7C, 0x06, 0x06, 0x86, 0xC6, 0x7C, 0x18, 0x18, 0x00, 0x00}, //0x24, '$'
   {0x00, 0x00, 0x00, 0x00, 0xC2, 0xC6, 0x0C, 0x18, 0x30, 0x60, 0xC6, 0x86, 0x00, 0x00, 0x00, 0x00}, //0x25, '%'
   {0x00, 0x00, 0x38, 0x6C, 0x6C, 0x38, 0x76, 0xDC, 0xCC, 0xCC, 0xCC, 0x76, 0x00, 0x00, 0x00, 0x00}, //0x26, '&'
   {0x00, 0x30, 0x30, 0x30, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, //0x27, '''
   {0x00, 0x00, 0x0C, 0x18, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x18, 0x0C, 0x00, 0x00, 0x00, 0x00}, //0x28, '('
   {0x00, 0x00, 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x18, 0x30, 0x00, 0x00, 0x00, 0x00}, //0x29, ')'
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, //0x2A, '*'
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, //0x2B, '+'
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x18, 0x30, 0x00, 0x00, 0x00}, //0x2C, '
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, //0x2D, '-'
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00}, //0x2E, '.'
   {0x00, 0x00, 0x00, 0x00, 0x02, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xC0, 0x80, 0x00, 0x00, 0x00, 0x00}, //0x2F, '/'
   {0x00, 0x00, 0x38, 0x6C, 0xC6, 0xC6, 0xD6, 0xD6, 0xC6, 0xC6, 0x6C, 0x38, 0x00, 0x00, 0x00, 0x00}, //0x30, '0'
   {0x00, 0x00, 0x18, 0x38, 0x78, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00, 0x00, 0x00, 0x00}, //0x31, '1'
   {0x00, 0x00, 0x7C, 0xC6, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xC0, 0xC6, 0xFE, 0x00, 0x00, 0x00, 0x00}, //0x32, '2'
   {0x00, 0x00, 0x7C, 0xC6, 0x06, 0x06, 0x3C, 0x06, 0x06, 0x06, 0xC6, 0x7C, 0x00, 0x00, 0x00, 0x00}, //0x33, '3'
   {0x00, 0x00, 0x0C, 0x1C, 0x3C, 0x6C, 0xCC, 0xFE, 0x0C, 0x0C, 0x0C, 0x1E, 0x00, 0x00, 0x00, 0x00}, //0x34, '4'
   {0x00, 0x00, 0xFE, 0xC0, 0xC0, 0xC0, 0xFC, 0x06, 0x06, 0x06, 0xC6, 0x7C, 0x00, 0x00, 0x00, 0x00}, //0x35, '5'
   {0x00, 0x00, 0x38, 0x60, 0xC0, 0xC0, 0xFC, 0xC6, 0xC6, 0xC6, 0xC6, 0x7C, 0x00, 0x00, 0x00, 0x00}, //0x36, '6'
   {0x00, 0x00, 0xFE, 0xC6, 0x06, 0x06, 0x0C, 0x18, 0x30, 0x30, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00}, //0x37, '7'
   {0x00, 0x00, 0x7C, 0xC6, 0xC6, 0xC6, 0x7C, 0xC6, 0xC6, 0xC6, 0xC6, 0x7C, 0x00, 0x00, 0x00, 0x00}, //0x38, '8'
   {0x00, 0x00, 0x7C, 0xC6, 0xC6, 0xC6, 0x7E, 0x06, 0x06, 0x06, 0x0C, 0x78, 0x00, 0x00, 0x00, 0x00}, //0x39, '9'
   {0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00}, //0x3A, ':'
   {0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30, 0x00, 0x00, 0x00, 0x00}, //0x3B, ';'
   {0x00, 0x00, 0x00, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x30, 0x18, 0x0C, 0x06, 0x00, 0x00, 0x00, 0x00}, //0x3C, '<'
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, //0x3D, '='
   {0x00, 0x00, 0x00, 0x60, 0x30, 0x18, 0x0C, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x00, 0x00, 0x00, 0x00}, //0x3E, '>'
   {0x00, 0x00, 0x7C, 0xC6, 0xC6, 0x0C, 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00}, //0x3F, '?'
   {0x00, 0x00, 0x00, 0x7C, 0xC6, 0xC6, 0xDE, 0xDE, 0xDE, 0xDC, 0xC0, 0x7C, 0x00, 0x00, 0x00, 0x00}, //0x40, '@'
   {0x00, 0x00, 0x10, 0x38, 0x6C, 0xC6, 0xC6, 0xFE, 0xC6, 0xC6, 0xC6, 0xC6, 0x00, 0x00, 0x00, 0x00}, //0x41, 'A'
   {0x00, 0x00, 0xFC, 0x66, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x66, 0x66, 0xFC, 0x00, 0x00, 0x00, 0x00}, //0x42, 'B'
   {0x00, 0x00, 0x3C, 0x66, 0xC2, 0xC0, 0xC0, 0xC0, 0xC0, 0xC2, 0x66, 0x3C, 0x00, 0x00, 0x00, 0x00}, //0x43, 'C'
   {0x00, 0x00, 0xF8, 0x6C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x6C, 0xF8, 0x00, 0x00, 0x00, 0x00}, //0x44, 'D'
   {0x00, 0x00, 0xFE, 0x66, 0x62, 0x68, 0x78, 0x68, 0x60, 0x62, 0x66, 0xFE, 0x00, 0x00, 0x00, 0x00}, //0x45, 'E'
   {0x00, 0x00, 0xFE, 0x66, 0x62, 0x68, 0x78, 0x68, 0x60, 0x60, 0x60, 0xF0, 0x00, 0x00, 0x00, 0x00}, //0x46, 'F'
   {0x00, 0x00, 0x3C, 0x66, 0xC2, 0xC0, 0xC0, 0xDE, 0xC6, 0xC6, 0x66, 0x3A, 0x00, 0x00, 0x00, 0x00}, //0x47, 'G'
   {0x00, 0x00, 0xC6, 0xC6, 0xC6, 0xC6, 0xFE, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x00, 0x00, 0x00, 0x00}, //0x48, 'H'
   {0x00, 0x00, 0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00, 0x00, 0x00, 0x00}, //0x49, 'I'
   {0x00, 0x00, 0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0xCC, 0xCC, 0xCC, 0x78, 0x00, 0x00, 0x00, 0x00}, //0x4A, 'J'
   {0x00, 0x00, 0xE6, 0x66, 0x66, 0x6C, 0x78, 0x78, 0x6C, 0x66, 0x66, 0xE6, 0x00, 0x00, 0x00, 0x00}, //0x4B, 'K'
   {0x00, 0x00, 0xF0, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x62, 0x66, 0xFE, 0x00, 0x00, 0x00, 0x00}, //0x4C, 'L'
   {0x00, 0x00, 0xC6, 0xEE, 0xFE, 0xFE, 0xD6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x00, 0x00, 0x00, 0x00}, //0x4D, 'M'
   {0x00, 0x00, 0xC6, 0xE6, 0xF6, 0xFE, 0xDE, 0xCE, 0xC6, 0xC6, 0xC6, 0xC6, 0x00, 0x00, 0x00, 0x00}, //0x4E, 'N'
   {0x00, 0x00, 0x7C, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x7C, 0x00, 0x00, 0x00, 0x00}, //0x4F, 'O'
   {0x00, 0x00, 0xFC, 0x66, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x60, 0x60, 0xF0, 0x00, 0x00, 0x00, 0x00}, //0x50, 'P'
   {0x00, 0x00, 0x7C, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xD6, 0xDE, 0x7C, 0x0C, 0x0E, 0x00, 0x00}, //0x51, 'Q'
   {0x00, 0x00, 0xFC, 0x66, 0x66, 0x66, 0x7C, 0x6C, 0x66, 0x66, 0x66, 0xE6, 0x00, 0x00, 0x00, 0x00}, //0x52, 'R'
   {0x00, 0x00, 0x7C, 0xC6, 0xC6, 0x60, 0x38, 0x0C, 0x06, 0xC6, 0xC6, 0x7C, 0x00, 0x00, 0x00, 0x00}, //0x53, 'S'
   {0x00, 0x00, 0x7E, 0x7E, 0x5A, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00, 0x00, 0x00, 0x00}, //0x54, 'T'
   {0x00, 0x00, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x7C, 0x00, 0x00, 0x00, 0x00}, //0x55, 'U'
   {0x00, 0x00, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x6C, 0x38, 0x10, 0x00, 0x00, 0x00, 0x00}, //0x56, 'V'
   {0x00, 0x00, 0xC6, 0xC6, 0xC6, 0xC6, 0xD6, 0xD6, 0xD6, 0xFE, 0xEE, 0x6C, 0x00, 0x00, 0x00, 0x00}, //0x57, 'W'
   {0x00, 0x00, 0xC6, 0xC6, 0x6C, 0x7C, 0x38, 0x38, 0x7C, 0x6C, 0xC6, 0xC6, 0x00, 0x00, 0x00, 0x00}, //0x58, 'X'
   {0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00, 0x00, 0x00, 0x00}, //0x59, 'Y'
   {0x00, 0x00, 0xFE, 0xC6, 0x86, 0x0C, 0x18, 0x30, 0x60, 0xC2, 0xC6, 0xFE, 0x00, 0x00, 0x00, 0x00}, //0x5A, 'Z'
   {0x00, 0x00, 0x3C, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3C, 0x00, 0x00, 0x00, 0x00}, //0x5B, '['
   {0x00, 0x00, 0x00, 0x80, 0xC0, 0xE0, 0x70, 0x38, 0x1C, 0x0E, 0x06, 0x02, 0x00, 0x00, 0x00, 0x00}, //0x5C, '\'
   {0x00, 0x00, 0x3C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x3C, 0x00, 0x00, 0x00, 0x00}, //0x5D, ']'
   {0x10, 0x38, 0x6C, 0xC6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, //0x5E, '^'
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00}, //0x5F, '_'
   {0x30, 0x30, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, //0x60, '`'
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x0C, 0x7C, 0xCC, 0xCC, 0xCC, 0x76, 0x00, 0x00, 0x00, 0x00}, //0x61, 'a'
   {0x00, 0x00, 0xE0, 0x60, 0x60, 0x78, 0x6C, 0x66, 0x66, 0x66, 0x66, 0x7C, 0x00, 0x00, 0x00, 0x00}, //0x62, 'b'
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x7C, 0xC6, 0xC0, 0xC0, 0xC0, 0xC6, 0x7C, 0x00, 0x00, 0x00, 0x00}, //0x63, 'c'
   {0x00, 0x00, 0x1C, 0x0C, 0x0C, 0x3C, 0x6C, 0xCC, 0xCC, 0xCC, 0xCC, 0x76, 0x00, 0x00, 0x00, 0x00}, //0x64, 'd'
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x7C, 0xC6, 0xFE, 0xC0, 0xC0, 0xC6, 0x7C, 0x00, 0x00, 0x00, 0x00}, //0x65, 'e'
   {0x00, 0x00, 0x38, 0x6C, 0x64, 0x60, 0xF0, 0x60, 0x60, 0x60, 0x60, 0xF0, 0x00, 0x00, 0x00, 0x00}, //0x66, 'f'
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x76, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0x7C, 0x0C, 0xCC, 0x78, 0x00}, //0x67, 'g'
   {0x00, 0x00, 0xE0, 0x60, 0x60, 0x6C, 0x76, 0x66, 0x66, 0x66, 0x66, 0xE6, 0x00, 0x00, 0x00, 0x00}, //0x68, 'h'
   {0x00, 0x00, 0x18, 0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00, 0x00, 0x00, 0x00}, //0x69, 'i'
   {0x00, 0x00, 0x06, 0x06, 0x00, 0x0E, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x66, 0x66, 0x3C, 0x00}, //0x6A, 'j'
   {0x00, 0x00, 0xE0, 0x60, 0x60, 0x66, 0x6C, 0x78, 0x78, 0x6C, 0x66, 0xE6, 0x00, 0x00, 0x00, 0x00}, //0x6B, 'k'
   {0x00, 0x00, 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00, 0x00, 0x00, 0x00}, //0x6C, 'l'
   {0x00, 0x00, 0x00, 0x00, 0x00, 0xEC, 0xFE, 0xD6, 0xD6, 0xD6, 0xD6, 0xC6, 0x00, 0x00, 0x00, 0x00}, //0x6D, 'm'
   {0x00, 0x00, 0x00, 0x00, 0x00, 0xDC, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x00, 0x00, 0x00, 0x00}, //0x6E, 'n'
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x7C, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x7C, 0x00, 0x00, 0x00, 0x00}, //0x6F, 'o'
   {0x00, 0x00, 0x00, 0x00, 0x00, 0xDC, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7C, 0x60, 0x60, 0xF0, 0x00}, //0x70, 'p'
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x76, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0x7C, 0x0C, 0x0C, 0x1E, 0x00}, //0x71, 'q'
   {0x00, 0x00, 0x00, 0x00, 0x00, 0xDC, 0x76, 0x66, 0x60, 0x60, 0x60, 0xF0, 0x00, 0x00, 0x00, 0x00}, //0x72, 'r'
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x7C, 0xC6, 0x60, 0x38, 0x0C, 0xC6, 0x7C, 0x00, 0x00, 0x00, 0x00}, //0x73, 's'
   {0x00, 0x00, 0x10, 0x30, 0x30, 0xFC, 0x30, 0x30, 0x30, 0x30, 0x36, 0x1C, 0x00, 0x00, 0x00, 0x00}, //0x74, 't'
   {0x00, 0x00, 0x00, 0x00, 0x00, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0x76, 0x00, 0x00, 0x00, 0x00}, //0x75, 'u'
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00, 0x00, 0x00, 0x00}, //0x76, 'v'
   {0x00, 0x00, 0x00, 0x00, 0x00, 0xC6, 0xC6, 0xD6, 0xD6, 0xD6, 0xFE, 0x6C, 0x00, 0x00, 0x00, 0x00}, //0x77, 'w'
   {0x00, 0x00, 0x00, 0x00, 0x00, 0xC6, 0x6C, 0x38, 0x38, 0x38, 0x6C, 0xC6, 0x00, 0x00, 0x00, 0x00}, //0x78, 'x'
   {0x00, 0x00, 0x00, 0x00, 0x00, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x7E, 0x06, 0x0C, 0xF8, 0x00}, //0x79, 'y'
   {0x00, 0x00, 0x00, 0x00, 0x00, 0xFE, 0xCC, 0x18, 0x30, 0x60, 0xC6, 0xFE, 0x00, 0x00, 0x00, 0x00}, //0x7A, 'z'
   {0x00, 0x00, 0x0E, 0x18, 0x18, 0x18, 0x70, 0x18, 0x18, 0x18, 0x18, 0x0E, 0x00, 0x00, 0x00, 0x00}, //0x7B, '{'
   {0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00}, //0x7C, '|'
   {0x00, 0x00, 0x70, 0x18, 0x18, 0x18, 0x0E, 0x18, 0x18, 0x18, 0x18, 0x70, 0x00, 0x00, 0x00, 0x00}, //0x7D, '}'
   {0x00, 0x00, 0x76, 0xDC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}  //0x7E, '~'
};

inline Color get_pixel(Image* image, int x, int y)
{
    Color c = {0};
    memcpy(&c, &image->data[y*image->w*image->n + x*image->n], 3);
    return c;
}

inline void reverse_rgb_order(Image *image)
{
    for(int i = 0; i < image->w*image->h; ++i)
    {
        int n = i*image->n;
        u8 temp = image->data[n+0];
        image->data[n+0] = image->data[n+2]; // R -> B
        image->data[n+2] = temp;               // B -> R
    }
}

Color get_blended_color(u8* data, Color c, float opacity)
{
    u8 r = data[0];
    u8 g = data[1];
    u8 b = data[2];

    Color ret_color = {0};

    ret_color.r = opacity*c.r + (1.0 - opacity)*r;
    ret_color.g = opacity*c.g + (1.0 - opacity)*g;
    ret_color.b = opacity*c.b + (1.0 - opacity)*b;

    return ret_color;
}

float calc_iou(Rect* a, Rect* b)
{
    u16 inter_x1 = MAX(a->x, b->x);
    u16 inter_y1 = MAX(a->y, b->y);
    u16 inter_x2 = MIN(a->x + a->w, b->x + b->w);
    u16 inter_y2 = MIN(a->y + a->h, b->y + b->h);

    u16 inter_width = MAX(0, inter_x2 - inter_x1);
    u16 inter_height = MAX(0, inter_y2 - inter_y1);
    u16 inter_area = inter_width * inter_height;

    int area1 = a->w * a->h;
    int area2 = b->w * b->h;

    int union_area = area1 + area2 - inter_area;

    if (union_area == 0) return 0.0;

    return inter_area / (float)union_area;
}

void transform_scramble(Image* image, Rect r, u32 seed)
{
    u8* start = &image->data[r.y*image->w*image->n + r.x*image->n];

    if(seed > 0)
    {
        // seed of 0 means "don't seed"
        srand(seed);
    }

    // initialize unprocessed list
    int num_pixels = r.w*r.h;
    int unprocessed[num_pixels] = {0};
    int unprocessed_count = num_pixels;

    for(int i = 0; i < num_pixels; ++i)
        unprocessed[i] = i;

    for(;;)
    {
        if(unprocessed_count <= 1)
            break;

        int idx1 = rand() % unprocessed_count;
        int idx2 = rand() % unprocessed_count;

        // swap two pixels

        int u1 = unprocessed[idx1];
        int u2 = unprocessed[idx2];

        int offset1 = image->w*image->n*(u1/r.w) + image->n*(u1%r.w);
        int offset2 = image->w*image->n*(u2/r.w) + image->n*(u2%r.w);

        Color tmp = {0};
        memcpy(&tmp, start+offset1, 3);
        memcpy(start+offset1,start+offset2,3);
        memcpy(start+offset2, &tmp, 3);

        // remove both indices from unprocessed
        memcpy(&unprocessed[idx1],&unprocessed[unprocessed_count-1], sizeof(int));
        unprocessed_count--;
        memcpy(&unprocessed[idx2],&unprocessed[unprocessed_count-1], sizeof(int));
        unprocessed_count--;
    }
}

void transform_draw_rect(Image* image, Rect r, Color c, bool filled, float opacity)
{
    u8* start = &image->data[r.y*image->w*image->n + r.x*image->n];
    u8* curr = start;

    int n = image->n;
    int step = image->w*n;

    // draw first line
    for(int i = 0; i <= r.w; ++i)
    {
        Color r = opacity == 1.0 ? c : get_blended_color(curr+i*n,c,opacity);
        memcpy(curr+i*n, &r, 3);
    }

    curr += step;

    if(filled)
    {
        for(int j = 0; j < r.h-1; ++j)
        {
            for(int i = 0; i < r.w; ++i)
            {
                Color r = opacity == 1.0 ? c : get_blended_color(curr+i*n,c,opacity);
                memcpy(curr+i*n, &r, 3);
            }
            curr += step;
        }
    }
    else
    {
        for(int i = 0; i < r.h-1; ++i)
        {
            Color cl = opacity == 1.0 ? c : get_blended_color(curr,c,opacity);
            Color cr = opacity == 1.0 ? c : get_blended_color(curr+r.w*n,c,opacity);

            memcpy(curr,&cl, 3);         // left pixel
            memcpy(curr + r.w*n,&cr, 3); // right pixel

            curr += step;
        }
    }

    for(int i = 0; i <= r.w; ++i)
    {
        Color r = opacity == 1.0 ? c : get_blended_color(curr+i*n,c,opacity);
        memcpy(curr + i*n, &r, 3);
    }
}

void transform_rotate_rgb24(const uint8_t *src, uint8_t *dst, int width, int height, int rotation)
{
    int row, col;
    int dst_width = width;
    int dst_height = height;

    if (rotation == 90 || rotation == 270)
    {
        dst_width = height;
        dst_height = width;
    }

    for(int y = 0; y < height; ++y)
    {
        for(int x = 0; x < width; ++x)
        {
            const uint8_t *p = src + (y * width + x) * 3;
            uint8_t *q;

            switch(rotation) {
                case 0:
                    q = dst + (y * width + x) * 3;
                    break;
                case 270:
                    q = dst + (x * dst_width + (dst_width - 1 - y)) * 3;
                    break;
                case 180:
                    q = dst + ((dst_height - 1 - y) * width + (width - 1 - x)) * 3;
                    break;
                case 90:
                    q = dst + ((dst_height - 1 - x) * dst_width + y) * 3;
                    break;
                default:
                    // invalid rotation; fallback to no rotation
                    q = dst + (y * width + x) * 3;
                    break;
            }

            // copy RGB triplet
            q[0] = p[0];
            q[1] = p[1];
            q[2] = p[2];
        }
    }
}

static inline void put_pixel(Image *img, int x, int y, Color c)
{
    if (x < 0 || y < 0 || x >= img->w || y >= img->h) return;

    u8 *px = img->data + y * img->step + x * img->n;

    if (c.a == 255)
    {
        px[0] = c.r;
        px[1] = c.g;
        px[2] = c.b;
    }
    else
    {
        // linear blend: dst = (dst*(255-a) + src*a)/255
        px[0] = (px[0]*(255-c.a) + c.r*c.a)/255;
        px[1] = (px[1]*(255-c.a) + c.g*c.a)/255;
        px[2] = (px[2]*(255-c.a) + c.b*c.a)/255;
    }
}

void transform_draw_char(Image* image, char c, u16 x, u16 y, Color color)
{
    if (c < 32 || c > 126) return;
    const u8 *glyph = font8x16[c - 32];
    for (int row = 0; row < 16; row++) {
        u8 bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            if (bits & (1 << (7 - col))) {
                put_pixel(image, x + col, y + row, color);
            }
        }
    }
}

void transform_draw_string(Image* image, u16 x, u16 y, Color color, const char* fmt, ...)
{
    char buf[512]; // temp buffer, adjust if needed
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    int dx = x;
    const char* str = buf;
    while (*str) {
        if (dx + 8 > image->w) break; // stop if out of bounds
        transform_draw_char(image, *str, dx, y, color);
        dx += 8; // fixed width spacing
        str++;
    }
}

static inline Color transform_blend_color(Color a, Color b, float factor)
{
    if(factor < 0.0f) factor = 0.0f;
    if(factor > 1.0f) factor = 1.0f;

    Color r = {
        (u8)((1.0 - factor)*a.r + (factor)*b.r),
        (u8)((1.0 - factor)*a.g + (factor)*b.g),
        (u8)((1.0 - factor)*a.b + (factor)*b.b),
        (u8)((1.0 - factor)*a.a + (factor)*b.a)
    };

    return r;
}

static inline void blend_pixel(Image* img, int px, int py, Color c, float opacity)
{
    if (px < 0 || py < 0 || px >= img->w || py >= img->h) return;

    u8* dst = img->data + py * img->step + px * img->n;

    // Blend with existing pixel
    float alpha = (c.a / 255.0f) * opacity;
    dst[0] = (u8)((1 - alpha) * dst[0] + alpha * c.r);
    dst[1] = (u8)((1 - alpha) * dst[1] + alpha * c.g);
    dst[2] = (u8)((1 - alpha) * dst[2] + alpha * c.b);
}

static inline void draw_vline(Image* img, int x, int y1, int y2, Color c, float opacity) {

    if (x < 0 || x >= img->w) return;
    if (y1 > y2) { int tmp = y1; y1 = y2; y2 = tmp; }
    if (y1 < 0) y1 = 0;
    if (y2 >= img->h) y2 = img->h - 1;

    for (int y = y1; y <= y2; y++) {
        blend_pixel(img, x, y, c, opacity);
    }
}

void transform_draw_circle(Image* image, u16 x, u16 y, u16 radius, Color c, bool filled, float opacity)
{
    if (!image || radius == 0) return;

    int cx = x;
    int cy = y;
    int r = radius;

    int dx = r;
    int dy = 0;
    int err = 1 - dx;
    
    // clamp opacity
    if(opacity < 0.0f) opacity = 0.0f;
    if(opacity > 1.0f) opacity = 1.0f;

    while (dx >= dy)
    {
        if (filled)
        {
            draw_vline(image, cx + dx, cy - dy, cy + dy, c, opacity);
            draw_vline(image, cx - dx, cy - dy, cy + dy, c, opacity);
            draw_vline(image, cx + dy, cy - dx, cy + dx, c, opacity);
            draw_vline(image, cx - dy, cy - dx, cy + dx, c, opacity);
        }
        else
        {
            // Outline only
            blend_pixel(image, cx + dx, cy + dy, c, opacity);
            blend_pixel(image, cx - dx, cy + dy, c, opacity);
            blend_pixel(image, cx + dx, cy - dy, c, opacity);
            blend_pixel(image, cx - dx, cy - dy, c, opacity);
            blend_pixel(image, cx + dy, cy + dx, c, opacity);
            blend_pixel(image, cx - dy, cy + dx, c, opacity);
            blend_pixel(image, cx + dy, cy - dx, c, opacity);
            blend_pixel(image, cx - dy, cy - dx, c, opacity);
        }

        dy++;
        if (err < 0) {
            err += 2 * dy + 1;
        } else {
            dx--;
            err += 2 * (dy - dx + 1);
        }
    }
}

void transform_pixelate(Image* image, Rect r, float block_scale)
{
    u8* start = &image->data[r.y*image->w*image->n + r.x*image->n];
    u8* curr = start;

    int n = image->n;
    int step = image->w*n;

    int block_size = MIN(r.w, r.h)*block_scale;

    if(block_size == 0 || block_size == 1)
        return; // block_size match to pixel size

    int total_block_size = block_size * block_size;

    float avg_r = 0.0;
    float avg_g = 0.0;
    float avg_b = 0.0;

    int num_blocks_x = ceil(r.w / (float)block_size);
    int num_blocks_y = ceil(r.h / (float)block_size);

    int block_size_x = block_size;
    int block_size_y = block_size;

    for(int y = 0; y < num_blocks_y; ++y)
    {
        for(int x = 0; x < num_blocks_x; ++x)
        {
            avg_r = 0.0;
            avg_g = 0.0;
            avg_b = 0.0;

            curr = start + y*block_size_y*step + x*block_size_x*n;

            for(int j = 0; j < block_size_y; ++j)
            {
                for(int i = 0; i < block_size_x; ++i)
                {
                    avg_r += curr[i*n+0];
                    avg_g += curr[i*n+1];
                    avg_b += curr[i*n+2];
                }
                curr += step;
            }

            avg_r /= total_block_size;
            avg_g /= total_block_size;
            avg_b /= total_block_size;

            Color sc = {(u8)avg_r, (u8)avg_g, (u8)avg_b};

            int offset_x = x == num_blocks_x - 1 ? block_size_x - (r.w % block_size_x) : 0;
            int offset_y = y == num_blocks_y - 1 ? block_size_y - (r.h % block_size_y) : 0;

            // apply avgcolor to range
            curr = start + y*block_size_y*step + x*block_size_x*n;
            for(int j = 0; j < block_size_y - offset_y; ++j)
            {
                for(int i = 0; i < block_size_x - offset_x; ++i)
                {
                    memcpy(curr+i*n, &sc, 3);
                }
                curr += step;
            }
        }
    }
}

void transform_stretch_image(Image *dst, Image *src, Rect r)
{
    // Scaling factors
    float scaleX = (float)src->w / r.w;
    float scaleY = (float)src->h / r.h;

    // Iterate through the destination rectangle
    for (int dy = 0; dy < r.h; ++dy)
    {
        for (int dx = 0; dx < r.w; ++dx)
        {
            // Compute the corresponding position in the source image
            int sx = (int)(dx * scaleX);
            int sy = (int)(dy * scaleY);

            // Ensure we're within bounds for the source image
            if (sx >= src->w) sx = src->w - 1;
            if (sy >= src->h) sy = src->h - 1;

            // Get the source pixel's starting index
            u8 *src_pixel = src->data + sy * src->step + sx * src->n;

            // Get the destination pixel's starting index
            u8 *dst_pixel = dst->data + (r.y + dy) * dst->step + (r.x + dx) * dst->n;

            // check for magenta pixel on src
            bool ignore = (src_pixel[0] == 0xFF && src_pixel[1] == 0x00 && src_pixel[2]);

            if(ignore)
            {
                continue;
            }

            // Copy pixel data (assume both images have the same number of channels)
            for (int c = 0; c < src->n; c++)
                dst_pixel[c] = src_pixel[c];
        }
    }
}

// Down Scaling

#define KERNEL_TABLE_SIZE 1024
float lanczos_table[KERNEL_TABLE_SIZE];
float inv_a_scale = 0.0;

// performs pre-computations to make things fast
void lanczos_init(int a) {
    for (int i = 0; i < KERNEL_TABLE_SIZE; ++i) {
        float x = ((float)i / (KERNEL_TABLE_SIZE - 1)) * a;
        if (x == 0.0)
            lanczos_table[i] = 1.0;
        else if (x < a)
            lanczos_table[i] = (sin(PI*x) / (PI*x)) * (sin(PI*x/a) / (PI*x/a));
        else
            lanczos_table[i] = 0.0;
    }

    inv_a_scale = (KERNEL_TABLE_SIZE - 1) / (float)a;
}

static inline float fast_lanczos(double x, int a) {

    x = ABSF(x);
    if (x >= a)
        return 0.0;

    int idx = (int)(x*inv_a_scale);
    return lanczos_table[idx];
}

void lanczos_downscale_rotate(Image *in, Image *out, int a)
{
    double x_scale, y_scale;

    // Compute scales depending on rotation
    switch (out->rotation)
    {
        case 90:
        case 270:
            x_scale = (double)in->w / out->h;
            y_scale = (double)in->h / out->w;
            break;
        default: // 0° or 180°
            x_scale = (double)in->w / out->w;
            y_scale = (double)in->h / out->h;
            break;
    }

    for (int oy = 0; oy < out->h; ++oy)
    {
        for (int ox = 0; ox < out->w; ++ox)
        {
            double fx, fy;

            // Map output pixel (ox,oy) -> source coordinates (fx,fy)
            switch (out->rotation)
            {
                case 90:
                    fx = (in->w - 1) - oy * x_scale;
                    fy = ox * y_scale;
                    break;
                case 180:
                    fx = (in->w - 1) - ox * x_scale;
                    fy = (in->h - 1) - oy * y_scale;
                    break;
                case 270:
                    fx = oy * x_scale;
                    fy = (in->h - 1) - ox * y_scale;
                    break;
                default: // 0°
                    fx = ox * x_scale;
                    fy = oy * y_scale;
                    break;
            }

            // Lanczos accumulation
            int x_start = floor(fx - a);
            int x_end   = floor(fx + a);
            int y_start = floor(fy - a);
            int y_end   = floor(fy + a);

            double sum_r = 0.0, sum_g = 0.0, sum_b = 0.0;
            double sum_w = 0.0;

            for (int sy = y_start; sy <= y_end; ++sy)
            {
                int cy = sy < 0 ? 0 : (sy >= in->h ? in->h-1 : sy);
                double wy = fast_lanczos((fy - sy) / y_scale, a);

                for (int sx = x_start; sx <= x_end; ++sx)
                {
                    int cx = sx < 0 ? 0 : (sx >= in->w ? in->w-1 : sx);
                    double wx = fast_lanczos((fx - sx) / x_scale, a);
                    double w = wx * wy;
                    if (w == 0.0) continue;

                    int src_idx = (cy * in->w + cx) * in->n;
                    sum_r += in->data[src_idx + 0] * w;
                    sum_g += in->data[src_idx + 1] * w;
                    sum_b += in->data[src_idx + 2] * w;
                    sum_w += w;
                }
            }

            int dst_idx = (oy * out->w + ox) * out->n;
            if (sum_w > 0.0)
            {
                out->data[dst_idx + 0] = (uint8_t)(sum_r / sum_w + 0.5);
                out->data[dst_idx + 1] = (uint8_t)(sum_g / sum_w + 0.5);
                out->data[dst_idx + 2] = (uint8_t)(sum_b / sum_w + 0.5);
            }
            else
            {
                out->data[dst_idx + 0] = 0;
                out->data[dst_idx + 1] = 0;
                out->data[dst_idx + 2] = 0;
            }
        }
    }
}

void lanczos_downscale(Image *in, Image *out, int a)
{
    double x_scale = (double)in->w / out->w;
    double y_scale = (double)in->h / out->h;

    for (int y = 0; y < out->h; ++y)
    {
        double source_y = (y + 0.5) * y_scale;
        int y_start = floor(source_y - a);
        int y_end   = floor(source_y + a);

        for (int x = 0; x < out->w; ++x)
        {
            double source_x = (x + 0.5) * x_scale;
            int x_start = floor(source_x - a);
            int x_end   = floor(source_x + a);

            double sum_red = 0.0;
            double sum_green = 0.0;
            double sum_blue = 0.0;
            double sum_weights = 0.0;

            // Determine the contributing input pixel region based on 'a'
            // and the downscaling ratio

            for (int j = y_start; j <= y_end; ++j)
            {
                double weight_y = fast_lanczos((j - source_y) / y_scale, a);
                int clamped_j = j < 0 ? 0 : (j >= in->h ? in->h-1 : j);
                float row_off = clamped_j*in->w*in->n;

                for (int i = x_start; i <= x_end; ++i)
                {
                    // Calculate weights using the Lanczos kernel
                    double weight_x = fast_lanczos((i - source_x) / x_scale, a);
                    double weight = weight_x * weight_y;

                    int clamped_i = i < 0 ? 0 : (i >= in->w ? in->w-1 : i);
                    int offset = row_off + clamped_i*in->n;

                    sum_red   += in->data[offset+0] * weight;
                    sum_green += in->data[offset+1] * weight;
                    sum_blue  += in->data[offset+2] * weight;

                    sum_weights += weight;
                }
            }
            // Normalize and set the output pixel

            Color out_pixel;
            out_pixel.r = (u8)(sum_red / sum_weights + 0.5);
            out_pixel.g = (u8)(sum_green / sum_weights + 0.5);
            out_pixel.b = (u8)(sum_blue / sum_weights + 0.5);

            u8* curr = &out->data[y*out->w*out->n + x*out->n];
            memset(curr+0,out_pixel.r,1);
            memset(curr+1,out_pixel.g,1);
            memset(curr+2,out_pixel.b,1);
        }
    }
}

bool transform_downscale(Arena* arena, Image* source, Image* result, int scaled_size, int rotation)
{
    bool use_scaled_image = source->w > scaled_size || source->h > scaled_size;

    if(use_scaled_image)
    {
        const int a = 2; // number of iterations
        lanczos_init(a);

        // downscale largest dimension 
        float aspect = source->w / (float)source->h;

        int width_scaled = 0;
        int height_scaled = 0;

        if(aspect > 1.0)
        {
            // width is larger than height (most common)
            width_scaled = scaled_size;
            height_scaled = width_scaled / aspect;
        }
        else
        {
            height_scaled = scaled_size;
            width_scaled = height_scaled * aspect;
        }

        if(rotation == 90 || rotation == 270)
        {
            // swap the height and width
            int tmp = width_scaled;
            width_scaled = height_scaled;
            height_scaled = tmp;
        }

        result->w = width_scaled;
        result->h = height_scaled;
        result->n = source->n;
        result->step = width_scaled*result->n;
        result->rotation = rotation;
        result->scale_x = width_scaled / source->w;
        result->scale_y = height_scaled / source->h;
        result->arena = source->arena;
        result->frame_number = source->frame_number;
        result->detect_buffer = source->detect_buffer;

        int buffer_size = width_scaled*height_scaled*result->n;

        if(arena == NULL)
        {
            result->data = (u8*)malloc(buffer_size);
        }
        else
        {
            result->data = (u8*)arena_alloc(arena, buffer_size);
        }

        lanczos_downscale_rotate(source, result, a);
    }

    return use_scaled_image;
}


void transform_rect_upscale_rotate_inverse(
    Rect* r,
    u16 det_w, u16 det_h,   // detector (downscaled+rotated) image size
    u16 orig_w, u16 orig_h, // original frame size
    int rotation)
{
    float scale_x, scale_y;

    // These are the true dimensions of the image that was rotated
    int scaled_w = det_w;
    int scaled_h = det_h;

    // Figure out what the detector "saw" relative to the original
    switch (rotation)
    {
        case 90:
        case 270:
            scale_x = (float)orig_w / scaled_h;
            scale_y = (float)orig_h / scaled_w;
            break;
        default: // 0 or 180
            scale_x = (float)orig_w / scaled_w;
            scale_y = (float)orig_h / scaled_h;
            break;
    }

    // Collect all points: rect corners + landmarks
    Point points[] =
    {
        {r->x, r->y},
        {r->x + r->w, r->y},
        {r->x, r->y + r->h},
        {r->x + r->w, r->y + r->h},
        {r->landmarks[0].x, r->landmarks[0].y},
        {r->landmarks[1].x, r->landmarks[1].y},
        {r->landmarks[2].x, r->landmarks[2].y},
        {r->landmarks[3].x, r->landmarks[3].y},
        {r->landmarks[4].x, r->landmarks[4].y}
    };

    // printf("Rect before transform: %u %u %u %u\n", r->x, r->y, r->w, r->h);

    float minx = 1e9f, miny = 1e9f;
    float maxx = -1e9f, maxy = -1e9f;

    for (int i = 0; i < ArrayCount(points); ++i)
    {
        float fx = points[i].x;
        float fy = points[i].y;
        float ox, oy;

        // Undo rotation: map detector coords back into original orientation
        switch (rotation)
        {
            case 270:  // rotated right during downscale → rotate left to undo
                ox = fy;
                oy = (scaled_w - 1) - fx;
                break;
            case 180:
                ox = (scaled_w - 1) - fx;
                oy = (scaled_h - 1) - fy;
                break;
            case 90: // rotated left during downscale → rotate right to undo
                ox = (scaled_h - 1) - fy;
                oy = fx;
                break;
            case 0:
            default:
                ox = fx;
                oy = fy;
                break;
        }

        // Now scale back up to original frame
        ox *= scale_x;
        oy *= scale_y;

        if (i < 4)
        {
            if (ox < minx) minx = ox;
            if (oy < miny) miny = oy;
            if (ox > maxx) maxx = ox;
            if (oy > maxy) maxy = oy;
        }
        else
        {
            r->landmarks[i - 4].x = ox;
            r->landmarks[i - 4].y = oy;
        }
    }

    r->x = minx;
    r->y = miny;
    r->w = maxx - minx;
    r->h = maxy - miny;

    // printf("Rect after transform: %u %u %u %u\n", r->x, r->y, r->w, r->h);
}

// Generate 1D Gaussian kernel
static void generate_kernel(float sigma, float **kernel, int *k_size)
{
    int radius = (int)ceilf(3 * sigma);
    *k_size = 2 * radius + 1;
    *kernel = (float *)malloc((*k_size) * sizeof(float));

    float sum = 0.0f;
    for (int i = 0; i < *k_size; i++) {
        int x = i - radius;
        (*kernel)[i] = expf(-(x * x) / (2 * sigma * sigma));
        sum += (*kernel)[i];
    }
    for (int i = 0; i < *k_size; i++) {
        (*kernel)[i] /= sum;
    }
}

// Convolution pass in horizontal or vertical direction, restricted to ROI
static void convolve_roi(Image *src, Image *dst,Rect *roi, float *kernel, int k_size,int horizontal)
{
    int radius = k_size / 2;

    for (int y = roi->y; y < roi->y + roi->h; ++y)
    {
        u8 *src_row = src->data + y * src->step;
        u8 *dst_row = dst->data + y * dst->step;

        for (int x = roi->x; x < roi->x + roi->w; ++x)
        {
            for (int c = 0; c < src->n; c++)
            {
                float sum = 0.0f;

                for (int k = -radius; k <= radius; ++k)
                {
                    int xx = x + (horizontal ? k : 0);
                    int yy = y + (horizontal ? 0 : k);

                    // clamp to border of the ROI (or image if you prefer)
                    if (xx < roi->x) xx = roi->x;
                    if (xx >= roi->x + roi->w) xx = roi->x + roi->w - 1;
                    if (yy < roi->y) yy = roi->y;
                    if (yy >= roi->y + roi->h) yy = roi->y + roi->h - 1;

                    u8 *p = src->data + yy * src->step + xx * src->n + c;
                    sum += (*p) * kernel[k + radius];
                }

                dst_row[x * src->n + c] = (u8)fminf(fmaxf(sum, 0.0f), 255.0f);
            }
        }
    }
}

void transform_gaussian_blur(Image *image, Rect *r)
{
    if (!image || !r) return;
    if (r->x >= image->w || r->y >= image->h) return;

    // Clamp ROI inside image
    if (r->x + r->w > image->w) r->w = image->w - r->x;
    if (r->y + r->h > image->h) r->h = image->h - r->y;

    float base = (r->w < r->h ? r->w : r->h);
    float sigma = 0.24 * settings.blur_strength * base;   // tune multiplier to taste

    if (sigma < 0.7f) sigma = 0.7f;  // clamp minimum

    float *kernel;
    int k_size;
    generate_kernel(sigma, &kernel, &k_size);

    // temp image buffer for intermediate result
    Image tmp = *image;
    tmp.data = (u8 *)malloc(image->step * image->h);
    memcpy(tmp.data, image->data, image->step * image->h);

    // horizontal pass
    convolve_roi(image, &tmp, r, kernel, k_size, 1);
    // vertical pass (write back into original image buffer)
    convolve_roi(&tmp, image, r, kernel, k_size, 0);

    free(tmp.data);
    free(kernel);
}

void transform_box_blur(Image *image, Rect *r, u8 *buffer)
{
    if(!buffer) return;

    if (!image || !r) return;
    if (r->x >= image->w || r->y >= image->h) return;

    // Clamp ROI inside image
    if (r->x + r->w > image->w) r->w = image->w - r->x;
    if (r->y + r->h > image->h) r->h = image->h - r->y;

    float base = (r->w < r->h ? r->w : r->h);
    int radius = (int)(0.24f * settings.blur_strength * base);
    if (radius < 1) radius = 1;

    int k_size = 2 * radius + 1;
    float norm = 1.0f / k_size;

    // Precompute clamped indices for horizontal and vertical passes
    int *h_indices = (int *)malloc((r->w + 2 * radius) * sizeof(int));
    int *v_indices = (int *)malloc((r->h + 2 * radius) * sizeof(int));

    for (int i = -radius; i < r->w + radius; ++i)
    {
        int idx = r->x + i;
        if (idx < r->x) idx = r->x;
        if (idx >= r->x + r->w) idx = r->x + r->w - 1;
        h_indices[i + radius] = idx;
    }
    for (int i = -radius; i < r->h + radius; ++i)
    {
        int idx = r->y + i;
        if (idx < r->y) idx = r->y;
        if (idx >= r->y + r->h) idx = r->y + r->h - 1;
        v_indices[i + radius] = idx;
    }

    u8 *tmp_data = buffer;

    for (int pass = 0; pass < 3; ++pass)
    {
        // ---- Horizontal pass ----
        for (int y = r->y; y < r->y + r->h; ++y)
        {
            u8 *src_row = image->data + y * image->step;
            u8 *dst_row = tmp_data + y * image->step;

            for (int c = 0; c < image->n; ++c)
            {
                int sum = 0;

                // Initialize sum for first pixel
                for (int k = 0; k < k_size; ++k)
                    sum += src_row[h_indices[k] * image->n + c];

                dst_row[r->x * image->n + c] = (u8)(sum * norm);

                for (int x = r->x + 1; x < r->x + r->w; ++x)
                {
                    int left  = h_indices[x - r->x - 1 + 0]; // previous left
                    int right = h_indices[x - r->x + k_size - 1]; // new right
                    sum += src_row[right * image->n + c] - src_row[left * image->n + c];
                    dst_row[x * image->n + c] = (u8)(sum * norm);
                }
            }
        }

        // ---- Vertical pass ----
        for (int x = r->x; x < r->x + r->w; ++x)
        {
            for (int c = 0; c < image->n; ++c)
            {
                int sum = 0;

                // Initialize sum for first pixel
                for (int k = 0; k < k_size; ++k)
                    sum += tmp_data[v_indices[k] * image->step + x * image->n + c];

                u8 *dst_row = image->data + r->y * image->step;
                dst_row[x * image->n + c] = (u8)(sum * norm);

                for (int y = r->y + 1; y < r->y + r->h; ++y)
                {
                    int top    = v_indices[y - r->y - 1 + 0];
                    int bottom = v_indices[y - r->y + k_size - 1];
                    sum += tmp_data[bottom * image->step + x * image->n + c] -
                           tmp_data[top * image->step + x * image->n + c];
                    dst_row = image->data + y * image->step;
                    dst_row[x * image->n + c] = (u8)(sum * norm);
                }
            }
        }

        if (pass < 2)
            memcpy(tmp_data, image->data, image->step * image->h);
    }

    free(h_indices);
    free(v_indices);
}

void transform_apply(Image* image, int num_rects, Rect* rects, TransformType transform)
{
    u8 *buffer = NULL;
    
    if(transform == TRANSFORM_TYPE_BLUR)
    {
        buffer = (u8 *)malloc(image->step * image->h);  
    }

    for(int i = 0; i < num_rects; ++i)
    {
        Rect r = rects[i];

        switch(transform)
        {
            case TRANSFORM_TYPE_BLACKOUT:       transform_draw_rect(image, r,(Color){0,0,0,255}, true, 1.0); break;
            case TRANSFORM_TYPE_PIXELATE:       transform_pixelate(image, r, settings.block_scale); break;
            case TRANSFORM_TYPE_SCRAMBLE:       transform_scramble(image, r, 0);    break;
            case TRANSFORM_TYPE_SCRAMBLE_FIXED: transform_scramble(image, r, 409);  break; // @TODO: seed
            case TRANSFORM_TYPE_BLUR:           transform_box_blur(image, &r, buffer); break;
            case TRANSFORM_TYPE_TEXTURE: {
               if(settings.has_texture) {
                   transform_stretch_image(image, &texture_image, r);
               }
            }break;
            default: break;
        }
    }

    if(buffer)
    {
        free(buffer);
    }
}
