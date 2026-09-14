//
// Created by Jake Rieger on 9/14/2026.
//

#pragma once

#include "Signature.hpp"

#include <format>
#include <exception>

namespace Xen {
    class EngineException : public std::exception {
    public:
        explicit EngineException(const std::string& ExceptionType, const std::string& Message) {
            _Message = std::format("({}) in {}: {}", ExceptionType, _SignatureHere, Message);
        }

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

#define _ThrowEngineException(Type, Message) throw Type(#Type, Message)