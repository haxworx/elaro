#ifndef ELARO_INTL_H
#define ELARO_INTL_H

#include "elaro-config.h"

#if ENABLE_NLS
#include <libintl.h>
#define _(text) dgettext(PACKAGE_TARNAME, text)
#else
#define _(text) (text)
#endif
#define N_(text) text

#endif
