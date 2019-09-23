#ifndef EXT2_FS_H
#define EXT2_FS_H

struct vfs_file;
struct fs_struct;
struct inode;

struct vfs_file *ext2_vfs_file_new(struct fs_struct *fs, struct inode *inode);
void ext2_vfs_file_del(struct vfs_file *file);

#endif /* EXT2_FS_H */
