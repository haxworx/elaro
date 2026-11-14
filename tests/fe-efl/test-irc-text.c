#include <glib.h>
#include "src/fe-efl/ui/elaro_ui_text.h"

static void check(const char *input, const char *expected)
{
    char *markup = elaro_ui_text_markup(input);
    g_assert_cmpstr(markup, ==, expected);
    g_free(markup);
}
int main(void)
{
    eina_init();
    check("hello \002bold\002 text", "hello <b>bold</b> text");
    check("\002<b>é & text", "<b>&lt;b&gt;&eacute; &amp; text</b>");
    check("\002bold\035italic\002plain italic\017reset", "<b>bold</b><b><i>italic</i></b><i>plain italic</i>reset");
    check("\037underlined\017 normal", "<u>underlined</u> normal");
    check("\00304,02red\003 text \004FF0000,000000hex\017", "red text hex");
    check("\026reverse\026 \001control", "reverse control");
    check("see https://example.org/cat.png", "see <a href=image:https%3A%2F%2Fexample.org%2Fcat.png>https://example.org/cat.png</a>");
    check("(https://example.org/cat.JPG?x=1&y=2).", "(<a href=image:https%3A%2F%2Fexample.org%2Fcat.JPG%3Fx%3D1%26y%3D2>https://example.org/cat.JPG?x=1&amp;y=2</a>).");
    check("https://example.org/page", "<a href=url:https%3A%2F%2Fexample.org%2Fpage>https://example.org/page</a>");
    check("\002https://example.org/a.gif\002", "<b><a href=image:https%3A%2F%2Fexample.org%2Fa.gif>https://example.org/a.gif</a></b>");
    check("<a href='bad'> https://example.org/a.png", "&lt;a href=&apos;bad&apos;&gt; <a href=image:https%3A%2F%2Fexample.org%2Fa.png>https://example.org/a.png</a>");
    g_assert_false(elaro_ui_text_image_url_check("file:///tmp/image.png"));
    g_assert_false(elaro_ui_text_image_url_check("https://example.org/page?image=a.png"));
    check("https://example.org/page?a=1&b=2", "<a href=url:https%3A%2F%2Fexample.org%2Fpage%3Fa%3D1%26b%3D2>https://example.org/page?a=1&amp;b=2</a>");
    check("file:///tmp/test", "file:///tmp/test");
    check("(https://example.org/wiki/Test_(example)).", "(<a href=url:https%3A%2F%2Fexample.org%2Fwiki%2FTest_%28example%29>https://example.org/wiki/Test_(example)</a>).");
    check("https://example.org/song.MP3?download=1&token=abc", "<a href=audio:https%3A%2F%2Fexample.org%2Fsong.MP3%3Fdownload%3D1%26token%3Dabc>https://example.org/song.MP3?download=1&amp;token=abc</a>");
    g_assert_true(elaro_ui_text_audio_url_check("https://example.org/music.flac#track"));
    g_assert_true(elaro_ui_text_audio_url_check("http://example.org/song.opus"));
    g_assert_false(elaro_ui_text_audio_url_check("file:///tmp/song.mp3"));
    g_assert_false(elaro_ui_text_audio_url_check("https://example.org/page?file=music.mp3"));
    eina_shutdown();
    return 0;
}
