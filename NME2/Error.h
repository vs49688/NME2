#pragma once

#include <QString>

#include <stdexcept>

class FormatError : public std::runtime_error {
    public:
    FormatError(const char* err) : std::runtime_error(err) { }
    FormatError(QString err) : std::runtime_error(err.toStdString()) { }
};

class DATFormatError : public FormatError {
    public:
    DATFormatError(const char* err) : FormatError(err) { }
    DATFormatError(QString err) : FormatError(err.toStdString().c_str()) { }
};

class USMFormatError : public FormatError {
    public:
    USMFormatError(const char* err) : FormatError(err) { }
    USMFormatError(QString err) : FormatError(err.toStdString().c_str()) { }
};