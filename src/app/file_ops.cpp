// New / open / save, the "save changes?" question, the typed-path dialog used where
// native file dialogs aren't available (Linux), and the window title.
#include "app/editor.hpp"
#include "io/export.hpp"
#include "io/mazz_file.hpp"
#include "platform/file_dialog.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>

#ifndef MAZZ_VERSION
#define MAZZ_VERSION "dev"
#endif

namespace fs = std::filesystem;

namespace mazz {

namespace {
std::string withMazzExtension(const std::string& path) {
    std::string ext = fs::path(path).extension().string();
    for (char& c : ext) c = (char)std::tolower((unsigned char)c);
    return ext == ".mazz" ? path : path + ".mazz";
}
} // namespace

std::string Editor::displayName() const {
    return path_.empty() ? std::string("Untitled") : fs::path(path_).filename().string();
}

void Editor::updateTitle() {
    const std::string t = displayName() + (doc_.dirty() ? " \xE2\x80\xA2" : "") + " \xE2\x80\x94 Mazzuola";
    if (t != title_) {
        title_ = t;
        glfwSetWindowTitle(window_, t.c_str());
    }
}

void Editor::touch() { markViewsDirty(); }

// ---------------------------------------------------------------- quit / new / open

void Editor::requestQuit() {
    glfwSetWindowShouldClose(window_, GLFW_FALSE); // decided below, not by the close button
    whenSaved([this] { quit_ = true; });
}

void Editor::fileNew() {
    whenSaved([this] {
        cancelTool();
        doc_.reset();
        path_.clear();
        exportPath_.clear();
        exportNote_.clear();
        texel_ = 0.5;
        touch();
    });
}

bool Editor::openPath(const std::string& path) {
    MazzModel m;
    std::string err;
    if (!loadMazzFile(path, m, err)) {
        showMessage("Could not open the model", fs::path(path).filename().string() + "\n\n" + err);
        return false;
    }
    cancelTool();
    doc_.reset(std::move(m.mesh));
    texel_ = m.texel;
    path_ = path;
    exportPath_.clear();
    exportNote_.clear();
    touch();
    return true;
}

void Editor::fileOpen() {
    whenSaved([this] {
        const std::string start = path_.empty() ? std::string() : fs::path(path_).parent_path().string();
        if (nativeDialogsAvailable()) {
            std::string path;
            if (openFileDialog(start, {"mazz"}, path)) openPath(path);
            return;
        }
        askPath("Open model", false, start, [this](const std::string& p) { openPath(p); });
    });
}

// ---------------------------------------------------------------- save

bool Editor::savePath(const std::string& pathIn) {
    const std::string path = withMazzExtension(pathIn);
    MazzModel m;
    m.mesh = doc_.mesh;
    m.texel = texel_;
    std::string err;
    if (!saveMazzFile(path, m, err)) {
        showMessage("Could not save the model", fs::path(path).filename().string() + "\n\n" + err);
        return false;
    }
    path_ = path;
    doc_.markSaved();
    return true;
}

void Editor::fileSave(bool saveAs) {
    // After a successful save, carry on with whatever was waiting for it (quit, open...).
    auto finish = [this](bool ok) {
        auto then = std::move(afterSave_);
        afterSave_ = nullptr;
        if (ok && then) then();
    };
    if (!saveAs && !path_.empty()) {
        finish(savePath(path_));
        return;
    }
    const std::string start = path_.empty() ? std::string() : fs::path(path_).parent_path().string();
    const std::string name = path_.empty() ? "model.mazz" : displayName();
    if (nativeDialogsAvailable()) {
        std::string path;
        finish(saveFileDialog(start, name, {"mazz"}, path) && savePath(path));
        return;
    }
    askPath("Save model as", true, start.empty() ? name : (fs::path(start) / name).string(),
            [this, finish](const std::string& p) { finish(savePath(p)); });
}

// ---------------------------------------------------------------- export

bool Editor::exportPath(const std::string& pathIn, ExportFormat f) {
    // The format decides the extension: "lamp" or "lamp.mazz" become "lamp.glb".
    std::string path = pathIn;
    ExportFormat had;
    if (!exportFormatFromPath(path, had) || had != f) path = fs::path(path).replace_extension(exportExtension(f)).string();
    std::string err;
    if (!exportModel(path, f, doc_.mesh, std::string("Mazzuola ") + MAZZ_VERSION, err)) {
        showMessage("Could not export the model", fs::path(path).filename().string() + "\n\n" + err);
        return false;
    }
    exportPath_ = path;
    exportFormat_ = f;
    exportNote_ = "Exported to " + fs::path(path).filename().string() + "  (Cmd+Shift+E exports again)";
#if !defined(__APPLE__)
    for (size_t p = exportNote_.find("Cmd"); p != std::string::npos; p = exportNote_.find("Cmd", p)) exportNote_.replace(p, 3, "Ctrl");
#endif
    exportNoteMesh_ = doc_.meshVersion();
    return true;
}

void Editor::fileExport(ExportFormat f) {
    // Starts where the last export went, else next to the model; named after the model.
    const std::string ext = exportExtension(f);
    std::string start, name = "model." + ext;
    if (!exportPath_.empty()) start = fs::path(exportPath_).parent_path().string();
    else if (!path_.empty()) start = fs::path(path_).parent_path().string();
    if (!exportPath_.empty()) name = fs::path(exportPath_).stem().string() + "." + ext;
    else if (!path_.empty()) name = fs::path(path_).stem().string() + "." + ext;
    if (nativeDialogsAvailable()) {
        std::string path;
        if (saveFileDialog(start, name, {ext}, path)) exportPath(path, f);
        return;
    }
    askPath(std::string("Export as ") + exportFormatName(f), true,
            start.empty() ? name : (fs::path(start) / name).string(),
            [this, f](const std::string& p) { exportPath(p, f); });
}

// ---------------------------------------------------------------- questions

void Editor::whenSaved(std::function<void()> then) {
    // Scripts are never asked (they would wait forever); they save when they mean to.
    if (!doc_.dirty() || scriptMode_) {
        then();
        return;
    }
    afterSave_ = std::move(then);
    unsavedOpen_ = true;
}

void Editor::askPath(const std::string& title, bool saving, const std::string& start,
                     std::function<void(const std::string&)> done) {
    pathTitle_ = title;
    pathSaving_ = saving;
    std::snprintf(pathBuf_, sizeof pathBuf_, "%s", start.c_str());
    pathDone_ = std::move(done);
    pathOpen_ = true;
}

void Editor::showMessage(const std::string& title, const std::string& text) {
    if (scriptMode_) scriptFail(title + ": " + text); // scripts report it instead of waiting
    messageTitle_ = title;
    messageText_ = text;
    messageOpen_ = !scriptMode_;
}

void Editor::fileDialogs() {
    // Unsaved changes: Save / Don't save / Cancel.
    if (unsavedOpen_) {
        ImGui::OpenPopup("Save changes?");
        unsavedOpen_ = false;
    }
    if (ImGui::BeginPopupModal("Save changes?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Save the changes to %s first?", displayName().c_str());
        ImGui::Spacing();
        if (ImGui::Button("Save") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
            ImGui::CloseCurrentPopup();
            fileSave(false); // runs afterSave_ when saved
        }
        ImGui::SameLine();
        if (ImGui::Button("Don't save")) {
            ImGui::CloseCurrentPopup();
            auto then = std::move(afterSave_);
            afterSave_ = nullptr;
            if (then) then();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            ImGui::CloseCurrentPopup();
            afterSave_ = nullptr;
        }
        ImGui::EndPopup();
    }

    // Typed path (no native dialogs here).
    if (pathOpen_) {
        ImGui::OpenPopup("##path");
        pathOpen_ = false;
    }
    ImGui::SetNextWindowSize(ImVec2(520, 0), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("##path", nullptr, ImGuiWindowFlags_NoTitleBar)) {
        ImGui::TextUnformatted(pathTitle_.c_str());
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(-1);
        const bool enter = ImGui::InputText("##p", pathBuf_, sizeof pathBuf_, ImGuiInputTextFlags_EnterReturnsTrue);
        if (enter || ImGui::Button(pathSaving_ ? "Save" : "Open")) {
            ImGui::CloseCurrentPopup();
            auto done = std::move(pathDone_);
            pathDone_ = nullptr;
            if (done && pathBuf_[0]) done(pathBuf_);
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            ImGui::CloseCurrentPopup();
            pathDone_ = nullptr;
            afterSave_ = nullptr;
        }
        ImGui::EndPopup();
    }

    // Messages (errors).
    if (messageOpen_) {
        ImGui::OpenPopup("##message");
        messageOpen_ = false;
    }
    if (ImGui::BeginPopupModal("##message", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar)) {
        ImGui::TextUnformatted(messageTitle_.c_str());
        ImGui::Separator();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30);
        ImGui::TextUnformatted(messageText_.c_str());
        ImGui::PopTextWrapPos();
        if (ImGui::Button("OK") || ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Escape))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

} // namespace mazz
