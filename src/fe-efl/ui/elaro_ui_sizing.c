#include "elaro_ui_util.h"
#include <Elementary.h>

/* Keep application dimensions in logical units. Toolkit-calculated minimums
 * must never be rescaled here; only explicitly registered hints are ours. */
typedef struct {
   int min[2], max[2], padding[4], box[2], wrap;
   unsigned int flags;
   Evas_Object *obj, *win;
   Ecore_Job *job;
} Elaro_Sizing;

static int
_scaled(int value)
{
   return value > 0 ? ELM_SCALE_SIZE(value) : value;
}

static void
_sizing_apply(void *data, Evas_Object *obj, void *info)
{
   Elaro_Sizing *s = data;
   /* Legacy min_set writes the widget's restricted minimum, which its next
    * sizing pass overwrites. User minimums survive and combine with it. */
   if (s->flags & 1) efl_gfx_hint_size_min_set(obj,
       EINA_SIZE2D(_scaled(s->min[0]), _scaled(s->min[1])));
   if (s->flags & 2) evas_object_size_hint_max_set(obj, _scaled(s->max[0]), _scaled(s->max[1]));
   if (s->flags & 4) evas_object_size_hint_padding_set(obj, _scaled(s->padding[0]), _scaled(s->padding[1]),
                                                       _scaled(s->padding[2]), _scaled(s->padding[3]));
   if (s->flags & 8) elm_box_padding_set(obj, _scaled(s->box[0]), _scaled(s->box[1]));
   if (s->flags & 16) elm_label_wrap_width_set(obj, _scaled(s->wrap));
}

/* Entries recalculate their own minimum after text and focus changes. Keep
 * dimensions explicitly fixed by the application stable through that pass. */
static void
_sizing_hints_changed(void *data, Evas *evas, Evas_Object *obj, void *info)
{
   Elaro_Sizing *s = data;
   if ((s->flags & 3) != 3) return;
   int minw, minh, maxw, maxh;
   evas_object_size_hint_min_get(obj, &minw, &minh);
   evas_object_size_hint_max_get(obj, &maxw, &maxh);
   int w = s->min[0] > 0 && s->min[0] == s->max[0] ? _scaled(s->min[0]) : 0;
   int h = s->min[1] > 0 && s->min[1] == s->max[1] ? _scaled(s->min[1]) : 0;
   if ((w && minw != w) || (h && minh != h))
     evas_object_size_hint_min_set(obj, w ? w : minw, h ? h : minh);
   if ((w && maxw != w) || (h && maxh != h))
     evas_object_size_hint_max_set(obj, w ? w : maxw, h ? h : maxh);
}

static void
_sizing_job(void *data)
{
   Elaro_Sizing *s = data;
   s->job = NULL;
   _sizing_apply(s, s->obj, NULL);
}

static void
_sizing_changed(void *data, Evas_Object *obj, void *info)
{
   Elaro_Sizing *s = data;
   if (!s->job) s->job = ecore_job_add(_sizing_job, s);
}

static void
_sizing_free(void *data, Evas *evas, Evas_Object *obj, void *info)
{
   Elaro_Sizing *s = data;
   if (s->job) ecore_job_del(s->job);
   if (s->win) evas_object_smart_callback_del_full(s->win, "theme,changed", _sizing_changed, s);
   free(s);
}

static Elaro_Sizing *
_sizing_get(Evas_Object *obj)
{
   Elaro_Sizing *s = evas_object_data_get(obj, "elaro-sizing");
   if (s) return s;
   s = calloc(1, sizeof(*s));
   if (!s) return NULL;
   evas_object_data_set(obj, "elaro-sizing", s);
   s->obj = obj;
   s->win = elm_win_get(obj);
   /* Images and boxes do not emit layout theme changes. The window does,
    * including for elm_config_scale_set(); wait until its children finish. */
   if (s->win) evas_object_smart_callback_add(s->win, "theme,changed", _sizing_changed, s);
   evas_object_event_callback_add(obj, EVAS_CALLBACK_DEL, _sizing_free, s);
   evas_object_event_callback_add(obj, EVAS_CALLBACK_CHANGED_SIZE_HINTS, _sizing_hints_changed, s);
   return s;
}

void
elaro_ui_fixed_height_set(Evas_Object *obj, int height)
{
   Elaro_Sizing *s = _sizing_get(obj);
   if (!s) return;
   if (!(s->flags & 2)) s->max[0] = -1;
   s->min[1] = s->max[1] = height;
   s->flags |= 3;
   _sizing_apply(s, obj, NULL);
}

void
elaro_ui_size_min_set(Evas_Object *obj, int w, int h)
{
   Elaro_Sizing *s = _sizing_get(obj);
   if (!s) return;
   s->min[0] = w; s->min[1] = h; s->flags |= 1;
   _sizing_apply(s, obj, NULL);
}

void
elaro_ui_size_max_set(Evas_Object *obj, int w, int h)
{
   Elaro_Sizing *s = _sizing_get(obj);
   if (!s) return;
   s->max[0] = w; s->max[1] = h; s->flags |= 2;
   _sizing_apply(s, obj, NULL);
}

void
elaro_ui_padding_set(Evas_Object *obj, int l, int r, int t, int b)
{
   Elaro_Sizing *s = _sizing_get(obj);
   if (!s) return;
   s->padding[0] = l; s->padding[1] = r; s->padding[2] = t; s->padding[3] = b; s->flags |= 4;
   _sizing_apply(s, obj, NULL);
}

void
elaro_ui_box_padding_set(Evas_Object *obj, int x, int y)
{
   Elaro_Sizing *s = _sizing_get(obj);
   if (!s) return;
   s->box[0] = x; s->box[1] = y; s->flags |= 8;
   _sizing_apply(s, obj, NULL);
}

void
elaro_ui_wrap_width_set(Evas_Object *obj, int width)
{
   Elaro_Sizing *s = _sizing_get(obj);
   if (!s) return;
   s->wrap = width; s->flags |= 16;
   _sizing_apply(s, obj, NULL);
}

