#pragma once

#define BIT(x) (1<<(x))
#define SETBIT(a,b) ((a)|BIT(b)))
#define MASK(a,b) ((a)&(b))
#define CHECKBIT(a,b) ((a)&BIT(b))