#pragma once
#include "nodes.h"

/* In-memory filesystem tree.
   Replaces the C++ FileSystemModel class. */
typedef struct {
    Node* root;
    Node* cwd;          /* non-owning pointer into the tree */
    char  current_user[64];
} FSModel;

void    fs_init(FSModel* fs, const char* initial_user);
void    fs_free(FSModel* fs);

/* Returns heap-allocated string.  Caller must free(). */
char*   fs_pwd(const FSModel* fs);
int     fs_cd (FSModel* fs, const char* path, char* errbuf, int errsz);

/* Returns heap-allocated array of non-owning Node*.  Caller must free() the array.
   *count is set on success; on error returns NULL and writes to errbuf. */
Node**  fs_ls(const FSModel* fs, const char* path, int* count,
              char* errbuf, int errsz);

/* Returns the node at path; NULL if not found.
   follow_symlinks: 1 = dereference symlinks, 0 = return the link itself. */
Node*   fs_get_node(const FSModel* fs, const char* path, int follow_symlinks);

int     fs_mkdir     (FSModel* fs, const char* path,
                      char* errbuf, int errsz);
int     fs_touch     (FSModel* fs, const char* path, const char* content,
                      char* errbuf, int errsz);
int     fs_rm        (FSModel* fs, const char* path, int recursive,
                      char* errbuf, int errsz);
int     fs_mv        (FSModel* fs, const char* src,  const char* dst,
                      char* errbuf, int errsz);
int     fs_cp        (FSModel* fs, const char* src,  const char* dst,
                      char* errbuf, int errsz);
int     fs_ln        (FSModel* fs, const char* target, const char* link_path,
                      char* errbuf, int errsz);

/* Returns heap-allocated file content.  Caller must free(). NULL on error. */
char*   fs_cat       (const FSModel* fs, const char* path,
                      char* errbuf, int errsz);
int     fs_write_file(FSModel* fs, const char* path, const char* content,
                      char* errbuf, int errsz);

int     fs_chmod(FSModel* fs, const char* path, const char* octal,
                 char* errbuf, int errsz);
int     fs_chown(FSModel* fs, const char* path, const char* new_owner,
                 char* errbuf, int errsz);
