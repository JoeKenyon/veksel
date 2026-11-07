#pragma once

bool platform_init(const char *title, int w, int h);
bool platform_begin_frame();   // false when the window should close; opens a fullscreen "root" window
void platform_end_frame();
void platform_shutdown();
