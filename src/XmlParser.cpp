#include "XmlParser.h"

#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>

const XmlNode* XmlNode::firstChild(const std::string& childName) const {
    for (const auto& c : children) {
        if (c.name == childName) return &c;
    }
    return nullptr;
}

std::vector<const XmlNode*> XmlNode::childrenByName(const std::string& childName) const {
    std::vector<const XmlNode*> result;
    for (const auto& c : children) {
        if (c.name == childName) result.push_back(&c);
    }
    return result;
}

std::string XmlNode::attr(const std::string& key, const std::string& def) const {
    auto it = attrs.find(key);
    return it == attrs.end() ? def : it->second;
}

bool XmlNode::hasAttr(const std::string& key) const {
    return attrs.find(key) != attrs.end();
}

namespace {

struct Parser {
    const char* p;
    const char* end;

    [[noreturn]] void fail(const std::string& msg) {
        throw std::runtime_error("xml parse error: " + msg);
    }

    bool eof() const { return p >= end; }

    void skipWs() {
        while (!eof() && std::isspace(static_cast<unsigned char>(*p))) ++p;
    }

    bool starts(const char* s) const {
        std::size_t n = std::strlen(s);
        if (static_cast<std::size_t>(end - p) < n) return false;
        return std::memcmp(p, s, n) == 0;
    }

    bool consume(const char* s) {
        if (!starts(s)) return false;
        p += std::strlen(s);
        return true;
    }

    void skipProlog() {
        while (!eof()) {
            skipWs();
            if (consume("<?")) {
                while (!eof() && !consume("?>")) ++p;
            } else if (consume("<!--")) {
                while (!eof() && !consume("-->")) ++p;
            } else if (consume("<!")) {
                // DOCTYPE or other declaration; skip to matching '>'.
                int depth = 1;
                while (!eof() && depth > 0) {
                    if (*p == '<') ++depth;
                    else if (*p == '>') --depth;
                    ++p;
                }
            } else {
                break;
            }
        }
    }

    std::string readName() {
        const char* s = p;
        while (!eof()) {
            char c = *p;
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == ':' || c == '.') {
                ++p;
            } else {
                break;
            }
        }
        if (p == s) fail("expected name");
        return std::string(s, p - s);
    }

    std::string readQuoted() {
        if (eof()) fail("expected quoted string");
        char q = *p;
        if (q != '"' && q != '\'') fail("expected quote");
        ++p;
        const char* s = p;
        while (!eof() && *p != q) ++p;
        if (eof()) fail("unterminated quoted string");
        std::string result(s, p - s);
        ++p; // consume closing quote
        return result;
    }

    void readAttrs(XmlNode& node) {
        while (true) {
            skipWs();
            if (eof()) fail("unexpected eof in tag");
            if (*p == '>' || *p == '/') return;
            std::string name = readName();
            skipWs();
            if (!consume("=")) fail("expected '=' after attribute name");
            skipWs();
            node.attrs[name] = readQuoted();
        }
    }

    static std::string decodeEntities(const std::string& in) {
        std::string out;
        out.reserve(in.size());
        for (std::size_t i = 0; i < in.size();) {
            if (in[i] == '&') {
                std::size_t semi = in.find(';', i);
                if (semi != std::string::npos) {
                    std::string e = in.substr(i + 1, semi - i - 1);
                    if (e == "lt")       { out += '<'; i = semi + 1; continue; }
                    else if (e == "gt")  { out += '>'; i = semi + 1; continue; }
                    else if (e == "amp") { out += '&'; i = semi + 1; continue; }
                    else if (e == "quot"){ out += '"'; i = semi + 1; continue; }
                    else if (e == "apos"){ out += '\''; i = semi + 1; continue; }
                }
            }
            out += in[i++];
        }
        return out;
    }

    XmlNode parseElement() {
        if (!consume("<")) fail("expected '<'");
        XmlNode node;
        node.name = readName();
        readAttrs(node);
        skipWs();
        if (consume("/>")) return node;
        if (!consume(">")) fail("expected '>'");

        std::string buf;
        while (!eof()) {
            if (starts("<!--")) {
                consume("<!--");
                while (!eof() && !consume("-->")) ++p;
                continue;
            }
            if (starts("</")) {
                consume("</");
                std::string endName = readName();
                skipWs();
                if (!consume(">")) fail("expected '>' in end tag");
                if (endName != node.name) fail("mismatched end tag </" + endName + "> for <" + node.name + ">");
                node.text = decodeEntities(buf);
                return node;
            }
            if (*p == '<') {
                node.children.push_back(parseElement());
                continue;
            }
            buf += *p++;
        }
        fail("unexpected eof inside <" + node.name + ">");
    }
};

} // namespace

XmlNode XmlParser::parseString(const std::string& src) {
    Parser P;
    P.p = src.c_str();
    P.end = P.p + src.size();
    // Skip UTF-8 BOM if present.
    if (src.size() >= 3 &&
        static_cast<unsigned char>(src[0]) == 0xEF &&
        static_cast<unsigned char>(src[1]) == 0xBB &&
        static_cast<unsigned char>(src[2]) == 0xBF) {
        P.p += 3;
    }
    P.skipProlog();
    return P.parseElement();
}

XmlNode XmlParser::parseFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open XML file: " + path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return parseString(ss.str());
}
