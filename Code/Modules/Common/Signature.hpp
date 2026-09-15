//
// Created by Jake Rieger on 9/14/2026.
//

#pragma once

#include <source_location>
#include <string>
#include <string_view>

namespace Signature {
    namespace detail {
        constexpr bool IsIdent(char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
        }

        constexpr std::string_view RTrim(std::string_view s) {
            while (!s.empty() && s.back() == ' ')
                s.remove_suffix(1);
            return s;
        }

        // True if `s` ends with the complete token "operator".
        constexpr bool EndsWithOperator(std::string_view s) {
            constexpr std::string_view kw = "operator";
            if (s.size() < kw.size()) return false;
            if (s.substr(s.size() - kw.size()) != kw) return false;
            return s.size() == kw.size() || !IsIdent(s[s.size() - kw.size() - 1]);
        }

        // Index of the '(' that opens the parameter list.
        constexpr std::size_t FindParamOpen(std::string_view s) {
            int angle = 0;
            for (std::size_t i = 0; i < s.size(); ++i) {
                const char c = s[i];
                if (c == '<') {
                    ++angle;
                } else if (c == '>') {
                    if (angle > 0) --angle;
                } else if (c == '(' && angle == 0) {
                    // "operator()" and "operator()(...)": these parens are the name.
                    if (EndsWithOperator(RTrim(s.substr(0, i)))) {
                        const std::size_t close = s.find(')', i);
                        if (close == std::string_view::npos) break;
                        i = close;
                        continue;
                    }
                    return i;
                }
            }
            return std::string_view::npos;
        }

        constexpr std::size_t MatchClose(std::string_view s, std::size_t open) {
            int depth = 0;
            for (std::size_t i = open; i < s.size(); ++i) {
                if (s[i] == '(') ++depth;
                else if (s[i] == ')' && --depth == 0) return i;
            }
            return std::string_view::npos;
        }

        // Walk left from the parameter list to the first character of the
        // (possibly qualified, possibly templated) function name.
        constexpr std::size_t FindNameStart(std::string_view s, std::size_t open) {
            std::size_t i = open;
            int angle = 0, paren = 0, brack = 0;
            while (i > 0) {
                const char c = s[--i];
                if (c == '>') ++angle;
                else if (c == '<') {
                    if (angle > 0) --angle;
                } else if (c == ')') ++paren;
                else if (c == '(') {
                    if (paren > 0) --paren;
                } else if (c == ']') ++brack;
                else if (c == '[') {
                    if (brack > 0) --brack;
                } else if (c == ' ' && angle == 0 && paren == 0 && brack == 0) {
                    const std::string_view head = RTrim(s.substr(0, i));
                    // "operator new", "operator int", "operator co_await": the space
                    // is part of the name, so keep going.
                    if (EndsWithOperator(head)) {
                        i = head.size();
                        continue;
                    }
                    return i + 1;
                }
            }
            return 0;
        }

        // Drop a trailing " [with T = int; U = char]" (GCC) or " [T = int]" (Clang).
        constexpr std::string_view StripTemplateTail(std::string_view s) {
            if (s.empty() || s.back() != ']') return s;
            int depth = 0;
            for (std::size_t i = s.size(); i-- > 0;) {
                if (s[i] == ']') ++depth;
                else if (s[i] == '[' && --depth == 0) return (i > 0 && s[i - 1] == ' ') ? RTrim(s.substr(0, i)) : s;
            }
            return s;
        }

    }  // namespace detail

    // Core transformation. constexpr, allocation-free, returns views into `full`.
    constexpr std::string_view TrimSignature(std::string_view raw) {
        const std::string_view full = detail::StripTemplateTail(raw);
        // GCC spells lambdas "main()::<lambda(int)>" -- no return type to strip.
        if (!full.empty() && full.back() == '>' && full.find("<lambda") != std::string_view::npos) return full;
        const std::size_t open = detail::FindParamOpen(full);
        if (open == std::string_view::npos) return full;  // unknown format
        const std::size_t close = detail::MatchClose(full, open);
        if (close == std::string_view::npos) return full;
        const std::size_t start = detail::FindNameStart(full, open);
        return full.substr(start, close + 1 - start);
    }

    // Just the qualified name, no parameter list: "Widget::draw".
    constexpr std::string_view QualifiedName(std::string_view raw) {
        const std::string_view full = detail::StripTemplateTail(raw);
        const std::size_t open      = detail::FindParamOpen(full);
        if (open == std::string_view::npos) return full;
        const std::size_t start = detail::FindNameStart(full, open);
        return full.substr(start, open - start);
    }

    // MSVC's __FUNCSIG__ spells out elaborated type specifiers and pointer sizes.
    inline std::string TidyMSVC(std::string_view s) {
        std::string out(s);
        for (std::string_view junk : {"class ",
                                      "struct ",
                                      "enum ",
                                      "union ",
                                      " __ptr64",
                                      "__cdecl ",
                                      "__thiscall ",
                                      "__stdcall ",
                                      "__fastcall "}) {
            for (std::size_t p = out.find(junk); p != std::string::npos; p = out.find(junk, p)) {
                out.erase(p, junk.size());
            }
        }
        return out;
    }

    // Convenience: signature of the *calling* function.
    //   void f() { std::string s = Signature::Here(); }   // -> "f()"
    inline std::string Here(const std::source_location& loc = std::source_location::current()) {
        return TidyMSVC(TrimSignature(loc.function_name()));
    }
}  // namespace Signature

#if defined(_MSC_VER) && !defined(__clang__)
    #define _SignatureRaw __FUNCSIG__
#else
    #define _SignatureRaw __PRETTY_FUNCTION__
#endif
#define _SignatureHere (::Signature::TidyMSVC(::Signature::TrimSignature(_SignatureRaw)))
