#include "zforth.h"

void message(const char *msg);
void beep(int freq, int duration);

int zforth_init();
int zforth_eval(const char *buf);