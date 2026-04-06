#include "nodes.h"
#include <stdexcept>
#include <algorithm>

// ---------------------------------------------------------------------------
// AccessMetadata
// ---------------------------------------------------------------------------
static std::string perm_bits(int p) {
    std::string s;
    s += (p & Perm::READ)    ? 'r' : '-';
    s += (p & Perm::WRITE)   ? 'w' : '-';
    s += (p & Perm::EXECUTE) ? 'x' : '-';
    return s;
}

std::string AccessMetadata::permission_string() const {
    return perm_bits(owner_perms) + perm_bits(group_perms) + perm_bits(other_perms);
}

std::string AccessMetadata::octal_string() const {
    return std::to_string(owner_perms) + std::to_string(group_perms) + std::to_string(other_perms);
}

bool AccessMetadata::from_octal(const std::string& s, int& op, int& gp, int& xp) {
    if (s.size() != 3) return false;
    for (char c : s) if (c < '0' || c > '7') return false;
    op = s[0] - '0';
    gp = s[1] - '0';
    xp = s[2] - '0';
    return true;
}

// ---------------------------------------------------------------------------
// Node
// ---------------------------------------------------------------------------
Node::Node(const std::string& n, NodeType t, const std::string& owner)
    : name(n), type(t)
{
    access.owner = owner;
}

char Node::type_label() const {
    if (is_dir())     return 'd';
    if (is_symlink()) return 'l';
    return '-';
}

// ---------------------------------------------------------------------------
// FileNode
// ---------------------------------------------------------------------------
FileNode::FileNode(const std::string& name, const std::string& cnt, const std::string& owner)
    : Node(name, NodeType::FILE, owner), content(cnt)
{
    access.owner_perms = Perm::RW;
    access.group_perms = Perm::READ;
    access.other_perms = Perm::READ;
}

// ---------------------------------------------------------------------------
// DirectoryNode
// ---------------------------------------------------------------------------
DirectoryNode::DirectoryNode(const std::string& name, const std::string& owner)
    : Node(name, NodeType::DIRECTORY, owner)
{}

void DirectoryNode::add_child(std::shared_ptr<Node> node) {
    node->parent = shared_from_this(); // requires enable_shared_from_this – see below
    children[node->name] = node;
}

std::shared_ptr<Node> DirectoryNode::remove_child(const std::string& n) {
    auto it = children.find(n);
    if (it == children.end()) return nullptr;
    auto node = it->second;
    node->parent.reset();
    children.erase(it);
    return node;
}

std::shared_ptr<Node> DirectoryNode::get_child(const std::string& n) const {
    auto it = children.find(n);
    return (it != children.end()) ? it->second : nullptr;
}

bool DirectoryNode::has_child(const std::string& n) const {
    return children.count(n) > 0;
}

std::vector<std::shared_ptr<Node>> DirectoryNode::sorted_children() const {
    std::vector<std::shared_ptr<Node>> dirs, files;
    for (auto& [name, node] : children) {
        if (node->is_dir()) dirs.push_back(node);
        else                files.push_back(node);
    }
    // children map is already sorted by name (std::map), so no extra sort needed
    std::vector<std::shared_ptr<Node>> result;
    result.insert(result.end(), dirs.begin(),  dirs.end());
    result.insert(result.end(), files.begin(), files.end());
    return result;
}

// ---------------------------------------------------------------------------
// SymlinkNode
// ---------------------------------------------------------------------------
SymlinkNode::SymlinkNode(const std::string& name, const std::string& target, const std::string& owner)
    : Node(name, NodeType::SYMLINK, owner), target_path(target)
{
    access.owner_perms = Perm::RWX;
    access.group_perms = Perm::RX;
    access.other_perms = Perm::RX;
}
