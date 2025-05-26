#pragma once
#include <imgui.h>
#include <glm/gtc/type_ptr.hpp> // 使用 glm::value_ptr

namespace dk::ui {
    static bool im_gui_blender_float(const char* label, float* v,
        float       step = 1.0f, float    speed = 0.1f,
        float       v_min = FLT_MAX, float v_max = FLT_MAX,
        const char* format = "%.4f")
    {
        ImGui::PushID(label);
        // 微调内边距让箭头更紧凑
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));

        // 左箭头
        if (ImGui::ArrowButton("##dec", ImGuiDir_Left))
            *v -= step;
        ImGui::SameLine();

        // 拖拽 & 输入复合控件
        // 直接用 DragFloat：它支持拖拽修改、双击输入
        ImGui::DragFloat("##drag", v, speed,
            (v_min < v_max ? v_min : -FLT_MAX),
            (v_min < v_max ? v_max : +FLT_MAX),
            format);
        ImGui::SameLine();

        // 右箭头
        if (ImGui::ArrowButton("##inc", ImGuiDir_Right))
            *v += step;
        ImGui::SameLine();

        // 最后绘制标签
        ImGui::TextUnformatted(label);

        ImGui::PopStyleVar(2);
        ImGui::PopID();

        // 返回值可根据需要改为 v 是否变化
        return true;
    }

    // 针对 glm::vec3 的三分量封装
    static bool im_gui_blender_vec3(const char* label, glm::vec3& value,
        float       step = 1.0f, float    speed = 0.1f,
        float       v_min = FLT_MAX, float v_max = FLT_MAX,
        const char* format = "%.4f")
    {
        ImGui::PushID(label);
        ImGui::Text("%s", label);
        //ImGui::SameLine();

        bool currentlyActive = false;

        // 对 XYZ 三通道分别调用
        bool changed = false;
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));
        changed |= im_gui_blender_float("X", &value.x, step, speed, v_min, v_max, format);
        currentlyActive |= ImGui::IsItemActive();
        changed |= im_gui_blender_float("Y", &value.y, step, speed, v_min, v_max, format);
        currentlyActive |= ImGui::IsItemActive();
        changed |= im_gui_blender_float("Z", &value.z, step, speed, v_min, v_max, format);
        currentlyActive |= ImGui::IsItemActive();

        if (currentlyActive)
        {
            SDL_HideCursor();
        }
        else
        {
            SDL_ShowCursor();
        }
        ImGui::PopStyleVar();
        ImGui::PopID();
        return changed;
    }

}
