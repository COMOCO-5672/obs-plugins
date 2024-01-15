#include "obs-module.h"
#include "obs-source.h"
#include "source-manager.h"
#include "drawing-source.h"
#include <string>
#include "pthread.h"
#include "zmath.h"
#include "graphics/matrix4.h"
#include "obs.h"

#define blog(log_level, format, ...)                    \
	blog(log_level, "[draw_source: '%s'] " format, \
	     obs_source_get_name(context->source), ##__VA_ARGS__)

#define debug(format, ...) blog(LOG_DEBUG, format, ##__VA_ARGS__)
#define info(format, ...) blog(LOG_INFO, format, ##__VA_ARGS__)
#define warn(format, ...) blog(LOG_WARNING, format, ##__VA_ARGS__)

enum draw_type {
    DRAW_NONE = 0,
    DRAW_PEN = 1,
    DRAW_CIRCLE = 2,
    DRAW_RECT = 3,
    DRAW_LINE = 4,
    DRAW_TEXT = 9,
    DRAW_CLEAR = 10,
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

static void mis_get_this_line_start_point(draw_line_t *line, z_point *r_2_2, z_point *r_2_1)
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

        r_2_2->x = x1;
        r_2_2->y = y1;
        r_2_1->x = x2;
        r_2_1->y = y2;
    }
    else if (is_x_equal) {
        float x_start = (float)line->start_x - (float)line->base.width / 2;
        float x_end = (float)x_start + (float)line->base.width;
        float y_start = line->start_y;
        float y_end = line->end_y;
        if (y_start > y_end)
            mis_swapf(&y_start, &y_end);
        r_2_2->x = x_start;
        r_2_2->y = y_start;
        r_2_1->x = x_start;
        r_2_1->y = y_end;
    }
    else if (is_y_equal) {
        float x_start = line->start_x;
        float x_end = line->end_x;
        float y_start = (float)line->start_y - (float)line->base.width / 2;
        float y_end = y_start + (float)line->base.width;
        if (x_start > x_end)
            mis_swapf(&x_start, &x_end);

        r_2_2->x = x_start;
        r_2_2->y = y_start;
        r_2_1->x = x_end;
        r_2_1->y = y_start;
    }
}

static void mis_get_last_line_end_point(draw_line_t *line, z_point *r_1_3, z_point *r_1_4)
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

        r_1_3->x = x3;
        r_1_3->y = y3;
        r_1_4->x = x4;
        r_1_4->y = y4;
    }

    else if (is_x_equal) {
        float x_start = (float)line->start_x - (float)line->base.width / 2;
        float x_end = (float)x_start + (float)line->base.width;
        float y_start = line->start_y;
        float y_end = line->end_y;
        if (y_start > y_end)
            mis_swapf(&y_start, &y_end);

        r_1_3->x = x_end;
        r_1_3->y = y_start;
        r_1_4->x = x_end;
        r_1_4->y = y_end;

    }
    else if (is_y_equal) {
        float x_start = line->start_x;
        float x_end = line->end_x;
        float y_start = (float)line->start_y - (float)line->base.width / 2;
        float y_end = y_start + (float)line->base.width;
        if (x_start > x_end)
            mis_swapf(&x_start, &x_end);

        r_1_3->x = x_end;
        r_1_3->y = y_start;
        r_1_4->x = x_end;
        r_1_4->y = y_end;

    }
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

static void mis_setup_rectangle(draw_rect_t *rect)
{
    int width_ex = rect->base.width / 2;
    draw_line_t line;

    line.base.rgba = rect->base.rgba;
    line.base.width = rect->base.width;
    line.start_x = rect->x - width_ex;
    line.start_y = rect->y;
    line.end_x = rect->x + rect->width + width_ex;
    line.end_y = rect->y;
    z_point leftTopPoint;
    z_point rightTopPoint;
    z_point leftBottomPoint;
    z_point rightBottomPoint;

    leftTopPoint.x = line.start_x;
    leftTopPoint.y = line.start_y;
    rightTopPoint.x = line.start_x + rect->width;
    rightTopPoint.y = line.start_y;
    leftBottomPoint.x = line.start_x;
    leftBottomPoint.y = line.start_y + rect->height;
    rightBottomPoint.x = line.start_x + rect->width;
    rightBottomPoint.y = line.start_y + rect->height;

    z_point p1_1, p1_2, p1_3, p1_4;
    p1_1.x = leftTopPoint.x - width_ex;
    p1_1.y = leftTopPoint.y - width_ex;
    p1_2.x = leftTopPoint.x + width_ex;
    p1_2.y = leftTopPoint.y - width_ex;
    p1_3.x = leftTopPoint.x + width_ex;
    p1_3.y = leftTopPoint.y + width_ex;
    p1_4.x = leftTopPoint.x - width_ex;
    p1_4.y = leftTopPoint.y + width_ex;

    z_point p2_1, p2_2, p2_3, p2_4;
    p2_1.x = rightTopPoint.x - width_ex;
    p2_1.y = rightTopPoint.y - width_ex;
    p2_2.x = rightTopPoint.x + width_ex;
    p2_2.y = rightTopPoint.y - width_ex;
    p2_3.x = rightTopPoint.x + width_ex;
    p2_3.y = rightTopPoint.y + width_ex;
    p2_4.x = rightTopPoint.x - width_ex;
    p2_4.y = rightTopPoint.y + width_ex;

    z_point p3_1, p3_2, p3_3, p3_4;
    p3_1.x = rightBottomPoint.x - width_ex;
    p3_1.y = rightBottomPoint.y - width_ex;
    p3_2.x = rightBottomPoint.x + width_ex;
    p3_2.y = rightBottomPoint.y - width_ex;
    p3_3.x = rightBottomPoint.x + width_ex;
    p3_3.y = rightBottomPoint.y + width_ex;
    p3_4.x = rightBottomPoint.x - width_ex;
    p3_4.y = rightBottomPoint.y + width_ex;

    z_point p4_1, p4_2, p4_3, p4_4;
    p4_1.x = leftBottomPoint.x - width_ex;
    p4_1.y = leftBottomPoint.y - width_ex;
    p4_2.x = leftBottomPoint.x + width_ex;
    p4_2.y = leftBottomPoint.y - width_ex;
    p4_3.x = leftBottomPoint.x + width_ex;
    p4_3.y = leftBottomPoint.y + width_ex;
    p4_4.x = leftBottomPoint.x - width_ex;
    p4_4.y = leftBottomPoint.y + width_ex;
    z_point points[16];
    points[0] = p1_1; points[1] = p1_2; points[2] = p1_3; points[3] = p1_4;
    points[4] = p2_1; points[5] = p2_2; points[6] = p2_3; points[7] = p2_4;
    points[8] = p3_1; points[9] = p3_2; points[10] = p3_3; points[11] = p3_4;
    points[12] = p4_1; points[13] = p4_2; points[14] = p4_3; points[15] = p4_4;

    gs_vertex2f(p1_1.x, p1_1.y); gs_vertex2f(p1_2.x, p1_2.y); gs_vertex2f(p1_4.x, p1_4.y);

    gs_vertex2f(p1_2.x, p1_2.y); gs_vertex2f(p1_4.x, p1_4.y); gs_vertex2f(p1_3.x, p1_3.y);

    gs_vertex2f(p1_2.x, p1_2.y);	gs_vertex2f(p1_3.x, p1_3.y); gs_vertex2f(p2_1.x, p2_1.y);

    gs_vertex2f(p2_1.x, p2_1.y); gs_vertex2f(p1_3.x, p1_3.y); gs_vertex2f(p2_4.x, p2_4.y);

    gs_vertex2f(p2_1.x, p2_1.y);	gs_vertex2f(p2_4.x, p2_4.y); gs_vertex2f(p2_2.x, p2_2.y);

    gs_vertex2f(p2_2.x, p2_2.y);	gs_vertex2f(p2_4.x, p2_4.y); gs_vertex2f(p2_3.x, p2_3.y);

    gs_vertex2f(p2_3.x, p2_3.y);	gs_vertex2f(p2_4.x, p2_4.y); gs_vertex2f(p2_1.x, p2_1.y);

    gs_vertex2f(p2_3.x, p2_3.y);	gs_vertex2f(p2_4.x, p2_4.y); gs_vertex2f(p3_1.x, p3_1.y);

    gs_vertex2f(p2_3.x, p2_3.y);	gs_vertex2f(p3_1.x, p3_1.y); gs_vertex2f(p3_2.x, p3_2.y);

    gs_vertex2f(p3_2.x, p3_2.y); gs_vertex2f(p3_1.x, p3_1.y); gs_vertex2f(p3_3.x, p3_3.y);

    gs_vertex2f(p3_1.x, p3_1.y); gs_vertex2f(p3_4.x, p3_4.y); gs_vertex2f(p3_3.x, p3_3.y);

    gs_vertex2f(p3_1.x, p3_1.y); gs_vertex2f(p4_2.x, p4_2.y); gs_vertex2f(p3_4.x, p3_4.y);

    gs_vertex2f(p3_4.x, p3_4.y); gs_vertex2f(p4_2.x, p4_2.y); gs_vertex2f(p4_3.x, p4_3.y);

    gs_vertex2f(p4_2.x, p4_2.y); gs_vertex2f(p4_4.x, p4_4.y); gs_vertex2f(p4_3.x, p4_3.y);

    gs_vertex2f(p4_1.x, p4_1.y); gs_vertex2f(p4_4.x, p4_4.y); gs_vertex2f(p4_2.x, p4_2.y);

    gs_vertex2f(p4_1.x, p4_1.y);  gs_vertex2f(p4_2.x, p4_2.y); gs_vertex2f(p1_3.x, p1_3.y);

    gs_vertex2f(p1_4.x, p1_4.y);  gs_vertex2f(p4_1.x, p4_1.y); gs_vertex2f(p1_3.x, p1_3.y);


}

static void mis_setup_circle_point(draw_point_t *point)
{
    gs_load_vertexbuffer(point->base.buf);
    struct matrix4 box;
    matrix4_identity(&box);

    vec4_set(&box.x, 1280, 0, 0, 0);
    vec4_set(&box.y, 0, 720, 0, 0);
    vec4_set(&box.z, 0, 0, 0, 0);
    vec4_set(&box.t, 0, 0, 0, 0);


    struct vec3 pos;
    vec3_set(&pos, (float)point->x / (float)1280, (float)point->y / (float)720, 0.0f);
    vec3_transform(&pos, &pos, &box);
    vec3_mulf(&pos, &pos, 1); //位置

    //for (int i = 0; i < circle->line_width; i++)
    //{
    //	gs_matrix_push();
    //	gs_matrix_translate(&pos);
    //	gs_matrix_scale3f(circle->width / 2 + i, circle->width / 2 + i, 1.0f);
    //	gs_draw(GS_LINESTRIP, 0, 0);
    //	gs_matrix_pop();
    //}


    for (int i = 0; i < point->line_width * 2; i++) {
        gs_matrix_push();
        gs_matrix_translate(&pos);
        gs_matrix_scale3f(point->width / 2 + i * 0.5, point->width / 2 + i * 0.5, 1.0f);
        gs_draw(GS_LINESTRIP, 0, 0);
        gs_matrix_pop();
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
    gs_texture_t *tex = !texture->render_text ? gs_texrender_get_texture(texture->texrender) : texture->image_texture;
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

static void draw_canvas_data(void *data, int32_t mouse_x, int32_t mouse_y, bool pressed, bool moving, bool released, uint32_t color, int shapeType, int size)
{
    if (!data)
        return;

    const auto context = reinterpret_cast<SourceManager *>(data);

    gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);

    gs_eparam_t *effectcolor = gs_effect_get_param_by_name(solid, "color");
    gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");

    unsigned int rgba = color;
    vec4 colorVal;
    vec4_set(&colorVal, (float)mis_get_rgba_r(rgba) / 0xff,
        (float)mis_get_rgba_g(rgba) / 0xff,
        (float)mis_get_rgba_b(rgba) / 0xff,
        (float)mis_get_rgba_a(rgba) / 0xff);

    gs_drawing_texture *draw_texture = context->GetCurrentPageTexture();
    if (!draw_texture)
        return;

    if (size > 0)
        context->SetLineWidth(size);

    obs_enter_graphics();

    obs_video_info ovi;
    obs_get_video_info(&ovi);
    const auto t_width = ovi.base_width;
    const auto t_height = ovi.base_height;

    gs_ortho(0.0f, static_cast<float>(t_width), 0.0f, static_cast<float>(t_height),
        -100.0f, 100.0f);

    gs_texrender_reset(draw_texture->texrender);
    gs_effect_set_vec4(effectcolor, &colorVal);

    gs_texrender_begin(draw_texture->texrender
        , std::get<0>(context->GetCanvasSize())
        , std::get<1>(context->GetCanvasSize()));

    if (draw_texture->point.is_frist_draw) {
        draw_texture->point.is_frist_draw = false;
        vec4 cleanColor;
        vec4_set(&cleanColor, 1.0, 1.0, 1.0, 0.0);
        gs_clear(GS_CLEAR_COLOR, &cleanColor, 1.0f, 0);
        obs_enter_graphics();
        gs_render_start(true);
        for (int i = 0; i <= 360; i += 1) {
            float pos = i * 0.0174532925199432957692369076848f;
            gs_vertex2f(cosf(pos), sinf(pos));
        }
        draw_texture->point.base.buf = gs_render_save();
        obs_leave_graphics();
    }

    gs_technique_begin(tech);
    gs_technique_begin_pass(tech, 0);
    gs_render_start(false);

    z_point p;
    p.x = static_cast<float>(mouse_x);
    p.y = static_cast<float>(mouse_y);

    if (!draw_texture->point_array)
        draw_texture->point_array = z_new_fpoint_array(24, 1.0f, 0.18f);

    if (pressed && context) {

        if (shapeType != DRAW_PEN) {
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
        }

        switch (shapeType) {

        case DRAW_PEN:

            draw_texture->line.start_x = mouse_x;
            draw_texture->line.start_y = mouse_y;
            draw_texture->line.base.rgba = color;
            draw_texture->line.base.width = context->GetLineWidth();
            if (draw_texture->point_array) {
                z_insert_point(draw_texture->point_array, p);
                for (int i = 0; i < draw_texture->point_array->len - 1; ++i) {
                    draw_texture->line.start_x = static_cast<int32_t>(draw_texture->point_array->point[i].p.x);
                    draw_texture->line.start_y = static_cast<int32_t>(draw_texture->point_array->point[i].p.y);
                    if (i == draw_texture->point_array->len - 2) {
                        break;
                    }
                    draw_texture->line.end_x = static_cast<int32_t>(draw_texture->point_array->point[i + 1].p.x);
                    draw_texture->line.end_y = static_cast<int32_t>(draw_texture->point_array->point[i + 1].p.y);
                    mis_setup_line(&draw_texture->line);
                }
                draw_texture->point.index = draw_texture->point_array->len - 1;

                draw_texture->line.end_x = static_cast<int32_t>(draw_texture->point_array->point[draw_texture->point.index].p.x);
                draw_texture->line.end_y = static_cast<int32_t>(draw_texture->point_array->point[draw_texture->point.index].p.y);

                mis_setup_line(&draw_texture->line);
            }

            break;
        case DRAW_LINE:

            draw_texture->line.start_x = mouse_x;
            draw_texture->line.start_y = mouse_y;
            draw_texture->line.base.rgba = color;
            draw_texture->line.base.width = context->GetLineWidth();

            break;
        case DRAW_RECT:

            draw_texture->rect.x = mouse_x;
            draw_texture->rect.y = mouse_y;
            draw_texture->rect.base.rgba = color;
            draw_texture->rect.base.width = context->GetLineWidth();

            break;
        case DRAW_CIRCLE:

            draw_texture->point.x = mouse_x;
            draw_texture->point.y = mouse_y;

            draw_texture->point.base.rgba = color;
            draw_texture->point.radius = 1;
            draw_texture->point.line_width = context->GetLineWidth();
            break;

        default:
            break;
        }
    }

    if (moving && draw_texture->point_array) {

        if (shapeType != DRAW_PEN) {
            gs_texrender_reset(draw_texture->tmp_render);
            gs_texrender_begin(draw_texture->tmp_render
                , std::get<0>(context->GetCanvasSize())
                , std::get<1>(context->GetCanvasSize()));

            gs_texture_t *render_targat = gs_texrender_get_texture(draw_texture->texrender);
            draw_texture->copy_texture = gs_texrender_get_texture(draw_texture->tmp_render);
            gs_copy_texture(render_targat, draw_texture->copy_texture);
            gs_texrender_end(draw_texture->tmp_render);
        }

        switch (shapeType) {

        case DRAW_PEN: {
            z_insert_point(draw_texture->point_array, p);
            for (int i = draw_texture->point.index; i < draw_texture->point_array->len - 1; ++i) {
                float x = draw_texture->point_array->point[i].p.x;
                float y = draw_texture->point_array->point[i].p.y;
                float ppDistance = sqrtf((draw_texture->line.start_x - x) * (draw_texture->line.start_x - x) + (draw_texture->line.start_y - y) * (draw_texture->line.start_y - y));
                if (ppDistance <= 10) {
                    break;
                }

                draw_texture->line.end_x = x;
                draw_texture->line.end_y = y;
                mis_setup_line(&draw_texture->line);
                draw_texture->line.start_x = x;
                draw_texture->line.start_y = y;
            }

            draw_texture->point.index = draw_texture->point_array->len - 1;
            draw_texture->line.end_x = draw_texture->point_array->point[draw_texture->point.index].p.x;
            draw_texture->line.end_y = draw_texture->point_array->point[draw_texture->point.index].p.y;

            break;
        }
        case DRAW_LINE:

            draw_texture->line.end_x = mouse_x;
            draw_texture->line.end_y = mouse_y;

            mis_setup_line(&draw_texture->line);
            break;

        case DRAW_RECT:
            draw_texture->rect.width = mouse_x - draw_texture->rect.x;
            draw_texture->rect.height = mouse_y - draw_texture->rect.y;

            mis_setup_rectangle(&draw_texture->rect);
            break;

        case DRAW_CIRCLE:
            draw_texture->point.radius = (mouse_x - draw_texture->point.x) / 2;
            draw_texture->point.width = mouse_x - draw_texture->point.x;
            draw_texture->point.height = mouse_y - draw_texture->point.y;

            mis_setup_circle_point(&draw_texture->point);
            break;

        default:
            break;
        }

    }
    if (released) {
        int32_t page_index = 0;
        std::string cur_str = "";
        switch (shapeType) {
        case DRAW_PEN:
            z_drop_fpoint_array(draw_texture->point_array);
            draw_texture->point_array = nullptr;
            draw_texture->point.index = 0;
            break;
        case DRAW_TEXT:
            if (draw_texture->image_texture) {
                gs_texture_t *render_targat = gs_texrender_get_texture(draw_texture->texrender);
                gs_copy_texture(render_targat, draw_texture->image_texture);
                draw_texture->render_text = false;
                gs_texture_destroy(draw_texture->image_texture);
                draw_texture->image_texture = nullptr;
            }
            break;

        case DRAW_LINE:
        case DRAW_RECT:
        case DRAW_CIRCLE:
            gs_texrender_destroy(draw_texture->tmp_render);
            draw_texture->tmp_render = NULL;
            draw_texture->copy_texture = NULL;
            break;

        case DRAW_CLEAR:
            page_index = context->GetCurrentKeyCurrentPage();
            cur_str = context->GetCurrentKey();
            context->RemovePage(cur_str, page_index);
            context->AddPage(cur_str, page_index);
            break;

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

static void draw_page_change(void *data, const char *key, int32_t page_index)
{
    const auto context = reinterpret_cast<SourceManager *>(data);
    if (!context || page_index < 0)
        return;

    if (key) {
        if (!context->HasKey(key)) {
            if (!context->AddKey(key))
                return;
        }
        context->SetCurrentKey(key);
    }

    context->SetCurrentPage(page_index);

    draw_info_changed(data, context->props);
}

static void draw_source_set_video_frame(void *data, int x, int y, struct obs_source_frame *frame)
{
    const auto context = reinterpret_cast<SourceManager *>(data);
    if (!context || !frame)
        return;

    const auto texture = context->GetCurrentPageTexture();
    const uint32_t canvas_width = std::get<0>(context->GetCanvasSize());
    const uint32_t canvas_height = std::get<1>(context->GetCanvasSize());

    obs_enter_graphics();
    if (!texture->image_texture) {
        texture->image_texture = gs_texture_create(canvas_width
            , canvas_height
            , GS_RGBA
            , 1
            , nullptr
            , 0);
    }

    if (texture->image_texture) {
        const auto tmp_texture = gs_texture_create(frame->width
            , frame->height
            , GS_RGBA
            , 1
            , const_cast<const uint8_t **>(&frame->data[0])
            , 0);
        gs_texrender_reset(texture->texrender);
        gs_texrender_begin(texture->texrender
            , canvas_width
            , canvas_height);
        gs_copy_texture(texture->image_texture, gs_texrender_get_texture(texture->texrender));
        gs_texrender_end(texture->texrender);
        gs_copy_texture_region(texture->image_texture, x, y
            , tmp_texture, 0, 0, frame->width,
            frame->height);
        texture->render_text = true;
        gs_texture_destroy(tmp_texture);
    }

    obs_leave_graphics();
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
    drawing_source_info.set_video_frame = draw_source_set_video_frame;
    obs_register_source(&drawing_source_info);

    return true;
}



