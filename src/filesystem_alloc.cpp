#include <iostream>
#include <cstdio>
#include "filesystem.h"

int FileSystem::INodeAlloc() {
    if(storage.superBlock->s_free_INODE_NUM == 0) {
        cout << "没有空闲inode可以分配" << endl;
        return -1;
    } else {
        int pos = 0;
        for(int i = 0; i < storage.superBlock->s_INODE_NUM; i++) {
            if(storage.inode_bitmap[i] == 0) {
                pos = i;
                break;
            }
        }
        FILE* fw = storage.image.get_file_write();
        storage.superBlock->s_free_INODE_NUM--;
        fseek(fw, SUPERBLOCK_START_ADDR, SEEK_SET);
        fwrite(storage.superBlock, sizeof(SuperBlock), 1, fw);
        storage.inode_bitmap[pos] = 1;
        fseek(fw, INODEBITMAP_START_ADDR + pos, SEEK_SET);
        fwrite(&storage.inode_bitmap[pos], sizeof(bool), 1, fw);
        fflush(fw);

        return INODE_START_ADDR + pos * storage.superBlock->s_INODE_SIZE;


    }
}

int FileSystem::BlockAlloc() {
    int top; //栈顶指针
    if (storage.superBlock->s_free_BLOCK_NUM==0) {
        cout << "已经没有空闲块分配" << endl;
        return -1;
    }
    else{
        //如果已经是最后一块，此时top应该为0
        top = (storage.superBlock->s_free_BLOCK_NUM-1) % storage.superBlock->s_blocks_per_group;
    }
    //将栈顶取出
    //如果已是栈底，将当前块号地址返回，即为栈底块号，并将栈底指向的新空闲块堆栈覆盖原来的栈
    int alloc_addr;
    FILE* fr = storage.image.get_file_read();
    if(top == 0){
        alloc_addr = storage.superBlock->s_free_addr;
        storage.superBlock->s_free_addr = storage.superBlock->s_free[0];	//取出下一个存有空闲块堆栈的空闲块的位置，更新空闲块堆栈指针

        //取出对应空闲块内容，覆盖原来的空闲块堆栈
        //取出下一个空闲块堆栈，覆盖原来的
        fseek(fr, storage.superBlock->s_free_addr, SEEK_SET);
        fread(storage.superBlock->s_free, sizeof(storage.superBlock->s_free), 1, fr);
        fflush(fr);
        storage.superBlock->s_free_BLOCK_NUM--;

    }
    else{	//如果不为栈底，则将栈顶指向的地址返回，栈顶指针-1.
        alloc_addr = storage.superBlock->s_free[top];	//保存返回地址
        storage.superBlock->s_free[top] = -1;	//清栈顶
        top--;		//栈顶指针-1
        storage.superBlock->s_free_BLOCK_NUM--;	//空闲块数-1

    }
    FILE* fw = storage.image.get_file_write();
    //更新超级块
    fseek(fw, SUPERBLOCK_START_ADDR, SEEK_SET);
    fwrite(storage.superBlock, sizeof(SuperBlock), 1, fw);
    fflush(fw);

    //更新block位图
    storage.block_bitmap[(alloc_addr-BLOCK_STARTADDR)/BLOCK_SIZE] = 1;
    fseek(fw, (alloc_addr-BLOCK_STARTADDR)/BLOCK_SIZE+BLOCKBIITMAP_START_ADDR, SEEK_SET);
    fwrite(&storage.block_bitmap[(alloc_addr-BLOCK_STARTADDR)/BLOCK_SIZE], sizeof(bool), 1, fw);
    fflush(fw);

    return alloc_addr;

}

bool FileSystem::FreeBlock(int addr) {
    if( (addr - BLOCK_STARTADDR) % storage.superBlock->s_BLOCK_SIZE != 0 ){
        cout << "不是磁盘块所处正确位置" << endl;
        return false;
    }
    unsigned int block_id = static_cast<unsigned int>((addr - BLOCK_STARTADDR) / storage.superBlock->s_BLOCK_SIZE);	//inode节点号
    //该地址还未使用，不能释放空间
    if(storage.block_bitmap[block_id]==0){
        printf("该block（磁盘块）还未使用，无法释放\n");
        return false;
    }

    int top;
    if(storage.superBlock->s_free_BLOCK_NUM == storage.superBlock->s_BLOCK_NUM){
        cout << "没有可释放的磁盘块" << endl;
        return false;
    }
    else{	//非满
        top = (storage.superBlock->s_free_BLOCK_NUM-1) % storage.superBlock->s_blocks_per_group;
        char buffer[BLOCK_SIZE] = {0};
        fseek(storage.image.get_file_write(), addr, SEEK_SET);
        fwrite(buffer, sizeof(buffer), 1, storage.image.get_file_write());

        if(top == storage.superBlock->s_blocks_per_group-1){

            //该空闲块作为新的空闲块堆栈
            storage.superBlock->s_free[0] = storage.superBlock->s_free_addr;	//新的空闲块堆栈第一个地址指向旧的空闲块堆栈指针
            //清空元素
            for(int i= 1;i < storage.superBlock->s_blocks_per_group; i++){
                storage.superBlock->s_free[i] = -1;
            }
            fseek(storage.image.get_file_write(), addr, SEEK_SET);
            fwrite(storage.superBlock->s_free, sizeof(storage.superBlock->s_free),1, storage.image.get_file_write());	//填满这个磁盘块，512字节

        }
        else{
            top++;
            storage.superBlock->s_free[top] = addr;
        }
    }


    storage.superBlock->s_free_BLOCK_NUM++;	//空闲块数+1
    fseek(storage.image.get_file_write(),SUPERBLOCK_START_ADDR,SEEK_SET);
    fwrite(storage.superBlock, sizeof(SuperBlock),1, storage.image.get_file_write());

    //更新block位图
    storage.block_bitmap[block_id] = 0;
    fseek(storage.image.get_file_write(), block_id + BLOCKBIITMAP_START_ADDR, SEEK_SET);
    fwrite(&storage.block_bitmap[block_id], sizeof(bool), 1, storage.image.get_file_write());
    fflush(storage.image.get_file_write());
    return true;
}

bool FileSystem::INodeFree(int addr) {
    int t = INODE_START_ADDR;
    if( ( addr - t) % storage.superBlock->s_INODE_SIZE != 0 ){
        cout << "不是正确的inode节点位置" << endl;
        return false;
    }
    int inode_id = (addr - t)/ storage.superBlock->s_INODE_SIZE;	//inode节点号
    if(storage.inode_bitmap[inode_id] == 0){
        cout << "未使用该inode，无法释放" << endl;
        return false;
    }

    //清空inode内容
    iNode helper = {0};
    fseek(storage.image.get_file_write(), addr, SEEK_SET);
    fwrite(&helper, sizeof(helper), 1, storage.image.get_file_write());

    //更新超级块
    storage.superBlock->s_free_INODE_NUM++;
    //空闲inode数+1
    fseek(storage.image.get_file_write(), SUPERBLOCK_START_ADDR, SEEK_SET);
    fwrite(storage.superBlock, sizeof(SuperBlock), 1, storage.image.get_file_write());

    //更新inode位图
    storage.inode_bitmap[inode_id] = 0;
    fseek(storage.image.get_file_write(), INODEBITMAP_START_ADDR + inode_id,SEEK_SET);
    fwrite(&storage.inode_bitmap[inode_id], sizeof(bool), 1, storage.image.get_file_write());
    fflush(storage.image.get_file_write());

    return true;
}
