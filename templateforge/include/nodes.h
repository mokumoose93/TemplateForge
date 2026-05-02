#pragma once
#include <stddef.h>

/* Permission bits (replaces Perm namespace) */
#define PERM_NONE    0
#define PERM_EXECUTE 1
#define PERM_WRITE   2
#define PERM_READ    4
#define PERM_RX      5
#define PERM_RW      6
#define PERM_RWX     7

typedef struct {
    char owner[64];
    char group[64];
    int  owner_perms;
    int  group_perms;
    int  other_perms;
} AccessMetadata;

void access_perm_string (const AccessMetadata* a, char out[10]);
void access_octal_string(const AccessMetadata* a, char out[4]);
int  access_from_octal  (const char* s, int* op, int* gp, int* xp);

/* Node type tag — replaces the C++ class hierarchy */
typedef enum { NT_FILE, NT_DIR, NT_SYMLINK } NodeType;

/* Forward declaration so NodeChild can reference Node */
typedef struct Node Node;

typedef struct {
    char* key;   /* owned copy of child name, kept for sorted-map semantics */
    Node* node;  /* owned */
} NodeChild;

struct Node {
    char*          name;    /* owned */
    NodeType       type;
    AccessMetadata access;
    Node*          parent;  /* NON-owning back-pointer */
    union {
        struct { char* content;                                    } file;
        struct { NodeChild* entries; int count; int cap;           } dir;
        struct { char* target;                                     } symlink;
    };
};

/* Creation — each returns a heap-allocated Node (caller owns via the tree) */
Node* node_new_file   (const char* name, const char* content, const char* owner);
Node* node_new_dir    (const char* name, const char* owner);
Node* node_new_symlink(const char* name, const char* target,  const char* owner);
Node* node_deep_copy  (const Node* src);
void  node_free       (Node* n);   /* recursive for dirs */
char  node_type_label (const Node* n);

/* Directory child operations */
void  dir_add_child    (Node* dir, Node* child);
Node* dir_remove_child (Node* dir, const char* name); /* caller takes ownership */
Node* dir_get_child    (const Node* dir, const char* name);
int   dir_has_child    (const Node* dir, const char* name);
/* Returns heap array of non-owning Node* (dirs first, then files, both alpha).
   *count_out set to element count.  Caller must free() the array, not the nodes. */
Node** dir_sorted_children(const Node* dir, int* count_out);
