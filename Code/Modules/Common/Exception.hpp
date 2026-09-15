//
// Created by Jake Rieger on 9/14/2026.
//

#pragma once

#include "Signature.hpp"

#include <format>
#include <exception>

namespace Xen {
    inline std::string
    FormatExceptionMessage(const std::string& Signature, const std::string& ExceptionType, const std::string& Message) {
        return std::format("({}) in {}: {}", ExceptionType, Signature, Message);
    }

    class EngineException : public std::exception {
    public:
        explicit EngineException(const std::string& Message) : _Message(Message) {}
        const char* what() const noexcept override { return _Message.c_str(); }

    private:
        std::string _Message;
    };
}  // namespace Xen

#define _DefineEngineException(Type)                                                                                   \
    class Type : public Xen::EngineException {                                                                         \
    public:                                                                                                            \
        using EngineException::EngineException;                                                                        \
    };

#define _ThrowEngineException(Type, Message)                                                                           \
    const auto __MsgFmt_##Type = Xen::FormatExceptionMessage(_SignatureHere, #Type, Message);                          \
    _LogCritical(__MsgFmt_##Type.c_str());                                                                             \
    throw Type(__MsgFmt_##Type)