#pragma once

bool platform_init(const char *title, int w, int h);
bool platform_begin_frame();
void platform_end_frame();
void platform_shutdown();
