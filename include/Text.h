#pragma once

#include <cstdint>
#include <string>
#include <vector>

inline std::string trim(const std::string& s) {
    std::size_t begin = 0;
    while (begin < s.size() && (s[begin] == ' ' || s[begin] == '\t' || s[begin] == '\r')) {
        ++begin;
    }
    std::size_t end = s.size();
    while (end > begin && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r')) {
        --end;
    }
    return s.substr(begin, end - begin);
}

inline std::vector<std::string> splitList(const std::string& text, char separator) {
    std::vector<std::string> parts;
    std::string current;
    for (char ch : text) {
        if (ch == separator) {
            std::string item = trim(current);
            if (!item.empty()) parts.push_back(item);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    std::string item = trim(current);
    if (!item.empty()) parts.push_back(item);
    return parts;
}

inline std::vector<char32_t> decodeUtf8(const std::string& s) {
    std::vector<char32_t> out;
    const auto* p = reinterpret_cast<const unsigned char*>(s.data());
    const std::size_t n = s.size();
    std::size_t i = 0;
    while (i < n) {
        unsigned char c = p[i];
        if (c < 0x80) {
            out.push_back(c);
            ++i;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < n) {
            out.push_back((char32_t(c & 0x1F) << 6) | (p[i + 1] & 0x3F));
            i += 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < n) {
            out.push_back((char32_t(c & 0x0F) << 12) | (char32_t(p[i + 1] & 0x3F) << 6) | (p[i + 2] & 0x3F));
            i += 3;
        } else if ((c & 0xF8) == 0xF0 && i + 3 < n) {
            out.push_back((char32_t(c & 0x07) << 18) | (char32_t(p[i + 1] & 0x3F) << 12) |
                          (char32_t(p[i + 2] & 0x3F) << 6) | (p[i + 3] & 0x3F));
            i += 4;
        } else {
            out.push_back(c);
            ++i;
        }
    }
    return out;
}

inline std::string encodeUtf8(const std::vector<char32_t>& cps) {
    std::string out;
    for (char32_t cp : cps) {
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

inline char32_t lowerCodePoint(char32_t cp) {
    if (cp >= U'A' && cp <= U'Z') return cp + 32;
    if (cp >= 0x0410 && cp <= 0x042F) return cp + 0x20;
    if (cp == 0x0401) return 0x0451;
    return cp;
}

inline std::string foldCase(const std::string& s) {
    auto cps = decodeUtf8(s);
    for (char32_t& cp : cps) cp = lowerCodePoint(cp);
    return encodeUtf8(cps);
}

inline bool containsFold(const std::string& text, const std::string& part) {
    if (part.empty()) return true;
    return foldCase(text).find(foldCase(part)) != std::string::npos;
}

inline bool equalsFold(const std::string& a, const std::string& b) {
    return foldCase(a) == foldCase(b);
}

inline std::vector<char32_t> sortKey(const std::string& s) {
    std::vector<char32_t> key;
    for (char32_t cp : decodeUtf8(foldCase(s))) {
        if (cp == 0x0451) {
            key.push_back(0x0435);
            key.push_back(1);
        } else {
            key.push_back(cp);
            key.push_back(0);
        }
    }
    return key;
}

inline bool lessText(const std::string& a, const std::string& b) {
    return sortKey(a) < sortKey(b);
}
