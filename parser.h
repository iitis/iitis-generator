#ifndef _PARSER_H_
#define _PARSER_H_

#include "generator.h"

/** Representation of a parsed traffic file line */
struct mgp_line {
	thash *args;         /**< arguments: thash of arg name -> struct mgp_arg */
};

/** Argument value */
struct mgp_arg {
	mmatic *mm;
	const char *name;                  /**< argument name */

	const char *as_string;             /**< value as string */
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

/** Get argument value as int
 * @note supports underlying functions
 * @retval 0 if given argument was not found */
int mgp_get_int(struct mgp_line *l, const char *name);

/** Get argument value as string
 * @note does not support struct mgp_arg.isfunc
 * @retval 0 if given argument was not found */
const char *mgp_get_string(struct mgp_line *l, const char *name);

/** Get argument value as float
 * @note does not support struct mgp_arg.isfunc
 * @retval 0 if given argument was not found */
float mgp_get_float(struct mgp_line *l, const char *name);

#endif
