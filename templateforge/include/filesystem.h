#pragma once
#include "nodes.h"
#include <string>
#include <vector>
#include <stdexcept>

// ---------------------------------------------------------------------------
// FileSystemError  — thrown by all FileSystemModel operations
// ---------------------------------------------------------------------------
class FileSystemError : public std::runtime_error {
public:
    explicit FileSystemError(const std::string& msg) : std::runtime_error(msg) {}
};

// ---------------------------------------------------------------------------
// FileSystemModel  — the in-memory tree
// ---------------------------------------------------------------------------
class FileSystemModel {
public:
    explicit FileSystemModel(const std::string& initial_user = "user");

    // ----- Navigation --------------------------------------------------
    std::string pwd()  const;
    void        cd(const std::string& path);
    std::vector<std::shared_ptr<Node>> ls(const std::string& path = "") const;

    // ----- Queries -----------------------------------------------------
    std::shared_ptr<Node> get_node(const std::string& path,
                                   bool follow_symlinks = true) const;

    // ----- Structural operations (may throw FileSystemError) -----------
    void mkdir(const std::string& path);
    void touch(const std::string& path, const std::string& content = "");
    void rm   (const std::string& path, bool recursive = false);
    void mv   (const std::string& src,  const std::string& dst);
    void cp   (const std::string& src,  const std::string& dst);
    void ln   (const std::string& target, const std::string& link_path);

    // ----- File I/O ----------------------------------------------------
    std::string cat        (const std::string& path) const;
    void        write_file (const std::string& path, const std::string& content);

    // ----- Access control ----------------------------------------------
    void chmod(const std::string& path, const std::string& octal);
    void chown(const std::string& path, const std::string& new_owner);

    // ----- Public state ------------------------------------------------
    std::string                    current_user;
    std::shared_ptr<DirectoryNode> root;
    std::shared_ptr<DirectoryNode> cwd;

private:
    // Path utilities
    std::vector<std::string> split_path(const std::string& path) const;
    std::vector<std::string> resolve_path(const std::string& path) const;
    std::shared_ptr<Node>    node_at_segments(const std::vector<std::string>& segs,
                                              bool follow_symlinks = true) const;
    std::shared_ptr<Node>    resolve_symlink(const std::shared_ptr<Node>& link,
                                             int depth = 0) const;

    // Returns (parent_dir, child_name) for a path
    std::pair<std::shared_ptr<DirectoryNode>, std::string>
        parent_and_name(const std::string& path) const;

    // Permission helpers
    bool check_permission(const std::shared_ptr<Node>& node, int required) const;
    void require_permission(const std::shared_ptr<Node>& node,
                            int required, const std::string& verb) const;

    // Deep-copy a node subtree (used by cp)
    std::shared_ptr<Node> deep_copy(const std::shared_ptr<Node>& node) const;

    static constexpr int SYMLINK_LIMIT = 40;
};
