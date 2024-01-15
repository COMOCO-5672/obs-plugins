#pragma once

#include <iostream>
#include <string>
#include <unordered_map>

#include "obs-module.h"
#include "obs-source.h"
#include "drawing-source.h"
#include "zmath.h"
#include <mutex>

struct gs_drawing_texture {
    gs_texrender_t *texrender;
    gs_texrender_t *tmp_render;
    gs_texture_t  *copy_texture;

    gs_texture_t *image_texture;
    z_fpoint_array *point_array;
    bool render_text;

    enum gs_color_format format;
    uint32_t width;
    uint32_t height;

    std::mutex mutex;

    union {
        draw_line_t line;
        draw_rect_t rect;
        draw_point_t point;
    };
};

class KeySource {
public:
    KeySource();
    virtual ~KeySource();

    bool AddPage(int32_t page_index);
    bool RemovePage(int32_t page_index);
    bool SetCurrentPage(int32_t page_index);
    bool UpdateTexture(int32_t page_index, gs_drawing_texture* texture);
    gs_drawing_texture *GetPageIndexTexture(int32_t page_index);
    int32_t GetPageSize();
    int32_t GetCurrentPage();

private:
    void release_draw_texture(gs_drawing_texture* texture);

private:
    int32_t m_cur_page_idx_ = 0;
    std::unordered_map<int32_t, gs_drawing_texture *> m_page_list_;

};

class SourceManager {
public:
    SourceManager(obs_source_t *source);
    virtual ~SourceManager();

    bool HasKey(const std::string &key);

    bool AddKey(const std::string &key);
    bool AddPage(const std::string &key, int32_t page_index);

    bool RemoveKey(const std::string &key);
    bool RemovePage(const std::string &key, int32_t page_index);

    bool UpdateDrawingTexture(const std::string &key, int32_t page_index,
        gs_drawing_texture *texture);
    gs_drawing_texture *GetPageTexture(const std::string &key, int32_t page_index);

    gs_drawing_texture *GetCurrentPageTexture();

    int32_t GetPageSize(const std::string &key);

    void UpdateCanvasSize(uint32_t width, uint32_t height);

    std::tuple<uint32_t, uint32_t> GetCanvasSize();

    void SetLineWidth(int32_t width);
    int32_t GetLineWidth();

    void SetCurrentKey(const std::string &key);
    std::string GetCurrentKey();

    void SetCurrentPage(int32_t page_index);

    int32_t GetCurrentKeyCurrentPage();

    std::unordered_map<std::string, int32_t> GetKeyInfo();

public:
    obs_source_t *source { nullptr };
    obs_properties_t *props { nullptr };

private:
    
    std::string m_current_key_ = "";
    int32_t m_current_idx_ = 0;

    // canvas_data
    uint32_t m_canvas_width_ = 0;
    uint32_t m_canvas_height_ = 0;
    int32_t m_line_width_ = 0;

    std::unordered_map<std::string, KeySource *> m_draw_list;

};
