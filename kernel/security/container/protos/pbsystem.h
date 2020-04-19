/* SPDX-License-Identifier: GPL-2.0 */
/* Header and types for nanopb to work with the Linux kernel */
#include <linux/kernel.h>
#include <linux/string.h>

/* Small types.  */

/* Signed.  */
typedef signed char		int_least8_t;
typedef short int		int_least16_t;
typedef int			int_least32_t;
typedef long int		int_least64_t;

/* Unsigned.  */
typedef unsigned char		uint_least8_t;
typedef unsigned short int	uint_least16_t;
typedef unsigned int		uint_least32_t;
typedef unsigned long int	uint_least64_t;

/* Fast types.  */

/* Signed.  */
typedef signed char		int_fast8_t;
typedef long int		int_fast16_t;
typedef long int		int_fast32_t;
typedef long int		int_fast64_t;

/* Unsigned.  */
typedef unsigned char		uint_fast8_t;
typedef unsigned long int	uint_fast16_t;
typedef unsigned long int	uint_fast32_t;
typedef unsigned long int	uint_fast64_t;
