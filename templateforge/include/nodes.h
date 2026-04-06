#pragma once
#include <string>
#include <map>
#include <memory>
#include <vector>

// ---------------------------------------------------------------------------
// Permission bits  (Unix-style rwx stored as an int bitmask)
// ---------------------------------------------------------------------------
namespace Perm {
    constexpr int NONE    = 0;
    constexpr int EXECUTE = 1;
    constexpr int WRITE   = 2;
    constexpr int READ    = 4;
    constexpr int RX      = READ | EXECUTE;   // 5
    constexpr int RW      = READ | WRITE;     // 6
    constexpr int RWX     = READ | WRITE | EXECUTE; // 7
}

// ---------------------------------------------------------------------------
// AccessMetadata  — attached to every node
// ---------------------------------------------------------------------------
struct AccessMetadata {
    std::string owner       {"user"};
    std::string group       {"users"};
    int         owner_perms {Perm::RWX};
    int         group_perms {Perm::RX};
    int         other_perms {Perm::RX};

    std::string permission_string() const; // e.g. "rwxr-xr-x"
    std::string octal_string()      const; // e.g. "755"

    // Parse "755" -> (owner, group, other) bits.  Returns false on bad input.
    static bool from_octal(const std::string& s, int& op, int& gp, int& xp);
};

// ---------------------------------------------------------------------------
// Node  — abstract base for every tree element
// ---------------------------------------------------------------------------
enum class NodeType { FILE, DIRECTORY, SYMLINK };

class DirectoryNode; // forward declaration

class Node : public std::enable_shared_from_this<Node> {
public:
    std::string             name;
    NodeType                type;
    AccessMetadata          access;
    std::weak_ptr<Node>     parent; // weak to avoid reference cycles

    Node(const std::string& name, NodeType type, const std::string& owner = "user");
    virtual ~Node() = default;

    bool is_file()    const { return type == NodeType::FILE; }
    bool is_dir()     const { return type == NodeType::DIRECTORY; }
    bool is_symlink() const { return type == NodeType::SYMLINK; }
    char type_label() const;
};

// ---------------------------------------------------------------------------
// FileNode  — leaf node holding text content
// ---------------------------------------------------------------------------
class FileNode : public Node {
public:
    std::string content;
    FileNode(const std::string& name,
             const std::string& content = "",
             const std::string& owner   = "user");
};

// ---------------------------------------------------------------------------
// DirectoryNode  — internal node containing named children
// ---------------------------------------------------------------------------
class DirectoryNode : public Node {
public:
    std::map<std::string, std::shared_ptr<Node>> children; // sorted by name

    explicit DirectoryNode(const std::string& name,
                           const std::string& owner = "user");

    void                    add_child(std::shared_ptr<Node> node);
    std::shared_ptr<Node>   remove_child(const std::string& name);
    std::shared_ptr<Node>   get_child(const std::string& name) const;
    bool                    has_child(const std::string& name)  const;

    // Directories first (alpha), then files (alpha)
    std::vector<std::shared_ptr<Node>> sorted_children() const;
};

// ---------------------------------------------------------------------------
// SymlinkNode  — a node whose target_path points elsewhere in the tree
// ---------------------------------------------------------------------------
class SymlinkNode : public Node {
public:
    std::string target_path;
    SymlinkNode(const std::string& name,
                const std::string& target_path,
                const std::string& owner = "user");
};
