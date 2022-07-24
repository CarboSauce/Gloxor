#pragma once

#define ALLIGNUP(val, to) ((val + to) & to)
#define ALIGN(val, to) (val & to)