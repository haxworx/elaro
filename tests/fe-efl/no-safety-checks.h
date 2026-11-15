#ifndef ELARO_TEST_NO_SAFETY_CHECKS_H
#define ELARO_TEST_NO_SAFETY_CHECKS_H

/* Exercise Eina's real disabled macros even when the installed EFL enables
 * safety checks. Include this after Eina has loaded its configuration. */
#include <Eina.h>
#undef EINA_SAFETY_CHECKS
#undef EINA_SAFETY_CHECKS_H_
#undef EINA_SAFETY_ON_NULL_RETURN
#undef EINA_SAFETY_ON_NULL_RETURN_VAL
#undef EINA_SAFETY_ON_NULL_GOTO
#undef EINA_SAFETY_ON_TRUE_RETURN
#undef EINA_SAFETY_ON_TRUE_RETURN_VAL
#undef EINA_SAFETY_ON_TRUE_GOTO
#undef EINA_SAFETY_ON_FALSE_RETURN
#undef EINA_SAFETY_ON_FALSE_RETURN_VAL
#undef EINA_SAFETY_ON_FALSE_GOTO
#include <eina_safety_checks.h>

#endif
