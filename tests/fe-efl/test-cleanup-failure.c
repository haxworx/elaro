#include "src/fe-efl/ui/elaro_ui_private.h"
#include <assert.h>
#include "no-safety-checks.h"

static char *
_test_executable_find(const char *name)
{
   return strdup("/bin/true");
}

static char *
_test_text_markup(const char *text)
{
   return strdup(text);
}

static Eina_Strbuf *
_test_strbuf_new(void)
{
   return NULL;
}

static void
_test_strbuf_free(Eina_Strbuf *buffer)
{
   /* Unlike free(NULL), an Eina destructor may dereference its argument. */
   assert(buffer);
   eina_strbuf_free(buffer);
}

#define elaro_executable_find _test_executable_find
#define elaro_ui_text_markup _test_text_markup
#define eina_strbuf_new _test_strbuf_new
#define eina_strbuf_free _test_strbuf_free
#include "src/fe-efl/ui/elaro_ui_notify.c"
#undef eina_strbuf_free
#undef eina_strbuf_new
#undef elaro_ui_text_markup
#undef elaro_executable_find

int
main(int argc, char **argv)
{
   assert(elm_init(argc, argv));
   /* Earlier strings have been allocated, but the body buffer fails. Cleanup
    * must release those strings without passing NULL to eina_strbuf_free(). */
   elaro_ui_notify_private("test", "alice", "hello");
   elm_shutdown();
   return 0;
}
