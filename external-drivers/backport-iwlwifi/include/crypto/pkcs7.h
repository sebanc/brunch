/* Automatically created during backport process */
#ifndef CPTCFG_BPAUTO_PKCS7
#include_next <crypto/pkcs7.h>
#else
#define pkcs7_verify LINUX_BACKPORT(pkcs7_verify)
#define pkcs7_get_content_data LINUX_BACKPORT(pkcs7_get_content_data)
#define pkcs7_parse_message LINUX_BACKPORT(pkcs7_parse_message)
#define pkcs7_free_message LINUX_BACKPORT(pkcs7_free_message)
#define pkcs7_validate_trust LINUX_BACKPORT(pkcs7_validate_trust)
#include <crypto/backport-pkcs7.h>
#endif /* CPTCFG_BPAUTO_PKCS7 */
