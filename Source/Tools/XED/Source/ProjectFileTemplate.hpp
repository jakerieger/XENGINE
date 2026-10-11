//
// Created by Jake Rieger on 10/1/2026.
//

#pragma once

#include <Common/XenCommon.hpp>

namespace Xen {
    inline std::string ParseTemplate(const std::string& Template,
                                     const std::unordered_map<std::string, std::string>& Vars) {
        std::string Result;
        Result.reserve(Template.size());
        size_t Pos = 0;

        for (;;) {
            const size_t Start = Template.find("${", Pos);
            if (Start == std::string::npos) break;
            const size_t End = Template.find('}', Start + 2);
            if (End == std::string::npos) break;

            Result.append(Template, Pos, Start - Pos);  // Text before "${"
            std::string Key = Template.substr(Start + 2, End - Start - 2);

            auto It = Vars.find(Key);
            if (It != Vars.end()) Result += It->second;
            else Result.append(Template, Start, End - Start + 1);  // Unknown var - leave as is

            Pos = End + 1;
        }

        Result.append(Template, Pos, std::string::npos);
        return Result;
    }

    namespace CMakeSourceEditor {
        namespace detail {
            constexpr size_t NPos = std::string_view::npos;

            struct Token {
                std::string value;  // argument text without quotes / brackets
                size_t begin = 0;   // offset of the first character of the argument
                size_t end   = 0;   // offset just past the argument
            };

            struct Call {
                std::vector<Token> args;
                size_t nameBegin  = 0;
                size_t closeParen = 0;
            };

            inline bool IsSpace(char c) {
                return c == ' ' || c == '\t' || c == '\r' || c == '\n';
            }
            inline bool IsIdentStart(char c) {
                return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
            }
            inline bool IsIdentChar(char c) {
                return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
            }

            inline bool EqualsNoCase(std::string_view a, std::string_view b) {
                return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
                           return std::tolower(static_cast<unsigned char>(x)) ==
                                  std::tolower(static_cast<unsigned char>(y));
                       });
            }

            // If t[i] starts a bracket opener ("[[", "[=[", "[==[", ...) returns the number of '='.
            inline std::optional<size_t> BracketOpen(std::string_view t, size_t i) {
                if (i >= t.size() || t[i] != '[') return std::nullopt;
                size_t j = i + 1;
                while (j < t.size() && t[j] == '=')
                    ++j;
                if (j < t.size() && t[j] == '[') return j - i - 1;
                return std::nullopt;
            }

            // Returns the offset just past the bracket closer, or npos if unterminated.
            inline size_t SkipBracket(std::string_view t, size_t i, size_t eq) {
                const std::string closer = "]" + std::string(eq, '=') + "]";
                const size_t close       = t.find(closer, i + eq + 2);
                return close == NPos ? NPos : close + closer.size();
            }

            // t[i] == '#'. Returns the offset where the comment ends.
            inline size_t SkipComment(std::string_view t, size_t i) {
                if (auto eq = BracketOpen(t, i + 1)) {
                    const size_t end = SkipBracket(t, i + 1, *eq);
                    return end == NPos ? t.size() : end;
                }
                const size_t nl = t.find('\n', i);
                return nl == NPos ? t.size() : nl;
            }

            // Parses the argument list whose '(' is at openParen. Returns false if unbalanced.
            inline bool ParseArgs(std::string_view t, size_t openParen, Call& call) {
                int depth = 1;
                size_t i  = openParen + 1;
                while (i < t.size()) {
                    const char c = t[i];
                    if (IsSpace(c)) {
                        ++i;
                        continue;
                    }
                    if (c == '#') {
                        i = SkipComment(t, i);
                        continue;
                    }
                    if (c == '(') {
                        ++depth;
                        ++i;
                        continue;
                    }
                    if (c == ')') {
                        if (--depth == 0) {
                            call.closeParen = i;
                            return true;
                        }
                        ++i;
                        continue;
                    }

                    Token tok;
                    tok.begin = i;
                    if (c == '"') {
                        size_t j = i + 1;
                        while (j < t.size() && t[j] != '"')
                            j += (t[j] == '\\') ? 2 : 1;
                        if (j >= t.size()) return false;
                        tok.value = std::string(t.substr(i + 1, j - i - 1));
                        i         = j + 1;
                    } else if (auto eq = BracketOpen(t, i)) {
                        const size_t contentBegin = i + *eq + 2;
                        const size_t end          = SkipBracket(t, i, *eq);
                        if (end == NPos) return false;
                        tok.value = std::string(t.substr(contentBegin, end - (*eq + 2) - contentBegin));
                        i         = end;
                    } else {
                        size_t j = i;
                        while (j < t.size() && !IsSpace(t[j]) && t[j] != '(' && t[j] != ')' && t[j] != '#' &&
                               t[j] != '"')
                            j += (t[j] == '\\') ? 2 : 1;
                        j         = std::min(j, t.size());
                        tok.value = std::string(t.substr(i, j - i));
                        i         = j;
                    }
                    tok.end = i;
                    call.args.push_back(std::move(tok));
                }
                return false;
            }

            // Finds the first call to `command` whose first argument is `target`.
            inline std::optional<Call>
            FindCall(std::string_view t, std::string_view command, std::string_view target, std::string& error) {
                size_t i = 0;
                while (i < t.size()) {
                    const char c = t[i];
                    if (c == '#') {
                        i = SkipComment(t, i);
                        continue;
                    }
                    if (!IsIdentStart(c)) {
                        ++i;
                        continue;
                    }

                    const size_t nameBegin = i;
                    while (i < t.size() && IsIdentChar(t[i]))
                        ++i;
                    const std::string_view name = t.substr(nameBegin, i - nameBegin);

                    size_t j = i;
                    while (j < t.size() && (t[j] == ' ' || t[j] == '\t'))
                        ++j;
                    if (j >= t.size() || t[j] != '(') continue;

                    Call call;
                    call.nameBegin = nameBegin;
                    if (!ParseArgs(t, j, call)) {
                        error = "Unbalanced parentheses or unterminated string in call to '" + std::string(name) + "'.";
                        return std::nullopt;
                    }
                    if (EqualsNoCase(name, command) && !call.args.empty() && call.args[0].value == target) return call;
                    i = call.closeParen + 1;
                }
                error = "No " + std::string(command) + "(" + std::string(target) + " ...) call found.";
                return std::nullopt;
            }

            inline std::string NormalizePath(std::string s) {
                std::replace(s.begin(), s.end(), '\\', '/');
                while (s.rfind("./", 0) == 0)
                    s.erase(0, 2);
                return s;
            }

            inline std::string FormatArg(std::string_view s) {
                if (!s.empty() && s.find_first_of(" \t\r\n()#\";\\") == NPos) return std::string(s);
                std::string out = "\"";
                for (char c : s) {
                    if (c == '"' || c == '\\') out += '\\';
                    out += c;
                }
                return out + "\"";
            }

            inline size_t LineStart(std::string_view t, size_t pos) {
                if (pos == 0) return 0;
                const size_t nl = t.rfind('\n', pos - 1);
                return nl == NPos ? 0 : nl + 1;
            }

            inline std::string LeadingWhitespace(std::string_view t, size_t lineStart) {
                size_t j = lineStart;
                while (j < t.size() && (t[j] == ' ' || t[j] == '\t'))
                    ++j;
                return std::string(t.substr(lineStart, j - lineStart));
            }
        }  // namespace detail

        // Edits CMake text in memory. Sources already listed (or repeated in `sources`)
        // are skipped, so calling this twice is harmless. Returns false and sets
        // `error` if the call can't be found or the file is malformed.
        inline bool AddSourcesToText(std::string& text,
                                     std::string_view target,
                                     const std::vector<std::string>& sources,
                                     std::string& error,
                                     std::string_view command       = "xen_add_game_executable",
                                     std::string_view defaultIndent = "        ") {
            using namespace detail;

            const auto call = FindCall(text, command, target, error);
            if (!call) return false;

            std::vector<std::string> existing;
            for (size_t a = 1; a < call->args.size(); ++a)
                existing.push_back(NormalizePath(call->args[a].value));

            std::vector<std::string> toAdd;
            for (const auto& src : sources) {
                std::string n = NormalizePath(src);
                if (n.empty()) continue;
                if (std::find(existing.begin(), existing.end(), n) != existing.end() ||
                    std::find(toAdd.begin(), toAdd.end(), n) != toAdd.end())
                    continue;
                toAdd.push_back(std::move(n));
            }
            if (toAdd.empty()) return true;

            const std::string eol         = text.find("\r\n") != NPos ? "\r\n" : "\n";
            const Token& last             = call->args.back();
            const size_t newlineAfterLast = text.find('\n', last.end);

            std::string insertion;
            size_t insertAt;
            if (newlineAfterLast != NPos && newlineAfterLast < call->closeParen) {
                // Multi-line call: one source per line, right after the last argument's line,
                // using that line's indentation.
                const size_t lastLine    = LineStart(text, last.begin);
                const size_t cmdLine     = LineStart(text, call->nameBegin);
                const std::string indent = (lastLine != cmdLine)
                                             ? LeadingWhitespace(text, lastLine)
                                             : LeadingWhitespace(text, cmdLine) + std::string(defaultIndent);
                insertAt                 = newlineAfterLast + 1;
                for (const auto& s : toAdd)
                    insertion += indent + FormatArg(s) + eol;
            } else {
                // Single-line call: append after the last argument.
                insertAt = last.end;
                for (const auto& s : toAdd)
                    insertion += " " + FormatArg(s);
            }

            text.insert(insertAt, insertion);
            return true;
        }

        // Reads `cmakeFile`, adds the sources, and writes it back (via a temp file
        // so a failed write can't leave a half-written CMakeLists.txt).
        inline bool AddSourcesToFile(const std::filesystem::path& cmakeFile,
                                     std::string_view target,
                                     const std::vector<std::string>& sources,
                                     std::string& error,
                                     std::string_view command = "xen_add_game_executable") {
            std::string text;
            {
                std::ifstream in(cmakeFile, std::ios::binary);
                if (!in) {
                    error = "Could not open " + cmakeFile.string();
                    return false;
                }
                text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
            }

            const std::string original = text;
            if (!AddSourcesToText(text, target, sources, error, command)) return false;
            if (text == original) return true;  // nothing new to add

            std::filesystem::path tmp = cmakeFile;
            tmp += ".tmp";
            {
                std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
                out.write(text.data(), static_cast<std::streamsize>(text.size()));
                if (!out) {
                    error = "Could not write " + tmp.string();
                    return false;
                }
            }

            std::error_code ec;
            std::filesystem::rename(tmp, cmakeFile, ec);
            if (ec) {
                std::filesystem::remove(tmp, ec);
                error = "Could not replace " + cmakeFile.string() + ": " + ec.message();
                return false;
            }
            return true;
        }
    }  // namespace CMakeSourceEditor
}  // namespace Xen