#ifndef _PARSER_H_
#define _PARSER_H_

#include "generator.h"

/** Representation of a parsed traffic file line */
struct mgp_line {
	mmatic *mm;
	thash *args;         /**< arguments: thash of arg name -> struct mgp_arg */
};

/** Argument value */
struct mgp_arg {
	struct mgp_line *line;             /**< parent */
	const char *name;                  /**< argument name */

	char *as_string;                   /**< value as string */
	int as_int;                        /**< value as integer */
	float as_float;                    /**< value as float */

	bool isfunc;                       /**< is a function reference? */
	int (*fptr)(struct mgp_arg *);     /**< function pointer */
	struct mgp_line *fargs;            /**< function arguments */
	void *fdata;                       /**< function private data */
};

/** Parse a line of traffic file
 * Parse a line into struct mgp_line, which can be later accessed via mgp_get_*() functions.
 *
 * For positional arguments, list of argc->name mapping can be supplied. Otherwise, arguments will
 * be named like arg1, arg2, ...
 *
 * If argmax is given and there still is some part of line that was not parsed, pointer to the
 * beginning of this part will be returned in rest.
 *
 * @retval NULL parse error, see errmsg
 */
struct mgp_line *mgp_parse_line(
	mmatic *mm,          /**< [in] memory to allocate in */
	const char *line,    /**< [in] line to parse */
	int argmax,          /**< [in] stop after argmax positional arguments, 0=no limit */
	char **rest,         /**< [out] pointer to the rest of line, may be NULL */
	char **errmsg,       /**< [out] optional error message, may be NULL */
	...                  /**< [in] optional mapping of argc->name, end with NULL */
);

/** Fetch an integer argument
 * Ensures given argument is defined. If not, sets it to a default value.
 * @param defval   set value to defval if argument not specified */
struct mgp_arg *mgp_prepare_int(struct mgp_line *l, const char *name, int defval);

/** Fetch a string argument
 * @see mgp_prepare_int()
 * @param defval   use a strdup of defval if argument not specified */
struct mgp_arg *mgp_prepare_string(struct mgp_line *l, const char *name, const char *defval);

/** Fetch a float argument
 * @see mgp_prepare_int()
 * @param defval   set value to defval if argument not specified */
struct mgp_arg *mgp_prepare_float(struct mgp_line *l, const char *name, float defval);

/** Get the actual value of an integer argument
 * @note Supports functions */
static inline int mgp_int(struct mgp_arg *arg) { return arg->isfunc ? arg->fptr(arg) : arg->as_int; }

/** Get the actual value of a string argument */
static inline char *mgp_string(struct mgp_arg *arg) { return arg->as_string; }

/** Get the actual value of a float argument */
static inline float mgp_float(struct mgp_arg *arg) { return arg->as_float; }

/** Shortcut */
#define mgp_get_int(l, name, defval) mgp_int(mgp_prepare_int(l, name, defval))

/** Shortcut */
#define mgp_get_string(l, name, defval) mgp_string(mgp_prepare_string(l, name, defval))

/** Shortcut */
#define mgp_get_float(l, name, defval) mgp_float(mgp_prepare_float(l, name, defval))

#endif
