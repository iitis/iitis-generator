#include "parser.h"

int fun_const(struct mgp_arg *arg)
{
	return mgp_get_int(arg->fargs, "arg1");
}
