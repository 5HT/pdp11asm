#pragma once

#include <stddef.h>

bool make_radio86rk_rom(const char* fileName, unsigned start, const char* buf, size_t size, char *error_buf, size_t error_buf_size);

