#pragma once

#include <map>
#include <string>
#include <vector>

// Minimal, tolerant XML-subset parser tailored for this assignment's scene files.
// Supports: elements with attributes (single or double quoted), text content,
// XML declarations (<?xml ... ?>), and comments (<!-- ... -->).
// Does NOT support: CDATA, entity references beyond the common 5, namespaces,
// DOCTYPE declarations, processing instructions other than <?xml ... ?>.
struct XmlNode {
    std::string name;
    std::map<std::string, std::string> attrs;
    std::string text;                       // concatenated text content
    std::vector<XmlNode> children;

    const XmlNode* firstChild(const std::string& childName) const;
    std::vector<const XmlNode*> childrenByName(const std::string& childName) const;
    std::string attr(const std::string& key, const std::string& def = "") const;
    bool hasAttr(const std::string& key) const;
};

class XmlParser {
public:
    static XmlNode parseFile(const std::string& path);
    static XmlNode parseString(const std::string& src);
};
