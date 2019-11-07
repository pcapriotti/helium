#ifndef EXT2_FS_H
#define EXT2_FS_H

struct vfs;
struct vfs_file;
struct ext2;
struct ext2_inode;

struct vfs_file *ext2_vfs_file_new(struct ext2 *fs, struct ext2_inode *inode);
struct vfs *ext2_into_vfs(struct ext2 *fs);

#endif /* EXT2_FS_H */
