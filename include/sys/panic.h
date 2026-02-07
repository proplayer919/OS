#ifndef SYS_PANIC_H
#define SYS_PANIC_H

void panic(const char *message);

#define PANIC_IF(cond, msg) \
	do { \
		if (cond) { \
			panic(msg); \
		} \
	} while (0)

#endif
