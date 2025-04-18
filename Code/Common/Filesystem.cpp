// Author: Jake Rieger
// Created: 12/12/24.
//

#include <sstream>
#include "Filesystem.hpp"

#ifdef _WIN32
    // Windows does not define the S_ISREG and S_ISDIR macros in stat.h, so we do.
    // We have to define _CRT_INTERNAL_NONSTDC_NAMES 1 before #including sys/stat.h
    // in order for Microsoft's stat.h to define names like S_IFMT, S_IFREG, and S_IFDIR,
    // rather than just defining  _S_IFMT, _S_IFREG, and _S_IFDIR as it normally does.
    #define _CRT_INTERNAL_NONSTDC_NAMES 1
    #include <Windows.h>
    #include <sys/stat.h>
    #if !defined(S_ISREG) && defined(S_IFMT) && defined(S_IFREG)
        #define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
    #endif
    #if !defined(S_ISDIR) && defined(S_IFMT) && defined(S_IFDIR)
        #define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
    #endif
#else
    #include <sys/stat.h>
#endif

namespace x {
#pragma region FileReader
    std::vector<u8> FileReader::ReadAllBytes(const Path& path) {
        std::ifstream file(path.Str(), std::ios::binary | std::ios::ate);
        if (!file.is_open()) { return {}; }
        const std::streamsize fileSize = file.tellg();
        std::vector<u8> bytes(fileSize);
        file.seekg(0, std::ios::beg);
        if (!file.read(reinterpret_cast<char*>(bytes.data()), fileSize)) { return {}; }
        file.close();
        return bytes;
    }

    str FileReader::ReadAllText(const Path& path) {
        const std::ifstream file(path.Str());
        if (!file.is_open()) { return {}; }
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    std::vector<str> FileReader::ReadAllLines(const Path& path) {
        std::ifstream file(path.Str());
        std::vector<str> lines;
        if (!file.is_open()) { return {}; }
        str line;
        while (std::getline(file, line)) {
            lines.push_back(line);
        }
        return lines;
    }

    std::vector<u8> FileReader::ReadBlock(const Path& path, size_t size, u64 offset) {
        std::ifstream file(path.Str(), std::ios::binary | std::ios::ate);
        if (!file) { return {}; }
        const std::streamsize fileSize = file.tellg();
        if (offset >= (u64)fileSize || size == 0 || offset + size > (u64)fileSize) { return {}; }
        file.seekg((std::streamsize)offset, std::ios::beg);
        if (!file) { return {}; }
        std::vector<u8> buffer(size);
        file.read(reinterpret_cast<char*>(buffer.data()), (std::streamsize)size);
        if (!file) { return {}; }
        return buffer;
    }

    size_t FileReader::QueryFileSize(const Path& path) {
        std::ifstream file(path.Str(), std::ios::binary | std::ios::ate);
        if (!file.is_open()) { return 0; }
        const std::streamsize fileSize = file.tellg();
        return fileSize;
    }
#pragma endregion

#pragma region FileWriter
    bool FileWriter::WriteAllBytes(const Path& path, const std::vector<u8>& data) {
        std::ofstream file(path.Str(), std::ios::binary | std::ios::trunc);
        // Overwrite existing file
        if (!file) return false;
        file.write(RCAST<const char*>(data.data()), CAST<std::streamsize>(data.size()));
        return file.good();
    }

    bool FileWriter::WriteAllText(const Path& path, const str& text) {
        std::ofstream file(path.Str(), std::ios::out | std::ios::trunc);
        if (!file) return false;
        str outText = text;
        if (outText.empty()) { return false; }
        // Enforce newline for final text value
        if (outText.back() != '\n') { outText += '\n'; }
        file << outText;
        return file.good();
    }

    bool FileWriter::WriteAllLines(const Path& path, const std::vector<str>& lines) {
        std::ofstream file(path.Str(), std::ios::out | std::ios::trunc);
        if (!file) return false;
        for (const auto& line : lines) {
            file << line << '\n';
            if (!file.good()) { return false; }
        }
        return file.good();
    }

    bool FileWriter::WriteBlock(const Path& path, const std::vector<u8>& data, u64 offset) {
        std::ofstream file(path.Str(),
                           std::ios::binary | std::ios::in | std::ios::out);  // Open in binary read/write mode
        if (!file) return false;
        file.seekp(CAST<std::streampos>(offset), std::ios::beg);
        // seek to offset
        if (!file) return false;  // Failed to seek
        file.write(RCAST<const char*>(data.data()), CAST<std::streamsize>(data.size()));
        return file.good();
    }

    std::future<std::vector<u8>> AsyncFileReader::ReadAllBytes(const Path& path) {
        return runAsync([path]() { return FileReader::ReadAllBytes(path); });
    }

    std::future<str> AsyncFileReader::ReadAllText(const Path& path) {
        return runAsync([path]() { return FileReader::ReadAllText(path); });
    }

    std::future<std::vector<str>> AsyncFileReader::ReadAllLines(const Path& path) {
        return runAsync([path]() { return FileReader::ReadAllLines(path); });
    }

    std::future<std::vector<u8>> AsyncFileReader::ReadBlock(const Path& path, size_t size, u64 offset) {
        return runAsync([path, size, offset]() { return FileReader::ReadBlock(path, size, offset); });
    }

    std::future<bool> AsyncFileWriter::WriteAllBytes(const Path& path, const std::vector<u8>& data) {
        return runAsync([path, data]() { return FileWriter::WriteAllBytes(path, data); });
    }

    std::future<bool> AsyncFileWriter::WriteAllText(const Path& path, const str& text) {
        return runAsync([path, text]() { return FileWriter::WriteAllText(path, text); });
    }

    std::future<bool> AsyncFileWriter::WriteAllLines(const Path& path, const std::vector<str>& lines) {
        return runAsync([path, lines]() { return FileWriter::WriteAllLines(path, lines); });
    }

    std::future<bool> AsyncFileWriter::WriteBlock(const Path& path, const std::vector<u8>& data, u64 offset) {
        return runAsync([path, data, offset]() { return FileWriter::WriteBlock(path, data, offset); });
    }
#pragma endregion

#pragma region Stream IO
    StreamReader::StreamReader(const Path& path) : _stream(path.Str(), std::ios::binary | std::ios::ate) {
        if (_stream.is_open()) {
            _size = CAST<u64>(_stream.tellg());
            _stream.seekg(0, std::ios::beg);
        } else {
            _size = 0;
        }
    }

    StreamReader::~StreamReader() {
        Close();
    }

    StreamReader::StreamReader(StreamReader&& other) noexcept : _stream(std::move(other._stream)), _size(other._size) {
        other._size = 0;
    }

    StreamReader& StreamReader::operator=(StreamReader&& other) noexcept {
        if (this != &other) {
            Close();
            _stream     = std::move(other._stream);
            _size       = other._size;
            other._size = 0;
        }
        return *this;
    }

    bool StreamReader::Read(vector<u8>& data, size_t size) {
        if (!IsOpen() || size == 0) return false;
        const auto currentPos = Position();
        if (currentPos + size > _size) { size = CAST<size_t>(_size - currentPos); }
        data.resize(size);
        _stream.read(RCAST<char*>(data.data()), size);
        return _stream.good();
    }

    bool StreamReader::ReadAll(vector<u8>& data) {
        if (!IsOpen()) return false;

        const auto size = Size();
        if (size == 0) {
            data.clear();
            return true;
        }

        Seek(0);
        data.resize(CAST<size_t>(size));
        _stream.read(RCAST<char*>(data.data()), size);
        return _stream.good();
    }

    bool StreamReader::ReadLine(str& line) {
        if (!IsOpen()) return false;
        return CAST<bool>(std::getline(_stream, line));
    }

    bool StreamReader::IsOpen() const {
        return _stream.is_open() && _stream.good();
    }

    bool StreamReader::Seek(u64 offset) {
        if (!IsOpen()) return false;
        _stream.seekg(offset);
        return _stream.good();
    }

    u64 StreamReader::Position() {
        if (!IsOpen()) return 0;
        return CAST<u64>(_stream.tellg());
    }

    u64 StreamReader::Size() const {
        return _size;
    }

    void StreamReader::Close() {
        if (_stream.is_open()) { _stream.close(); }
    }

    StreamWriter::StreamWriter(const Path& path, bool append)
        : _stream(path.Str(), std::ios::binary | (append ? std::ios::app : std::ios::trunc)) {}

    StreamWriter::~StreamWriter() {
        Close();
    }

    StreamWriter::StreamWriter(StreamWriter&& other) noexcept : _stream(std::move(other._stream)) {}

    StreamWriter& StreamWriter::operator=(StreamWriter&& other) noexcept {
        if (this != &other) {
            Close();
            _stream = std::move(other._stream);
        }
        return *this;
    }

    bool StreamWriter::Write(const vector<u8>& buffer) {
        return Write(buffer, buffer.size());
    }

    bool StreamWriter::Write(const vector<u8>& buffer, size_t size) {
        if (!IsOpen() || size == 0) return false;
        if (size > buffer.size()) size = buffer.size();
        _stream.write(RCAST<cstr>(buffer.data()), size);
        return _stream.good();
    }

    bool StreamWriter::WriteLine(const str& line) {
        if (!IsOpen()) return false;
        _stream << line << '\n';
        return _stream.good();
    }

    bool StreamWriter::Flush() {
        if (!IsOpen()) return false;
        _stream.flush();
        return _stream.good();
    }

    bool StreamWriter::IsOpen() const {
        return _stream.is_open() && _stream.good();
    }

    bool StreamWriter::Seek(u64 offset) {
        if (!IsOpen()) return false;
        _stream.seekp(offset);
        return _stream.good();
    }

    u64 StreamWriter::Position() {
        if (!IsOpen()) return 0;
        return CAST<u64>(_stream.tellp());
    }

    void StreamWriter::Close() {
        if (_stream.is_open()) {
            _stream.flush();
            _stream.close();
        }
    }
#pragma endregion

#pragma region Path
    Path Path::Current() {
        char buffer[MAX_PATH];
        ::GetModuleFileNameA(nullptr, buffer, MAX_PATH);
        const str::size_type pos = str(buffer).find_last_of("\\/");
        return Path(str(buffer).substr(0, pos));
    }

    Path Path::Parent() const {
        const size_t lastSeparator = path.find_last_of(PATH_SEPARATOR);
        if (lastSeparator == std::string::npos || lastSeparator == 0) { return Path(std::to_string(PATH_SEPARATOR)); }
        return Path(path.substr(0, lastSeparator));
    }

    bool Path::Exists() const {
        struct stat info {};
        return stat(path.c_str(), &info) == 0;
    }

    bool Path::IsFile() const {
        struct stat info {};
        if (stat(path.c_str(), &info) != 0) {
            std::perror(path.c_str());
            return false;
        }
        return S_ISREG(info.st_mode);
    }

    bool Path::IsDirectory() const {
        struct stat info {};
        if (stat(path.c_str(), &info) != 0) {
            std::perror(path.c_str());
            return false;
        }
        return S_ISDIR(info.st_mode);
    }

    bool Path::HasExtension() const {
        const size_t pos = path.find_last_of('.');
        const size_t sep = path.find_last_of(PATH_SEPARATOR);
        return pos != str::npos && (sep == str::npos || pos > sep);
    }

    str Path::Extension() const {
        if (!HasExtension()) { return ""; }
        return path.substr(path.find_last_of('.') + 1);
    }

    Path Path::ReplaceExtension(const str& ext) const {
        if (!HasExtension()) return Path(path + "." + ext);
        return Path(path.substr(0, path.find_last_of('.')) + "." + ext);
    }

    Path Path::Join(const str& subPath) const {
        return Path(Join(path, subPath));
    }

    Path Path::operator/(const str& subPath) const {
        return Path(Join(path, subPath));
    }

    str Path::Str() const {
        return path;
    }

    const char* Path::CStr() const {
        return path.c_str();
    }

    str Path::Filename() const {
        size_t pos = path.rfind('\\');
        if (pos != str::npos) { return path.substr(pos + 1); }
        pos = path.rfind('/');
        if (pos != str::npos) { return path.substr(pos + 1); }
        return path;
    }

    Path Path::RelativeTo(const Path& basePath) const {
        const str thisStr = Str();
        str baseStr       = basePath.Str();

        if (!baseStr.empty() && baseStr.back() == PATH_SEPARATOR) { baseStr += PATH_SEPARATOR; }
        if (thisStr.substr(0, baseStr.length()) != baseStr) { return *this; }

        const str relativePath = thisStr.substr(baseStr.length());

        if (relativePath.empty()) { return Path("."); }

        return Path(relativePath);
    }

    bool Path::operator==(const Path& other) const {
        return path == other.path;
    }

    bool Path::Create() const {
        if (Exists()) return true;

#ifdef _WIN32
        if (!CreateDirectoryA(path.c_str(), nullptr)) {
            const DWORD error = GetLastError();
            if (error != ERROR_ALREADY_EXISTS) { return false; }
        }
#else
        if (mkdir(path.c_str(), 0755) != 0) {
            if (errno != EEXIST) { return false; }
        }
#endif
        return true;
    }

    bool Path::CreateAll() const {
        if (Exists()) return true;

        if (path != str(1, PATH_SEPARATOR)) {
            Path parentPath = Parent();
            if (!parentPath.Exists()) {
                if (!parentPath.CreateAll()) return false;
            }
        }

        return Create();
    }

    bool Path::Copy(const Path& dest) const {
        X_ASSERT(IsFile());
        if (dest == *this) { return true; }
        if (!::CopyFileA(path.c_str(), dest.path.c_str(), FALSE)) { return false; }
        return true;
    }

    bool Path::CopyDirectory(const Path& dest) const {
        X_ASSERT(IsDirectory());

        const DWORD srcAttrs = ::GetFileAttributesA(path.c_str());
        if (srcAttrs == INVALID_FILE_ATTRIBUTES) { return false; }
        if (!(srcAttrs & FILE_ATTRIBUTE_DIRECTORY)) { return false; }

        if (!::CreateDirectoryA(dest.CStr(), nullptr) && ::GetLastError() != ERROR_ALREADY_EXISTS) { return false; }

        const str searchPattern = path + "\\*";
        WIN32_FIND_DATAA findData;
        const HANDLE hFind = ::FindFirstFileA(searchPattern.c_str(), &findData);
        if (hFind == INVALID_HANDLE_VALUE) { return false; }

        bool success {true};
        do {
            if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) { continue; }

            Path srcPath  = Join(findData.cFileName);
            Path destPath = dest / findData.cFileName;

            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (!srcPath.CopyDirectory(destPath)) { success = false; }
            } else {
                if (!srcPath.Copy(destPath)) { success = false; }
            }
        } while (::FindNextFileA(hFind, &findData));

        const DWORD lastError = ::GetLastError();
        ::FindClose(hFind);

        if (lastError != ERROR_NO_MORE_FILES) { success = false; }

        return success;
    }

    DirectoryEntries Path::Entries() const {
        return DirectoryEntries(*this);
    }

    str Path::Join(const str& lhs, const str& rhs) {
        if (lhs.empty()) { return lhs; }
        if (rhs.empty()) { return rhs; }
        if (lhs.back() == PATH_SEPARATOR) return lhs + rhs;
        return lhs + PATH_SEPARATOR + rhs;
    }

    str Path::Normalize(const str& rawPath) {
        str result;
        std::vector<str> parts;
        size_t start = 0;
        while (start < rawPath.size()) {
            size_t end = rawPath.find(PATH_SEPARATOR, start);
            if (end == std::string::npos) { end = rawPath.size(); }
            str part = rawPath.substr(start, end - start);
            if (part == ".." && !parts.empty() && parts.back() != "..") {
                parts.pop_back();
            } else if (!part.empty() && part != ".") {
                parts.push_back(part);
            }
            start = end + 1;
        }
        for (const auto& part : parts) {
            result += PATH_SEPARATOR + part;
        }

#ifdef _WIN32
        // Remove the first '/' if Windows path
        result = result.substr(1, result.size() - 1);
#endif

        return result.empty() ? str(1, PATH_SEPARATOR) : result;
    }

    FindHandleWrapper::FindHandleWrapper() : mHandle(INVALID_HANDLE_VALUE) {}

    FindHandleWrapper::FindHandleWrapper(HANDLE handle) : mHandle(handle) {}

    FindHandleWrapper::~FindHandleWrapper() {
        if (mHandle != INVALID_HANDLE_VALUE) { ::FindClose(mHandle); }
    }

    FindHandleWrapper::FindHandleWrapper(FindHandleWrapper&& other) noexcept : mHandle(other.mHandle) {
        other.mHandle = INVALID_HANDLE_VALUE;
    }

    FindHandleWrapper& FindHandleWrapper::operator=(FindHandleWrapper&& other) noexcept {
        if (this != &other) {
            if (mHandle != INVALID_HANDLE_VALUE) { ::FindClose(mHandle); }
            mHandle       = other.mHandle;
            other.mHandle = INVALID_HANDLE_VALUE;
        }
        return *this;
    }

    HANDLE FindHandleWrapper::Get() const {
        return mHandle;
    }

    bool FindHandleWrapper::IsValid() const {
        return mHandle != INVALID_HANDLE_VALUE;
    }

    DirectoryEntries::DirectoryEntries(const Path& path) : mPath(path) {}

    DirectoryIterator::DirectoryIterator() : mIsEnd(true) {}

    DirectoryIterator::DirectoryIterator(const Path& path) : mIsEnd(false), mRoot(path) {
        if (!mRoot.IsDirectory()) {
            mIsEnd = true;
            return;
        }

        str searchPattern = mRoot.Str() + "\\*";
        WIN32_FIND_DATA findData;
        mFindHandle = FindHandleWrapper(::FindFirstFileA(searchPattern.c_str(), &findData));

        if (!mFindHandle.IsValid()) {
            mIsEnd = true;
            return;
        }

        ProcessCurrentEntry(findData);
    }

    DirectoryIterator::reference DirectoryIterator::operator*() const {
        return mCurrent;
    }

    DirectoryIterator::pointer DirectoryIterator::operator->() const {
        return &mCurrent;
    }

    DirectoryIterator& DirectoryIterator::operator++() {
        if (mIsEnd) { return *this; }

        WIN32_FIND_DATAA findData;
        if (::FindNextFileA(mFindHandle.Get(), &findData) == 0) {
            mIsEnd = true;
            return *this;
        }

        ProcessCurrentEntry(findData);
        return *this;
    }

    bool DirectoryIterator::operator==(const DirectoryIterator& other) const {
        if (mIsEnd && other.mIsEnd) { return true; }
        if (mIsEnd || other.mIsEnd) { return false; }
        return mCurrent == other.mCurrent;
    }

    bool DirectoryIterator::operator!=(const DirectoryIterator& other) const {
        return !(*this == other);
    }

    void DirectoryIterator::ProcessCurrentEntry(const WIN32_FIND_DATAA& findData) {
        if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) {
            this->operator++();
            return;
        }

        mCurrent = mRoot / findData.cFileName;
    }

    DirectoryIterator DirectoryEntries::begin() {
        return DirectoryIterator(mPath);
    }

    DirectoryIterator DirectoryEntries::end() {
        return DirectoryIterator();
    }
#pragma endregion
}  // namespace x