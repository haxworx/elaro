#include "src/fe-efl/ui/elaro_ui_private.h"
#include <assert.h>
#include <utime.h>
#include <pthread.h>
#include <unistd.h>

/* Deterministic downloads: never contact Gravatar or send a real email. */
typedef struct
{
    Ecore_File_Download_Completion_Cb done;
    Ecore_File_Download_Progress_Cb progress;
    void *data;
    char *file;
} Fake_Download;
static unsigned int requests, lookups, aborts;
static Fake_Download *pending;

Eina_Bool ecore_file_download(const char *url, const char *dst,
    Ecore_File_Download_Completion_Cb done, Ecore_File_Download_Progress_Cb progress,
    void *data, Ecore_File_Download_Job **job)
{
    assert(!pending);
    assert(!strncmp(url, "https://gravatar.com/avatar/", 28));
    assert(strstr(url, "?s=96&d=404"));
    assert(!strchr(url, '@'));
    pending = calloc(1, sizeof(*pending));
    pending->done = done; pending->progress = progress;
    pending->data = data; pending->file = strdup(dst);
    *job = (Ecore_File_Download_Job *)pending;
    requests++;
    return EINA_TRUE;
}

static void finish(int status)
{
    Fake_Download *job = pending;
    assert(job);
    pending = NULL;
    job->done(job->data, job->file, status);
    free(job->file); free(job);
}

void ecore_file_download_abort(Ecore_File_Download_Job *job)
{
    assert((void *)job == pending);
    aborts++;
    finish(1);
}

void elaro_engine_submit(Elaro_Engine *engine, Elaro_Engine_Command *c)
{
    assert(c->type == ELARO_ENGINE_USER_INFO);
    assert(!strcmp(c->server, "test") && !strcmp(c->target, "alice"));
    lookups++;
    free(c->server); free(c->target); free(c);
}
void elaro_ui_dcc_send_show(Elaro_Ui *ui, const char *server, const char *nick) {}
void elaro_ui_buffer_title_update(Elaro_Ui *ui) {}
void elaro_ui_buffer_select(Elaro_Ui_Buffer *buffer) {}
Elaro_Ui_Buffer *elaro_ui_buffer_get(Elaro_Ui *ui, const char *server, const char *target)
{
    return ui->current;
}

static pthread_t main_thread;
static unsigned int ticks;
static Eina_List *slow_cache_list(const char *dir)
{
    assert(!pthread_equal(pthread_self(), main_thread));
    usleep(150000); /* A slow disk must not stop UI timers. */
    return ecore_file_ls(dir);
}
#define ecore_file_ls slow_cache_list
#include "src/fe-efl/ui/elaro_ui_avatar.c"
#undef ecore_file_ls

static Eina_Bool heartbeat(void *data)
{
    ticks++;
    return ECORE_CALLBACK_RENEW;
}

static void settle(Avatar *avatar)
{
    double deadline = ecore_time_get() + 3;
    do {
        ecore_main_loop_iterate_may_block(EINA_FALSE);
        usleep(1000);
        assert(ecore_time_get() < deadline);
    } while (avatar && avatar->work);
}

#include "src/fe-efl/ui/elaro_ui_users.c"

static Avatar *record(Elaro_Ui *ui, const char *email)
{
    char *hash = elaro_avatar_hash_get(email);
    Avatar *avatar = eina_hash_find(ui->avatars, hash);
    free(hash);
    assert(avatar);
    return avatar;
}

static void make_png(Elaro_Ui *ui, const char *path)
{
    Evas_Object *image = evas_object_image_add(evas_object_evas_get(ui->win));
    evas_object_image_size_set(image, 1, 1);
    unsigned int *pixels = evas_object_image_data_get(image, EINA_TRUE);
    *pixels = 0xff336699;
    evas_object_image_data_set(image, pixels);
    char *png = elaro_string_printf("%s.png", path);
    assert(evas_object_image_save(image, png, NULL, "compress=0"));
    assert(!rename(png, path));
    free(png);
    evas_object_del(image);
}

int main(int argc, char **argv)
{
    Elaro_Ui ui = {0};
    Elaro_Ui_Buffer channel = { .server = "test", .target = "#test", .channel = EINA_TRUE };
    Elaro_Engine_Event info = { .server = "test", .target = "alice",
        .text = "Alice <alice@example.org>" };
    char *cache = elaro_path_join(getenv("HOME"), ".cache", NULL);
    setenv("XDG_CACHE_HOME", cache, 1);
    free(cache);
    ecore_main_loop_glib_always_integrate_disable();
    assert(elm_init(argc, argv) > 0);
    main_thread = pthread_self();
    Ecore_Timer *pulse = ecore_timer_add(0.01, heartbeat, NULL);
    ui.win = elm_win_util_standard_add("avatar-test", "Avatar test");
    ui.nicks = elm_genlist_add(ui.win);
    ui.current = &channel;
    elaro_ui_users_init(&ui);
    Elaro_Engine_User nick = { .nick = "alice" };
    elaro_ui_users_update(&ui, &nick, 1);
    User *u = eina_hash_find(ui.users, "alice");
    assert(u);
    assert(!_user_tooltip(u, ui.nicks, ui.win, u->item));
    const char *placeholder_file, *placeholder_group;
    Evas_Object *placeholder = evas_object_data_get(u->preview, "image");
    elm_image_file_get(placeholder, &placeholder_file, &placeholder_group);
    assert(placeholder_file && placeholder_group);
    assert(!strcmp(placeholder_group, "elaro/icons/person"));
    assert(lookups == 1 && requests == 0);
    /* A server response for another network must not identify this user. */
    info.server = "other";
    elaro_ui_users_details(&ui, &info);
    assert(!u->email && requests == 0);
    info.server = "test";
    elaro_ui_users_details(&ui, &info);
    assert(!strcmp(u->email, "alice@example.org") && requests == 0);
    Avatar *avatar = record(&ui, u->email);
    assert(avatar->work); /* Hover returned before the deliberately slow disk scan. */
    settle(avatar);
    assert(ticks >= 5 && requests == 1);
    /* Repeated hovers share an in-flight download. */
    assert(!_user_tooltip(u, ui.nicks, ui.win, u->item));
    assert(lookups == 1 && requests == 1);
    avatar = record(&ui, u->email);
    assert(eina_list_count(avatar->images) == 1);
    make_png(&ui, pending->file);
    finish(200);
    settle(avatar);
    for (unsigned int i = 0; i < 100; i++) {
        ecore_main_loop_iterate_may_block(EINA_FALSE);
        usleep(1000);
    }
    assert(avatar->ready && _fresh(avatar->path, AVATAR_LIFETIME));
    assert(_user_tooltip(u, ui.nicks, ui.win, u->item) == u->preview);
    assert(requests == 1);
    elaro_ui_users_clear(&ui);
    assert(!avatar->images);
    elaro_ui_avatars_shutdown(&ui);

    /* A new cache instance loads the saved image without a download. */
    Evas_Object *image = elm_icon_add(ui.win);
    elaro_ui_avatar_load(&ui, "ALICE@example.org", image);
    avatar = record(&ui, "alice@example.org");
    settle(avatar);
    assert(requests == 1);
    struct utimbuf old = { .actime = 1, .modtime = 1 };
    assert(!utime(avatar->path, &old));
    avatar->expires = 0;
    avatar->attempted = 0;
    elaro_ui_avatar_load(&ui, "alice@example.org", image);
    settle(avatar);
    assert(requests == 2);
    evas_object_del(image); /* Channel leaves while its avatar is downloading. */
    assert(!avatar->images);
    finish(404);
    settle(avatar);
    assert(_fresh(avatar->missing, AVATAR_MISSING_LIFETIME));
    elaro_ui_avatars_shutdown(&ui);
    image = elm_icon_add(ui.win);
    elaro_ui_avatar_load(&ui, "alice@example.org", image);
    avatar = record(&ui, "alice@example.org");
    settle(avatar);
    assert(requests == 2); /* Negative cache survives restarts too. */
    assert(!utime(avatar->missing, &old));
    avatar->expires = 0;
    avatar->attempted = 0;
    elaro_ui_avatar_load(&ui, "alice@example.org", image);
    settle(avatar);
    assert(requests == 3);
    assert(pending->progress(pending->data, pending->file, AVATAR_MAX_BYTES + 1, 0, 0, 0));
    elaro_ui_avatars_shutdown(&ui); /* Aborts callbacks before freeing cache records. */
    assert(aborts == 1 && !pending);
    assert(!evas_object_data_get(image, "avatar-record"));
    evas_object_del(image);
    /* Close the UI while cache preparation is still running. */
    image = elm_icon_add(ui.win);
    elaro_ui_avatar_load(&ui, "closing@example.org", image);
    elaro_ui_avatars_shutdown(&ui);
    evas_object_del(image);
    for (unsigned int i = 0; i < 300; i++) {
        ecore_main_loop_iterate_may_block(EINA_FALSE);
        usleep(1000);
    }
    assert(!pending && requests == 3);
    /* Invalid downloaded data is rejected through async image callbacks. */
    image = elm_icon_add(ui.win);
    elaro_ui_avatar_load(&ui, "invalid@example.org", image);
    avatar = record(&ui, "invalid@example.org");
    settle(avatar);
    assert(pending);
    FILE *invalid = fopen(pending->file, "w");
    assert(invalid);
    fputs("This is not an image", invalid);
    fclose(invalid);
    finish(200);
    for (unsigned int i = 0; i < 500; i++) {
        ecore_main_loop_iterate_may_block(EINA_FALSE);
        usleep(1000);
    }
    settle(avatar);
    assert(!avatar->ready && !_fresh(avatar->path, AVATAR_LIFETIME));
    elm_image_file_get(image, &placeholder_file, &placeholder_group);
    assert(placeholder_group && !strcmp(placeholder_group, "elaro/icons/person"));
    elaro_ui_avatars_shutdown(&ui);
    evas_object_del(image);

    ecore_timer_del(pulse);
    elaro_ui_users_shutdown(&ui);
    evas_object_del(ui.win);
    elm_shutdown();
    return 0;
}
