#include "obs-module.h"
#include "obs.h"
#include "obs-source.h"
#include "source-manager.h"
#include "drawing-source.h"
#include <string>

#include "zmath.h"

#define blog(log_level, format, ...)                    \
	blog(log_level, "[draw_source: '%s'] " format, \
	     obs_source_get_name(context->source), ##__VA_ARGS__)

#define debug(format, ...) blog(LOG_DEBUG, format, ##__VA_ARGS__)
#define info(format, ...) blog(LOG_INFO, format, ##__VA_ARGS__)
#define warn(format, ...) blog(LOG_WARNING, format, ##__VA_ARGS__)

enum draw_type {
    DRAW_PEN,
    DRAW_RECT,
    DRAW_LINE,
    DRAW_TEXT,
    DRAW_CLEAR,
};

static bool draw_info_changed(void *data, obs_properties_t *props)
{
    const auto context = reinterpret_cast<SourceManager *>(data);
    if (!context)
        return false;

    obs_data_set_string(obs_source_get_settings(context->source), "current_key", context->GetCurrentKey().c_str());

    obs_property_t *p = obs_properties_get(props, "PAGE");

    if (p) {
        obs_property_list_clear(p);
        p = obs_properties_add_list(props, "PAGE", obs_module_text("draw"),
            OBS_COMBO_TYPE_LIST,
            OBS_COMBO_FORMAT_STRING);
    }

    const auto key_info = context->GetKeyInfo();
    for (auto &key : key_info)
        obs_property_list_add_string(p, key.first.c_str(), std::to_string(key.second).c_str());

    return false;
}

static void mis_setup_line(draw_line_t *line)
{
    if (line->base.width <= 0)
        return;

    line->base.buf = NULL;
    bool is_x_equal = line->start_x == line->end_x;
    bool is_y_equal = line->start_y == line->end_y;

    if (is_x_equal == 0 && is_y_equal == 0) {
        float k1 = ((float)line->start_y - (float)line->end_y) / ((float)line->start_x - (float)line->end_x);
        float k2 = -1 / k1;
        float b = line->end_y - k1 * line->end_x;
        float b2;
        int x_start;
        int x_end;
        int y_start;
        int y_end;

        if (line->start_x > line->end_x) {
            x_start = line->end_x;
            x_end = line->start_x;
            y_start = line->end_y;
            y_end = line->start_y;
        }
        else {
            x_start = line->start_x;
            x_end = line->end_x;
            y_start = line->start_y;
            y_end = line->end_y;
        }
        b2 = y_start - k2 * x_start; //!  垂线方程 y=k2 * x + b2;
        float b2_2 = y_end - k2 * x_end;

        int x_offset = x_end - x_start;
        int y_offset = y_end - y_start;

        float a = (float)line->base.width * cos(atanf(k1)); //! atanf 获取线宽垂直高度

        float y1 = y_start - a / 2;
        float y2 = y_start + a / 2;

        if (y1 > y2) {
            mis_swapf(&y1, &y2);
        }
        float x1 = (float)(y1 - b2) / k2;
        float x2 = (float)(y2 - b2) / k2;

        //! 
        float y3 = y_end - a / 2;
        float y4 = y_end + a / 2;

        if (y3 > y4) {
            mis_swapf(&y3, &y4);
        }
        float x3 = (float)(y3 - b2_2) / k2;
        float x4 = (float)(y4 - b2_2) / k2;

        gs_vertex2f(x2, y2);
        gs_vertex2f(x1, y1);
        gs_vertex2f(x3, y3);
        gs_vertex2f(x2, y2);
        gs_vertex2f(x3, y3);
        gs_vertex2f(x4, y4);

    }
    else if (is_x_equal && is_y_equal) {
        gs_vertex2f(line->start_x, line->end_x);
    }
    else if (is_x_equal) {
        float x_start = (float)line->start_x - (float)line->base.width / 2;
        float x_end = (float)x_start + (float)line->base.width;
        float y_start = line->start_y;
        float y_end = line->end_y;
        if (y_start > y_end)
            mis_swapf(&y_start, &y_end);

        gs_vertex2f(x_start, y_end);
        gs_vertex2f(x_start, y_start);
        gs_vertex2f(x_end, y_end);
        gs_vertex2f(x_end, y_start);
    }
    else if (is_y_equal) {
        float x_start = line->start_x;
        float x_end = line->end_x;
        float y_start = (float)line->start_y - (float)line->base.width / 2;
        float y_end = y_start + (float)line->base.width;
        if (x_start > x_end)
            mis_swapf(&x_start, &x_end);

        gs_vertex2f(x_start, y_end);
        gs_vertex2f(x_start, y_start);
        gs_vertex2f(x_end, y_end);
        gs_vertex2f(x_end, y_start);

    }
}

static const char *draw_source_get_name(void *unused)
{
    UNUSED_PARAMETER(unused);
    return obs_module_text("DrawingInput");
}

static void *draw_source_create(obs_data_t *settings, obs_source_t *source)
{
    UNUSED_PARAMETER(settings);
    const auto context = new SourceManager(source);
    return context;
}

static void draw_source_update(void *data, obs_data_t *settings)
{
    auto *context = (SourceManager *)data;

    if (!context)
        return;

    struct obs_video_info ovi;
    if (obs_get_video_info(&ovi))
        context->UpdateCanvasSize(ovi.base_width, ovi.base_height);

    std::string key = obs_data_get_string(settings, "key");

    if (!context->HasKey(key)) {
        if (!context->AddKey(key))
            return;
    }
    context->SetCurrentKey(key);

    draw_info_changed(data, context->props);
}

static void draw_source_show(void *data)
{
    UNUSED_PARAMETER(data);
}

static void draw_source_hide(void *data)
{
    UNUSED_PARAMETER(data);
}

static uint32_t draw_source_get_width(void *data)
{
    UNUSED_PARAMETER(data);
    obs_video_info ovi;
    if (obs_get_video_info(&ovi))
        return ovi.base_width;

    return -1;
}

static uint32_t draw_source_get_height(void *data)
{
    UNUSED_PARAMETER(data);
    obs_video_info ovi;
    if (obs_get_video_info(&ovi))
        return ovi.base_height;

    return -1;
}

static obs_properties_t *draw_source_properties(void *data)
{
    const auto context = reinterpret_cast<SourceManager *>(data);

    if (!context)
        return nullptr;

    obs_properties_t *props = obs_properties_create();
    context->props = props;

    draw_info_changed(data, props);

    return props;
}

static void draw_source_render(void *data, gs_effect_t *effect, bool is_display)
{
    const auto context = reinterpret_cast<SourceManager *>(data);
    if (!context)
        return;

    const auto texture = context->GetCurrentPageTexture();
    if (!texture)
        return;

    gs_texrender_reset(texture->texrender);
    gs_technique_t *tech = gs_effect_get_technique(effect, "Draw");
    gs_eparam_t *image = gs_effect_get_param_by_name(effect, "image");
    size_t      passes;
    gs_texture_t *tex = gs_texrender_get_texture(texture->texrender);
    //printf( "texture target: %p\n", tex);
    gs_effect_set_texture(image, tex);
    passes = gs_technique_begin(tech);
    for (size_t i = 0; i < passes; i++) {
        if (gs_technique_begin_pass(tech, i)) {
            gs_draw_sprite(tex
                , 0
                , std::get<0>(context->GetCanvasSize())
                , std::get<1>(context->GetCanvasSize()));
            gs_technique_end_pass(tech);
        }
    }
    gs_technique_end(tech);
}

static void draw_source_tick(void *data, float seconds)
{
    return;
}

static void draw_source_destroy(void *data)
{
    const auto context = (SourceManager *)data;

    delete context;
}

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("drawing-source", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
    return "drawing source";
}

static void draw_canvas_data(void *data, int32_t x, int32_t y, bool pressed, bool moving, bool released, uint32_t color, int shapeType, int size)
{
    if (!data)
        return;

    const auto context = reinterpret_cast<SourceManager *>(data);

    static int index = 0;
    gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);

    gs_eparam_t *effectcolor = gs_effect_get_param_by_name(solid, "color");
    gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");

    unsigned int rgba = color;
    struct vec4 colorVal;
    vec4_set(&colorVal, (float)mis_get_rgba_r(rgba) / 0xff,
        (float)mis_get_rgba_g(rgba) / 0xff,
        (float)mis_get_rgba_b(rgba) / 0xff,
        (float)mis_get_rgba_a(rgba) / 0xff);

    gs_drawing_texture *draw_texture = context->GetCurrentPageTexture();
    if (!draw_texture)
        return;

    obs_enter_graphics();

    struct obs_video_info ovi;
    obs_get_video_info(&ovi);

    int twidth = (int)ovi.base_width;
    int theight = (int)ovi.base_height;
    //mis->lineWidth = theight / size;
    context->SetLineWidth(10);

    gs_ortho(0.0f, twidth, 0.0f, theight,
        -100.0f, 100.0f);

    gs_texrender_reset(draw_texture->texrender);
    gs_effect_set_vec4(effectcolor, &colorVal);

    gs_texrender_begin(draw_texture->texrender
        , std::get<0>(context->GetCanvasSize())
        , std::get<1>(context->GetCanvasSize()));

    gs_technique_begin(tech);
    gs_technique_begin_pass(tech, 0);
    gs_render_start(false);

    z_point p;
    p.x = x;
    p.y = y;

    static z_point r_1_3 = { INT_MIN,INT_MIN };
    static z_point r_1_4 = { INT_MIN,INT_MIN };
    static z_point r_2_2 = { INT_MIN,INT_MIN };
    static z_point r_2_1 = { INT_MIN,INT_MIN };

    if (!draw_texture->point_array)
        draw_texture->point_array = z_new_fpoint_array(24, 1.0f, 0.18f);

    static bool b_paint = true;
    if (pressed && context) {
        switch (shapeType) {
        case DRAW_LINE:
        {
            draw_texture->line.base.rgba = color;
            draw_texture->line.base.width = context->GetLineWidth();
            if (!draw_texture->tmp_render) {
                gs_texrender_destroy(draw_texture->tmp_render);
                draw_texture->tmp_render = NULL;
            }

            draw_texture->tmp_render = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
            gs_texrender_reset(draw_texture->tmp_render);
            gs_texrender_begin(draw_texture->tmp_render
                , std::get<0>(context->GetCanvasSize())
                , std::get<1>(context->GetCanvasSize()));

            gs_texture_t *rendTargat = gs_texrender_get_texture(draw_texture->texrender);
            draw_texture->copy_texture = gs_texrender_get_texture(draw_texture->tmp_render);

            gs_copy_texture(draw_texture->copy_texture, rendTargat);
            gs_texrender_end(draw_texture->tmp_render);

            draw_texture->line.start_x = x;
            draw_texture->line.start_y = y;

            break;
        }
        default:
            break;
        }
    }

    if (moving && draw_texture->point_array) {
        switch (shapeType) {
        case DRAW_LINE:
        {
            draw_texture->line.end_x = x;
            draw_texture->line.end_y = y;

            gs_texrender_reset(draw_texture->tmp_render);
            gs_texrender_begin(draw_texture->tmp_render
                , std::get<0>(context->GetCanvasSize())
                , std::get<1>(context->GetCanvasSize()));

            gs_texture_t *render_targat = gs_texrender_get_texture(draw_texture->texrender);
            draw_texture->copy_texture = gs_texrender_get_texture(draw_texture->tmp_render);
            gs_copy_texture(render_targat, draw_texture->copy_texture);
            gs_texrender_end(draw_texture->tmp_render);
            mis_setup_line(&draw_texture->line);

            break;
        }
        default:
            break;
        }

    }
    if (released) {
        r_1_3.x = INT_MIN;
        r_1_3.y = INT_MIN;
        r_1_4.x = INT_MIN;
        r_1_4.y = INT_MIN;
        r_2_2.x = INT_MIN;
        r_2_2.y = INT_MIN;
        r_2_1.x = INT_MIN;
        r_2_1.y = INT_MIN;

        switch (shapeType) {
        case DRAW_LINE:
        {
            gs_texrender_destroy(draw_texture->tmp_render);
            draw_texture->tmp_render = NULL;
            draw_texture->copy_texture = NULL;
            break;
        }
        default:
            break;
        }
    }

    gs_render_stop(GS_TRISTRIP);

    gs_technique_end_pass(tech);
    gs_technique_end(tech);
    gs_texrender_end(draw_texture->texrender);
    obs_leave_graphics();
}

static void draw_page_change(void *data, int32_t page_index)
{
    const auto context = reinterpret_cast<SourceManager *>(data);
    if (!context || page_index < 0)
        return;

    context->SetCurrentPage(page_index);

	draw_info_changed(data, context->props);
}

bool obs_module_load(void)
{
    obs_source_info drawing_source_info = { };
    drawing_source_info.id = "drawing_source";
    drawing_source_info.type = OBS_SOURCE_TYPE_INPUT;
    drawing_source_info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_SRGB;
    drawing_source_info.get_name = draw_source_get_name;
    drawing_source_info.create = draw_source_create;
    drawing_source_info.destroy = draw_source_destroy;
    drawing_source_info.update = draw_source_update;
    drawing_source_info.show = draw_source_show;
    drawing_source_info.hide = draw_source_hide;
    drawing_source_info.get_properties = draw_source_properties;
    drawing_source_info.video_tick = draw_source_tick;
    drawing_source_info.video_render = draw_source_render;
    drawing_source_info.get_width = draw_source_get_width;
    drawing_source_info.get_height = draw_source_get_height;
    drawing_source_info.icon_type = OBS_ICON_TYPE_CUSTOM;
    drawing_source_info.draw_page_change = draw_page_change;
    drawing_source_info.set_canvas_data = draw_canvas_data;
    obs_register_source(&drawing_source_info);

    return true;
}



