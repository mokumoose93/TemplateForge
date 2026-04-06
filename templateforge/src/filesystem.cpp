#include "filesystem.h"
#include <sstream>
#include <algorithm>

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
FileSystemModel::FileSystemModel(const std::string& initial_user)
    : current_user(initial_user)
{
    root = std::make_shared<DirectoryNode>("/", "root");
    root->access.owner_perms = Perm::RWX;
    root->access.group_perms = Perm::RX;
    root->access.other_perms = Perm::RX;
    cwd = root;
}

// ---------------------------------------------------------------------------
// Path utilities
// ---------------------------------------------------------------------------
std::vector<std::string> FileSystemModel::split_path(const std::string& path) const {
    std::vector<std::string> segs;
    std::istringstream ss(path);
    std::string part;
    while (std::getline(ss, part, '/')) {
        if (!part.empty()) segs.push_back(part);
    }
    return segs;
}

std::vector<std::string> FileSystemModel::resolve_path(const std::string& path) const {
    std::vector<std::string> segs;

    if (!path.empty() && path[0] == '/') {
        // absolute — start from root
    } else {
        // relative — start from cwd
        auto node = cwd;
        std::vector<std::string> parts;
        auto cur = std::static_pointer_cast<Node>(node);
        while (cur && cur != root) {
            parts.push_back(cur->name);
            cur = cur->parent.lock();
        }
        for (auto it = parts.rbegin(); it != parts.rend(); ++it)
            segs.push_back(*it);
    }

    for (auto& part : split_path(path)) {
        if (part == ".")  continue;
        if (part == "..") { if (!segs.empty()) segs.pop_back(); }
        else              segs.push_back(part);
    }
    return segs;
}

std::shared_ptr<Node> FileSystemModel::node_at_segments(
    const std::vector<std::string>& segs, bool follow_symlinks) const
{
    std::shared_ptr<Node> cur = root;
    for (auto& seg : segs) {
        auto dir = std::dynamic_pointer_cast<DirectoryNode>(cur);
        if (!dir) return nullptr;
        auto child = dir->get_child(seg);
        if (!child) return nullptr;
        if (follow_symlinks && child->is_symlink()) {
            child = resolve_symlink(child);
            if (!child) return nullptr;
        }
        cur = child;
    }
    return cur;
}

std::shared_ptr<Node> FileSystemModel::resolve_symlink(
    const std::shared_ptr<Node>& link, int depth) const
{
    if (depth > SYMLINK_LIMIT) return nullptr;
    auto sl = std::dynamic_pointer_cast<SymlinkNode>(link);
    if (!sl) return link;
    auto target = node_at_segments(resolve_path(sl->target_path), false);
    if (!target) return nullptr;
    if (target->is_symlink()) return resolve_symlink(target, depth + 1);
    return target;
}

std::shared_ptr<Node> FileSystemModel::get_node(
    const std::string& path, bool follow_symlinks) const
{
    auto segs = resolve_path(path);
    if (segs.empty()) return root;
    return node_at_segments(segs, follow_symlinks);
}

std::pair<std::shared_ptr<DirectoryNode>, std::string>
FileSystemModel::parent_and_name(const std::string& path) const
{
    auto segs = resolve_path(path);
    if (segs.empty()) return {nullptr, ""};
    std::string name = segs.back();
    segs.pop_back();
    auto parent_node = segs.empty()
        ? std::static_pointer_cast<Node>(root)
        : node_at_segments(segs);
    auto parent_dir = std::dynamic_pointer_cast<DirectoryNode>(parent_node);
    return {parent_dir, name};
}

// ---------------------------------------------------------------------------
// Permission helpers
// ---------------------------------------------------------------------------
bool FileSystemModel::check_permission(
    const std::shared_ptr<Node>& node, int required) const
{
    if (current_user == "root") return true;
    int effective = (current_user == node->access.owner)
                    ? node->access.owner_perms
                    : node->access.other_perms;
    return (effective & required) == required;
}

void FileSystemModel::require_permission(
    const std::shared_ptr<Node>& node, int required, const std::string& verb) const
{
    if (!check_permission(node, required)) {
        throw FileSystemError(
            "Permission denied: cannot " + verb + " '" + node->name +
            "' (user: " + current_user + ")"
        );
    }
}

// ---------------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------------
std::string FileSystemModel::pwd() const {
    std::vector<std::string> parts;
    auto cur = std::static_pointer_cast<Node>(cwd);
    while (cur && cur != root) {
        parts.push_back(cur->name);
        cur = cur->parent.lock();
    }
    if (parts.empty()) return "/";
    std::string result;
    for (auto it = parts.rbegin(); it != parts.rend(); ++it)
        result += "/" + *it;
    return result;
}

void FileSystemModel::cd(const std::string& path) {
    auto node = get_node(path);
    if (!node)
        throw FileSystemError("cd: no such directory: '" + path + "'");
    if (!node->is_dir())
        throw FileSystemError("cd: not a directory: '" + path + "'");
    require_permission(node, Perm::EXECUTE, "enter");
    cwd = std::dynamic_pointer_cast<DirectoryNode>(node);
}

std::vector<std::shared_ptr<Node>> FileSystemModel::ls(const std::string& path) const {
    std::shared_ptr<Node> target = path.empty() ? cwd : get_node(path);
    if (!target)
        throw FileSystemError("ls: no such file or directory: '" + path + "'");
    if (!target->is_dir()) return {target};
    require_permission(target, Perm::READ, "list");
    return std::dynamic_pointer_cast<DirectoryNode>(target)->sorted_children();
}

// ---------------------------------------------------------------------------
// Structural operations
// ---------------------------------------------------------------------------
void FileSystemModel::mkdir(const std::string& path) {
    auto segs = resolve_path(path);
    if (segs.empty()) throw FileSystemError("mkdir: cannot create root");

    std::shared_ptr<Node> cur = root;
    for (auto& seg : segs) {
        auto dir = std::dynamic_pointer_cast<DirectoryNode>(cur);
        if (!dir)
            throw FileSystemError("mkdir: '" + cur->name + "' is not a directory");
        auto child = dir->get_child(seg);
        if (!child) {
            require_permission(cur, Perm::WRITE, "write to");
            auto new_dir = std::make_shared<DirectoryNode>(seg, current_user);
            dir->add_child(new_dir);
            cur = new_dir;
        } else if (child->is_dir()) {
            cur = child;
        } else {
            throw FileSystemError(
                "mkdir: '" + seg + "' exists and is not a directory");
        }
    }
}

void FileSystemModel::touch(const std::string& path, const std::string& content) {
    auto [parent, name] = parent_and_name(path);
    if (!parent)
        throw FileSystemError("touch: no such parent directory for '" + path + "'");
    require_permission(parent, Perm::WRITE, "write to");
    auto existing = parent->get_child(name);
    if (existing) {
        if (!existing->is_file())
            throw FileSystemError("touch: '" + name + "' exists and is not a file");
        require_permission(existing, Perm::WRITE, "write to");
        std::dynamic_pointer_cast<FileNode>(existing)->content = content;
    } else {
        parent->add_child(std::make_shared<FileNode>(name, content, current_user));
    }
}

void FileSystemModel::rm(const std::string& path, bool recursive) {
    auto [parent, name] = parent_and_name(path);
    if (!parent)
        throw FileSystemError("rm: cannot remove '" + path + "'");
    auto node = parent->get_child(name);
    if (!node)
        throw FileSystemError("rm: no such file or directory: '" + path + "'");
    require_permission(parent, Perm::WRITE, "remove from");
    if (node->is_dir()) {
        auto dir = std::dynamic_pointer_cast<DirectoryNode>(node);
        if (!recursive && !dir->children.empty())
            throw FileSystemError("rm: '" + path + "' is non-empty (use -r)");
    }
    parent->remove_child(name);
}

void FileSystemModel::mv(const std::string& src, const std::string& dst) {
    auto [src_parent, src_name] = parent_and_name(src);
    if (!src_parent)
        throw FileSystemError("mv: cannot move '" + src + "'");
    auto node = src_parent->get_child(src_name);
    if (!node)
        throw FileSystemError("mv: no such file or directory: '" + src + "'");
    require_permission(src_parent, Perm::WRITE, "remove from");

    auto dst_node = get_node(dst);
    if (dst_node && dst_node->is_dir()) {
        auto dst_dir = std::dynamic_pointer_cast<DirectoryNode>(dst_node);
        require_permission(dst_dir, Perm::WRITE, "write to");
        if (dst_dir->has_child(node->name))
            throw FileSystemError("mv: '" + node->name + "' already exists in destination");
        src_parent->remove_child(src_name);
        dst_dir->add_child(node);
    } else {
        auto [dst_parent, dst_name] = parent_and_name(dst);
        if (!dst_parent)
            throw FileSystemError("mv: invalid destination: '" + dst + "'");
        require_permission(dst_parent, Perm::WRITE, "write to");
        src_parent->remove_child(src_name);
        dst_parent->remove_child(dst_name); // ok if absent
        node->name = dst_name;
        dst_parent->add_child(node);
    }
}

std::shared_ptr<Node> FileSystemModel::deep_copy(const std::shared_ptr<Node>& node) const {
    if (node->is_file()) {
        auto fn = std::dynamic_pointer_cast<FileNode>(node);
        auto copy = std::make_shared<FileNode>(fn->name, fn->content, fn->access.owner);
        copy->access = fn->access;
        return copy;
    }
    if (node->is_symlink()) {
        auto sl = std::dynamic_pointer_cast<SymlinkNode>(node);
        auto copy = std::make_shared<SymlinkNode>(sl->name, sl->target_path, sl->access.owner);
        copy->access = sl->access;
        return copy;
    }
    // Directory — recurse
    auto dir = std::dynamic_pointer_cast<DirectoryNode>(node);
    auto copy_dir = std::make_shared<DirectoryNode>(dir->name, dir->access.owner);
    copy_dir->access = dir->access;
    for (auto& [cn, child] : dir->children)
        copy_dir->add_child(deep_copy(child));
    return copy_dir;
}

void FileSystemModel::cp(const std::string& src, const std::string& dst) {
    auto src_node = get_node(src);
    if (!src_node)
        throw FileSystemError("cp: no such file or directory: '" + src + "'");
    require_permission(src_node, Perm::READ, "read");

    auto clone = deep_copy(src_node);

    auto dst_node = get_node(dst);
    if (dst_node && dst_node->is_dir()) {
        auto dst_dir = std::dynamic_pointer_cast<DirectoryNode>(dst_node);
        require_permission(dst_dir, Perm::WRITE, "write to");
        if (dst_dir->has_child(clone->name))
            throw FileSystemError("cp: '" + clone->name + "' already exists in destination");
        dst_dir->add_child(clone);
    } else {
        auto [dst_parent, dst_name] = parent_and_name(dst);
        if (!dst_parent)
            throw FileSystemError("cp: invalid destination: '" + dst + "'");
        require_permission(dst_parent, Perm::WRITE, "write to");
        dst_parent->remove_child(dst_name);
        clone->name = dst_name;
        dst_parent->add_child(clone);
    }
}

void FileSystemModel::ln(const std::string& target, const std::string& link_path) {
    auto [parent, name] = parent_and_name(link_path);
    if (!parent)
        throw FileSystemError("ln: invalid link path: '" + link_path + "'");
    require_permission(parent, Perm::WRITE, "write to");
    if (parent->has_child(name))
        throw FileSystemError("ln: '" + name + "' already exists");
    parent->add_child(std::make_shared<SymlinkNode>(name, target, current_user));
}

// ---------------------------------------------------------------------------
// File I/O
// ---------------------------------------------------------------------------
std::string FileSystemModel::cat(const std::string& path) const {
    auto node = get_node(path);
    if (!node)
        throw FileSystemError("cat: no such file: '" + path + "'");
    if (node->is_dir())
        throw FileSystemError("cat: '" + path + "' is a directory");
    require_permission(node, Perm::READ, "read");
    return std::dynamic_pointer_cast<FileNode>(node)->content;
}

void FileSystemModel::write_file(const std::string& path, const std::string& content) {
    auto node = get_node(path);
    if (!node)
        throw FileSystemError("write: no such file: '" + path + "'");
    if (!node->is_file())
        throw FileSystemError("write: '" + path + "' is not a file");
    require_permission(node, Perm::WRITE, "write to");
    std::dynamic_pointer_cast<FileNode>(node)->content = content;
}

// ---------------------------------------------------------------------------
// Access control
// ---------------------------------------------------------------------------
void FileSystemModel::chmod(const std::string& path, const std::string& octal) {
    auto node = get_node(path, false);
    if (!node)
        throw FileSystemError("chmod: no such file or directory: '" + path + "'");
    if (current_user != "root" && current_user != node->access.owner)
        throw FileSystemError("chmod: permission denied (only owner or root)");
    int op, gp, xp;
    if (!AccessMetadata::from_octal(octal, op, gp, xp))
        throw FileSystemError("chmod: invalid permission string '" + octal + "'");
    node->access.owner_perms = op;
    node->access.group_perms = gp;
    node->access.other_perms = xp;
}

void FileSystemModel::chown(const std::string& path, const std::string& new_owner) {
    if (current_user != "root")
        throw FileSystemError("chown: permission denied (root only)");
    auto node = get_node(path, false);
    if (!node)
        throw FileSystemError("chown: no such file or directory: '" + path + "'");
    node->access.owner = new_owner;
}
