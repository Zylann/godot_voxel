// Macros defined in Godot 3.2 in core/error_macros.h
// Copied here for Voxel Tools 3.1 branch

#define ERR_FAIL_COND_MSG(m_cond, m_msg)                                                                   \
	{                                                                                                      \
		if (unlikely(m_cond)) {                                                                            \
			ERR_EXPLAIN(m_msg);                                                                            \
			_err_print_error(FUNCTION_STR, __FILE__, __LINE__, "Condition ' " _STR(m_cond) " ' is true."); \
			return;                                                                                        \
		}                                                                                                  \
		_err_error_exists = false;                                                                         \
	}

#define ERR_FAIL_COND_V_MSG(m_cond, m_retval, m_msg)                                                                                 \
	{                                                                                                                                \
		if (unlikely(m_cond)) {                                                                                                      \
			ERR_EXPLAIN(m_msg);                                                                                                      \
			_err_print_error(FUNCTION_STR, __FILE__, __LINE__, "Condition ' " _STR(m_cond) " ' is true. returned: " _STR(m_retval)); \
			return m_retval;                                                                                                         \
		}                                                                                                                            \
		_err_error_exists = false;                                                                                                   \
	}
