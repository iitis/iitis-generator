#include <stdarg.h>
#include "generator.h"
#include "parser.h"

struct mgp_line *mgp_parse_line(mmatic *mm, const char *line,
	int argmax, const char *rest, const char *errmsg, ...)
{
	struct mgp_line *ret;
	struct mgp_arg *arg;
	va_list vl; const char *argname;
	int i, j;
	bool inq, end = false;
	char *l, *key, *val;
	tlist *mapping;

	ret = mmatic_zalloc(mm, sizeof *ret);
	ret->args = thash_create_strkey(NULL, mm);

	mapping = tlist_create(NULL, mm);
	l = mmatic_strdup(mm, line);

	/* read the mapping */
	va_start(vl, errmsg);
	while ((argname = va_arg(vl, const char *)))
		tlist_push(ret->mapping, argname);
	va_end(vl);

	/* read the line */
	i = 0;
	for (argc = 0; argc < argmax; argc++) {
		arg = mmatic_zalloc(mm, sizeof *arg);
		inq = false;

		for (j = i; l[j]; j++) {
			if (l[j] == '"' && (j == 0 || l[j-1] != '\\')) {
				if (inq) { j++; break; }
				else { inq = true; continue; }
			}

			if (inq) continue;
			if (l[j] == '=') break;
			if (l[j] == ' ') break;
		}

		if (l[j] == ' ' || !l[j]) { /* =argument without a name */
			/* get key from mapping */
			arg->name = tlist_shift(mapping);
			if (!arg->name) {
				errmsg = mmatic_printf(mm, "no name mapping for argc=%d\n", argc);
				return NULL;
			}

			if (!l[j])
				end = true;

			/* treat i->j part as value */
			if (inq) {
				l[j-1] = '\0';
				arg->as_string = l + i + 1;
			} else {
				l[j] = '\0';
				arg->as_string = l + i;
			}
			i = j + 1;
		} else if (l[j] == '=') { /* = argument with name */
			/* treat i->j part as key */
			if (inq) {
				l[j-1] = '\0';
				key = l + i + 1;
			} else {
				l[j] = '\0';
				key = l + i;
			}
			i = j + 1;

			/* read the value */
			inq = false;
			for (j = i; l[j]; j++) {
				if (l[j] == '"' && l[j-1] != '\\') {
					if (inq) { j++; break; }
					else { inq = true; continue; }
				}

				if (inq) continue;
				if (l[j] == ' ') break;
			}

			if (!l[j])
				end = true;

			l[j] = '\0';
			arg->as_string = l + i;
			i = j + 1;
		} else {
			errmsg = mmatic_printf(mm, "invalid character in argc=%d: '%c'", argc, l[j]);
			return NULL;
		}

		/* XXX: now argument name is in arg->name and its value in arg->as_string */

		if (arg->isfunc) {
			/* TODO */
		} else {
			/* get other representations */
			arg->as_int = atoi(arg->as_string);
			arg->as_float = strtof(arg->as_string, NULL);
		}

		/* store the argument */
		thash_set(ret->args, arg->name, arg);

		if (end)
			break;
	}

	return ret;
}

/** Get argument value as string
 * @note does not support struct mgp_arg.isfunc */
const char *mgp_get_string(struct mgp_line *l, const char *name)
{

}

/** Get argument value as int, supports underlying functions */
int mgp_get_int(struct mgp_line *l, const char *name)
{

}

/** Get argument value as float
 * @note does not support struct mgp_arg.isfunc */
float mgp_get_float(struct mgp_line *l, const char *name)
{

}
