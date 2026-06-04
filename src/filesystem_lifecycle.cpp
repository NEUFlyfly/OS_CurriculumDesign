//
// Created by 林智超 on 2019-06-17.
//

#include <cstring>
#include <cstdio>
#include "filesystem.h"

bool FileSystem::Format() {
    //初始化超级块
    storage.superBlock->s_INODE_NUM = INODE_NUM;
    storage.superBlock->s_BLOCK_NUM = BLOCK_NUM;
    storage.superBlock->s_SUPERBLOCK_SIZE = sizeof(SuperBlock);
    storage.superBlock->s_INODE_SIZE = INODE_SIZE;
    storage.superBlock->s_BLOCK_SIZE = BLOCK_SIZE;
    storage.superBlock->s_free_INODE_NUM = INODE_NUM;
    storage.superBlock->s_free_BLOCK_NUM = BLOCK_NUM;
    storage.superBlock->s_blocks_per_group = BLOCKS_PER_GROUP;
    storage.superBlock->s_free_addr = BLOCK_STARTADDR;
    storage.superBlock->s_Superblock_StartAddr = SUPERBLOCK_START_ADDR;
    storage.superBlock->s_BlockBitmap_StartAddr = BLOCKBIITMAP_START_ADDR;
    storage.superBlock->s_InodeBitmap_StartAddr = INODEBITMAP_START_ADDR;
    storage.superBlock->s_Block_StartAddr = BLOCK_STARTADDR;
    storage.superBlock->s_Inode_StartAddr = INODE_START_ADDR;


    FILE* fr = storage.image.get_file_read();
    FILE* fw = storage.image.get_file_write();

    memset(storage.inode_bitmap,0,sizeof(storage.inode_bitmap));
    fseek(storage.image.get_file_write(), INODEBITMAP_START_ADDR, SEEK_SET);
    fwrite(storage.inode_bitmap,sizeof(storage.inode_bitmap),1, fw);

    memset(storage.block_bitmap,0,sizeof(storage.block_bitmap));
    fseek(fw,BLOCKBIITMAP_START_ADDR,SEEK_SET);
    fwrite(storage.block_bitmap,sizeof(storage.block_bitmap),1, fw);

    //初始化磁盘块区，根据成组链接法组织
    for(int i = BLOCK_NUM/BLOCKS_PER_GROUP-1; i>=0; i--){	//一共INODE_NUM/BLOCKS_PER_GROUP组，一组FREESTACKNUM（128）个磁盘块 ，第一个磁盘块作为索引
        if(i == BLOCK_NUM/BLOCKS_PER_GROUP-1)
            //最后一组 下面一组没有空闲盘块
            storage.superBlock->s_free[0] = -1;
        else
            storage.superBlock->s_free[0] = BLOCK_STARTADDR + (i+1)*BLOCKS_PER_GROUP*BLOCK_SIZE;	//指向下一个空闲块
        for(int j=1; j<BLOCKS_PER_GROUP; j++){
            storage.superBlock->s_free[j] = BLOCK_STARTADDR + (i*BLOCKS_PER_GROUP + j)*BLOCK_SIZE;
        }
        fseek(fw,BLOCK_STARTADDR+i*BLOCKS_PER_GROUP*BLOCK_SIZE,SEEK_SET);
        fwrite(storage.superBlock->s_free,sizeof(storage.superBlock->s_free),1,fw);	//填满这个磁盘块，512字节
    }
    //超级块写入到虚拟磁盘文件
    fseek(fw, SUPERBLOCK_START_ADDR,SEEK_SET);
    fwrite(storage.superBlock, sizeof(SuperBlock),1,fw);

    fflush(fw);

    //读取inode位图
    fseek(fr,INODEBITMAP_START_ADDR,SEEK_SET);
    fread(storage.inode_bitmap,sizeof(storage.inode_bitmap),1,fr);

    //读取block位图
    fseek(fr,BLOCKBIITMAP_START_ADDR,SEEK_SET);
    fread(storage.block_bitmap,sizeof(storage.block_bitmap),1,fr);

    fflush(fr);

    //创建根目录 "/"
    iNode cur;

    //申请inode
    int inode_addr_cur = INodeAlloc();

    //给这个inode申请磁盘块
    int block_addr = BlockAlloc();

    //在这个磁盘块里加入一个条目 "."
    Dir dir_vec[16] = {0};
    strcpy(dir_vec[0].name,".");
    dir_vec[0].inode_addr = inode_addr_cur;

    //写回磁盘块
    fseek(fw, block_addr, SEEK_SET);
    fwrite(dir_vec, sizeof(dir_vec),1,fw);

    //给inode赋值
    cur.inode_id = 0;
    strcpy(cur.user_name, this->state.cur_user_name);
    strcpy(cur.user_group, this->state.cur_grop_name);
    cur.inode_cnt = 1;	//一个项，当前目录,"."
    cur.inode_dirblock[0] = block_addr; //根目录指向

    for(int i = 1; i < 10; i++){
        cur.inode_dirblock[i] = -1;
    }
    cur.inode_size = storage.superBlock->s_BLOCK_SIZE;
    cur.inode_indirect_block_first = -1;	//没使用一级间接块
    cur.inode_mode = MODE_DIR | DIR_DEFAULT_PERMISSION; //初始化mode


    //写回inode
    fseek(fw, inode_addr_cur,SEEK_SET);
    fwrite(&cur, sizeof(iNode), 1, fw);
    fflush(fw);

    //创建目录及配置文件
    char home_dir[] = "home";
    char root_dir[] = "root";
    char etc_dir[] = "etc";

    if(!MakeDir(ROOT_DIR_ADDR, home_dir)) return false;
    FindDir(ROOT_DIR_ADDR, home_dir);

    int home_dir_addr = this->state.cur_dir_addr;
    if(!MakeDir(home_dir_addr, root_dir)) return false;
    Chmod(home_dir_addr, root_dir, 0660);

    this->state.cur_dir_addr = ROOT_DIR_ADDR;
    strcpy(this->state.cur_dir_name, "/");
    if(!MakeDir(ROOT_DIR_ADDR, etc_dir)) return false;
    FindDir(ROOT_DIR_ADDR, etc_dir);

    char buf[1000] = {0};

    sprintf(buf,"root:x:%d:%d\n", state.nextUID++, state.nextGID++);	//增加条目，用户名：加密密码：用户ID：用户组ID
    Create(state.cur_dir_addr, "user", buf);	//创建用户信息文件
    sprintf(buf,"root:root\n");	//增加条目，用户名：密码
    Create(state.cur_dir_addr,"passwd",buf);	//创建用户密码文件
    Chmod(state.cur_dir_addr,"passwd", 0660);	//修改权限，禁止其它用户读取该文件



    //用户组
    sprintf(buf,"root::0:root\n");	//增加管理员用户组，用户组名：口令（一般为空，这里没有使用）：组标识号：组内用户列表（用，分隔）
    sprintf(buf+strlen(buf),"user::1:\n");	//增加普通用户组，组内用户列表为空
    Create(state.cur_dir_addr,"group",buf);	//创建用户组信息文件

    this->state.cur_dir_addr = ROOT_DIR_ADDR;
    strcpy(this->state.cur_dir_name, "/");

    return true;
}

bool FileSystem::InitFileSystem() {
    //读取superblock
    FILE* fr = storage.image.get_file_read();
    if(fr == NULL) return false;
    fseek(fr, SUPERBLOCK_START_ADDR, SEEK_SET);
    fread(this->storage.superBlock, sizeof(SuperBlock), 1, fr);

    //读取inode位图
    fseek(fr, INODEBITMAP_START_ADDR, SEEK_SET);
    fread(this->storage.inode_bitmap, sizeof(storage.inode_bitmap), 1, fr);

    //读取block位图
    fseek(fr,BLOCKBIITMAP_START_ADDR,SEEK_SET);
    fread(storage.block_bitmap,sizeof(storage.block_bitmap),1,fr);
    return true;
}
