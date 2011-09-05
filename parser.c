#include <stdarg.h>
#include <ctype.h>
#include <dlfcn.h>
#include "generator.h"
#include "parser.h"

/** Return address of function referenced in traffic file
 * @retval NULL invalid function
 */
static int (*funcname2ptr(const char *funcname))()
{
	static void *me = NULL;
	static char buf[128];

	/* a trick for conversion void* -> int (*fun)() */
	static union {
		void *ptr;
		int (*fun)();
	} void2func;

	if (!me)
		me = dlopen(NULL, RTLD_NOW | RTLD_GLOBAL);

	snprintf(buf, sizeof buf, "fun_%s", funcname);
	void2func.ptr = dlsym(me, buf);

	return void2func.fun;
}

struct mgp_line *mgp_parse_line(mmatic *mm, const char *line, int argmax, char **rest, char **errmsg, ...)
{
	struct mgp_line *ret;
	struct mgp_arg *arg;
	va_list vl; const char *argname;
	int argnum, argc, i, j;
	bool inq, end = false;
	int inp;
	char *l, *fname, *fargs;
	tlist *mapping;

	ret = mmatic_zalloc(mm, sizeof *ret);
	ret->args = thash_create_strkey(NULL, mm);
	ret->mm = mm;

	mapping = tlist_create(NULL, mm);
	l = mmatic_strdup(mm, line);

	/* read the mapping */
	va_start(vl, errmsg);
	while ((argname = va_arg(vl, const char *)))
		tlist_push(mapping, argname);
	va_end(vl);

	/* read the line */
	i = 0;
	argc = 1;
	for (argnum = 1; l[i] && (argmax == 0 || argnum <= argmax); argnum++) {
		arg = mmatic_zalloc(mm, sizeof *arg);
		arg->line = ret;

		/* check if the argument has a name= prefix */
		inq = false;
		inp = 0;
		for (j = i; l[j]; j++) {
			if (l[j] == '"' && (j == 0 || l[j-1] != '\\')) {
				if (inq) { j++; break; }
				else { inq = true; continue; }
			}
			if (inq) continue;

			if (l[j] == '(') { inp++; continue; }
			else if (inp && l[j] == ')') { inp--; continue; }

			if (l[j] == ',') l[j] = ' ';
			if (inp) continue;

			if (l[j] == '=') break;
			if (isspace(l[j])) break;
		}

		if (isspace(l[j]) || !l[j]) { /* =argument without a name */
			/* get key from mapping */
			arg->name = tlist_shift(mapping);
			if (!arg->name) {
				arg->name = mmatic_printf(mm, "arg%d", argc++);
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
				arg->name = l + i + 1;
			} else {
				l[j] = '\0';
				arg->name = l + i;
			}
			i = j + 1;

			/* read the value */
			inq = false;
			inp = 0;
			for (j = i; l[j]; j++) {
				if (l[j] == '"' && l[j-1] != '\\') {
					if (inq) { j++; break; }
					else { inq = true; continue; }
				}
				if (inq) continue;

				if (l[j] == '(') { inp++; continue; }
				else if (inp && l[j] == ')') { inp--; continue; }

				if (l[j] == ',') l[j] = ' ';
				if (inp) continue;

				if (isspace(l[j])) break;
			}

			if (!l[j])
				end = true;

			l[j] = '\0';
			arg->as_string = l + i;
			i = j + 1;
		} else {
			if (errmsg)
				*errmsg = mmatic_printf(mm, "invalid character in argument number %d: '%c'", argnum, l[j]);
			return NULL;
		}

		/* XXX: now argument name is in arg->name and its value in arg->as_string */

		/* check if is a function */
		if (pjf_match("/^[^ ]+\\(.*\\)$/", arg->as_string) > 0) {
			arg->isfunc = true;

			/* split into function name and its args */
			fname = mmatic_strdup(mm, arg->as_string);
			fargs = strchr(fname, '(');
			*fargs++ = '\0';
			fargs[strlen(fargs) - 1] = '\0';

			/* set fptr */
			arg->fptr = funcname2ptr(fname);
			if (!arg->fptr) {
				if (errmsg)
					*errmsg = mmatic_printf(mm, "invalid function name in argument number %d: '%s'",
						argnum, fname);
				return NULL;
			}

			/* parse args recursively */
			arg->fargs = mgp_parse_line(mm, fargs, 0, NULL, errmsg, NULL);
		} else {
			arg->isfunc = false;
		}

		/* store the argument */
		thash_set(ret->args, arg->name, arg);

		/* go to next statement */
		if (end) break;
		while (isspace(l[i])) i++;
	}

	if (rest)
		*rest = l + i;

	return ret;
}

/*
 * Fetch
 */

/** Fetch given argument, creating it if it does not exist */
static struct mgp_arg *fetch(struct mgp_line *l, const char *name)
{
	struct mgp_arg *arg;

	arg = thash_get(l->args, name);
	if (!arg) {
		arg = mmatic_zalloc(l->mm, sizeof *arg);
		arg->name = mmatic_strdup(l->mm, name);
		thash_set(l->args, name, arg);
	}

	return arg;
}

struct mgp_arg *mgp_fetch_int(struct mgp_line *l, const char *name, int defval)
{
	struct mgp_arg *arg = fetch(l, name);

	if (!arg->as_string)
		arg->as_int = defval;
	else if (!arg->isfunc)
		arg->as_int = atoi(arg->as_string);

	return arg;
}

struct mgp_arg *mgp_fetch_string(struct mgp_line *l, const char *name, const char *defval)
{
	struct mgp_arg *arg = fetch(l, name);

	if (!arg->as_string)
		arg->as_string = mmatic_strdup(l->mm, defval);

	return arg;
}

struct mgp_arg *mgp_fetch_float(struct mgp_line *l, const char *name, float defval)
{
	struct mgp_arg *arg = fetch(l, name);

	if (!arg->as_string)
		arg->as_float = defval;
	else
		arg->as_float = strtof(arg->as_string, NULL);

	return arg;
}
