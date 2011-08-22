/** Argument value */
struct mgp_arg {
	const char *name;       /**< argument name */

	const char *as_string;  /**< value as string */
	int as_int;             /**< value as integer */
	float as_float;         /**< value as float */

	bool isfunc;            /**< is a function reference? */
	int (*funcptr)();       /**< function pointer */
	void *arg;              /**< function argument */
};

/** Representation of a parsed traffic file line */
struct mgp_line {
	thash *args;         /**< arguments: thash of arg name -> struct mgp_arg */
};

/** Parse a line of traffic file */
struct mgp_line *mgp_parse_line(
	mmatic *mm,          /**< [in] memory to allocate in */
	const char *line,    /**< [in] line to parse */
	int argmax,          /**< [in] stop after argmax arguments */
	const char *rest,    /**< [out] pointer to the rest of line, may be NULL */
	const char *errmsg,  /**< [out] error message, optional */
	...                  /**< [in] mapping of argument number -> key name */
);

/** Get argument value as string
 * @note does not support struct mgp_arg.isfunc */
const char *mgp_get_string(struct mgp_line *l, const char *name);

/** Get argument value as int, supports underlying functions */
int mgp_get_int(struct mgp_line *l, const char *name);

/** Get argument value as float
 * @note does not support struct mgp_arg.isfunc */
float mgp_get_float(struct mgp_line *l, const char *name);
