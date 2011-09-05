#include <stdlib.h>
#include "parser.h"

int fun_const(struct mgp_arg *arg)
{
	struct mgp_arg *v;

	if (!arg->fdata) {
		v = mgp_prepare_int(arg->fargs, "arg1", 1);
		arg->fdata = v;
	} else v = arg->fdata;

	return mgp_int(v);
}

/**********************/

struct uniform_data {
	struct mgp_arg *from;
	struct mgp_arg *to;
};

int fun_uniform(struct mgp_arg *arg)
{
	struct uniform_data *d;
	int base, range;

	if (!arg->fdata) {
		d = mmatic_zalloc(arg->line->mm, sizeof *d);
		arg->fdata = d;

		d->from = mgp_prepare_int(arg->fargs, "arg1", 1);
		d->to   = mgp_prepare_int(arg->fargs, "arg2", 100);

		/* initialize random number generator */
		srand48(time(NULL));
	} else d = arg->fdata;

	base = mgp_int(d->from);
	range = mgp_int(d->to) - base + 1;

	return base + (lrand48() % range);
}
