/*  The Clipboard Project - Cut, copy, and paste anything, anywhere, all from the terminal.
    Copyright (C) 2023 Jackson Huff and other contributors on GitHub.com
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.*/
#pragma once
#include <array>
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <regex>
#include <string_view>
#include <thread>
#include <vector>

#if defined(_WIN32) || defined(_WIN64)
#include <io.h>
#include <shlobj.h>
#include <windows.h>
#endif

#include <clipboard/fork.hpp>
#include <clipboard/gui.hpp>

namespace fs = std::filesystem;

#if !defined(GIT_COMMIT_HASH)
#define GIT_COMMIT_HASH "not available"
#endif

#if !defined(CLIPBOARD_VERSION)
#define CLIPBOARD_VERSION "(version not available)"
#endif

extern Forker forker;

struct GlobalFilepaths {
    fs::path temporary;
    fs::path persistent;
    fs::path main;
    fs::path home;
};
extern GlobalFilepaths global_path;

struct Constants {
    std::string_view clipboard_version = CLIPBOARD_VERSION;
    std::string_view clipboard_commit = GIT_COMMIT_HASH;
    std::string_view data_file_name = "rawdata.clipboard";
    std::string_view default_clipboard_name = "0";
    std::string_view temporary_directory_name = "Clipboard";
    std::string_view persistent_directory_name = ".clipboard";
    std::string_view original_files_name = "originals";
    std::string_view notes_name = "notes";
    std::string_view mime_name = "mime";
    std::string_view lock_name = "lock";
    std::string_view data_directory = "data";
    std::string_view metadata_directory = "metadata";
    std::string_view import_export_directory = "Exported_Clipboards";
    std::string_view ignore_regex_name = "ignore";
};
constexpr Constants constants;

enum class CopyPolicy { ReplaceAll, ReplaceOnce, SkipOnce, SkipAll, Unknown };

struct Copying {
    bool use_safe_copy = true;
    CopyPolicy policy = CopyPolicy::Unknown;
    fs::copy_options opts = fs::copy_options::overwrite_existing | fs::copy_options::recursive | fs::copy_options::copy_symlinks;
    std::vector<fs::path> items;
    std::vector<std::pair<std::string, std::error_code>> failedItems;
    std::string buffer;
    std::string mime;
};
extern Copying copying;

bool isPersistent(const auto& clipboard) {
    return clipboard.find_first_of("_") != std::string::npos;
}

static auto thisPID() {
#if defined(_WIN32) || defined(_WIN64)
    return GetCurrentProcessId();
#elif defined(__linux__) || defined(__unix__) || defined(__APPLE__)
    return getpid();
#endif
}

std::string fileContents(const fs::path& path);
std::vector<std::string> fileLines(const fs::path& path);

size_t writeToFile(const fs::path& path, const std::string& content, bool append = false);

class Clipboard {
    fs::path root;
    std::string this_name;

public:
    bool is_persistent = false;

    class DataDirectory {
        fs::path root;

    public:
        fs::path raw;

        operator fs::path() { return root; }
        operator fs::path() const { return root; }
        auto operator=(const auto& other) { return root = other; }
        auto operator/(const auto& other) { return root / other; }
    } data;

    class MetadataDirectory {
        fs::path root;

    public:
        fs::path notes;
        fs::path originals;
        fs::path lock;
        fs::path ignore;
        operator fs::path() { return root; }
        operator fs::path() const { return root; }
        auto operator=(const auto& other) { return root = other; }
        auto operator/(const auto& other) { return root / other; }
    } metadata;

    Clipboard() = default;
    Clipboard(const auto& clipboard_name) {
        this_name = clipboard_name;

        is_persistent = isPersistent(this_name) || getenv("CLIPBOARD_ALWAYS_PERSIST");

        root = (is_persistent ? global_path.persistent : global_path.temporary) / this_name;

        data = root / constants.data_directory;

        metadata = root / constants.metadata_directory;

        data.raw = data / constants.data_file_name;

        metadata.notes = metadata / constants.notes_name;

        metadata.originals = metadata / constants.original_files_name;

        metadata.lock = metadata / constants.lock_name;

        metadata.ignore = metadata / constants.ignore_regex_name;

        fs::create_directories(data);
        fs::create_directories(metadata);
    }
    operator fs::path() { return root; }
    operator fs::path() const { return root; }
    auto operator=(const auto& other) { return root = other; }
    auto operator/(const auto& other) { return root / other; }
    std::string string() { return root.string(); }
    bool holdsData() {
        if (!fs::exists(data) || fs::is_empty(data)) return false;
        if (fs::exists(data.raw) && fs::is_empty(data.raw)) return false;
        return true;
    }
    bool holdsRawData() { return fs::exists(data.raw) && !fs::is_empty(data.raw); }
    bool holdsIgnoreRegexes() { return fs::exists(metadata.ignore) && !fs::is_empty(metadata.ignore); }
    std::vector<std::regex> ignoreRegexes() {
        std::vector<std::regex> regexes;
        if (!holdsIgnoreRegexes()) return regexes;
        for (const auto& line : fileLines(metadata.ignore))
            regexes.emplace_back(line);
        return regexes;
    }
    void applyIgnoreRegexes() {
        if (!holdsIgnoreRegexes()) return;
        auto regexes = ignoreRegexes();
        if (holdsRawData())
            for (const auto& regex : regexes)
                writeToFile(data.raw, std::regex_replace(fileContents(data.raw), regex, ""));
        else
            for (const auto& regex : regexes)
                for (const auto& entry : fs::directory_iterator(data))
                    if (std::regex_match(entry.path().filename().string(), regex)) fs::remove_all(entry.path());
    }
    bool isUnused() {
        if (holdsData()) return false;
        if (fs::exists(metadata.notes) && !fs::is_empty(metadata.notes)) return false;
        if (fs::exists(metadata.originals) && !fs::is_empty(metadata.originals)) return false;
        return true;
    }
    bool isLocked() { return fs::exists(metadata.lock); }
    void getLock() {
        if (isLocked()) {
            auto pid = std::stoi(fileContents(metadata.lock));
#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
            if (getpgrp() == getpgid(pid)) return; // if we're in the same process group, we're probably in a self-referencing pipe like cb | cb
#endif
            while (true) {
#if defined(_WIN32) || defined(_WIN64)
                if (WaitForSingleObject(OpenProcess(SYNCHRONIZE, FALSE, pid), 0) == WAIT_OBJECT_0) break;
#elif defined(__linux__) || defined(__unix__) || defined(__APPLE__)
                if (kill(pid, 0) == -1) break;
#endif
                if (!isLocked()) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }
        }
        writeToFile(metadata.lock, std::to_string(thisPID()));
    }
    void releaseLock() { fs::remove(metadata.lock); }
    std::string name() { return this_name; }
};
extern Clipboard path;

extern std::vector<std::string> arguments;

extern std::string clipboard_invocation;

extern std::string clipboard_name;

extern std::string locale;

extern bool output_silent;
extern bool progress_silent;
extern bool confirmation_silent;
extern bool no_color;
extern bool no_emoji;
extern bool all_option;

extern std::string preferred_mime;
extern std::vector<std::string> available_mimes;

enum class ProgressState : int { Done, Active, Cancel };

extern std::condition_variable cv;
extern std::mutex m;
extern std::atomic<ProgressState> progress_state;
static std::thread indicator;

struct Successes {
    std::atomic<unsigned long> files;
    std::atomic<unsigned long> directories;
    std::atomic<unsigned long long> bytes;
    std::atomic<unsigned long> clipboards;
};
extern Successes successes;

struct IsTTY {
    bool in = true;
    bool out = true;
    bool err = true;
};
extern IsTTY is_tty;

enum class Action : unsigned int { Cut, Copy, Paste, Clear, Show, Edit, Add, Remove, Note, Swap, Status, Info, Load, Import, Export, History, Ignore };

extern Action action;

enum class IOType : unsigned int { File, Pipe, Text };

extern IOType io_type;

template <typename T, size_t N>
class EnumArray : public std::array<T, N> {
public:
    T& operator[](Action index) { return std::array<T, N>::operator[](static_cast<unsigned int>(index)); } // switch to std::to_underlying when available
};

extern EnumArray<std::string_view, 17> actions;
extern EnumArray<std::string_view, 17> action_shortcuts;
extern EnumArray<std::string_view, 17> doing_action;
extern EnumArray<std::string_view, 17> did_action;

extern std::array<std::pair<std::string_view, std::string_view>, 7> colors;

class TerminalSize {
public:
    size_t rows;
    size_t columns;
    TerminalSize(const unsigned int& rows, const unsigned int& columns) : rows {std::max(1u, rows)}, columns {std::max(1u, columns)} {}
};

static std::string formatMessage(const std::string_view& str, bool colorful = !no_color) {
    std::string temp(str); // a string to do scratch work on
    auto replaceThis = [&](const std::string_view& str, const std::string_view& with) {
        for (size_t i = 0; (i = temp.find(str, i)) != std::string::npos; i += with.length())
            temp.replace(i, str.length(), with);
    };
    for (const auto& key : colors) // iterate over all the possible colors to replace
        replaceThis(key.first, colorful ? key.second : "");
    if (no_emoji) {
        replaceThis("✅", "✓");
        replaceThis("❌", "✗");
        replaceThis("🟡", "-");
        replaceThis("💡", "•");
        replaceThis("🔷", "•");
    }
    return temp;
}

void incrementSuccessesForItem(const auto& item) {
    fs::is_directory(item) ? successes.directories++ : successes.files++;
}

class Message {
private:
    std::string_view internal_message;

public:
    Message(const auto& message) : internal_message(std::move(message)) {}
    std::string operator()() const { return std::move(formatMessage(internal_message)); }
    size_t rawLength() const { return std::regex_replace(std::string(internal_message), std::regex("\\[[a-z]+\\]"), "").length(); }
};

std::string formatNumbers(const auto& num) {
    static std::stringstream ss;
    ss.str(std::string());
    ss << std::fixed << std::setprecision(2);
    ss << num;
    return ss.str();
}

std::string formatBytes(const auto& bytes) {
    if (bytes < (1024 * 10.0)) return formatNumbers(bytes) + "B";
    if (bytes < (1024 * 1024 * 10.0)) return formatNumbers(bytes / 1024.0) + "kB";
    if (bytes < (1024 * 1024 * 1024 * 10.0)) return formatNumbers(bytes / (1024.0 * 1024.0)) + "MB";
    return formatNumbers(bytes / (1024.0 * 1024.0 * 1024.0)) + "GB";
}

void setLanguagePT();
void setLanguageTR();
void setLanguageES();
void setupHandlers();
void setLocale();
void showHelpMessage(int& argc, char* argv[]);
void setupItems(int& argc, char* argv[]);
void setClipboardName(int& argc, char* argv[]);
void setupVariables(int& argc, char* argv[]);
void createTempDirectory();
void syncWithGUIClipboard(bool force = false);
void syncWithGUIClipboard(const std::string& text);
void syncWithGUIClipboard(const ClipboardPaths& clipboard);
void showClipboardContents();
void setupAction(int& argc, char* argv[]);
void checkForNoItems();
bool stopIndicator(bool change_condition_variable);
void startIndicator();
void setupIndicator();
void deduplicateItems();
unsigned long long totalItemSize();
bool stopIndicator(bool change_condition_variable = true);
void checkItemSize();
TerminalSize thisTerminalSize();
void clearData(bool force_clear);
void copyFiles();
void removeOldFiles();
bool userIsARobot();
void pasteFiles();
void clearClipboard();
void performAction();
void updateGUIClipboard(bool force = false);
std::string pipedInContent();
void showFailures();
void showSuccesses();
[[nodiscard]] CopyPolicy userDecision(const std::string& item);
void setTheme(const std::string& theme);

extern Message help_message;
extern Message check_clipboard_status_message;
extern Message clipboard_item_one_contents_message;
extern Message clipboard_item_many_contents_message;
extern Message clipboard_text_contents_message;
extern Message no_clipboard_contents_message;
extern Message clipboard_action_prompt;
extern Message no_valid_action_message;
extern Message choose_action_items_message;
extern Message fix_redirection_action_message;
extern Message redirection_no_items_message;
extern Message paste_success_message;
extern Message clear_success_message;
extern Message clear_fail_message;
extern Message clipboard_failed_one_message;
extern Message clipboard_failed_many_message;
extern Message and_more_fails_message;
extern Message and_more_items_message;
extern Message fix_problem_message;
extern Message not_enough_storage_message;
extern Message item_already_exists_message;
extern Message bad_response_message;
extern Message working_message;
extern Message cancelled_message;
extern Message cancelled_with_progress_message;
extern Message byte_success_message;
extern Message one_item_success_message;
extern Message many_files_success_message;
extern Message many_directories_success_message;
extern Message one_file_one_directory_success_message;
extern Message one_file_many_directories_success_message;
extern Message many_files_one_directory_success_message;
extern Message many_files_many_directories_success_message;
extern Message one_clipboard_success_message;
extern Message many_clipboards_success_message;
extern Message clipboard_name_message;
extern Message internal_error_message;

extern ClipboardContent getGUIClipboard(const std::string& requested_mime);
extern void writeToGUIClipboard(const ClipboardContent& clipboard);
extern const bool GUIClipboardSupportsCut;

namespace PerformAction {
void copyItem(const fs::path& f);
void copy();
void copyText();
void paste();
void pipeIn();
void pipeOut();
void clear();
void show();
void edit();
void addData();
void addText();
void removeFiles();
void removeRegex();
void noteText();
void notePipe();
void swap();
void addFiles();
void status();
void info();
void load();
void showFilepaths();
void swap();
void importClipboards();
void exportClipboards();
void infoJSON();
void ignoreRegex();
void statusJSON();
} // namespace PerformAction