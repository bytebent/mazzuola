// New / open / save / export, the unsaved-changes question, and the typed-path dialog
// used where native file dialogs aren't available (Linux, scripted runs).
#include "app/editor.hpp"
#include "io/export.hpp"
#include "io/mazz_file.hpp"
#include "platform/file_dialog.hpp"
#include "ui/theme.hpp"
#include <algorithm>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

namespace mazz {

namespace {
std::string lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}
std::string withExtension(const std::string& path, const std::string& ext) {
    if (lower(fs::path(path).extension().string()) == "." + ext) return path;
    return path + "." + ext;
}
} // namespace

std::string Editor::modelDir() const {
    if (doc_.path.empty()) return "";
    return fs::path(doc_.path).parent_path().string();
}

std::string Editor::displayName() const {
    if (doc_.path.empty()) return "Untitled";
    return fs::path(doc_.path).filename().string();
}

void Editor::rebaseRefs(Model& m, const std::string& oldDir, const std::string& newDir) const {
    for (RefImage& r : m.refs) {
        const std::string abs = resolveRefPath(r.path, oldDir);
        if (newDir.empty()) {
            r.path = abs;
            continue;
        }
        std::error_code ec;
        const fs::path rel = fs::relative(fs::path(abs), fs::path(newDir), ec);
        // Relative when it stays reasonably close (no climbing out of the whole tree).
        if (!ec && !rel.empty() && rel.string().rfind("../../../", 0) != 0) r.path = rel.generic_string();
        else r.path = abs;
    }
}

// ---------------------------------------------------------------- new / open

void Editor::fileNew() {
    if (drag_ != DragKind::None) cancelDrag();
    primActive_ = false;
    doc_.reset();
    textures_.clear();
    touch();
}

bool Editor::openPath(const std::string& path) {
    Model m;
    std::string err;
    if (!loadMazzFile(path, m, err)) {
        showMessage("Could not open the model", path + "\n\n" + err);
        return false;
    }
    if (drag_ != DragKind::None) cancelDrag();
    primActive_ = false;
    doc_.reset();
    doc_.model = std::move(m);
    doc_.path = path;
    doc_.markSaved();
    textures_.clear();
    touch();
    frameSelection();
    return true;
}

void Editor::fileOpen() {
    const std::string start = modelDir();
    if (nativeDialogsAvailable()) {
        std::string path;
        if (openFileDialog(start, {"mazz"}, path)) openPath(path);
        return;
    }
    askPath("Open model", false, start, [this](const std::string& p) { openPath(p); });
}

// ---------------------------------------------------------------- save

bool Editor::savePath(const std::string& pathIn) {
    const std::string path = withExtension(pathIn, "mazz");
    Model m = doc_.model;
    const std::string newDir = fs::path(path).parent_path().string();
    rebaseRefs(m, modelDir(), newDir);
    std::string err;
    if (!saveMazzFile(path, m, err)) {
        showMessage("Could not save the model", path + "\n\n" + err);
        return false;
    }
    // Reference paths now follow the new folder; that is not an undoable edit.
    doc_.model.refs = m.refs;
    doc_.path = path;
    doc_.markSaved();
    touch();
    return true;
}

void Editor::fileSave(bool saveAs) {
    if (!saveAs && !doc_.path.empty()) {
        if (savePath(doc_.path) && afterSave_) {
            auto then = std::move(afterSave_);
            afterSave_ = nullptr;
            then();
        }
        return;
    }
    auto done = [this](const std::string& p) {
        if (savePath(p) && afterSave_) {
            auto then = std::move(afterSave_);
            afterSave_ = nullptr;
            then();
        }
    };
    const std::string start = modelDir();
    const std::string name = doc_.path.empty() ? "model.mazz" : displayName();
    if (nativeDialogsAvailable()) {
        std::string path;
        if (saveFileDialog(start, name, {"mazz"}, path)) done(path);
        else afterSave_ = nullptr;
        return;
    }
    askPath("Save model as", true, start.empty() ? name : (fs::path(start) / name).string(), done);
}

// ---------------------------------------------------------------- export

bool Editor::exportPath(const std::string& path) {
    const std::string ext = lower(fs::path(path).extension().string());
    std::string err;
    bool ok = false;
    if (ext == ".glb") ok = exportGlb(path, doc_.model, err);
    else if (ext == ".gltf") ok = exportGltf(path, doc_.model, err);
    else if (ext == ".obj") ok = exportObj(path, doc_.model, err);
    else err = "Unknown file type (use .glb, .gltf or .obj).";
    if (!ok) showMessage("Could not export", path + "\n\n" + err);
    return ok;
}

void Editor::fileExport(const std::string& ext) {
    std::string start = modelDir();
    std::string stem = doc_.path.empty() ? "model" : fs::path(doc_.path).stem().string();
    const std::string name = stem + "." + ext;
    auto done = [this, ext](const std::string& p) { exportPath(withExtension(p, ext)); };
    if (nativeDialogsAvailable()) {
        std::string path;
        if (saveFileDialog(start, name, {ext}, path)) done(path);
        return;
    }
    askPath("Export " + ext, true, start.empty() ? name : (fs::path(start) / name).string(), done);
}

void Editor::loadRefImage() {
    const int dir = refDir_;
    auto add = [this, dir](const std::string& p) {
        RefImage r;
        r.dir = (ViewDir)dir;
        r.path = p;
        const std::string base = modelDir();
        if (!base.empty()) {
            std::error_code ec;
            const fs::path rel = fs::relative(fs::path(p), fs::path(base), ec);
            if (!ec && !rel.empty() && rel.string().rfind("../../../", 0) != 0) r.path = rel.generic_string();
        }
        edit("Add reference image", [&](Model& m, Selection&) {
            m.refs.push_back(r);
            return true;
        });
    };
    const std::vector<std::string> exts = {"png", "jpg", "jpeg", "tga", "bmp"};
    if (nativeDialogsAvailable()) {
        std::string path;
        if (openFileDialog(modelDir(), exts, path)) add(path);
        return;
    }
    askPath("Load reference image", false, modelDir(), add);
}

// ---------------------------------------------------------------- questions

void Editor::whenSaved(std::function<void()> then) {
    if (!doc_.dirty || scriptMode_) {
        // Scripts never get asked (they'd wait forever); they decide by themselves.
        then();
        return;
    }
    afterSave_ = std::move(then);
    unsavedOpen_ = true;
}

void Editor::unsavedPrompt() {
    if (unsavedOpen_) {
        ImGui::OpenPopup("Unsaved changes");
        unsavedOpen_ = false;
    }
    ImGui::SetNextWindowSize(ImVec2(380, 0), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Unsaved changes", nullptr, ImGuiWindowFlags_NoResize)) {
        ImGui::TextWrapped("Save the changes to %s?", displayName().c_str());
        ImGui::Spacing();
        const bool enter = ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter);
        if (ImGui::Button("Save (Return)") || enter) {
            ImGui::CloseCurrentPopup();
            fileSave(false); // runs afterSave_ when it worked
        }
        ImGui::SameLine();
        if (ImGui::Button("Don't save")) {
            ImGui::CloseCurrentPopup();
            auto then = std::move(afterSave_);
            afterSave_ = nullptr;
            if (then) then();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel (Esc)") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            ImGui::CloseCurrentPopup();
            afterSave_ = nullptr;
        }
        ImGui::EndPopup();
    }
}

void Editor::askPath(const std::string& title, bool save, const std::string& start,
                     std::function<void(const std::string&)> done) {
    pathDialogTitle_ = title;
    pathDialogSave_ = save;
    std::snprintf(pathBuf_, sizeof pathBuf_, "%s", start.c_str());
    pathDialogDone_ = std::move(done);
    pathDialogOpen_ = true;
}

void Editor::pathDialog() {
    if (pathDialogOpen_) {
        ImGui::OpenPopup("##pathdialog");
        pathDialogOpen_ = false;
    }
    ImGui::SetNextWindowSize(ImVec2(520, 0), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("##pathdialog", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar)) {
        ImGui::TextUnformatted(pathDialogTitle_.c_str());
        ImGui::TextDisabled("Type the file's full path.");
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(-1);
        const bool entered = ImGui::InputText("##path", pathBuf_, sizeof pathBuf_, ImGuiInputTextFlags_EnterReturnsTrue);
        if (ImGui::Button(pathDialogSave_ ? "Save (Return)" : "Open (Return)") || entered) {
            ImGui::CloseCurrentPopup();
            auto done = std::move(pathDialogDone_);
            pathDialogDone_ = nullptr;
            if (done && pathBuf_[0]) done(pathBuf_);
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel (Esc)") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            ImGui::CloseCurrentPopup();
            pathDialogDone_ = nullptr;
            afterSave_ = nullptr;
        }
        ImGui::EndPopup();
    }
}

void Editor::showMessage(const std::string& title, const std::string& text) {
    if (scriptMode_) std::fprintf(stderr, "Mazzuola: %s: %s\n", title.c_str(), text.c_str());
    messageTitle_ = title;
    messageText_ = text;
    messageOpen_ = true;
}

void Editor::messageBox() {
    if (messageOpen_) {
        ImGui::OpenPopup("##message");
        messageOpen_ = false;
    }
    ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("##message", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar)) {
        ImGui::TextUnformatted(messageTitle_.c_str());
        ImGui::Separator();
        ImGui::TextWrapped("%s", messageText_.c_str());
        ImGui::Spacing();
        if (ImGui::Button("OK", ImVec2(100, 0)) || ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Escape))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

} // namespace mazz
