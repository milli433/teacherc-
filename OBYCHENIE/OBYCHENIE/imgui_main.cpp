// ============================================================
// C++ Learning Program - ImGui GUI Version
// Eye-friendly dark theme with syntax highlighting
// ============================================================
#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable : 4996)
#define NOMINMAX
#define IMGUI_MODE

// Pull in all data structures, createTopics(), AI, progress:
#include "cpp_learning_program.cpp"

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_dx11.h"

#include <d3d11.h>
#include <dxgi.h>
#include <tchar.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <shlwapi.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "shlwapi.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ============================================================
// PALETTE — One-Dark inspired, easy on eyes
// ============================================================
namespace C {
    static const ImVec4 BG          = {0.110f, 0.122f, 0.141f, 1.0f}; // #1C1F24
    static const ImVec4 BG2         = {0.145f, 0.161f, 0.188f, 1.0f}; // #252930
    static const ImVec4 BG3         = {0.176f, 0.196f, 0.224f, 1.0f}; // #2D3239
    static const ImVec4 SIDEBAR     = {0.094f, 0.102f, 0.122f, 1.0f}; // #181A1F
    static const ImVec4 BORDER      = {0.220f, 0.243f, 0.275f, 1.0f}; // #383E46
    static const ImVec4 ACCENT      = {0.380f, 0.682f, 0.937f, 1.0f}; // #61AEEF
    static const ImVec4 ACCENT_DIM  = {0.247f, 0.443f, 0.608f, 1.0f}; // #3F719B
    static const ImVec4 TEXT        = {0.667f, 0.698f, 0.749f, 1.0f}; // #AAB2BE
    static const ImVec4 TEXT_BRIGHT = {0.875f, 0.882f, 0.898f, 1.0f}; // #DFE1E5
    static const ImVec4 TEXT_DIM    = {0.408f, 0.447f, 0.502f, 1.0f}; // #687280
    static const ImVec4 CODE_BG     = {0.082f, 0.094f, 0.114f, 1.0f}; // #15181D
    static const ImVec4 KW          = {0.878f, 0.424f, 0.463f, 1.0f}; // #E06C76 keywords
    static const ImVec4 STR         = {0.596f, 0.761f, 0.471f, 1.0f}; // #98C278 strings
    static const ImVec4 CMT         = {0.357f, 0.404f, 0.447f, 1.0f}; // #5B6772 comments
    static const ImVec4 NUM         = {0.820f, 0.604f, 0.404f, 1.0f}; // #D19A67 numbers
    static const ImVec4 PREPROC     = {0.780f, 0.557f, 0.875f, 1.0f}; // #C78EDF preprocessor
    static const ImVec4 HEADING     = {0.902f, 0.745f, 0.318f, 1.0f}; // #E6BE51 section heads
    static const ImVec4 SUCCESS     = {0.596f, 0.761f, 0.471f, 1.0f}; // #98C278
    static const ImVec4 ERROR_C     = {0.878f, 0.424f, 0.463f, 1.0f}; // #E06C76
    static const ImVec4 STREAK      = {0.976f, 0.643f, 0.149f, 1.0f}; // #F9A426 streak
    static const ImVec4 PURPLE      = {0.780f, 0.557f, 0.875f, 1.0f}; // #C78EDF
}

// ============================================================
// APPLICATION STATE
// ============================================================
enum class Screen { HOME, LESSON, AI, STATS };
enum class LessonTab { THEORY, CODE, QUIZ };

struct QuizState {
    int  questionIdx  = 0;
    int  selectedAns  = -1;    // -1 = not chosen
    bool showResult   = false;
    int  correct      = 0;
    bool finished     = false;
};

struct AppState {
    Screen     screen          = Screen::HOME;
    int        topicIdx        = -1; // selected topic
    int        lessonIdx       = -1; // selected lesson (-1 = topic overview)
    LessonTab  lessonTab       = LessonTab::THEORY;
    QuizState  quiz;
    bool       showFinalTask   = false;

    // AI panel
    char       aiInput[2048]   = {};
    string     aiResponse;
    string     aiStatus;        // "idle" / "loading" / "done"
    atomic<bool> aiLoading     {false};
    mutex      aiMutex;

    // UI helpers
    float      sidebarW        = 240.0f;
    float      fontSize        = 16.0f;
};

static AppState g_app;
static vector<Topic>   g_topics;
static Progress        g_progress;

// ============================================================
// DIRECTX 11 + WINDOW GLOBALS
// ============================================================
static ID3D11Device*           g_pd3dDevice           = nullptr;
static ID3D11DeviceContext*    g_pd3dDeviceContext     = nullptr;
static IDXGISwapChain*         g_pSwapChain            = nullptr;
static UINT                    g_ResizeWidth  = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView* g_mainRenderTargetView  = nullptr;
static HWND                    g_hwnd                  = nullptr;

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ============================================================
// HELPER: UTF-8 → WIDE (for Windows API)
// ============================================================
static wstring ToWide(const string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    wstring w(n - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    return w;
}

// ============================================================
// HIDDEN-PROCESS SYSTEM CALL (no console flash in GUI app)
// ============================================================
static int SystemHidden(const string& cmd) {
    STARTUPINFOA si = {};
    si.cb          = sizeof(si);
    si.dwFlags     = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};
    string cmdCopy = cmd; // CreateProcessA needs non-const
    if (!CreateProcessA(nullptr, &cmdCopy[0], nullptr, nullptr,
                        FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
        return -1;
    WaitForSingleObject(pi.hProcess, 40000);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)code;
}

// Override callAI for GUI (silent curl)
static string callAI_GUI(const string& prompt, const string& context = "") {
    string systemMsg = "Ты помощник по C++ для школьников. Объясняй просто, по-русски, с примерами кода. Отвечай по существу вопроса. Будь кратким и понятным.";
    string userMsg   = context.empty() ? prompt : context + "\n" + prompt;
    vector<string> models = {
        "meta-llama/llama-3.3-70b-instruct:free",
        "google/gemma-3-27b-it:free",
        "qwen/qwen3-coder:free",
        "google/gemma-3-12b-it:free",
        "meta-llama/llama-3.2-3b-instruct:free"
    };
    for (int m = 0; m < (int)models.size(); m++) {
        string json = "{"
            "\"model\":\"" + models[m] + "\","
            "\"messages\":["
            "{\"role\":\"system\",\"content\":\"" + escapeJson(systemMsg) + "\"},"
            "{\"role\":\"user\",\"content\":\"" + escapeJson(userMsg) + "\"}"
            "]}";
        { ofstream f("_ai_req.json"); if (!f) return "Ошибка: не удалось создать файл."; f << json; }
        string curlCmd =
            "curl -s --max-time 30 -X POST https://openrouter.ai/api/v1/chat/completions "
            "-H \"Authorization: Bearer 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\" "
            "-H \"Content-Type: application/json\" "
            "-d @_ai_req.json > _ai_resp.txt 2>_ai_err.txt";
        int ret = SystemHidden(curlCmd);
        string response;
        { ifstream f("_ai_resp.txt"); if (f) { response.assign(istreambuf_iterator<char>(f), {}); } }
        remove("_ai_req.json"); remove("_ai_resp.txt"); remove("_ai_err.txt");
        if (response.empty() || ret != 0) continue;
        if (response.find("\"code\":429") != string::npos) { if (m < (int)models.size()-1) continue; }
        if (response.find("\"code\":404") != string::npos ||
            response.find("not found")   != string::npos) continue;
        size_t pos = response.find("\"content\":\"");
        if (pos == string::npos) { if (m < (int)models.size()-1) continue; break; }
        pos += 11;
        string content; bool escaped = false;
        for (size_t i = pos; i < response.size(); i++) {
            char c = response[i];
            if (escaped) {
                if (c=='n') content+='\n'; else if (c=='t') content+='\t';
                else if (c=='"') content+='"'; else if (c=='\\') content+='\\';
                else content+=c; escaped=false;
            } else if (c=='\\') escaped=true;
              else if (c=='"') break;
              else content+=c;
        }
        if (!content.empty()) return content;
    }
    return "Все модели сейчас заняты. Подожди 1-2 минуты и попробуй снова.";
}

// ============================================================
// IMGUI THEME
// ============================================================
static void ApplyDarkTheme() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding    = 6.0f;
    s.FrameRounding     = 4.0f;
    s.ChildRounding     = 6.0f;
    s.ScrollbarRounding = 4.0f;
    s.GrabRounding      = 4.0f;
    s.TabRounding       = 4.0f;
    s.FramePadding      = {8.0f, 6.0f};
    s.ItemSpacing       = {8.0f, 6.0f};
    s.WindowPadding     = {12.0f, 12.0f};
    s.ScrollbarSize     = 10.0f;
    s.IndentSpacing     = 16.0f;
    s.TabBorderSize     = 0.0f;
    s.WindowBorderSize  = 1.0f;
    s.ChildBorderSize   = 1.0f;

    ImVec4* col = s.Colors;
    col[ImGuiCol_WindowBg]             = C::BG;
    col[ImGuiCol_ChildBg]              = C::BG2;
    col[ImGuiCol_PopupBg]              = C::BG3;
    col[ImGuiCol_Border]               = C::BORDER;
    col[ImGuiCol_FrameBg]              = C::BG3;
    col[ImGuiCol_FrameBgHovered]       = {C::BG3.x+0.05f, C::BG3.y+0.05f, C::BG3.z+0.05f, 1.0f};
    col[ImGuiCol_FrameBgActive]        = C::ACCENT_DIM;
    col[ImGuiCol_TitleBg]              = C::SIDEBAR;
    col[ImGuiCol_TitleBgActive]        = C::SIDEBAR;
    col[ImGuiCol_MenuBarBg]            = C::SIDEBAR;
    col[ImGuiCol_ScrollbarBg]          = C::BG;
    col[ImGuiCol_ScrollbarGrab]        = C::BG3;
    col[ImGuiCol_ScrollbarGrabHovered] = C::BORDER;
    col[ImGuiCol_ScrollbarGrabActive]  = C::ACCENT_DIM;
    col[ImGuiCol_CheckMark]            = C::ACCENT;
    col[ImGuiCol_SliderGrab]           = C::ACCENT;
    col[ImGuiCol_SliderGrabActive]     = C::ACCENT;
    col[ImGuiCol_Button]               = C::BG3;
    col[ImGuiCol_ButtonHovered]        = C::ACCENT_DIM;
    col[ImGuiCol_ButtonActive]         = C::ACCENT;
    col[ImGuiCol_Header]               = C::ACCENT_DIM;
    col[ImGuiCol_HeaderHovered]        = {C::ACCENT_DIM.x+0.06f, C::ACCENT_DIM.y+0.06f, C::ACCENT_DIM.z+0.06f, 1.0f};
    col[ImGuiCol_HeaderActive]         = C::ACCENT;
    col[ImGuiCol_Separator]            = C::BORDER;
    col[ImGuiCol_SeparatorHovered]     = C::ACCENT_DIM;
    col[ImGuiCol_SeparatorActive]      = C::ACCENT;
    col[ImGuiCol_ResizeGrip]           = C::BG3;
    col[ImGuiCol_ResizeGripHovered]    = C::ACCENT_DIM;
    col[ImGuiCol_ResizeGripActive]     = C::ACCENT;
    col[ImGuiCol_Tab]                  = C::BG3;
    col[ImGuiCol_TabHovered]           = C::ACCENT_DIM;
    col[ImGuiCol_TabSelected]          = C::ACCENT_DIM;
    col[ImGuiCol_TabSelectedOverline]  = C::ACCENT;
    col[ImGuiCol_TabDimmed]            = C::BG3;
    col[ImGuiCol_TabDimmedSelected]    = C::BG3;
    col[ImGuiCol_Text]                 = C::TEXT;
    col[ImGuiCol_TextDisabled]         = C::TEXT_DIM;
    col[ImGuiCol_PlotLines]            = C::ACCENT;
    col[ImGuiCol_PlotLinesHovered]     = C::ACCENT;
    col[ImGuiCol_PlotHistogram]        = C::ACCENT;
    col[ImGuiCol_PlotHistogramHovered] = C::ACCENT;
    col[ImGuiCol_NavHighlight]         = C::ACCENT;
    col[ImGuiCol_ModalWindowDimBg]     = {0,0,0,0.5f};
}

// ============================================================
// PROGRESS BAR WIDGET
// ============================================================
static void DrawProgressBar(float fraction, float width, float height,
                            ImVec4 fillColor = C::ACCENT, const char* label = nullptr) {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    float w = (width > 0.0f) ? width : ImGui::GetContentRegionAvail().x;
    float h = (height > 0.0f) ? height : ImGui::GetFrameHeight();

    // Background
    draw->AddRectFilled(pos, {pos.x+w, pos.y+h}, ImGui::ColorConvertFloat4ToU32(C::BG3), 4.0f);
    // Fill
    float fill = w * fraction;
    if (fill > 2.0f)
        draw->AddRectFilled(pos, {pos.x+fill, pos.y+h}, ImGui::ColorConvertFloat4ToU32(fillColor), 4.0f);
    // Border
    draw->AddRect(pos, {pos.x+w, pos.y+h}, ImGui::ColorConvertFloat4ToU32(C::BORDER), 4.0f);

    if (label) {
        ImVec2 ts = ImGui::CalcTextSize(label);
        float tx  = pos.x + (w - ts.x) * 0.5f;
        float ty  = pos.y + (h - ts.y) * 0.5f;
        draw->AddText({tx, ty}, ImGui::ColorConvertFloat4ToU32(C::TEXT_BRIGHT), label);
    }
    ImGui::Dummy({w, h});
}

// ============================================================
// SYNTAX HIGHLIGHTER
// ============================================================
static const char* CppKeywords[] = {
    "int","void","bool","char","float","double","long","short","unsigned","signed",
    "const","constexpr","static","inline","extern","auto","register","volatile",
    "if","else","for","while","do","switch","case","default","break","continue",
    "return","goto","sizeof","typedef","struct","union","enum","class","namespace",
    "using","public","private","protected","virtual","override","final","this",
    "new","delete","nullptr","true","false","template","typename","explicit",
    "operator","friend","mutable","try","catch","throw","noexcept","include",
    "string","vector","map","set","cout","cin","endl","std",nullptr
};

static bool IsCppKeyword(const string& token) {
    for (int i = 0; CppKeywords[i]; i++)
        if (token == CppKeywords[i]) return true;
    return false;
}

struct ColoredToken { string text; ImVec4 color; };

static vector<ColoredToken> TokenizeCppLine(const string& line) {
    vector<ColoredToken> tokens;
    size_t i = 0, n = line.size();

    // Preprocessor directive
    if (!line.empty() && line[0] == '#') {
        tokens.push_back({line, C::PREPROC});
        return tokens;
    }

    while (i < n) {
        // Single-line comment
        if (i+1 < n && line[i]=='/' && line[i+1]=='/') {
            tokens.push_back({line.substr(i), C::CMT});
            break;
        }
        // String literal
        if (line[i] == '"') {
            size_t j = i+1;
            while (j < n && !(line[j]=='"' && line[j-1]!='\\')) j++;
            if (j < n) j++;
            tokens.push_back({line.substr(i, j-i), C::STR});
            i = j; continue;
        }
        // Char literal
        if (line[i] == '\'') {
            size_t j = i+1;
            while (j < n && !(line[j]=='\'' && line[j-1]!='\\')) j++;
            if (j < n) j++;
            tokens.push_back({line.substr(i, j-i), C::STR});
            i = j; continue;
        }
        // Number
        if (isdigit((unsigned char)line[i]) ||
            (line[i]=='-' && i+1 < n && isdigit((unsigned char)line[i+1]))) {
            size_t j = i; if(line[j]=='-') j++;
            while (j < n && (isdigit((unsigned char)line[j]) || line[j]=='.' || line[j]=='x' ||
                             (line[j]>='a'&&line[j]<='f') || (line[j]>='A'&&line[j]<='F'))) j++;
            tokens.push_back({line.substr(i, j-i), C::NUM});
            i = j; continue;
        }
        // Identifier / keyword
        if (isalpha((unsigned char)line[i]) || line[i]=='_') {
            size_t j = i;
            while (j < n && (isalnum((unsigned char)line[j]) || line[j]=='_')) j++;
            string tok = line.substr(i, j-i);
            tokens.push_back({tok, IsCppKeyword(tok) ? C::KW : C::TEXT_BRIGHT});
            i = j; continue;
        }
        // Punctuation/operator
        tokens.push_back({string(1, line[i]), C::TEXT_DIM});
        i++;
    }
    return tokens;
}

static void RenderCode(const string& code) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, C::CODE_BG);
    float avail = ImGui::GetContentRegionAvail().x;
    ImGui::BeginChild("##code_block", {avail, 0.0f}, ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Border);
    ImGui::PushStyleColor(ImGuiCol_Text, C::TEXT);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);

    // Split into lines, tokenize each
    istringstream ss(code);
    string line;
    bool first = true;
    while (getline(ss, line)) {
        if (!first) ImGui::NewLine();
        first = false;
        if (line.empty()) { continue; }
        auto tokens = TokenizeCppLine(line);
        bool firstTok = true;
        for (auto& tok : tokens) {
            if (!firstTok) ImGui::SameLine(0.0f, 0.0f);
            firstTok = false;
            ImGui::PushStyleColor(ImGuiCol_Text, tok.color);
            ImGui::TextUnformatted(tok.text.c_str());
            ImGui::PopStyleColor();
        }
    }
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
    ImGui::PopStyleColor();
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================
// THEORY RENDERER — highlights [NEW] sections and separators
// ============================================================
static void RenderTheory(const string& theory) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, C::BG2);
    float avail = ImGui::GetContentRegionAvail().x;
    // Use a scrollable child that fills available height
    ImGui::BeginChild("##theory", {avail, 0.0f}, ImGuiChildFlags_Border);
    ImGui::SetCursorPos({ImGui::GetCursorPosX()+8, ImGui::GetCursorPosY()+6});

    istringstream ss(theory);
    string line;
    while (getline(ss, line)) {
        // [NEW] section header
        if (line.find("[NEW]") != string::npos) {
            ImGui::PushStyleColor(ImGuiCol_Text, C::HEADING);
            // Strip "[NEW] " prefix
            string hdr = line;
            size_t p = hdr.find("[NEW]");
            if (p != string::npos) hdr = hdr.substr(p + 6);
            ImGui::TextUnformatted(("  " + hdr).c_str());
            ImGui::PopStyleColor();
            continue;
        }
        // Separator lines (---... or ===...)
        bool isSep = !line.empty() && (line[0]=='-' || line[0]=='=') && line.size() > 10;
        if (isSep) {
            ImGui::PushStyleColor(ImGuiCol_Separator, C::BORDER);
            ImGui::Separator();
            ImGui::PopStyleColor();
            continue;
        }
        // Code snippet lines (indented or starting with keywords/symbols typical to code)
        bool looksLikeCode = !line.empty() && (
            line[0]=='\t' ||
            (line.size()>2 && line[0]==' ' && line[1]==' ' && line[2]==' ') ||
            line.find("int ") == 0 || line.find("//") != string::npos ||
            line.find("#include") != string::npos || line.find("cout") != string::npos ||
            line.find("return ") != string::npos
        );
        if (looksLikeCode) {
            ImGui::PushStyleColor(ImGuiCol_Text, C::TEXT_BRIGHT);
            auto bg = ImGui::GetWindowDrawList();
            ImVec2 p0 = ImGui::GetCursorScreenPos();
            // Slight highlight for inline code
            ImGui::TextUnformatted(line.c_str());
            (void)bg; (void)p0;
            ImGui::PopStyleColor();
            continue;
        }
        // Empty line
        if (line.empty()) { ImGui::Dummy({0, 4}); continue; }
        // Normal text
        ImGui::PushStyleColor(ImGuiCol_Text, C::TEXT);
        ImGui::TextWrapped("%s", line.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.0f);
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================
// QUIZ RENDERER
// ============================================================
static void RenderQuizPanel(const Lesson& lesson, const string& lessonKey) {
    const auto& questions = lesson.quiz;
    if (questions.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, C::TEXT_DIM);
        ImGui::TextWrapped("Для этого урока тест не предусмотрен.");
        ImGui::PopStyleColor();
        return;
    }

    QuizState& qs = g_app.quiz;

    if (qs.finished) {
        // Result screen
        float pct = questions.empty() ? 0.0f : (float)qs.correct / (float)questions.size();
        ImGui::Dummy({0, 12});
        ImGui::PushStyleColor(ImGuiCol_Text, C::TEXT_BRIGHT);
        ImGui::SetWindowFontScale(1.3f);
        ImGui::TextUnformatted("Тест завершён!");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        ImGui::Dummy({0, 8});
        char buf[64];
        snprintf(buf, sizeof(buf), "Правильно: %d / %d  (%.0f%%)", qs.correct, (int)questions.size(), pct*100.0f);
        DrawProgressBar(pct, ImGui::GetContentRegionAvail().x, 20.0f,
                        pct >= 0.7f ? C::SUCCESS : C::ERROR_C, buf);
        ImGui::Dummy({0, 12});

        if (pct >= 0.7f) {
            ImGui::PushStyleColor(ImGuiCol_Text, C::SUCCESS);
            ImGui::TextUnformatted("Отлично! Урок засчитан.");
            ImGui::PopStyleColor();
            g_progress.completedLessons[lessonKey] = true;
            g_progress.quizScores[lessonKey + "_quiz"] = (int)(pct * 100.0f);
            saveProgress(g_progress);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, C::ERROR_C);
            ImGui::TextUnformatted("Не расстраивайся! Повтори теорию и попробуй ещё раз.");
            ImGui::PopStyleColor();
        }
        ImGui::Dummy({0, 12});
        if (ImGui::Button("Пройти тест снова", {200, 36})) {
            qs = QuizState{};
        }
        return;
    }

    // Current question
    const Question& q = questions[qs.questionIdx];
    ImGui::Dummy({0, 8});
    // Progress indicator
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "Вопрос %d / %d", qs.questionIdx+1, (int)questions.size());
        DrawProgressBar((float)(qs.questionIdx+1)/(float)questions.size(),
                        ImGui::GetContentRegionAvail().x * 0.5f, 16.0f,
                        C::ACCENT, buf);
    }
    ImGui::Dummy({0, 10});

    // Question text
    ImGui::PushStyleColor(ImGuiCol_Text, C::TEXT_BRIGHT);
    ImGui::SetWindowFontScale(1.1f);
    ImGui::TextWrapped("%s", q.text.c_str());
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();
    ImGui::Dummy({0, 10});

    float btnW = ImGui::GetContentRegionAvail().x - 10.0f;

    for (int i = 0; i < (int)q.options.size(); i++) {
        bool isSelected  = (qs.selectedAns == i);
        bool isCorrect   = (i == q.correctAnswer);
        bool showResult  = qs.showResult;

        // Color coding after answer
        if (showResult) {
            if (isCorrect)
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.18f,0.40f,0.18f,1.0f});
            else if (isSelected && !isCorrect)
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.45f,0.12f,0.12f,1.0f});
            else
                ImGui::PushStyleColor(ImGuiCol_Button, C::BG3);
        } else {
            if (isSelected)
                ImGui::PushStyleColor(ImGuiCol_Button, C::ACCENT_DIM);
            else
                ImGui::PushStyleColor(ImGuiCol_Button, C::BG3);
        }

        string label = string("  ") + char('A'+i) + ".  " + q.options[i];
        if (ImGui::Button(label.c_str(), {btnW, 38.0f}) && !showResult) {
            qs.selectedAns = i;
        }
        ImGui::PopStyleColor();
        ImGui::Dummy({0, 2});
    }

    // Check answer / Next button
    ImGui::Dummy({0, 10});
    if (!qs.showResult) {
        if (qs.selectedAns >= 0) {
            if (ImGui::Button("Проверить ответ", {160, 36})) {
                qs.showResult = true;
                if (qs.selectedAns == q.correctAnswer) qs.correct++;
            }
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, C::TEXT_DIM);
            ImGui::TextUnformatted("Выбери ответ...");
            ImGui::PopStyleColor();
        }
    } else {
        // Show explanation
        ImGui::Separator();
        ImGui::Dummy({0, 6});
        if (qs.selectedAns == q.correctAnswer) {
            ImGui::PushStyleColor(ImGuiCol_Text, C::SUCCESS);
            ImGui::TextUnformatted("✓  Правильно!");
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, C::ERROR_C);
            ImGui::TextUnformatted("✗  Неправильно.");
            ImGui::PopStyleColor();
            ImGui::PushStyleColor(ImGuiCol_Text, C::TEXT);
            string correct_opt = string("Правильный ответ: ") + char('A'+q.correctAnswer) + ".  " + q.options[q.correctAnswer];
            ImGui::TextWrapped("%s", correct_opt.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::Dummy({0, 4});
        ImGui::PushStyleColor(ImGuiCol_Text, C::TEXT_DIM);
        ImGui::TextWrapped("Пояснение: %s", q.explanation.c_str());
        ImGui::PopStyleColor();
        ImGui::Dummy({0, 10});

        const char* nextLabel = (qs.questionIdx + 1 < (int)questions.size())
                                    ? "Следующий вопрос  →" : "Завершить тест  ✓";
        if (ImGui::Button(nextLabel, {200, 36})) {
            qs.questionIdx++;
            qs.selectedAns = -1;
            qs.showResult  = false;
            if (qs.questionIdx >= (int)questions.size()) {
                qs.finished = true;
            }
        }
    }
}

// ============================================================
// AI PANEL
// ============================================================
static void RenderAIPanel(const string& contextHint = "") {
    ImGui::Dummy({0, 8});
    ImGui::PushStyleColor(ImGuiCol_Text, C::ACCENT);
    ImGui::SetWindowFontScale(1.2f);
    ImGui::TextUnformatted("AI-Помощник по C++");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();
    ImGui::PushStyleColor(ImGuiCol_Text, C::TEXT_DIM);
    ImGui::TextUnformatted("Задавай любые вопросы по C++. AI ответит на русском.");
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Dummy({0, 8});

    // Response area
    if (!g_app.aiResponse.empty() || !g_app.aiStatus.empty()) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, C::BG3);
        float h = ImGui::GetContentRegionAvail().y - 100.0f;
        if (h < 80.0f) h = 80.0f;
        ImGui::BeginChild("##ai_response", {0, h}, ImGuiChildFlags_Border);

        if (g_app.aiLoading.load()) {
            ImGui::PushStyleColor(ImGuiCol_Text, C::ACCENT);
            ImGui::TextUnformatted("Думаю...");
            ImGui::PopStyleColor();
        } else if (!g_app.aiResponse.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, C::TEXT_BRIGHT);
            ImGui::TextWrapped("%s", g_app.aiResponse.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::Dummy({0, 8});
    }

    // Input
    float inputW = ImGui::GetContentRegionAvail().x - 110.0f;
    ImGui::PushStyleColor(ImGuiCol_FrameBg, C::BG3);
    ImGui::SetNextItemWidth(inputW);
    bool enterPressed = ImGui::InputText("##ai_input", g_app.aiInput, sizeof(g_app.aiInput),
                                         ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PopStyleColor();
    ImGui::SameLine();

    bool canSend = !g_app.aiLoading.load() && g_app.aiInput[0] != '\0';
    if (!canSend) { ImGui::BeginDisabled(); }
    if ((ImGui::Button("Спросить", {100, 0}) || enterPressed) && canSend) {
        string q   = g_app.aiInput;
        string ctx = contextHint;
        g_app.aiInput[0] = '\0';
        g_app.aiResponse.clear();
        g_app.aiLoading.store(true);
        thread([q, ctx]() {
            string r = callAI_GUI(q, ctx);
            {
                lock_guard<mutex> lock(g_app.aiMutex);
                g_app.aiResponse = r;
            }
            g_app.aiLoading.store(false);
        }).detach();
    }
    if (!canSend) { ImGui::EndDisabled(); }
    ImGui::Dummy({0, 4});
    if (!contextHint.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, C::TEXT_DIM);
        ImGui::TextWrapped("Контекст: %s", contextHint.c_str());
        ImGui::PopStyleColor();
    }
}

// ============================================================
// STATISTICS PANEL
// ============================================================
static void RenderStats() {
    ImGui::Dummy({0, 8});
    ImGui::PushStyleColor(ImGuiCol_Text, C::ACCENT);
    ImGui::SetWindowFontScale(1.2f);
    ImGui::TextUnformatted("Статистика прогресса");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Dummy({0, 8});

    // Overall
    int total = 0, done = 0;
    for (auto& t : g_topics) {
        total += (int)t.lessons.size();
        for (int li = 0; li < (int)t.lessons.size(); li++) {
            string key = t.name + "_" + to_string(li);
            if (g_progress.completedLessons.count(key) && g_progress.completedLessons.at(key))
                done++;
        }
    }
    float frac = (total > 0) ? (float)done / total : 0.0f;

    // Streak
    ImGui::PushStyleColor(ImGuiCol_Text, C::STREAK);
    string streakStr = "  Серия: " + to_string(g_progress.streak) + " дней подряд!";
    ImGui::TextUnformatted(streakStr.c_str());
    ImGui::PopStyleColor();
    ImGui::Dummy({0, 6});

    {
        char buf[64];
        snprintf(buf, sizeof(buf), "Общий прогресс: %d / %d уроков", done, total);
        DrawProgressBar(frac, ImGui::GetContentRegionAvail().x * 0.8f, 22.0f, C::ACCENT, buf);
    }
    ImGui::Dummy({0, 16});

    // Per-topic table
    if (ImGui::BeginTable("##stats_table", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Тема",           ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Прогресс",       ImGuiTableColumnFlags_WidthFixed, 160.0f);
        ImGui::TableSetupColumn("Лучший тест",    ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableHeadersRow();

        for (int ti = 0; ti < (int)g_topics.size(); ti++) {
            const Topic& t = g_topics[ti];
            int tdone = 0;
            for (int li = 0; li < (int)t.lessons.size(); li++) {
                string key = t.name + "_" + to_string(li);
                if (g_progress.completedLessons.count(key) && g_progress.completedLessons.at(key))
                    tdone++;
            }
            bool allDone = (tdone == (int)t.lessons.size() && tdone > 0);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (allDone) {
                ImGui::PushStyleColor(ImGuiCol_Text, C::SUCCESS);
                ImGui::TextUnformatted(("✓ " + t.name).c_str());
                ImGui::PopStyleColor();
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, C::TEXT);
                ImGui::TextUnformatted(("  " + t.name).c_str());
                ImGui::PopStyleColor();
            }

            ImGui::TableSetColumnIndex(1);
            {
                float tf = (t.lessons.empty()) ? 0.0f : (float)tdone / t.lessons.size();
                char bar[32]; snprintf(bar, sizeof(bar), "%d/%d", tdone, (int)t.lessons.size());
                DrawProgressBar(tf, 140.0f, 14.0f, allDone ? C::SUCCESS : C::ACCENT, bar);
            }

            ImGui::TableSetColumnIndex(2);
            {
                string qKey = t.name + "_0_quiz";
                if (g_progress.quizScores.count(qKey)) {
                    int sc = g_progress.quizScores.at(qKey);
                    ImVec4 col = (sc >= 70) ? C::SUCCESS : ((sc >= 50) ? C::HEADING : C::ERROR_C);
                    ImGui::PushStyleColor(ImGuiCol_Text, col);
                    string s = to_string(sc) + "%";
                    ImGui::TextUnformatted(s.c_str());
                    ImGui::PopStyleColor();
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Text, C::TEXT_DIM);
                    ImGui::TextUnformatted("—");
                    ImGui::PopStyleColor();
                }
            }
        }
        ImGui::EndTable();
    }
}

// ============================================================
// HOME SCREEN
// ============================================================
static void RenderHome() {
    ImGui::Dummy({0, 20});

    // Big title
    ImGui::PushStyleColor(ImGuiCol_Text, C::ACCENT);
    ImGui::SetWindowFontScale(1.6f);
    float titleW = ImGui::CalcTextSize("C++ Учебник для школьников").x;
    ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - titleW) * 0.5f + ImGui::GetCursorPosX());
    ImGui::TextUnformatted("C++ Учебник для школьников");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Text, C::TEXT_DIM);
    float subtitleW = ImGui::CalcTextSize("Интерактивный курс с AI-помощником").x;
    ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - subtitleW) * 0.5f + ImGui::GetCursorPosX());
    ImGui::TextUnformatted("Интерактивный курс с AI-помощником");
    ImGui::PopStyleColor();

    ImGui::Dummy({0, 16});
    ImGui::Separator();
    ImGui::Dummy({0, 16});

    // Stats overview
    int total = 0, done = 0;
    for (auto& t : g_topics) {
        total += (int)t.lessons.size();
        for (int li = 0; li < (int)t.lessons.size(); li++) {
            string key = t.name + "_" + to_string(li);
            if (g_progress.completedLessons.count(key) && g_progress.completedLessons.at(key)) done++;
        }
    }
    float frac = (total > 0) ? (float)done / total : 0.0f;

    float col3W = (ImGui::GetContentRegionAvail().x - 40.0f) / 3.0f;

    // Card: Progress
    ImGui::PushStyleColor(ImGuiCol_ChildBg, C::BG3);
    ImGui::BeginChild("##card_progress", {col3W, 100.0f}, ImGuiChildFlags_Border);
    ImGui::Dummy({0, 8}); ImGui::SetCursorPosX(ImGui::GetCursorPosX()+8);
    ImGui::PushStyleColor(ImGuiCol_Text, C::ACCENT); ImGui::TextUnformatted("Прогресс"); ImGui::PopStyleColor();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX()+8);
    ImGui::SetWindowFontScale(1.8f);
    string doneStr = to_string(done) + "/" + to_string(total);
    ImGui::PushStyleColor(ImGuiCol_Text, C::TEXT_BRIGHT); ImGui::TextUnformatted(doneStr.c_str()); ImGui::PopStyleColor();
    ImGui::SetWindowFontScale(1.0f);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX()+8);
    ImGui::PushStyleColor(ImGuiCol_Text, C::TEXT_DIM); ImGui::TextUnformatted("уроков пройдено"); ImGui::PopStyleColor();
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::SameLine(0, 16);

    // Card: Streak
    ImGui::PushStyleColor(ImGuiCol_ChildBg, C::BG3);
    ImGui::BeginChild("##card_streak", {col3W, 100.0f}, ImGuiChildFlags_Border);
    ImGui::Dummy({0, 8}); ImGui::SetCursorPosX(ImGui::GetCursorPosX()+8);
    ImGui::PushStyleColor(ImGuiCol_Text, C::STREAK); ImGui::TextUnformatted("Серия дней"); ImGui::PopStyleColor();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX()+8);
    ImGui::SetWindowFontScale(1.8f);
    string streakStr = to_string(g_progress.streak);
    ImGui::PushStyleColor(ImGuiCol_Text, C::TEXT_BRIGHT); ImGui::TextUnformatted(streakStr.c_str()); ImGui::PopStyleColor();
    ImGui::SetWindowFontScale(1.0f);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX()+8);
    ImGui::PushStyleColor(ImGuiCol_Text, C::TEXT_DIM); ImGui::TextUnformatted("дней подряд!"); ImGui::PopStyleColor();
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::SameLine(0, 16);

    // Card: Topics
    ImGui::PushStyleColor(ImGuiCol_ChildBg, C::BG3);
    ImGui::BeginChild("##card_topics", {col3W, 100.0f}, ImGuiChildFlags_Border);
    ImGui::Dummy({0, 8}); ImGui::SetCursorPosX(ImGui::GetCursorPosX()+8);
    ImGui::PushStyleColor(ImGuiCol_Text, C::PURPLE); ImGui::TextUnformatted("Темы"); ImGui::PopStyleColor();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX()+8);
    ImGui::SetWindowFontScale(1.8f);
    string topicsStr = to_string(g_topics.size());
    ImGui::PushStyleColor(ImGuiCol_Text, C::TEXT_BRIGHT); ImGui::TextUnformatted(topicsStr.c_str()); ImGui::PopStyleColor();
    ImGui::SetWindowFontScale(1.0f);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX()+8);
    ImGui::PushStyleColor(ImGuiCol_Text, C::TEXT_DIM); ImGui::TextUnformatted("тем в курсе"); ImGui::PopStyleColor();
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Dummy({0, 16});

    // Overall progress bar
    {
        char buf[64]; snprintf(buf, sizeof(buf), "%.0f%% курса завершено", frac*100.0f);
        DrawProgressBar(frac, ImGui::GetContentRegionAvail().x, 24.0f, C::ACCENT, buf);
    }

    ImGui::Dummy({0, 20});

    // "Start" / "Continue" button
    const char* btnLabel = (done > 0) ? "Продолжить обучение" : "Начать обучение";
    float btnW = 220.0f, btnH = 44.0f;
    ImGui::PushStyleColor(ImGuiCol_Button,        C::ACCENT_DIM);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, C::ACCENT);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  C::ACCENT);
    ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - btnW) * 0.5f + ImGui::GetCursorPosX());
    if (ImGui::Button(btnLabel, {btnW, btnH})) {
        g_app.screen  = Screen::LESSON;
        // Find first incomplete topic
        g_app.topicIdx  = 0;
        g_app.lessonIdx = 0;
        for (int ti = 0; ti < (int)g_topics.size(); ti++) {
            bool anyIncomplete = false;
            for (int li = 0; li < (int)g_topics[ti].lessons.size(); li++) {
                string key = g_topics[ti].name + "_" + to_string(li);
                if (!g_progress.completedLessons.count(key) || !g_progress.completedLessons.at(key)) {
                    anyIncomplete = true;
                    g_app.topicIdx  = ti;
                    g_app.lessonIdx = li;
                    break;
                }
            }
            if (anyIncomplete) break;
        }
        g_app.lessonTab = LessonTab::THEORY;
        g_app.quiz = QuizState{};
    }
    ImGui::PopStyleColor(3);
}

// ============================================================
// LESSON VIEW
// ============================================================
static void RenderLessonView() {
    if (g_app.topicIdx < 0 || g_app.topicIdx >= (int)g_topics.size()) return;
    const Topic& topic = g_topics[g_app.topicIdx];
    bool showFinal = g_app.showFinalTask;

    // Breadcrumb
    ImGui::PushStyleColor(ImGuiCol_Text, C::TEXT_DIM);
    ImGui::TextUnformatted("Темы  /");
    ImGui::SameLine();
    ImGui::PopStyleColor();
    ImGui::PushStyleColor(ImGuiCol_Text, C::ACCENT);
    ImGui::TextUnformatted(topic.name.c_str());
    if (!showFinal && g_app.lessonIdx >= 0 && g_app.lessonIdx < (int)topic.lessons.size()) {
        ImGui::SameLine();
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Text, C::TEXT_DIM);
        ImGui::TextUnformatted("/");
        ImGui::SameLine();
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Text, C::TEXT_BRIGHT);
        ImGui::TextUnformatted(topic.lessons[g_app.lessonIdx].title.c_str());
    }
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Dummy({0, 6});

    // Final task view
    if (showFinal) {
        ImGui::PushStyleColor(ImGuiCol_Text, C::HEADING);
        ImGui::SetWindowFontScale(1.15f);
        ImGui::TextUnformatted("Финальное задание");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
        ImGui::Dummy({0, 8});
        ImGui::PushStyleColor(ImGuiCol_Text, C::TEXT_BRIGHT);
        ImGui::TextWrapped("%s", topic.finalTask.description.c_str());
        ImGui::PopStyleColor();
        ImGui::Dummy({0, 8});
        ImGui::PushStyleColor(ImGuiCol_Text, C::HEADING);
        ImGui::TextUnformatted("Требования:");
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Text, C::TEXT);
        ImGui::TextWrapped("%s", topic.finalTask.requirements.c_str());
        ImGui::PopStyleColor();
        ImGui::Dummy({0, 8});
        ImGui::PushStyleColor(ImGuiCol_Text, C::TEXT_DIM);
        ImGui::TextWrapped("Подсказка: %s", topic.finalTask.hint.c_str());
        ImGui::PopStyleColor();
        ImGui::Dummy({0, 12});
        // Mark as done
        string taskKey = "final_" + topic.name;
        if (ImGui::Button("Отметить выполненным  ✓", {240, 36})) {
            g_progress.completedLessons[taskKey] = true;
            saveProgress(g_progress);
        }
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, C::TEXT_DIM);
        if (g_progress.completedLessons.count(taskKey) && g_progress.completedLessons.at(taskKey)) {
            ImGui::TextUnformatted("  ✓ Выполнено");
        }
        ImGui::PopStyleColor();
        return;
    }

    if (g_app.lessonIdx < 0 || g_app.lessonIdx >= (int)topic.lessons.size()) return;
    const Lesson& lesson = topic.lessons[g_app.lessonIdx];
    string lessonKey     = topic.name + "_" + to_string(g_app.lessonIdx);

    // Lesson tabs
    ImGui::BeginChild("##lesson_content", {0, 0}, ImGuiChildFlags_None);

    if (ImGui::BeginTabBar("##lesson_tabs")) {
        if (ImGui::BeginTabItem("  Теория  ")) {
            ImGui::Dummy({0, 4});
            // Mark lesson as read when theory is viewed
            g_progress.completedLessons[lessonKey] = true;
            RenderTheory(lesson.theory);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("  Код  ")) {
            ImGui::Dummy({0, 4});
            RenderCode(lesson.codeExample);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("  Тест  ")) {
            ImGui::Dummy({0, 4});
            RenderQuizPanel(lesson, lessonKey);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("  AI-вопрос  ")) {
            ImGui::Dummy({0, 4});
            RenderAIPanel("Тема урока: " + lesson.title);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();
}

// ============================================================
// SIDEBAR
// ============================================================
static void RenderSidebar() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, C::SIDEBAR);
    ImGui::BeginChild("##sidebar", {g_app.sidebarW, 0.0f}, ImGuiChildFlags_None);

    // App logo / title
    ImGui::Dummy({0, 10});
    ImGui::PushStyleColor(ImGuiCol_Text, C::ACCENT);
    ImGui::SetWindowFontScale(1.1f);
    ImGui::SetCursorPosX(10.0f);
    ImGui::TextUnformatted("C++  Учебник");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();
    ImGui::SetCursorPosX(10.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, C::TEXT_DIM);
    ImGui::TextUnformatted("для школьников");
    ImGui::PopStyleColor();

    ImGui::Dummy({0, 4});
    ImGui::Separator();

    // Streak
    if (g_progress.streak > 0) {
        ImGui::Dummy({0, 4});
        ImGui::SetCursorPosX(10.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, C::STREAK);
        string s = "  " + to_string(g_progress.streak) + " дней подряд";
        ImGui::TextUnformatted(s.c_str());
        ImGui::PopStyleColor();
    }

    // Mini progress bar
    {
        int total = 0, done = 0;
        for (auto& t : g_topics) {
            total += (int)t.lessons.size();
            for (int li = 0; li < (int)t.lessons.size(); li++) {
                string key = t.name + "_" + to_string(li);
                if (g_progress.completedLessons.count(key) && g_progress.completedLessons.at(key)) done++;
            }
        }
        ImGui::Dummy({0, 4});
        ImGui::SetCursorPosX(8.0f);
        float barW = g_app.sidebarW - 16.0f;
        float frac = (total > 0) ? (float)done / total : 0.0f;
        char buf[32]; snprintf(buf, sizeof(buf), "%d/%d", done, total);
        DrawProgressBar(frac, barW, 12.0f, C::ACCENT, nullptr);
        ImGui::SetCursorPosX(10.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, C::TEXT_DIM);
        ImGui::TextUnformatted(buf);
        ImGui::PopStyleColor();
    }

    ImGui::Dummy({0, 4});
    ImGui::Separator();
    ImGui::Dummy({0, 4});

    // Navigation buttons
    auto NavBtn = [&](const char* label, Screen target, ImVec4 activeCol) {
        bool active = (g_app.screen == target);
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, C::ACCENT_DIM);
        else         ImGui::PushStyleColor(ImGuiCol_Button, {0,0,0,0});
        ImGui::PushStyleColor(ImGuiCol_Text, active ? C::TEXT_BRIGHT : C::TEXT);
        ImGui::SetCursorPosX(4.0f);
        if (ImGui::Button(label, {g_app.sidebarW - 8.0f, 30.0f})) {
            g_app.screen = target;
            if (target == Screen::AI) {
                g_app.aiResponse.clear();
            }
        }
        ImGui::PopStyleColor(2);
    };

    NavBtn("  Главная", Screen::HOME, C::ACCENT_DIM);
    NavBtn("  AI-Помощник", Screen::AI, C::ACCENT_DIM);
    NavBtn("  Статистика", Screen::STATS, C::ACCENT_DIM);

    ImGui::Dummy({0, 4});
    ImGui::Separator();
    ImGui::Dummy({0, 4});

    // Topic list
    ImGui::PushStyleColor(ImGuiCol_Text, C::TEXT_DIM);
    ImGui::SetCursorPosX(10.0f);
    ImGui::TextUnformatted("ТЕМЫ");
    ImGui::PopStyleColor();
    ImGui::Dummy({0, 2});

    for (int ti = 0; ti < (int)g_topics.size(); ti++) {
        const Topic& t = g_topics[ti];
        int tdone = 0;
        for (int li = 0; li < (int)t.lessons.size(); li++) {
            string key = t.name + "_" + to_string(li);
            if (g_progress.completedLessons.count(key) && g_progress.completedLessons.at(key)) tdone++;
        }
        bool allDone   = (tdone == (int)t.lessons.size() && tdone > 0);
        bool isActive  = (g_app.screen == Screen::LESSON && g_app.topicIdx == ti);

        if (isActive)  ImGui::PushStyleColor(ImGuiCol_Button, C::ACCENT_DIM);
        else            ImGui::PushStyleColor(ImGuiCol_Button, {0,0,0,0});

        ImVec4 textCol = allDone ? C::SUCCESS : (isActive ? C::TEXT_BRIGHT : C::TEXT);
        ImGui::PushStyleColor(ImGuiCol_Text, textCol);
        ImGui::SetCursorPosX(4.0f);

        string label = (allDone ? " ✓  " : "    ") + to_string(ti+1) + ". " + t.name;
        if (ImGui::Button(label.c_str(), {g_app.sidebarW - 8.0f, 26.0f})) {
            g_app.screen       = Screen::LESSON;
            g_app.topicIdx     = ti;
            g_app.lessonIdx    = 0;
            g_app.lessonTab    = LessonTab::THEORY;
            g_app.showFinalTask= false;
            g_app.quiz         = QuizState{};
        }
        ImGui::PopStyleColor(2);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================
// LESSON NAVIGATION PANEL (left sub-sidebar when in lesson)
// ============================================================
static void RenderLessonNav() {
    if (g_app.topicIdx < 0 || g_app.topicIdx >= (int)g_topics.size()) return;
    const Topic& topic = g_topics[g_app.topicIdx];

    ImGui::PushStyleColor(ImGuiCol_ChildBg, C::BG2);
    ImGui::BeginChild("##lesson_nav", {180.0f, 0.0f}, ImGuiChildFlags_Border);
    ImGui::Dummy({0, 6});

    ImGui::PushStyleColor(ImGuiCol_Text, C::TEXT_DIM);
    ImGui::SetCursorPosX(8.0f);
    ImGui::TextUnformatted("УРОКИ");
    ImGui::PopStyleColor();
    ImGui::Dummy({0, 2});

    for (int li = 0; li < (int)topic.lessons.size(); li++) {
        const Lesson& L = topic.lessons[li];
        string key = topic.name + "_" + to_string(li);
        bool done   = g_progress.completedLessons.count(key) && g_progress.completedLessons.at(key);
        bool active = (g_app.lessonIdx == li && !g_app.showFinalTask);

        if (active)  ImGui::PushStyleColor(ImGuiCol_Button, C::ACCENT_DIM);
        else         ImGui::PushStyleColor(ImGuiCol_Button, {0,0,0,0});
        ImGui::PushStyleColor(ImGuiCol_Text, done ? C::SUCCESS : (active ? C::TEXT_BRIGHT : C::TEXT));
        ImGui::SetCursorPosX(4.0f);

        string lbl = (done ? "✓ " : "  ") + to_string(li+1) + ". " + L.title;
        if (ImGui::Button(lbl.c_str(), {172.0f, 26.0f})) {
            g_app.lessonIdx    = li;
            g_app.showFinalTask= false;
            g_app.lessonTab    = LessonTab::THEORY;
            g_app.quiz         = QuizState{};
        }
        ImGui::PopStyleColor(2);
    }

    // Final task
    ImGui::Dummy({0, 4});
    ImGui::Separator();
    {
        string taskKey = "final_" + topic.name;
        bool done   = g_progress.completedLessons.count(taskKey) && g_progress.completedLessons.at(taskKey);
        bool active = g_app.showFinalTask;
        if (active)  ImGui::PushStyleColor(ImGuiCol_Button, C::ACCENT_DIM);
        else         ImGui::PushStyleColor(ImGuiCol_Button, {0,0,0,0});
        ImGui::PushStyleColor(ImGuiCol_Text, done ? C::SUCCESS : (active ? C::TEXT_BRIGHT : C::HEADING));
        ImGui::SetCursorPosX(4.0f);
        string lbl = (done ? "✓ " : "  ") + string("Фин. задание");
        if (ImGui::Button(lbl.c_str(), {172.0f, 26.0f})) {
            g_app.showFinalTask = true;
            g_app.lessonIdx     = -1;
        }
        ImGui::PopStyleColor(2);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================
// MAIN RENDER FUNCTION
// ============================================================
static void RenderMainUI() {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
    ImGui::Begin("##root", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize    |
        ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar();

    // ── Header ──────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, C::SIDEBAR);
    ImGui::BeginChild("##header", {0.0f, 44.0f}, ImGuiChildFlags_None);
    ImGui::SetCursorPos({16.0f, 10.0f});
    ImGui::PushStyleColor(ImGuiCol_Text, C::TEXT_BRIGHT);
    ImGui::TextUnformatted("C++ Учебник");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::SetCursorPosY(10.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, C::TEXT_DIM);
    ImGui::TextUnformatted("—  интерактивный курс для школьников");
    ImGui::PopStyleColor();

    // Right side: streak + version
    ImGui::SameLine(io.DisplaySize.x - 260.0f);
    ImGui::SetCursorPosY(10.0f);
    if (g_progress.streak > 0) {
        ImGui::PushStyleColor(ImGuiCol_Text, C::STREAK);
        string ss = "  " + to_string(g_progress.streak) + " дней подряд";
        ImGui::TextUnformatted(ss.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();
    }
    ImGui::PushStyleColor(ImGuiCol_Text, C::TEXT_DIM);
    ImGui::TextUnformatted("ImGui UI v1.0");
    ImGui::PopStyleColor();
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // ── Body ────────────────────────────────────────────────
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
    ImGui::BeginChild("##body", {0.0f, 0.0f}, ImGuiChildFlags_None);
    ImGui::PopStyleVar();

    RenderSidebar();
    ImGui::SameLine(0.0f, 0.0f);

    // Lesson sub-nav (only in lesson mode)
    if (g_app.screen == Screen::LESSON && g_app.topicIdx >= 0) {
        RenderLessonNav();
        ImGui::SameLine(0.0f, 0.0f);
    }

    // Main content area
    ImGui::PushStyleColor(ImGuiCol_ChildBg, C::BG);
    ImGui::BeginChild("##main_content", {0.0f, 0.0f}, ImGuiChildFlags_None);
    ImGui::SetCursorPos({16.0f, 12.0f});
    ImGui::BeginChild("##main_inner", {0.0f, 0.0f}, ImGuiChildFlags_None);

    switch (g_app.screen) {
    case Screen::HOME:   RenderHome();        break;
    case Screen::LESSON: RenderLessonView();  break;
    case Screen::AI:     RenderAIPanel();     break;
    case Screen::STATS:  RenderStats();       break;
    }

    ImGui::EndChild();
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::EndChild(); // body
    ImGui::End();
}

// ============================================================
// DX11 HELPERS
// ============================================================
bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount        = 2;
    sd.BufferDesc.Width   = 0;
    sd.BufferDesc.Height  = 0;
    sd.BufferDesc.Format  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags                = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage          = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow         = hWnd;
    sd.SampleDesc.Count     = 1;
    sd.SampleDesc.Quality   = 0;
    sd.Windowed             = TRUE;
    sd.SwapEffect           = DXGI_SWAP_EFFECT_DISCARD;

    UINT createFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL levels[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    if (D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            createFlags, levels, 2, D3D11_SDK_VERSION, &sd,
            &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
        return false;
    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain)        { g_pSwapChain->Release();         g_pSwapChain         = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release();  g_pd3dDeviceContext  = nullptr; }
    if (g_pd3dDevice)        { g_pd3dDevice->Release();         g_pd3dDevice         = nullptr; }
}

void CreateRenderTarget() {
    ID3D11Texture2D* backBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (backBuffer) {
        g_pd3dDevice->CreateRenderTargetView(backBuffer, nullptr, &g_mainRenderTargetView);
        backBuffer->Release();
    }
}

void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    switch (msg) {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED) return 0;
        g_ResizeWidth  = LOWORD(lParam);
        g_ResizeHeight = HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ============================================================
// WinMain
// ============================================================
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    // Setup console for UTF-8 (hidden but useful for debugging)
    SetConsoleCP(65001);
    SetConsoleOutputCP(65001);

    // Load + init data
    srand((unsigned)time(nullptr));
    g_progress = loadProgress();
    updateStreak(g_progress);
    g_topics   = createTopics();
    g_progress.totalLessons = 0;
    for (auto& t : g_topics) g_progress.totalLessons += (int)t.lessons.size();

    // Register window class
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L,
                       GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr,
                       L"CppLearnImGui", nullptr };
    RegisterClassExW(&wc);

    // Create window (1280x800 default, resizable)
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName,
        L"C++ Учебник для школьников — ImGui",
        WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800,
        nullptr, nullptr, wc.hInstance, nullptr);
    g_hwnd = hwnd;

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        MessageBoxW(nullptr, L"Не удалось создать DirectX 11 устройство.", L"Ошибка", MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    // ImGui setup
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Try to load a Cyrillic-capable font from Windows
    {
        ImFontConfig cfg;
        cfg.OversampleH = 2;
        cfg.OversampleV = 2;
        static const ImWchar ranges[] = {
            0x0020, 0x00FF, // Latin + Basic Latin
            0x0400, 0x04FF, // Cyrillic
            0,
        };
        const char* fontPaths[] = {
            "C:\\Windows\\Fonts\\segoeui.ttf",
            "C:\\Windows\\Fonts\\arial.ttf",
            "C:\\Windows\\Fonts\\tahoma.ttf",
            nullptr
        };
        bool loaded = false;
        for (int i = 0; fontPaths[i] && !loaded; i++) {
            if (GetFileAttributesA(fontPaths[i]) != INVALID_FILE_ATTRIBUTES) {
                io.Fonts->AddFontFromFileTTF(fontPaths[i], 16.0f, &cfg, ranges);
                // Also load a slightly larger version for headings
                loaded = true;
            }
        }
        if (!loaded) {
            io.Fonts->AddFontDefault();
        }
    }

    ApplyDarkTheme();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Main loop
    bool running = true;
    while (running) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) running = false;
        }
        if (!running) break;

        // Handle window resize
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        RenderMainUI();

        ImGui::Render();
        const float clearColor[4] = {
            C::BG.x * C::BG.w, C::BG.y * C::BG.w, C::BG.z * C::BG.w, C::BG.w
        };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clearColor);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0); // vsync
    }

    // Save progress on exit
    saveProgress(g_progress);

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}
