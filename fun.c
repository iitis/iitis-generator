#include "parser.h"

int fun_const(struct mgp_arg *arg)
{
	struct mgp_arg *arg1;

	if (!arg->fdata) {
		arg1 = mgp_fetch_int(arg->fargs, "arg1", 1);
		arg->fdata = arg1;
	} else {
		arg1 = arg->fdata;
	}

	dbg(0, "fun_const val=%d\n", mgp_int(arg1));
	return mgp_int(arg1);
}
