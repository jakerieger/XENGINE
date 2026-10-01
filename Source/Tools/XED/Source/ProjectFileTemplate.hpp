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
}  // namespace Xen