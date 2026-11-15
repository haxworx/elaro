#include "src/fe-efl/elaro_config.h"
#include "src/fe-efl/elaro_util.h"
#include <Eet.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#ifdef ELARO_TEST_NO_SAFETY_CHECKS
#include "no-safety-checks.h"
#endif

static int allocations_left = -1;

static Eina_Bool
_allocation_fail(void)
{
   if (allocations_left < 0) return EINA_FALSE;
   return allocations_left-- == 0;
}

static void *
_test_calloc(size_t count, size_t size)
{
   return _allocation_fail() ? NULL : calloc(count, size);
}

static char *
_test_strdup(const char *text)
{
   return _allocation_fail() ? NULL : strdup(text);
}

#define calloc _test_calloc
#define strdup _test_strdup
#include "src/fe-efl/elaro_config.c"
#undef calloc
#undef strdup

int
main(void)
{
   Elaro_Config *original, *copy;
   int i;

   assert(eet_init());
   original = _defaults();
   assert(original);

   /* Fail the record allocation and each string copy in turn. Cleanup must
    * only free the new copy's strings; original still owns all its fields. */
   for (i = 0; i < 10; i++)
     {
        allocations_left = i;
        copy = elaro_config_connection_copy(original);
        assert(!copy);
        assert(!strcmp(original->host, "irc.libera.chat"));
        assert(!strcmp(original->sasl_password, ""));
     }
   allocations_left = -1;
   copy = elaro_config_connection_copy(original);
   assert(copy);
   assert(copy->host != original->host);
   assert(!strcmp(copy->host, original->host));
   elaro_config_free(copy);
   elaro_config_free(original);
   eet_shutdown();
   return 0;
}
