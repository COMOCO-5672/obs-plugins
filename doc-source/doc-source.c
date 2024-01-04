#include <obs-module.h>
#include <graphics/image-file.h>
#include <util/platform.h>
#include <util/dstr.h>
#include <sys/stat.h>
#include <obs.h>

#include <util/threading.h>
#include <pthread.h>
#include "implement.h"

#define blog(log_level, format, ...)                    \
	blog(log_level, "[doc_source: '%s'] " format, \
	     obs_source_get_name(context->source), ##__VA_ARGS__)

#define debug(format, ...) blog(LOG_DEBUG, format, ##__VA_ARGS__)
#define info(format, ...) blog(LOG_INFO, format, ##__VA_ARGS__)
#define warn(format, ...) blog(LOG_WARNING, format, ##__VA_ARGS__)

typedef struct gs_document_file {
    gs_texture_t *texture;
    enum gs_color_format format;
    uint32_t width;
    uint32_t height;
};

typedef struct doc_size {
    int32_t width;
    int32_t height;
} doc_size_t;

typedef struct document_source_t {
    obs_source_t *source;

    char *file;
    char *doc_id;
    int32_t cur_page_index;
    int32_t total_page_num;

    struct vec4 color;
    struct vec4 color_srgb;

    struct gs_document_file document_tex;
    struct obs_source_frame *frame_cache;

    enum gs_color_format color_format;

    struct pthread_mutex_t_ *mutex;

};


static time_t get_modified_timestamp(const char *filename)
{
    struct stat stats;
    if (os_stat(filename, &stats) != 0)
        return -1;
    return stats.st_mtime;
}

static const char *doc_source_get_name(void *unused)
{
    UNUSED_PARAMETER(unused);
    return obs_module_text("DocumentInput");
}

static void doc_source_update(void *data, obs_data_t *settings)
{
    struct document_source_t *context = data;

    const char *file_name = obs_data_get_string(settings, "file_name");
    const char *doc_id = obs_data_get_string(settings, "doc_id");
    const int32_t doc_width = obs_data_get_int(settings, "doc_width");
    const int32_t doc_height = obs_data_get_int(settings, "doc_height");

    if (strlen(file_name))
        context->file = file_name;

    if (strlen(doc_id))
        context->doc_id = doc_id;

    context->file = file_name;
    context->doc_id = doc_id;
}

static void doc_source_defaults(obs_data_t *settings)
{
    obs_data_set_default_bool(settings, "unload", false);
    obs_data_set_default_bool(settings, "linear_alpha", false);
}

static void doc_source_show(void *data)
{
    UNUSED_PARAMETER(data);
}

static void doc_source_hide(void *data)
{
    struct document_source_t *context = data;
}

static void *doc_source_create(obs_data_t *settings, obs_source_t *source)
{
    struct document_source_t *context = bzalloc(sizeof(struct document_source_t));
    context->source = source;
    if (pthread_mutex_init(&context->mutex, NULL) != 0) {
        warn("init thread mutex error");
    }
    doc_source_update(context, settings);
    return context;
}

static void doc_source_destroy(void *data)
{
    struct document_source_t *context = data;
    if (context) {

        if (pthread_mutex_destroy(&context->mutex) != 0)
            warn("destroy thread mutex error");

        if (context->frame_cache)
            obs_source_frame_destroy(context->frame_cache);

        if (context->document_tex.texture)
            gs_texture_destroy(context->document_tex.texture);

    }

    bfree(context);
}

static uint32_t doc_source_getwidth(void *data)
{
    struct document_source_t *context = data;
    return context ? context->document_tex.width : 0;
}

static uint32_t doc_source_getheight(void *data)
{
    struct document_source_t *context = data;
    return context ? context->document_tex.height : 0;
}

static void doc_source_render(void *data, gs_effect_t *effect)
{
    struct document_source_t *context = data;
    bool need_create_tex = false, reset_tex = false;

    if (context) {
        reset_tex = !context->document_tex.texture || (context->document_tex.width != context->frame_cache->width
            || context->document_tex.height != context->frame_cache->height);
    }

    if (reset_tex && context->frame_cache) {
        if (context->document_tex.texture) {
            gs_texture_destroy(context->document_tex.texture);
        }

        pthread_mutex_lock(&context->mutex);

        context->document_tex.texture = gs_texture_create(context->frame_cache->width
            , context->frame_cache->height
            , GS_BGRA
            , 1
            , NULL
            , 0);

        pthread_mutex_unlock(&context->mutex);
    }

    if (!context->document_tex.texture)
        return;

    context->document_tex.width = context->frame_cache->width;
    context->document_tex.height = context->frame_cache->height;
    context->document_tex.format = GS_BGRA;

    pthread_mutex_lock(&context->mutex);

    gs_texture_t *tmp_texture = gs_texture_create(context->frame_cache->width
        , context->frame_cache->height
        , GS_BGRA
        , 1
        , &context->frame_cache->data[0]
        , 0);

    gs_copy_texture_region(context->document_tex.texture, 0, 0
        , tmp_texture, 0, 0, context->document_tex.width,
        context->document_tex.height);

    gs_texture_destroy(tmp_texture);

    pthread_mutex_unlock(&context->mutex);

    const bool previous = gs_framebuffer_srgb_enabled();
    gs_enable_framebuffer_srgb(true);

    gs_blend_state_push();
    gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);

    gs_eparam_t *const param = gs_effect_get_param_by_name(effect, "image");
    gs_effect_set_texture_srgb(param, context->document_tex.texture);

    gs_draw_sprite(context->document_tex.texture, 0,
        context->document_tex.width,
        context->document_tex.height);

    gs_blend_state_pop();

    gs_enable_framebuffer_srgb(previous);
}

static void doc_source_tick(void *data, float seconds)
{
    return;
    struct document_source_t *context = data;
    uint64_t frame_time = obs_get_video_frame_time();
}

static obs_properties_t *doc_source_properties(void *data)
{
    struct document_source_t *s = data;
    struct dstr path = { 0 };

    obs_properties_t *props = obs_properties_create();

    return props;
}

static void missing_file_callback(void *src, const char *new_path, void *data)
{
    struct document_source_t *s = src;

    obs_source_t *source = s->source;
    obs_data_t *settings = obs_source_get_settings(source);
    obs_data_set_string(settings, "file", new_path);
    obs_source_update(source, settings);
    obs_data_release(settings);

    UNUSED_PARAMETER(data);
}

static obs_missing_files_t *doc_source_missingfiles(void *data)
{
    struct document_source_t *s = data;
    obs_missing_files_t *files = obs_missing_files_create();

    if (strcmp(s->file, "") != 0) {
        if (!os_file_exists(s->file)) {
            obs_missing_file_t *file = obs_missing_file_create(
                s->file, missing_file_callback,
                OBS_MISSING_FILE_SOURCE, s->source, NULL);

            obs_missing_files_add_file(files, file);
        }
    }

    return files;
}

static void doc_source_set_video_frame(void *data, struct obs_source_frame *frame)
{
    struct document_source_t *context = data;
    if (!context || !frame)
        return;

    pthread_mutex_lock(&context->mutex);
    if (context->frame_cache && (context->frame_cache->width != frame->width
        || context->frame_cache->height != frame->height)) {
        info("change frame, width:%u, height:%u."
            , frame->width
            , frame->height);
        obs_source_frame_destroy(context->frame_cache);
        context->frame_cache = NULL;
    }

    if (!context->frame_cache) {
        info("change frame, alloc memory size, width:%u, height:%u.", frame->width, frame->height);
        context->frame_cache = obs_source_frame_create(frame->format, frame->width, frame->height);
    }

    memmove(context->frame_cache->data[0], frame->data[0], frame->width * frame->height * 4);
    context->color_format = GS_BGRA;
    pthread_mutex_unlock(&context->mutex);
}

static struct obs_source_info doc_source_info = {
    .id = "document_source",
    .type = OBS_SOURCE_TYPE_INPUT,
    .output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_SRGB,
    .get_name = doc_source_get_name,
    .create = doc_source_create,
    .destroy = doc_source_destroy,
    .update = doc_source_update,
    .get_defaults = doc_source_defaults,
    .show = doc_source_show,
    .hide = doc_source_hide,
    .get_width = doc_source_getwidth,
    .get_height = doc_source_getheight,
    .video_render = doc_source_render,
    .video_tick = doc_source_tick,
    .missing_files = doc_source_missingfiles,
    .get_properties = doc_source_properties,
    .set_video_frame = doc_source_set_video_frame,
    .icon_type = OBS_ICON_TYPE_CUSTOM,
};

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("doc-source", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
    return "document sources";
}

bool obs_module_load(void)
{
    obs_register_source(&doc_source_info);

    return true;
}
