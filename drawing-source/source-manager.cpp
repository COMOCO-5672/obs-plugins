#include "source-manager.h"

KeySource::KeySource()
{
    // Add an initial page at initialization
    AddPage(0);
}

KeySource::~KeySource()
{
    for (const auto &page : m_page_list_) {
        obs_enter_graphics();
        if (page.second->tmp_render)
            gs_texrender_destroy(page.second->tmp_render);

        if (page.second->texrender)
            gs_texrender_destroy(page.second->texrender);

        if (page.second->copy_texture)
            gs_texture_destroy(page.second->copy_texture);

        if (page.second->point_array)
            z_drop_fpoint_array(page.second->point_array);

        obs_leave_graphics();
    }
}

bool KeySource::AddPage(int32_t page_index)
{
    const auto find_page_item = m_page_list_.find(page_index);
    if (find_page_item == m_page_list_.end()) {
        const auto texture = new gs_drawing_texture();
        texture->texrender = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
        const bool insert_ret = m_page_list_.insert(std::pair(page_index, texture)).second;
        if (!insert_ret) {
            gs_texrender_destroy(texture->texrender);
            delete texture;
        }
        return insert_ret;
    }

    return true;
}

bool KeySource::RemovePage(int32_t page_index)
{
    const auto find_page_item = m_page_list_.find(page_index);
    if (find_page_item == m_page_list_.end())
        return true;

    return m_page_list_.erase(find_page_item)->second;
}

bool KeySource::SetCurrentPage(int32_t page_index)
{
    bool ret = true;
    if (m_page_list_.find(page_index) == m_page_list_.end()) {
        // If not found, add it.
        ret = AddPage(page_index);
    }

    if (ret)
        m_cur_page_idx_ = page_index;

    return ret;
}

bool KeySource::UpdateTexture(int32_t page_index, gs_drawing_texture *texture)
{
    const auto find_item = m_page_list_.find(page_index);
    if (find_item == m_page_list_.end())
        return false;

    find_item->second = texture;
    return true;
}

gs_drawing_texture *KeySource::GetPageIndexTexture(int32_t page_index)
{
    const auto find_item = m_page_list_.find(page_index);
    if (find_item == m_page_list_.end())
        return nullptr;

    return find_item->second;
}

int32_t KeySource::GetPageSize()
{
    return static_cast<int32_t>(m_page_list_.size());
}

int32_t KeySource::GetCurrentPage()
{
    return m_cur_page_idx_;
}

// source manager
SourceManager::SourceManager(obs_source_t *source_) : source(source_)
{

}

SourceManager::~SourceManager()
{

}

bool SourceManager::HasKey(const std::string &key)
{
    const auto find_item = m_draw_list.find(key);
    if (find_item == m_draw_list.end())
        return false;

    return true;
}

bool SourceManager::AddKey(const std::string &key)
{
    const auto find_item = m_draw_list.find(key);
    if (find_item == m_draw_list.end()) {
        // inset new map
        const auto key_source = new KeySource();
        return m_draw_list.insert(std::pair(key, key_source)).second;
    }
    return true;
}

bool SourceManager::AddPage(const std::string &key, int32_t page_index)
{
    const auto find_key_item = m_draw_list.find(key);
    if (find_key_item == m_draw_list.end())
        return false;

    return find_key_item->second->AddPage(page_index);
}

bool SourceManager::UpdateDrawingTexture(const std::string &key, int32_t page_index, gs_drawing_texture *texture)
{
    const bool ret = AddPage(key, page_index);
    if (!ret)
        return false;

    // update texture
    m_draw_list.find(key)->second->UpdateTexture(page_index, texture);
    return true;
}

bool SourceManager::RemoveKey(const std::string &key)
{
    const auto find_item = m_draw_list.find(key);
    if (find_item == m_draw_list.end())
        return true;

    m_draw_list.erase(find_item);
    return true;
}

bool SourceManager::RemovePage(const std::string &key, int32_t page_index)
{
    const auto find_item = m_draw_list.find(key);
    if (find_item == m_draw_list.end())
        return true;

    return find_item->second->RemovePage(page_index);
}

gs_drawing_texture *SourceManager::GetPageTexture(const std::string &key, int32_t page_index)
{
    const auto find_key_item = m_draw_list.find(key);
    if (find_key_item == m_draw_list.end())
        return nullptr;

    return find_key_item->second->GetPageIndexTexture(page_index);
}

gs_drawing_texture *SourceManager::GetCurrentPageTexture()
{
    return GetPageTexture(m_current_key_, m_current_idx_);
}

int32_t SourceManager::GetPageSize(const std::string &key)
{
    const auto find_item = m_draw_list.find(key);
    if (find_item == m_draw_list.end())
        return 0;

    return find_item->second->GetPageSize();
}

void SourceManager::UpdateCanvasSize(uint32_t width, uint32_t height)
{
    m_canvas_width_ = width;
    m_canvas_height_ = height;
}

std::tuple<uint32_t, uint32_t> SourceManager::GetCanvasSize()
{
    return { m_canvas_width_, m_canvas_height_ };
}

void SourceManager::SetLineWidth(int32_t width)
{
    m_line_width_ = width;
}

int32_t SourceManager::GetLineWidth()
{
    return m_line_width_;
}

void SourceManager::SetCurrentKey(std::string &key)
{
    m_current_key_ = key;
    const auto find_item = m_draw_list.find(m_current_key_);
    if (find_item == m_draw_list.end())
        m_current_idx_ = 0;

    m_current_idx_ = find_item->second->GetCurrentPage();
}

std::string SourceManager::GetCurrentKey()
{
    return m_current_key_;
}

void SourceManager::SetCurrentPage(int32_t page_index)
{
    m_current_idx_ = page_index;
    const auto find_item = m_draw_list.find(m_current_key_);
    if (find_item == m_draw_list.end())
        return;

    find_item->second->SetCurrentPage(page_index);
}

int32_t SourceManager::GetCurrentKeyCurrentPage()
{
    const auto find_item = m_draw_list.find(m_current_key_);
    if (find_item == m_draw_list.end())
        return -1;

    return find_item->second->GetCurrentPage();
}

std::unordered_map<std::string, int32_t> SourceManager::GetKeyInfo()
{
    std::unordered_map<std::string, int32_t> map;
    for (const auto &draw : m_draw_list) {
        map.insert(std::pair(draw.first, draw.second->GetCurrentPage()));
    }
    return map;
}

