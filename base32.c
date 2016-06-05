
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_base32.h"

/* If you declare any globals in php_base32.h uncomment this:
ZEND_DECLARE_MODULE_GLOBALS(base32)
*/

/* True global resources - no need for thread safety here */
static int le_base32;


static inline int bytes_encode(const char *text, int len, char **buffer, int buffers_len) {
	char base32[] = "ABCDEFGHIJKLMNPQRSTUVWXYZ3456789";
	unsigned int real_len = 0;
	unsigned int i = 0;
	unsigned char last_remain_bits = 0;
	unsigned char buf = 0;
	
	for(i = 0; i < len; i++) {
		unsigned char remain_bits = 3 + last_remain_bits;
		unsigned char index = 0;
		unsigned char need_bits = 5 - last_remain_bits;
		index = (((buf << need_bits)) | (((*(text + i) & 0xff)) >> remain_bits));
		*(*buffer + real_len) = base32[index];
		real_len += 1;
		
		//内存保护
		if (real_len >= buffers_len) {
			buf = 0;
			last_remain_bits = 0;
			break;
		}
		
		if (remain_bits < 5) {
			last_remain_bits = remain_bits;	
			// buf = (*(text +i)) & ~((0xffffffff >> remain_bits) << remain_bits);
			buf = ((*(text + i) << need_bits) & 0xff) >> need_bits;
		} else {
			index = (((*(text + i) << need_bits) & 0xff) >> need_bits) >> (remain_bits - 5);
			*(*buffer + real_len) = base32[index];
			last_remain_bits = remain_bits - 5;
			// buf = (*(text + i)) & ~((0xffffffff >> last_remain_bits) << last_remain_bits);
			buf = ((*(text + i) << (need_bits + 5)) & 0xff) >> (need_bits + 5);
			real_len += 1;
		}
		// 内存保护
		if (real_len >= buffers_len) {
			buf = 0;
			last_remain_bits = 0;
			break;
		}
	}
	
	//如果还有剩下的没有处理，这里另外处理
	if (last_remain_bits) {
		unsigned char index;
		index = buf << (5 - last_remain_bits);
		*(*buffer + real_len) = base32[index];
		real_len += 1;
		buf = 0;
		last_remain_bits = 0;
	}
	
	return real_len;
}

static inline int bytes_decode(const char *text, int len, char **buffer, int buffers_len) {
	int i;
	unsigned char buf;
	unsigned char buf_size = 0;
	unsigned char buf_left = 0;
	unsigned char buf_move = 0;
	
	for(i = 0; i < len; i++) {
		buf = *(text + i);
		
		if (buf >= 'A' && buf < 'O') {
			buf = buf - 'A';
		} else if (buf > 'O' && buf <= 'Z') {
			buf = (buf - 'P') + 14;
		} else if (buf > '2' && buf <= '9') {
			buf = (buf - '3') + 25;
		} else {
			continue;
		}
		
		if ((8 - buf_size) >= 5 ) {
			*(*buffer + buf_move) |= ((buf << (3 - buf_size)) & 0xff);
			buf_size = (buf_size + 5) & 7;
			
			if (buf_size == 0) {
				buf_move += 1;
				
				if (buf_move >= buffers_len) {
					break;
				}
			}
		} else {
			buf_left = buf_size - 3;
			*(*buffer + buf_move) |= (buf >> buf_left);
			buf_move += 1;
			
			if (buf_move >= buffers_len) {
				break;
			}
			// 处理下一个字符,把剩下的数放到下一个buf里面
			*(*buffer + buf_move) |= ((buf << (8 - buf_left)) & 0xff);
			buf_size = buf_left;
		}
	}
	
	return buf_move;
}

/* {{{ PHP_INI
 */
/* Remove comments and fill if you need to have entries in php.ini
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("base32.global_value",      "42", PHP_INI_ALL, OnUpdateLong, global_value, zend_base32_globals, base32_globals)
    STD_PHP_INI_ENTRY("base32.global_string", "foobar", PHP_INI_ALL, OnUpdateString, global_string, zend_base32_globals, base32_globals)
PHP_INI_END()
*/
/* }}} */

/* Remove the following function when you have successfully modified config.m4
   so that your module can be compiled into PHP, it exists only for testing
   purposes. */

/* Every user-visible function in PHP should document itself in the source */
/* {{{ proto string confirm_base32_compiled(string arg)
   Return a string to confirm that the module is compiled in */
PHP_FUNCTION(base32_encode)
{
	char *text = NULL;
	unsigned int text_len, result_len;
	char *result;
	char *buffers;
	unsigned int buffers_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &text, &text_len) == FAILURE) {
		return;
	}
	
	if (text_len > 0) {
		buffers_len = (text_len << 3) / 5;

		if ((text_len << 3) % 5) {
			buffers_len += 1;
		}

		buffers = (char *)ecalloc(sizeof(char), buffers_len + 1);
		
		if (!buffers) {
			RETURN_FALSE;
		}
		
		result_len = bytes_encode(text, text_len, &buffers, buffers_len);
		result_len = spprintf(&result, 0, "%s", buffers);
		efree(buffers);
	} else {
		result_len = spprintf(&result, 0, "");		
	}
	
	RETURN_STRINGL(result, result_len, 0);
}

PHP_FUNCTION(base32_decode)
{
	char *text = NULL;
	char *buffers;
	char *result;
	unsigned int text_len, result_len;
	unsigned int buffers_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &text, &text_len) == FAILURE) {
		return;
	}
	
	if (text_len > 0) {
		buffers_len = (text_len * 5) >> 3;
		buffers = (char *)ecalloc(sizeof(char), buffers_len + 1);
		
		if (!buffers) {
			RETURN_FALSE;
		}
		
		result_len = bytes_decode(text, text_len, &buffers, buffers_len);
		result_len = spprintf(&result, 0, "%s", buffers);
	} else {
		result_len = spprintf(&result, 0, "");
	}
	
	RETURN_STRINGL(result, result_len, 0);
}
/* }}} */
/* The previous line is meant for vim and emacs, so it can correctly fold and 
   unfold functions in source code. See the corresponding marks just before 
   function definition, where the functions purpose is also documented. Please 
   follow this convention for the convenience of others editing your code.
*/


/* {{{ php_base32_init_globals
 */
/* Uncomment this function if you have INI entries
static void php_base32_init_globals(zend_base32_globals *base32_globals)
{
	base32_globals->global_value = 0;
	base32_globals->global_string = NULL;
}
*/
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(base32)
{
	/* If you have INI entries, uncomment these lines 
	REGISTER_INI_ENTRIES();
	*/
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(base32)
{
	/* uncomment this line if you have INI entries
	UNREGISTER_INI_ENTRIES();
	*/
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(base32)
{
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(base32)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(base32)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "base32 support", "enabled");
	php_info_print_table_header(2, "author", "will");
	php_info_print_table_header(2, "email", "xuxianhua1985@126.com");
	php_info_print_table_header(2, "github", "https://github.com/will2008");
	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini
	DISPLAY_INI_ENTRIES();
	*/
}
/* }}} */

/* {{{ base32_functions[]
 *
 * Every user visible function must have an entry in base32_functions[].
 */
const zend_function_entry base32_functions[] = {
	PHP_FE(base32_encode,	NULL)		/* For testing, remove later. */
	PHP_FE(base32_decode,	NULL)		/* For testing, remove later. */
	PHP_FE_END	/* Must be the last line in base32_functions[] */
};
/* }}} */

/* {{{ base32_module_entry
 */
zend_module_entry base32_module_entry = {
	STANDARD_MODULE_HEADER,
	"base32",
	base32_functions,
	PHP_MINIT(base32),
	PHP_MSHUTDOWN(base32),
	PHP_RINIT(base32),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(base32),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(base32),
	PHP_BASE32_VERSION,
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_BASE32
ZEND_GET_MODULE(base32)
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
