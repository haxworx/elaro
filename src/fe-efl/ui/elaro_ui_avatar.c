#include "elaro_ui_private.h"
#include <sys/stat.h>
#include <unistd.h>

#define AVATAR_LIMIT 256
#define AVATAR_MAX_BYTES (256 * 1024)
#define AVATAR_LIFETIME (7 * 24 * 60 * 60)
#define AVATAR_MISSING_LIFETIME (60 * 60)

typedef struct _Avatar Avatar;

typedef struct
{
   Avatar *avatar;
   char *path, *missing, *temp, *dir;
   int status;
   Eina_Bool prepare, ready, download;
   time_t expires;
} Avatar_Work;

struct _Avatar
{
   char *path, *missing, *temp, *hash;
   Ecore_File_Download_Job *job;
   Ecore_Timer *timer;
   Avatar_Work *work;
   Eina_List *images;
   time_t attempted, expires;
   Eina_Bool ready;
};

static void
_work_start(Avatar *avatar, Eina_Bool prepare, int status);
static void
_download_start(Avatar *avatar);

void
elaro_ui_avatar_placeholder_set(Evas_Object *image)
{
   char *path;

   evas_object_data_del(image, "avatar-loaded");
   elm_image_async_open_set(image, EINA_FALSE);
   path = elaro_path_join(ELARO_DATA_DIR, "elaro.edj", NULL);
   if (!elm_image_file_set(image, path, "elaro/icons/person"))
     {
        free(path);
        path = elaro_path_join(ELARO_ASSET_BUILD_DIR, "elaro.edj", NULL);
        if (!elm_image_file_set(image, path, "elaro/icons/person"))
           elm_icon_standard_set(image, "user-identity");
     }
   free(path);
   elm_image_async_open_set(image, EINA_TRUE);
}

static void
_image_deleted(void *data, Evas *evas, Evas_Object *obj, void *info)
{
   Avatar *avatar;

   avatar = data;
   avatar->images = eina_list_remove(avatar->images, obj);
}

static void
_image_error(void *data, Evas_Object *image, void *info)
{
   Avatar *avatar;
   const char *file;

   avatar = evas_object_data_get(image, "avatar-record");
   elm_image_file_get(image, &file, NULL);
   if (!avatar || !file || strcmp(file, avatar->path)) return;
   avatar->ready = EINA_FALSE;
   avatar->expires = 0;
   elaro_ui_avatar_placeholder_set(image);
   if (!avatar->work && !avatar->job) _work_start(avatar, EINA_FALSE, -1);
}

static void
_image_opened(void *data, Evas_Object *image, void *info)
{
   Avatar *avatar;
   const char *file;
   int width, height;

   avatar = evas_object_data_get(image, "avatar-record");
   elm_image_file_get(image, &file, NULL);
   if (!avatar || !file || strcmp(file, avatar->path)) return;
   evas_object_image_size_get(elm_image_object_get(image), &width, &height);
   if (width <= 0 || height <= 0 || width > 512 || height > 512) _image_error(NULL, image, NULL);
   else evas_object_data_set(image, "avatar-loaded", image);
}

static void
_image_detach(Avatar *avatar, Evas_Object *image)
{
   avatar->images = eina_list_remove(avatar->images, image);
   evas_object_event_callback_del_full(image, EVAS_CALLBACK_DEL, _image_deleted, avatar);
   evas_object_smart_callback_del(image, "load,error", _image_error);
   evas_object_smart_callback_del(image, "load,open", _image_opened);
   evas_object_data_del(image, "avatar-record");
}

static void
_avatar_free(void *data)
{
   Avatar *avatar;
   Evas_Object *image;

   avatar = data;
   if (avatar->timer) ecore_timer_del(avatar->timer);
   avatar->timer = NULL;
   if (avatar->job) ecore_file_download_abort(avatar->job);
   /* Jobs own copied paths, and may finish after a channel or the UI closes.
    * Detaching instead of waiting keeps shutdown off the worker's critical path. */
   if (avatar->work) avatar->work->avatar = NULL;
   while (avatar->images)
     {
        image = eina_list_data_get(avatar->images);
        _image_detach(avatar, image);
     }
   free(avatar->path);
   free(avatar->missing);
   free(avatar->temp);
   free(avatar->hash);
   free(avatar);
}

static Eina_Bool
_fresh(const char *path, time_t lifetime)
{
   struct stat st;
   time_t now;

   now = time(NULL);
   return !stat(path, &st) && S_ISREG(st.st_mode) && now >= st.st_mtime
          && now - st.st_mtime < lifetime;
}

static void
_download_done(void *data, const char *file, int status)
{
   Avatar *avatar;

   avatar = data;
   avatar->job = NULL;
   if (avatar->timer) ecore_timer_del(avatar->timer);
   avatar->timer = NULL;
   _work_start(avatar, EINA_FALSE, status);
}

static int
_download_progress(void *data, const char *file, long int total, long int now,
                   long int upload_total, long int upload_now)
{
   return total > AVATAR_MAX_BYTES || now > AVATAR_MAX_BYTES;
}

static Eina_Bool
_download_timeout(void *data)
{
   Avatar *avatar;

   avatar = data;
   avatar->timer = NULL;
   if (avatar->job) ecore_file_download_abort(avatar->job);
   return ECORE_CALLBACK_CANCEL;
}

/* Keep disk use bounded too, across restarts. Filenames contain hashes only. */
static void
_cache_prune(const char *dir)
{
   Eina_List *files, *node;
   char *name, *path, *oldest;
   time_t oldest_time, modified;
   unsigned int count;

   files = ecore_file_ls(dir);
   count = 0;
   oldest = NULL;
   oldest_time = time(NULL);
   EINA_LIST_FOREACH (files, node, name)
     {
        path = elaro_path_join(dir, name, NULL);
        if (!_fresh(path, AVATAR_LIFETIME)) unlink(path);
        else
          {
             count++;
             modified = ecore_file_mod_time(path);
             if (!oldest || modified < oldest_time)
               {
                  free(oldest);
                  oldest = strdup(path);
                  oldest_time = modified;
               }
          }
        free(path);
     }
   if (count >= AVATAR_LIMIT && oldest) unlink(oldest);
   free(oldest);
   EINA_LIST_FREE (files, name)
      free(name);
}

static char *
_cache_dir(void)
{
   const char *base;
   char *path;

   base = getenv("XDG_CACHE_HOME");
   if (base && *base == '/') return elaro_path_join(base, "elaro", "avatars", NULL);
   path = elaro_path_join(eina_environment_home_get(), ".cache", "elaro", "avatars", NULL);
   return path;
}

static void
_work_free(Avatar_Work *work)
{
   free(work->path);
   free(work->missing);
   free(work->temp);
   free(work->dir);
   free(work);
}

/* No Evas objects or shared Avatar state are touched by this worker. */
static void
_cache_work(void *data, Ecore_Thread *thread)
{
   Avatar_Work *work;
   struct stat st;
   FILE *marker;
   int fd;

   work = data;
   if (work->prepare)
     {
        if (!ecore_file_mkpath(work->dir)) return;
        _cache_prune(work->dir);
        if (_fresh(work->path, AVATAR_LIFETIME) && !stat(work->path, &st))
          {
             work->ready = EINA_TRUE;
             work->expires = st.st_mtime + AVATAR_LIFETIME;
          }
        else if (_fresh(work->missing, AVATAR_MISSING_LIFETIME) && !stat(work->missing, &st))
           work->expires = st.st_mtime + AVATAR_MISSING_LIFETIME;
        else
          {
             free(work->temp);
             work->temp = elaro_string_printf("%s.XXXXXX", work->path);
             fd = mkstemp(work->temp);
             if (fd >= 0)
               {
                  close(fd);
                  unlink(work->temp);
                  work->download = EINA_TRUE;
               }
          }
     }
   else
     {
        if (work->status == 200 && !stat(work->temp, &st) && st.st_size > 0
            && st.st_size <= AVATAR_MAX_BYTES && !rename(work->temp, work->path))
          {
             unlink(work->missing);
             work->ready = EINA_TRUE;
             work->expires = time(NULL) + AVATAR_LIFETIME;
          }
        else if (work->status == 404)
          {
             marker = fopen(work->missing, "w");
             if (marker) fclose(marker);
             work->expires = time(NULL) + AVATAR_MISSING_LIFETIME;
          }
        else if (work->status == -1) unlink(work->path);
        if (work->temp) unlink(work->temp);
     }
}

static void
_image_show(Avatar *avatar, Evas_Object *image)
{
   evas_object_data_del(image, "avatar-loaded");
   /* Clear the theme layout before asynchronously opening a raster avatar.
    * Elementary otherwise tries to open the avatar as another Edje layout. */
   efl_file_unload(image);
   elm_image_file_set(image, avatar->path, NULL);
}

static void
_images_show(Avatar *avatar)
{
   Eina_List *node;
   Evas_Object *image;

   EINA_LIST_FOREACH (avatar->images, node, image)
      _image_show(avatar, image);
}

static void
_cache_done(void *data, Ecore_Thread *thread)
{
   Avatar_Work *work;
   Avatar *avatar;

   work = data;
   avatar = work->avatar;
   if (avatar)
     {
        avatar->work = NULL;
        avatar->ready = work->ready;
        avatar->expires = work->expires;
        if (work->download)
          {
             free(avatar->temp);
             avatar->temp = work->temp;
             work->temp = NULL;
             _download_start(avatar);
          }
        else if (avatar->ready) _images_show(avatar);
     }
   _work_free(work);
}

static void
_work_start(Avatar *avatar, Eina_Bool prepare, int status)
{
   Avatar_Work *work;

   work = calloc(1, sizeof(Avatar_Work));
   work->avatar = avatar;
   work->path = strdup(avatar->path);
   work->missing = strdup(avatar->missing);
   work->temp = avatar->temp ? strdup(avatar->temp) : NULL;
   work->dir = _cache_dir();
   work->prepare = prepare;
   work->status = status;
   avatar->work = work;
   if (!ecore_thread_run(_cache_work, _cache_done, _cache_done, work))
     {
        avatar->work = NULL;
        _work_free(work);
     }
}

static void
_download_start(Avatar *avatar)
{
   char *url;

   url = elaro_string_printf("https://gravatar.com/avatar/%s?s=96&d=404", avatar->hash);
   if (ecore_file_download(url, avatar->temp, _download_done, _download_progress, avatar,
                           &avatar->job))
      avatar->timer = ecore_timer_add(30, _download_timeout, avatar);
   free(url);
}

void
elaro_ui_avatar_load(Elaro_Ui *ui, const char *email, Evas_Object *image)
{
   Avatar *avatar, *candidate;
   Eina_Iterator *iter;
   char *hash, *dir;

   hash = elaro_avatar_hash_get(email);
   if (!hash) return;
   if (!ui->avatars) ui->avatars = eina_hash_string_superfast_new(_avatar_free);
   avatar = eina_hash_find(ui->avatars, hash);
   if (!avatar)
     {
        if (eina_hash_population(ui->avatars) >= AVATAR_LIMIT)
          {
             candidate = NULL;
             iter = eina_hash_iterator_data_new(ui->avatars);
             EINA_ITERATOR_FOREACH (iter, avatar)
                if (!avatar->job && !avatar->work && !avatar->images)
                  {
                     candidate = avatar;
                     break;
                  }
             eina_iterator_free(iter);
             if (!candidate)
               {
                  free(hash);
                  return;
               }
             eina_hash_del_by_data(ui->avatars, candidate);
          }
        dir = _cache_dir();
        avatar = calloc(1, sizeof(Avatar));
        avatar->hash = strdup(hash);
        avatar->path = elaro_path_join(dir, hash, NULL);
        avatar->missing = elaro_string_printf("%s.missing", avatar->path);
        free(dir);
        eina_hash_add(ui->avatars, hash, avatar);
     }
   /* An image can be updated by WHOIS more than once. Detach its old lookup. */
   candidate = evas_object_data_get(image, "avatar-record");
   if (candidate != avatar)
     {
        if (candidate)
          {
             _image_detach(candidate, image);
          }
        evas_object_data_set(image, "avatar-record", avatar);
        avatar->images = eina_list_append(avatar->images, image);
        evas_object_event_callback_add(image, EVAS_CALLBACK_DEL, _image_deleted, avatar);
        elm_image_async_open_set(image, EINA_TRUE);
        elm_image_prescale_set(image, 96);
        evas_object_smart_callback_add(image, "load,error", _image_error, NULL);
        evas_object_smart_callback_add(image, "load,open", _image_opened, NULL);
     }
   if (avatar->ready && avatar->expires > time(NULL)) _image_show(avatar, image);
   else if (!avatar->job && !avatar->work && avatar->expires <= time(NULL)
            && (!avatar->attempted || time(NULL) - avatar->attempted >= 60))
     {
        avatar->attempted = time(NULL);
        _work_start(avatar, EINA_TRUE, 0);
     }
   free(hash);
}

void
elaro_ui_avatars_shutdown(Elaro_Ui *ui)
{
   eina_hash_free(ui->avatars);
   ui->avatars = NULL;
}
