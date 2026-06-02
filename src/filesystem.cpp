//
// Created by 林智超 on 2019-06-17.
//

#include <iostream>
#include <filesystem.h>
#include <fstream>
#include "filesystem.h"

FileSystem::FileSystem(Image &_image, SuperBlock *superBlock1) : image(_image), superBlock(superBlock1) {
    this->nextGID = 0;
    this->nextUID = 0;
    this->is_login = false;
    strcpy(this->cur_user_name, "root");
    strcpy(this->cur_grop_name, "root");
    cur_dir_addr = ROOT_DIR_ADDR;
    strcpy(cur_dir_name, "/");
}

bool FileSystem::Format() {
    //初始化超级块
    superBlock->s_INODE_NUM = INODE_NUM;
    superBlock->s_BLOCK_NUM = BLOCK_NUM;
    superBlock->s_SUPERBLOCK_SIZE = sizeof(SuperBlock);
    superBlock->s_INODE_SIZE = INODE_SIZE;
    superBlock->s_BLOCK_SIZE = BLOCK_SIZE;
    superBlock->s_free_INODE_NUM = INODE_NUM;
    superBlock->s_free_BLOCK_NUM = BLOCK_NUM;
    superBlock->s_blocks_per_group = BLOCKS_PER_GROUP;
    superBlock->s_free_addr = BLOCK_STARTADDR;
    superBlock->s_Superblock_StartAddr = SUPERBLOCK_START_ADDR;
    superBlock->s_BlockBitmap_StartAddr = BLOCKBIITMAP_START_ADDR;
    superBlock->s_InodeBitmap_StartAddr = INODEBITMAP_START_ADDR;
    superBlock->s_Block_StartAddr = BLOCK_STARTADDR;
    superBlock->s_Inode_StartAddr = INODE_START_ADDR;


    FILE* fr = image.get_file_read();
    FILE* fw = image.get_file_write();

    memset(inode_bitmap,0,sizeof(inode_bitmap));
    fseek(image.get_file_write(), INODEBITMAP_START_ADDR, SEEK_SET);
    fwrite(inode_bitmap,sizeof(inode_bitmap),1, fw);

    memset(block_bitmap,0,sizeof(block_bitmap));
    fseek(fw,BLOCKBIITMAP_START_ADDR,SEEK_SET);
    fwrite(block_bitmap,sizeof(block_bitmap),1, fw);

    //初始化磁盘块区，根据成组链接法组织
    for(int i = BLOCK_NUM/BLOCKS_PER_GROUP-1; i>=0; i--){	//一共INODE_NUM/BLOCKS_PER_GROUP组，一组FREESTACKNUM（128）个磁盘块 ，第一个磁盘块作为索引
        if(i == BLOCK_NUM/BLOCKS_PER_GROUP-1)
            //最后一组 下面一组没有空闲盘块
            superBlock->s_free[0] = -1;
        else
            superBlock->s_free[0] = BLOCK_STARTADDR + (i+1)*BLOCKS_PER_GROUP*BLOCK_SIZE;	//指向下一个空闲块
        for(int j=1; j<BLOCKS_PER_GROUP; j++){
            superBlock->s_free[j] = BLOCK_STARTADDR + (i*BLOCKS_PER_GROUP + j)*BLOCK_SIZE;
        }
        fseek(fw,BLOCK_STARTADDR+i*BLOCKS_PER_GROUP*BLOCK_SIZE,SEEK_SET);
        fwrite(superBlock->s_free,sizeof(superBlock->s_free),1,fw);	//填满这个磁盘块，512字节
    }
    //超级块写入到虚拟磁盘文件
    fseek(fw, SUPERBLOCK_START_ADDR,SEEK_SET);
    fwrite(superBlock, sizeof(SuperBlock),1,fw);

    fflush(fw);

    //读取inode位图
    fseek(fr,INODEBITMAP_START_ADDR,SEEK_SET);
    fread(inode_bitmap,sizeof(inode_bitmap),1,fr);

    //读取block位图
    fseek(fr,BLOCKBIITMAP_START_ADDR,SEEK_SET);
    fread(block_bitmap,sizeof(block_bitmap),1,fr);

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
    strcpy(cur.user_name, this->cur_user_name);
    strcpy(cur.user_group, this->cur_grop_name);
    cur.inode_cnt = 1;	//一个项，当前目录,"."
    cur.inode_dirblock[0] = block_addr; //根目录指向

    for(int i = 1; i < 10; i++){
        cur.inode_dirblock[i] = -1;
    }
    cur.inode_size = superBlock->s_BLOCK_SIZE;
    cur.inode_indirect_block_first = -1;	//没使用一级间接块
    cur.inode_mode = MODE_DIR | DIR_DEFAULT_PERMISSION; //初始化mode


    //写回inode
    fseek(fw, inode_addr_cur,SEEK_SET);
    fwrite(&cur, sizeof(iNode), 1, fw);
    fflush(fw);

    //创建目录及配置文件
    MakeDir(ROOT_DIR_ADDR,"home");	//用户目录
    FindDir(ROOT_DIR_ADDR,"home");

    MakeDir(this->cur_dir_addr,"root");
    Chmod(this->cur_dir_addr, "root", 0660);

    FindDir(cur_dir_addr,"..");
    MakeDir(cur_dir_addr,"etc");	//配置文件目录
    FindDir(cur_dir_addr,"etc");

    char buf[1000] = {0};

    sprintf(buf,"root:x:%d:%d\n", nextUID++, nextGID++);	//增加条目，用户名：加密密码：用户ID：用户组ID
    Create(cur_dir_addr, "user", buf);	//创建用户信息文件
    sprintf(buf,"root:root\n");	//增加条目，用户名：密码
    Create(cur_dir_addr,"passwd",buf);	//创建用户密码文件
    Chmod(cur_dir_addr,"passwd", 0660);	//修改权限，禁止其它用户读取该文件



    //用户组
    sprintf(buf,"root::0:root\n");	//增加管理员用户组，用户组名：口令（一般为空，这里没有使用）：组标识号：组内用户列表（用，分隔）
    sprintf(buf+strlen(buf),"user::1:\n");	//增加普通用户组，组内用户列表为空
    Create(cur_dir_addr,"group",buf);	//创建用户组信息文件

    FindDir(cur_dir_addr,"..");	//回到根目录

    return true;
}


bool FileSystem::InitFileSystem() {
    //读取superblock
    FILE* fr = image.get_file_read();
    if(fr == NULL) return false;
    fseek(fr, SUPERBLOCK_START_ADDR, SEEK_SET);
    fread(this->superBlock, sizeof(SuperBlock), 1, fr);

    //读取inode位图
    fseek(fr, INODEBITMAP_START_ADDR, SEEK_SET);
    fread(this->inode_bitmap, sizeof(inode_bitmap), 1, fr);

    //读取block位图
    fseek(fr,BLOCKBIITMAP_START_ADDR,SEEK_SET);
    fread(block_bitmap,sizeof(block_bitmap),1,fr);
    return true;
}

bool FileSystem::Login() {
    char user_name[100] = {0};
    char pass_wd[100] = {0};
    cout << "username: ";
    cin >> user_name;
    cin.ignore(100, '\n');
    cout << "password: ";
    cin >> pass_wd;
    cin.ignore(100, '\n');
    if(check(user_name, pass_wd)) {
        is_login = true;
        return true;
    } else {
        is_login = false;
        return false;
    }

}

bool FileSystem::check(char name[], char passwd[]) {
    int user_inode_addr = -1;
    int passwd_inode_addr = -1;
    iNode user_inode;
    iNode passwd_inode;
    iNode cur_dir_inode;
    FILE* fr = image.get_file_read();
    Dir dir_vec[16]; //临时目录
    //进入配置文件目录

    //找到user文件和passwd文件的inode地址

    //取出当前inode地址
    FindDir(this->cur_dir_addr, "etc");
    fseek(fr, this->cur_dir_addr, SEEK_SET);
    fread(&cur_dir_inode, sizeof(iNode), 1, fr);
    for(int i = 0; i < 10; i++) {
        if(cur_dir_inode.inode_dirblock[i] == -1) {
            continue;
        }
        fseek(fr, cur_dir_inode.inode_dirblock[i], SEEK_SET);
        fread(&dir_vec, sizeof(dir_vec), 1, fr);
        for(int j = 0; j < 16; j++) {
            if(strcmp(dir_vec[j].name, "user") == 0 || strcmp(dir_vec->name, "passwd")) {
                iNode helper;
                fseek(fr, dir_vec[j].inode_addr, SEEK_SET);
                fread(&helper, sizeof(iNode), 1, fr);
                if( ((helper.inode_mode >> 9) & 1)==0 ){
                    //是文件
                    if( strcmp(dir_vec[j].name, "user")==0 ){
                        user_inode_addr = dir_vec[j].inode_addr;
                        user_inode = helper;
                    }
                    else if(strcmp(dir_vec[j].name, "passwd") == 0 ){
                        passwd_inode_addr = dir_vec[j].inode_addr;
                        passwd_inode = helper;
                    }
                }
            }
        }
        if(user_inode_addr != -1 && passwd_inode_addr != -1) {
            break;
        }
    }

    char user[100000];
    char buffer[600]; //存磁盘快内容
    int block_pointer = 0;
    int i;
    for(i = 0; i < user_inode.inode_size; i++) {
        if(i % superBlock->s_BLOCK_SIZE == 0) {
            //需要用新的磁盘块
            fseek(fr, user_inode.inode_dirblock[i/(superBlock->s_BLOCK_SIZE)], SEEK_SET);
            fread(buffer, superBlock->s_BLOCK_SIZE, 1, fr);
            block_pointer = 0;
        }
        user[i] = buffer[block_pointer++];
    }
    user[i] = '\0';
    if(strstr(user, name) == nullptr) {
        cout << "用户不存在" << endl;
        FindDir(cur_dir_addr, "..");
        return false;
    }

    block_pointer = 0;
    for(i = 0; i < passwd_inode.inode_size; i++) {
        if(i % superBlock->s_BLOCK_SIZE == 0) {
            //需要用新的磁盘块
            fseek(fr, passwd_inode.inode_dirblock[i/(superBlock->s_BLOCK_SIZE)], SEEK_SET);
            fread(buffer, superBlock->s_BLOCK_SIZE, 1, fr);
            block_pointer = 0;
        }
        user[i] = buffer[block_pointer++];
    }
    user[i] = '\0';

    char *p;
    if( (p = strstr(user, name)) == NULL) {
        cout << "passwd文件中不存在该用户" << endl;
        FindDir(cur_dir_addr, "..");
        return false;
    }

    while ((*p) != ':') {
        p++;
    }
    p++;
    block_pointer = 0;
    while((*p) != '\n') {
        buffer[block_pointer++] = *p;
        p++;
    }
    buffer[block_pointer] = '\0';
    //核对密码
    if(strcmp(buffer, passwd)==0){	//密码正确，登陆
        strcpy(cur_user_name, name);
        if(strcmp(name,"root")==0)
            strcpy(cur_grop_name, "root");	//当前登陆用户组名
        else
            strcpy(cur_grop_name,"user");	//当前登陆用户组名
        FindDir(cur_dir_addr, "..");
        FindDir(cur_dir_addr, "home");
		FindDir(cur_dir_addr, name);	//进入到用户目录
        strcpy(cur_user_dir_name, cur_dir_name);	//复制当前登陆用户目录名
        return true;
    }
    else{
        cout << "密码错误"<< endl;
        FindDir(cur_dir_addr, "..");	//回到根目录
        return false;
    }
}

void FileSystem::FindDir(int inode_addr, const char *name) {
    iNode cur;
    FILE* fr = image.get_file_read();
    fseek(fr, inode_addr, SEEK_SET);
    fread(&cur, sizeof(iNode), 1, fr);
    int filemode;




    int i = 0;
    while(i < 160){
        Dir dir_vec[16] = {0};
        if(cur.inode_dirblock[i/16] == -1){
            i += 16;
            continue;
        }
        //取出磁盘块
        int token_block = cur.inode_dirblock[i/16];
        fseek(fr, token_block, SEEK_SET);
        fread(&dir_vec, sizeof(dir_vec), 1, fr);

        //输出该磁盘块中的所有目录项
        for(int j = 0; j < 16; j++){
            if(strcmp(dir_vec[j].name, name)==0){
                iNode dir;
                //取出该目录项的inode，判断该目录项是目录还是文件
                fseek(fr, dir_vec[j].inode_addr, SEEK_SET);
                fread(&dir, sizeof(iNode),1,fr);

                if( ( (dir.inode_mode >> 9) & 1 ) == 1 ){
                    //找到该目录，判断是否具有进入权限
                    //root用户所有目录都可以查看
                    // 6 -> owner 3 ->group 0 ->other
                    if(strcmp(cur_user_name, dir.user_name) ==0 )
                        filemode = 6;
                    else if(strcmp(cur_grop_name, dir.user_group)==0)
                        filemode = 3;
                    else
                        filemode = 0;

                    if (strcmp(cur_user_name, "root") == 0) {

                    } else if( ((dir.inode_mode >> filemode >> 2) & 1) ==0  ){
                        cout << "Permission Dennied" << endl;
                        return ;
                    }

                    //找到该目录项，如果是目录，更换当前目录

                    this->cur_dir_addr = dir_vec[j].inode_addr;
                    if(strcmp(dir_vec[j].name, ".")==0){
                        //本目录，不动
                    }
                    else if(strcmp(dir_vec[j].name, "..")==0){
                        //上一次目录
                        int k;
                        for(k = strlen(cur_dir_name); k>=0; k--)
                             if(cur_dir_name[k]=='/')
                                break;
                        cur_dir_name[k]='\0';
                        if(strlen(cur_dir_name)==0) {
                            //根目录
                            cur_dir_name[0] = '/'; cur_dir_name[1] = '\0';
                        }
                    }
                    else{
                        if(cur_dir_name[strlen(cur_dir_name)-1]!='/')
                            strcat(cur_dir_name, "/");
                        strcat(cur_dir_name, dir_vec[j].name);
                    }
                    return ;
                }
                else{
                    //找到该目录项，如果不是目录，继续找
                }

            }

            i++;
        }

    }

    //没找到
    cout << "找不到该目录" << endl;
    return ;


}

bool FileSystem::MakeMenu() {
    cout << "╭─" << cur_user_name << "@" << HOSTNAME << " ";
    cout << cur_dir_name << endl;
    cout << "╰─> ";
    return true;
}

bool FileSystem::MakeDir(int inode_addr, char *name) {
    if(strlen(name) >= MAX_NAME_SIZE) {
        cout << "超过最大目录长度" << endl;
        return false;
    }
    FILE* fr = image.get_file_read();
    FILE* fw = image.get_file_write();
    Dir dir_vec[16];
    iNode cur;
    fseek(fr, inode_addr, SEEK_SET);
    fread(&cur, sizeof(iNode), 1, fr);
    int filemode;

    if(strcmp(cur_user_name, cur.user_name)==0 || strcmp(cur_user_name, "root") == 0)
        filemode = 6;
    else if(strcmp(cur_grop_name, cur.user_group)==0)
        filemode = 3;
    else
        filemode = 0;

    if( ((cur.inode_mode >> filemode >> 2) & 1) == 0) {
        cout << "Permission Dennied" << endl;
    }

    int i = 0;
    int find_pos_i = -1, find_pos_j = -1;
    // 160 -> 10 直接 * 16 = 160 个目录项
    while(i < 160) {
        int dir_in_block = i / 16;
        if(cur.inode_dirblock[dir_in_block] == -1) {
            i += 16;
            continue;
        }

        fseek(fr, cur.inode_dirblock[dir_in_block], SEEK_SET);
        fread(dir_vec, sizeof(dir_vec), 1, fr);
        fflush(fr);

        for(int j = 0; j < 16; j++) {
            if(strcmp(dir_vec[j].name, name) == 0) {
                iNode helper;
                fseek(fr, dir_vec[j].inode_addr, SEEK_SET);
                fread(&helper, sizeof(iNode), 1, fr);
                if ((( helper.inode_mode >> 9) & 1 )) {
                    cout << "目录已存在" << endl;
                    return false;
                }
            } else {
                if(strcmp(dir_vec[j].name, "") == 0) {
                    if(find_pos_i == -1) {
                        find_pos_i = dir_in_block;
                        find_pos_j = j;
                    }
                }
            }
        }
        i++;
    }

    //找到空闲位置
    if(find_pos_i != -1){

        //取出这个直接块，要加入的目录条目的位置
        fseek(fr, cur.inode_dirblock[find_pos_i], SEEK_SET);
        fread(dir_vec, sizeof(dir_vec), 1, fr);
        fflush(fr);

        //创建这个目录项
        strcpy(dir_vec[find_pos_j].name, name);	//目录名


        //写入两条记录 "." ".."，分别指向当前inode节点地址，和父inode节点
        int cur_inode_addr = INodeAlloc();	//分配当前节点地址
        if(cur_inode_addr == -1){
            cout << "inode节点不够" << endl;
            return false;
        }
        dir_vec[find_pos_j].inode_addr = cur_inode_addr; //给这个新的目录分配的inode地址

        //设置新的inode
        iNode p;
        p.inode_id = static_cast<unsigned short>((cur_inode_addr - INODE_START_ADDR) / superBlock->s_INODE_SIZE);
        strcpy(p.user_name, cur_user_name);
        strcpy(p.user_group, cur_grop_name);
        p.inode_cnt = 2;	//两个项，当前目录, "."和".."

        //分配这个inode的磁盘块，在磁盘号中写入两条记录 . 和 ..
        int curblockAddr = BlockAlloc();
        if(curblockAddr == -1){
            printf("block分配失败\n");
            return false;
        }
        Dir dir_vec_next[16] = {0};	//临时目录项列表 - 2
        strcpy(dir_vec_next[0].name, ".");
        strcpy(dir_vec_next[1].name, "..");
        dir_vec_next[0].inode_addr = cur_inode_addr;	//当前目录inode地址
        dir_vec_next[1].inode_addr = inode_addr;	//父目录inode地址

        //写入到当前目录的磁盘块
        fseek(fw, curblockAddr,SEEK_SET);
        fwrite(dir_vec_next, sizeof(dir_vec_next), 1, fw);

        p.inode_dirblock[0] = curblockAddr;
        for(int k = 1; k < 10; k++){
            p.inode_dirblock[k] = -1;
        }
        p.inode_size = superBlock->s_BLOCK_SIZE;
        p.inode_indirect_block_first = -1;	//没使用一级间接块
        p.inode_mode = MODE_DIR | DIR_DEFAULT_PERMISSION;

        //将inode写入到申请的inode地址
        fseek(fw, cur_inode_addr, SEEK_SET);
        fwrite(&p, sizeof(iNode), 1, fw);

        //将当前目录的磁盘块写回
        fseek(fw, cur.inode_dirblock[find_pos_i],SEEK_SET);
        fwrite(dir_vec, sizeof(dir_vec), 1, fw);

        //写回inode
        cur.inode_cnt++;
        fseek(fw, inode_addr, SEEK_SET);
        fwrite(&cur,sizeof(iNode), 1, fw);
        fflush(fw);
        return true;
    }
    else{
        cout << "无空闲目录项" << endl;
        printf("没找到空闲目录项,目录创建失败");
        return false;
    }
}

int FileSystem::INodeAlloc() {
    if(superBlock->s_free_INODE_NUM == 0) {
        cout << "没有空闲inode可以分配" << endl;
        return -1;
    } else {
        int pos = 0;
        for(int i = 0; i < superBlock->s_INODE_NUM; i++) {
            if(inode_bitmap[i] == 0) {
                pos = i;
                break;
            }
        }
        FILE* fw = image.get_file_write();
        superBlock->s_free_INODE_NUM--;
        fseek(fw, SUPERBLOCK_START_ADDR, SEEK_SET);
        fwrite(superBlock, sizeof(superBlock), 1, fw);
        inode_bitmap[pos] = 1;
        fseek(fw, INODEBITMAP_START_ADDR + pos, SEEK_SET);
        fwrite(&inode_bitmap[pos], sizeof(bool), 1, fw);
        fflush(fw);

        return INODE_START_ADDR + pos * superBlock->s_INODE_SIZE;


    }
}

int FileSystem::BlockAlloc() {
    int top; //栈顶指针
    if (superBlock->s_free_BLOCK_NUM==0) {
        cout << "已经没有空闲块分配" << endl;
        return -1;
    }
    else{
        //如果已经是最后一块，此时top应该为0
        top = (superBlock->s_free_BLOCK_NUM-1) % superBlock->s_blocks_per_group;
    }
    //将栈顶取出
    //如果已是栈底，将当前块号地址返回，即为栈底块号，并将栈底指向的新空闲块堆栈覆盖原来的栈
    int alloc_addr;
    FILE* fr = image.get_file_read();
    if(top == 0){
        alloc_addr = superBlock->s_free_addr;
        superBlock->s_free_addr = superBlock->s_free[0];	//取出下一个存有空闲块堆栈的空闲块的位置，更新空闲块堆栈指针

        //取出对应空闲块内容，覆盖原来的空闲块堆栈
        //取出下一个空闲块堆栈，覆盖原来的
        fseek(fr, superBlock->s_free_addr, SEEK_SET);
        fread(superBlock->s_free, sizeof(superBlock->s_free), 1, fr);
        fflush(fr);
        superBlock->s_free_BLOCK_NUM--;

    }
    else{	//如果不为栈底，则将栈顶指向的地址返回，栈顶指针-1.
        alloc_addr = superBlock->s_free[top];	//保存返回地址
        superBlock->s_free[top] = -1;	//清栈顶
        top--;		//栈顶指针-1
        superBlock->s_free_BLOCK_NUM--;	//空闲块数-1

    }
    FILE* fw = image.get_file_write();
    //更新超级块
    fseek(fw, SUPERBLOCK_START_ADDR, SEEK_SET);
    fwrite(superBlock, sizeof(SuperBlock), 1, fw);
    fflush(fw);

    //更新block位图
    block_bitmap[(alloc_addr-BLOCK_STARTADDR)/BLOCK_SIZE] = 1;
    fseek(fw, (alloc_addr-BLOCK_STARTADDR)/BLOCK_SIZE+BLOCKBIITMAP_START_ADDR, SEEK_SET);
    fwrite(&block_bitmap[(alloc_addr-BLOCK_STARTADDR)/BLOCK_SIZE], sizeof(bool), 1, fw);
    fflush(fw);

    return alloc_addr;

}

bool FileSystem::Create(int father_inode_addr, const char *name, char *file_content) {
    if(strlen(name) >= MAX_NAME_SIZE){
        cout << "超过最大文件名" << endl;
        return false;
    }
    FILE *fr = image.get_file_read();
    FILE *fw = image.get_file_write();
    Dir dir_vec[16];

    //从这个地址取出inode
    iNode cur;
    fseek(fr, father_inode_addr, SEEK_SET);
    fread(&cur, sizeof(iNode), 1, fr);

    int i = 0;
    int find_pos_i = -1, find_pos_j = -1;
    int dir_block_num;
    while(i < 160){
        dir_block_num = i/16;

        if(cur.inode_dirblock[dir_block_num]==-1){
            i += 16;
            continue;
        }
        fseek(fr, cur.inode_dirblock[dir_block_num], SEEK_SET);
        fread(dir_vec, sizeof(dir_vec), 1, fr);
        fflush(fr);

        //输出该磁盘块中的所有目录项
        for(int j = 0; j < 16; j++){

            if( find_pos_i == -1 && strcmp(dir_vec[j].name, "")==0 ){
                //找到一个空闲记录，将新文件创建到这个位置
                find_pos_i = dir_block_num;
                find_pos_j = j;
            }
            else if(strcmp(dir_vec[j].name, name)==0 ){
                //重名，取出inode，判断是否是文件
                iNode helper;
                fseek(fr, dir_vec[j].inode_addr, SEEK_SET);
                fread(&helper, sizeof(iNode), 1, fr);
                if( ((helper.inode_mode>>9) &1)==0 ){	//是文件且重名，不能创建文件
                    cout << "文件已经存在" << endl;
                    file_content[0] = '\0';
                    return false;
                }
            }
            i++;
        }

    }
    if(find_pos_i != -1){
        //取出之前那个空闲目录项对应的磁盘块
        fseek(fr, cur.inode_dirblock[find_pos_i], SEEK_SET);
        fread(dir_vec, sizeof(dir_vec), 1, fr);
        fflush(fr);

        //创建这个目录项
        strcpy(dir_vec[find_pos_j].name, name);
        int cur_inode_addr = INodeAlloc();
        if(cur_inode_addr == -1){
            cout << "INode 分配失败" << endl;
            return false;
        }
        dir_vec[find_pos_j].inode_addr = cur_inode_addr;

        //设置新条目的inode
        iNode p;
        p.inode_id = static_cast<unsigned short>((cur_inode_addr - INODE_START_ADDR) / superBlock->s_INODE_SIZE);
        strcpy(p.user_name, cur_user_name);
        strcpy(p.user_group, cur_grop_name);
        p.inode_cnt = 1;	//只有一个文件指向


        int k;

        //将buf内容存到磁盘块
        int len = static_cast<int>(strlen(file_content));
        for(int k = 0; k < len; k += superBlock->s_BLOCK_SIZE){
            int cur_block_Addr = BlockAlloc();
            if(cur_block_Addr == -1){
                cout << "Block分配失败" << endl;
                return false;
            }
            p.inode_dirblock[k/superBlock->s_BLOCK_SIZE] = cur_block_Addr;
            fseek(fw, cur_block_Addr,SEEK_SET);
            fwrite(file_content+k, superBlock->s_BLOCK_SIZE, 1, fw);
        }


        //对其他项赋值为-1
        for(k= len/superBlock->s_BLOCK_SIZE+1; k<10;k++){
            p.inode_dirblock[k] = -1;
        }


        if( len == 0){	//长度为0的话也分给它一个block
            int cur_block_Addr = BlockAlloc();
            if(cur_block_Addr == -1){
                cout << "Block分配失败" << endl;
                return false;
            }
            p.inode_dirblock[k/superBlock->s_BLOCK_SIZE] = cur_block_Addr;
            //写入到当前目录的磁盘块
            fseek(fw, cur_block_Addr, SEEK_SET);
            fwrite(file_content, superBlock->s_BLOCK_SIZE, 1, fw);

        }
        p.inode_size = len;
        p.inode_indirect_block_first = -1;	//没使用一级间接块
        p.inode_mode = 0;
        p.inode_mode = MODE_FILE | FILE_DEF_PERMISSION;

        //将inode写入到申请的inode地址
        fseek(fw, cur_inode_addr, SEEK_SET);
        fwrite(&p, sizeof(iNode), 1, fw);

        //将当前目录的磁盘块写回
        fseek(fw, cur.inode_dirblock[find_pos_i], SEEK_SET);
        fwrite(dir_vec, sizeof(dir_vec), 1, fw);

        //写回inode
        cur.inode_cnt++;
        fseek(fw, father_inode_addr, SEEK_SET);
        fwrite(&cur,sizeof(iNode), 1, fw);
        fflush(fw);
        return true;
    }
    else
        return false;
}

void FileSystem::Chmod(int father_inode_addr, const char *name, int mode) {

    if(strcmp(name,".")==0 || strcmp(name,"..")==0){
        cout << "usage: chmod [filename] [permissions] : Change the file permissions" << endl;
        return ;
    }

    FILE* fr = image.get_file_read();
    FILE* fw = image.get_file_write();
    iNode cur, res;
    fseek(fr, father_inode_addr, SEEK_SET);
    fread(&cur,sizeof(iNode), 1, fr);
    int i = 0, j = 0;
    Dir dir_vec[16] = {0};
    bool flag = false;
    while(i < 160) {
        if(cur.inode_dirblock[i/16] == -1){
            i += 16;
            continue;
        }
        //取出磁盘块
        int alloc_block_dir = cur.inode_dirblock[i/16];
        fseek(fr, alloc_block_dir, SEEK_SET);
        fread(&dir_vec, sizeof(dir_vec), 1, fr);
        fflush(fr);

        //输出该磁盘块中的所有目录项
        for(j = 0; j < 16; j++){
            if( strcmp(dir_vec[j].name, name)==0 ){	//找到该目录或者文件
                //取出对应的inode
                fseek(fr, dir_vec[j].inode_addr, SEEK_SET);
                fread(&res, sizeof(iNode),1,fr);
                fflush(fr);
                flag = true;
            }
            if(flag) break;
        }
        if(flag) break;
        i++;
    }
    if(i >= 160){
        cout << "当前文件名不存在" << endl;
        return ;
    }

    if(strcmp(cur_user_name, res.user_name) != 0 && strcmp(cur_user_name, "root")!=0){
        cout << "" << endl;
        return ;
    }


    res.inode_mode  = static_cast<unsigned short>((res.inode_mode >> 9 << 9) | mode);

    fseek(fw, dir_vec[j].inode_addr,SEEK_SET);
    fwrite(&res, sizeof(iNode), 1, fw);
    fflush(fw);
}

void FileSystem::ReadCommand(char *command) {
    char param_first[100];
    char param_second[100];
    char params_third[100];
    char buffer[100000];	//最大100K
    sscanf(command,"%s", param_first);
    if(strcmp(param_first, "ls")==0){
        ShowDir(cur_dir_addr);
    }
    else if(strcmp(param_first, "cd")==0){
        sscanf(command, "%s%s", param_first, param_second);
        FindDir(cur_dir_addr, param_second);
    }
    else if(strcmp(param_first,"mkdir")==0){
        sscanf(command, "%s%s", param_first, param_second);
        MakeDir(cur_dir_addr, param_second);
        Chmod(cur_dir_addr, param_second, 0660);
    }
    else if(strcmp(param_first, "rmdir")==0){
        sscanf(command, "%s%s", param_first, param_second);
        DeleteDir(cur_dir_addr, param_second);
    }
    else if(strcmp(param_first, "nano")==0){	//创建一个文件
        char* buffer = new char[255];
        sscanf(command, "%s%s", param_first, param_second);
        editor(cur_dir_addr, param_second, buffer);	//读取内容到buf
    }
    else if(strcmp(param_first, "touch")==0){
        sscanf(command, "%s%s", param_first, param_second);
        MakeFile(cur_dir_addr, param_second, buffer);	//读取内容到buf
    }
    else if(strcmp(param_first, "rm")==0){	//删除一个文件
        sscanf(command, "%s%s" , param_first, param_second);
        DelFile(cur_dir_addr, param_second);
    }
    else if(strcmp(param_first, "clear") == 0){
        system("clear");
    }
    else if(strcmp(param_first, "exit") == 0){
        Quit();
    }
    else if(strcmp(param_first, "useradd") == 0){
        param_second[0] = '\0';
        sscanf(command, "%s%s", param_first, param_second);
        if(strlen(param_second) == 0){
            cout << "Unknown option" << endl;
        }
        else{
            AddUser(param_second);
        }
    }
    else if(strcmp(param_first, "userdel") == 0){
        param_second[0] = '\0';
        sscanf(command, "%s%s", param_first, param_second);
        if(strlen(param_second) == 0){
            cout << "unlink user" << endl;
        }
        else{
            DeleteUser(param_second);
        }
    } else if(strcmp(param_first, "cat") == 0){
        sscanf(command, "%s%s", param_first, param_second);
        if(strlen(param_second) == 0){
            cout << "Unknown option" << endl;
        }
        Cat(cur_dir_addr, param_second);
    }
    else if(strcmp(param_first, "chmod")==0){
        param_second[0] = '\0';
        params_third[0] = '\0';
        sscanf(command, "%s%s%s", param_first, param_second, params_third);
        if(strlen(param_second)==0 || strlen(params_third)==0){
            cout << "usage: chmod [filename] [permissions] : Change the file permissions" << endl;
        }
        else{
            int num = 0;
            for(int i = 0; params_third[i]; i++) {
                num = num * 8 + params_third[i] - '0';
            }
            Chmod(cur_dir_addr, param_second, num);
        }
    }
    else if(strcmp(param_first, "help")==0){
        Help();
    }
    else if(strcmp(param_first, "format")==0){
        if(strcmp(cur_user_name, "root") != 0){
            cout << "Permission Dennied" << endl;
            return ;
        }
        Format();
        Quit();
    } else if(strcmp(param_first, "pwd")==0){
       cout << this->cur_dir_name << endl;
    }
    else{
        cout << "command not found: " << command << endl;
    }
    return ;
}

void FileSystem::readFirst(int addr) {
    FILE* fr = image.get_file_read();
    FILE* fw = image.get_file_write();
    iNode cur;

    fseek(fr, addr, SEEK_SET);
    fread(&cur,sizeof(iNode), 1, fr);
    fflush(fr);

    fseek(fr, addr, SEEK_SET);
    fread(&cur,sizeof(iNode), 1, fr);
    fflush(fr);

    //取出目录项数
    int cnt = cur.inode_cnt;

    //判断文件模式。6为owner，3为group，0为other
    int file_mode;
    if(strcmp(cur_user_name, cur.user_name)==0 || strcmp(cur_user_name, "root") == 0)
        file_mode = 6;
    else if(strcmp(cur_grop_name, cur.user_group)==0)
        file_mode = 3;
    else
        file_mode = 0;


    if (strcmp(cur_user_name, "root") == 0) {

    } else if( ((cur.inode_mode >> file_mode >> 2) & 1) ==0  ){
        cout << "Permission Dennied" << endl;
        return ;
    }

    //依次取出磁盘块
    int i = 0;
    while(i < cnt && i<160){
        Dir dir_vec[16] = {0};
        if(cur.inode_dirblock[i/16]==-1){
            i+=16;
            continue;
        }
        int allock_block_addr = cur.inode_dirblock[i/16];
        fseek(fr,allock_block_addr, SEEK_SET);
        fread(&dir_vec, sizeof(dir_vec), 1, fr);
        fflush(fr);

        for(int j=0; j < 16 && i < cnt;j++){

            iNode helper;
            //取出该目录项的inode，判断该目录项是目录还是文件
            fseek(fr, dir_vec[j].inode_addr, SEEK_SET);
            fread(&helper, sizeof(iNode),1,fr);
            fflush(fr);

            if( strcmp(dir_vec[j].name, "")==0 ){
                continue;
            }

            if(i > 2 && (strcmp(dir_vec[j].name, ".")==0 || strcmp(dir_vec[j].name, "..")== 0) ) {
                continue;
            }

//            //输出信息
//            if( ( (helper.inode_mode>9) & 1 ) == 1 ){
//                cout << "d";
//            }
//            else{
//                cout << "-";
//            }


            int permiss_index = 8;
            while(permiss_index >= 0){
                if( ((helper.inode_mode >> permiss_index) & 1) == 1){
//                    if(permiss_index % 3 == 2)	cout << "r";
//                    if(permiss_index % 3 == 1)	cout << "w";
//                    if(permiss_index % 3 == 0)	cout << "x";
                }
                else{
//                    cout << "-";
                }
                permiss_index--;
            }
//            printf("\t");
//
//            printf("%d\t", helper.inode_cnt);	//该文件链接
//            printf("%s\t",helper.user_name);	//文件所属用户名
//            printf("%s\t",helper.user_group);	//文件所属用户名
//            printf("%5d B\t", helper.inode_size);	//文件大小
//            printf("%s", dir_vec[j].name);	//文件名
//            cout << endl;
            i++;
        }

    }
}


void FileSystem::ShowDir(int addr) {
    readFirst(addr);
    FILE* fr = image.get_file_read();
    FILE* fw = image.get_file_write();
    iNode cur;

    fseek(fr, addr, SEEK_SET);
    fread(&cur,sizeof(iNode), 1, fr);
    fflush(fr);


    //取出目录项数
    int cnt = cur.inode_cnt;

    //判断文件模式。6为owner，3为group，0为other
    int file_mode;
    if(strcmp(cur_user_name, cur.user_name)==0 || strcmp(cur_user_name, "root") == 0)
        file_mode = 6;
    else if(strcmp(cur_grop_name, cur.user_group)==0)
        file_mode = 3;
    else
        file_mode = 0;


    if (strcmp(cur_user_name, "root") == 0) {

    } else if( ((cur.inode_mode >> file_mode >> 2) & 1) ==0  ){
        cout << "Permission Dennied" << endl;
        return ;
    }

    //依次取出磁盘块
    int i = 0;
    while(i < cnt && i<160){
        Dir dir_vec[16] = {0};
        if(cur.inode_dirblock[i/16]==-1){
            i+=16;
            continue;
        }
        int allock_block_addr = cur.inode_dirblock[i/16];
        fseek(fr,allock_block_addr, SEEK_SET);
        fread(&dir_vec, sizeof(dir_vec), 1, fr);
        fflush(fr);

        for(int j=0; j < 16 && i < cnt;j++){

            iNode helper;
            //取出该目录项的inode，判断该目录项是目录还是文件
            fseek(fr, dir_vec[j].inode_addr, SEEK_SET);
            fread(&helper, sizeof(iNode),1,fr);
            fflush(fr);

            if( strcmp(dir_vec[j].name, "")==0 ){
                continue;
            }

            if(i > 2 && (strcmp(dir_vec[j].name, ".")==0 || strcmp(dir_vec[j].name, "..")== 0) ) {
                continue;
            }

            //输出信息
            if( ( (helper.inode_mode>>9) & 1 ) == 1 ){
                cout << "d";
            }
            else{
                cout << "-";
            }


            int permiss_index = 8;
            while(permiss_index >= 0){
                if( ((helper.inode_mode >> permiss_index) & 1) == 1){
                    if(permiss_index % 3 == 2)	cout << "r";
                    if(permiss_index % 3 == 1)	cout << "w";
                    if(permiss_index % 3 == 0)	cout << "x";
                }
                else{
                    cout << "-";
                }
                permiss_index--;
            }
            printf("\t");

            printf("%d\t", helper.inode_cnt);	//该文件链接
            printf("%s\t",helper.user_name);	//文件所属用户名
            printf("%s\t",helper.user_group);	//文件所属用户名
            printf("%5d B\t", helper.inode_size);	//文件大小
            printf("%s", dir_vec[j].name);	//文件名
            cout << endl;
            i++;
        }

    }
}

bool FileSystem::DeleteDir(int addr, char *params) {

    FILE* fr = image.get_file_read();
    FILE* fw = image.get_file_write();

    if(strlen(params) >= MAX_NAME_SIZE){
        cout << "已经超过最长目录长度" << endl;
        return false;
    }
    if(strcmp(params, ".")==0 || strcmp(params, "..")==0){
        cout << "不能删除当前目录或者上级目录" << endl;
        return 0;
    }

    //从这个地址取出inode
    iNode cur;
    fseek(fr, addr,SEEK_SET);
    fread(&cur,sizeof(iNode), 1, fr);

    //判断文件模式。6为owner，3为group，0为other
    int filemode;
    if( strcmp(cur_user_name, cur.user_name)==0 )
        filemode = 6;
    else if(strcmp(cur_user_name, cur.user_group)==0)
        filemode = 3;
    else
        filemode = 0;

    if( (((cur.inode_mode >> filemode >> 1) & 1 ) ==0 ) && (strcmp(cur_user_name, "root")!=0) ){
        cout << "Permission Dennied" << endl;
        return false;
    }


    //依次取出磁盘块
    int i = 0;
    while(i < 160){	//小于160
        Dir dir_vec[16] = {0};

        if(cur.inode_dirblock[i/16] == -1){
            i += 16;
            continue;
        }
        //取出磁盘块
        int alloact_block_addr = cur.inode_dirblock[i/16];
        fseek(fr, alloact_block_addr, SEEK_SET);
        fread(&dir_vec, sizeof(dir_vec), 1, fr);

        //找到要删除的目录
        for(int j = 0; j < 16; j++){
            iNode helper;

            fseek(fr, dir_vec[j].inode_addr, SEEK_SET);
            fread(&helper, sizeof(iNode), 1, fr);

            if( strcmp(dir_vec[j].name, params) == 0){
                if( ( (helper.inode_mode >> 9) & 1 ) == 1 ){
                    //目录
                    DeleteFileOrDir(dir_vec[j].inode_addr);
                    strcpy(dir_vec[j].name, "");
                    dir_vec[j].inode_addr = -1;
                    fseek(fw, alloact_block_addr, SEEK_SET);
                    fwrite(&dir_vec, sizeof(dir_vec),1,fw);
                    cur.inode_cnt = static_cast<unsigned short>(cur.inode_cnt - 1);
                    fseek(fw, addr,SEEK_SET);
                    fwrite(&cur,sizeof(iNode), 1, fw);
                    fflush(fw);
                    return true;
                }
                else{
                    //非目录
                }
            }
            i++;
        }

    }


    return false;
}

void FileSystem::DeleteFileOrDir(int addr) {
    FILE* fr = image.get_file_read();
    FILE* fw = image.get_file_write();
    iNode cur;
    fseek(fr, addr,SEEK_SET);
    fread(&cur, sizeof(iNode), 1, fr);

    //取出目录项数
    int cnt = cur.inode_cnt;
    if(cnt <= 2){
        FreeBlock(cur.inode_dirblock[0]);
        INodeFree(addr);
        return ;
    }

    //依次取出磁盘块
    int i = 0;
    while(i < 160){	//小于160
        Dir dir_vec[16] = {0};

        if(cur.inode_dirblock[i/16]==-1){
            i+=16;
            continue;
        }
        //取出磁盘块
        int allocate_block = cur.inode_dirblock[i/16];
        fseek(fr, allocate_block,SEEK_SET);
        fread(&dir_vec, sizeof(dir_vec), 1, fr);

        //从磁盘块中依次取出目录项，递归删除
        bool isFree = false;
        for(int j = 0; j < 16; j++){

            if(! (strcmp(dir_vec[j].name, ".")==0 ||
                  strcmp(dir_vec[j].name, "..")==0 ||
                  strcmp(dir_vec[j].name, "")==0 ) ){
                isFree = true;
                DeleteFileOrDir(dir_vec[j].inode_addr);
            }
            i++;
        }

        //该磁盘块已空，回收
        if(isFree)
            FreeBlock(allocate_block);

    }
    //该inode已空，回收
    INodeFree(addr);
}

bool FileSystem::FreeBlock(int addr) {
    if( (addr - BLOCK_STARTADDR) % superBlock->s_BLOCK_SIZE != 0 ){
        cout << "不是磁盘块所处正确位置" << endl;
        return false;
    }
    unsigned int block_id = static_cast<unsigned int>((addr - BLOCK_STARTADDR) / superBlock->s_BLOCK_SIZE);	//inode节点号
    //该地址还未使用，不能释放空间
    if(block_bitmap[block_id]==0){
        printf("该block（磁盘块）还未使用，无法释放\n");
        return false;
    }

    int top;
    if(superBlock->s_free_BLOCK_NUM == superBlock->s_BLOCK_NUM){
        cout << "没有可释放的磁盘块" << endl;
        return false;
    }
    else{	//非满
        top = (superBlock->s_free_BLOCK_NUM-1) % superBlock->s_blocks_per_group;
        char buffer[BLOCK_SIZE] = {0};
        fseek(image.get_file_write(), addr, SEEK_SET);
        fwrite(buffer, sizeof(buffer), 1, image.get_file_write());

        if(top == superBlock->s_blocks_per_group-1){

            //该空闲块作为新的空闲块堆栈
            superBlock->s_free[0] = superBlock->s_free_addr;	//新的空闲块堆栈第一个地址指向旧的空闲块堆栈指针
            //清空元素
            for(int i= 1;i < superBlock->s_blocks_per_group; i++){
                superBlock->s_free[i] = -1;
            }
            fseek(image.get_file_write(), addr, SEEK_SET);
            fwrite(superBlock->s_free, sizeof(superBlock->s_free),1, image.get_file_write());	//填满这个磁盘块，512字节

        }
        else{
            top++;
            superBlock->s_free[top] = addr;
        }
    }


    superBlock->s_free_BLOCK_NUM++;	//空闲块数+1
    fseek(image.get_file_write(),SUPERBLOCK_START_ADDR,SEEK_SET);
    fwrite(superBlock, sizeof(SuperBlock),1, image.get_file_write());

    //更新block位图
    block_bitmap[block_id] = 0;
    fseek(image.get_file_write(), block_id + BLOCKBIITMAP_START_ADDR, SEEK_SET);
    fwrite(&block_bitmap[block_id], sizeof(bool), 1, image.get_file_write());
    fflush(image.get_file_write());
    return true;
}

bool FileSystem::INodeFree(int addr) {
    int t = INODE_START_ADDR;
    if( ( addr - t) % superBlock->s_INODE_SIZE != 0 ){
        cout << "不是正确的inode节点位置" << endl;
        return false;
    }
    int inode_id = (addr - t)/ superBlock->s_INODE_SIZE;	//inode节点号
    if(inode_bitmap[inode_id] == 0){
        cout << "未使用该inode，无法释放" << endl;
        return false;
    }

    //清空inode内容
    iNode helper = {0};
    fseek(image.get_file_write(), addr, SEEK_SET);
    fwrite(&helper, sizeof(helper), 1, image.get_file_write());

    //更新超级块
    superBlock->s_free_INODE_NUM++;
    //空闲inode数+1
    fseek(image.get_file_write(), SUPERBLOCK_START_ADDR, SEEK_SET);
    fwrite(superBlock, sizeof(SuperBlock), 1, image.get_file_write());

    //更新inode位图
    inode_bitmap[inode_id] = 0;
    fseek(image.get_file_write(), INODEBITMAP_START_ADDR + inode_id,SEEK_SET);
    fwrite(&inode_bitmap[inode_id], sizeof(bool), 1, image.get_file_write());
    fflush(image.get_file_write());

    return true;
}

void FileSystem::Quit() {
    memset(cur_user_name, 0, sizeof(cur_user_name));		//清空当前用户名
    memset(cur_user_dir_name, 0, sizeof(cur_user_dir_name));	//清空当前用户目录
    cur_dir_addr = ROOT_DIR_ADDR;	//当前用户目录地址设为根目录地址
    strcpy(cur_dir_name, "/");		//当前目录设为"/"
    is_login = false;
}

void FileSystem::MakeFile(int addr, char *param, char *buffer) {


    FILE* fr = image.get_file_read();
    Dir dir_vec[16];

    iNode cur, create_file_inode;
    fseek(fr, addr, SEEK_SET);
    fread(&cur, sizeof(iNode), 1, fr);

    //判断文件模式。6为owner，3为group，0为other
    int filemode;
    if(strcmp(cur_user_name, cur.user_name)==0 || strcmp(cur_user_name, "root") == 0)
        filemode = 6;
    else if(strcmp(cur_user_name, cur.user_group)==0)
        filemode = 3;
    else
        filemode = 0;

    if( ((cur.inode_mode >> filemode >> 2) & 1) == 0) {
        cout << "Permission Dennied" << endl;
        return ;
    }



    int i = 0;
    while(i < 160){

        if(cur.inode_dirblock[i/16] == -1){
            i += 16;
            continue;
        }
        fseek(fr, cur.inode_dirblock[i/16], SEEK_SET);
        fread(dir_vec, sizeof(dir_vec), 1, fr);
        fflush(fr);

        //输出该磁盘块中的所有目录项
        for(int j = 0; j < 16; j++){
            //当前是否有重名
            if(strcmp(dir_vec[j].name, param) ==0 ){

                fseek(fr, dir_vec[j].inode_addr, SEEK_SET);
                fread(&create_file_inode, sizeof(iNode), 1, fr);
                if( ((create_file_inode.inode_mode >> 9) & 1) == 0){
                    cout << "该文件已经存在" << endl;
                    return ;
                }
            }
            i++;
        }
    }

    //文件不存在，创建一个空文件
    if( ((cur.inode_mode >> filemode >> 1) & 1) == 1){
        //可写。可以创建文件
        buffer[0] = '\0';
        Create(addr, param, buffer);	//创建文件
    }
    else{
        cout << "Permission Dennied" << endl;
        return ;
    }
}

void FileSystem::DelFile(int addr, char *param) {
    if(strlen(param) >= MAX_NAME_SIZE){
        cout << "超过最长文件名" << endl;
        return ;
    }
    FILE* fr = image.get_file_read();
    FILE* fw = image.get_file_write();
    iNode cur;
    fseek(fr, addr, SEEK_SET);
    fread(&cur, sizeof(iNode), 1, fr);

    //取出目录项数
    int cnt = cur.inode_cnt;

    //判断文件模式。6为owner，3为group，0为other
    int filemode;
    if(strcmp(cur_user_name, cur.user_name)==0 || strcmp(cur_user_name, "root") == 0)
        filemode = 6;
    else if(strcmp(cur_grop_name, cur.user_group)==0)
        filemode = 3;
    else
        filemode = 0;

    if( ((cur.inode_mode >> filemode >> 1 ) & 1) ==0 ){
        cout << "Permission Dennied" << endl;
        return ;
    }

    //依次取出磁盘块
    int i = 0;
    while(i < 160){	//小于160
        Dir dir_vec[16] = {0};

        if(cur.inode_dirblock[i/16] == -1){
            i += 16;
            continue;
        }
        //取出磁盘块
        int allocat_block_addr = cur.inode_dirblock[i/16];
        fseek(fr, allocat_block_addr, SEEK_SET);
        fread(&dir_vec, sizeof(dir_vec), 1, fr);

        for(int j = 0; j < 16; j++){
            iNode helper;
            //取出该目录项的inode，判断该目录项是目录还是文件
            fseek(fr, dir_vec[j].inode_addr, SEEK_SET);
            fread(&helper, sizeof(iNode),1,fr);
            if( strcmp(dir_vec[j].name, param)==0){
                if( ( (helper.inode_mode >> 9) & 1 ) == 1 ){
                    //目录
                }
                else{
                    for(int k = 0; k < 10; k++)
                        if(helper.inode_dirblock[k] != -1)
                            FreeBlock(helper.inode_dirblock[k]);

                    //释放inode
                    INodeFree(dir_vec[j].inode_addr);

                    //删除该目录条目，写回磁盘
                    strcpy(dir_vec[j].name, "");
                    dir_vec[j].inode_addr = -1;
                    fseek(fw, allocat_block_addr, SEEK_SET);
                    fwrite(&dir_vec, sizeof(dir_vec), 1, fw);
                    cur.inode_cnt--;
                    fseek(fw, addr, SEEK_SET);
                    fwrite(&cur, sizeof(iNode), 1, fw);

                    fflush(fw);
                    return ;
                }
            }
            i++;
        }

    }
    cout << param << ": No such file or directory" << endl;
}

void FileSystem::AddUser(char *username) {



    // 取出文件指针
    FILE* fr = image.get_file_read();
    FILE* fw = image.get_file_write();

    if(strcmp(cur_user_name, "root")!=0){
        cout << "非root用户, 不能添加新用户" << endl;
        return ;
    }
    Dir dir_vec[16];
    int user_inode_Addr = -1;	//用户文件inode地址
    int passwd_inode_Addr = -1;	//用户密码文件inode地址
    int group_inode_Addr = -1;	//用户组文件inode地址
    iNode user_inode;		//用户文件的inode
    iNode passwd_inode;		//用户密码文件的inode
    iNode group_inode;		//用户组文件inode

    //原来的目录


    char helper_cur_user_name[100];
    char helper_cur_user_name_next[100];
    char helper_cur_user_dir_name[100];
    int  helper_cur_dir_addr;
    char helper_cur_dir_name[100];
    char helper_cur_group_name[100];

    iNode cur_dir_inode;	//当前目录的inode
    int i,j;

    strcpy(helper_cur_user_name, cur_user_name);
    strcpy(helper_cur_user_dir_name, cur_user_dir_name);
    helper_cur_dir_addr = cur_dir_addr;
    strcpy(helper_cur_dir_name, cur_dir_name);


    //返回根目录
    memset(cur_user_name, 0, sizeof(cur_user_name));
    memset(cur_user_dir_name, 0, sizeof(cur_user_dir_name));
    cur_dir_addr = ROOT_DIR_ADDR;
    strcpy(cur_dir_name, "/");



    //跳转用户文件夹
    FindDir(cur_dir_addr, "home");

    //保存现场
    strcpy( helper_cur_user_name_next , cur_user_name);
    strcpy( helper_cur_group_name, cur_grop_name);


    //更改
    strcpy(cur_user_name, username);
    strcpy(cur_grop_name, "user");


    if(!MakeDir(cur_dir_addr, username)){

        strcpy( cur_user_name, helper_cur_user_name);
        strcpy( cur_grop_name, helper_cur_group_name);
        strcpy(cur_user_dir_name, helper_cur_user_dir_name);
        cur_dir_addr = helper_cur_dir_addr;
        strcpy(cur_dir_name, helper_cur_dir_name);
        cout << "用户注册失败" << endl;
        return ;
    }

    Chmod(cur_dir_addr, username, 0600);
    //恢复现场
    strcpy( cur_user_name, helper_cur_user_name_next);
    strcpy( cur_grop_name, helper_cur_group_name);

    //回到根目录
    memset(cur_user_name, 0, sizeof(cur_user_name));
    memset(cur_user_dir_name, 0, sizeof(cur_user_dir_name));
    cur_dir_addr = ROOT_DIR_ADDR;
    strcpy(cur_dir_name, "/");

    //进入用户目录
    FindDir(cur_dir_addr, "etc");

    cout << "password: ";
    //用户密码
    char password[100] = {0};
    fflush(stdin);
    cin.getline(password, 100);



    fseek(fr, cur_dir_addr, SEEK_SET);
    fread(&cur_dir_inode, sizeof(iNode), 1, fr);


    for(int i = 0;i < 10; i++){
        if(cur_dir_inode.inode_dirblock[i] == -1){
            continue;
        }
        //在10个直接块中查询
        fseek(fr, cur_dir_inode.inode_dirblock[i], SEEK_SET);
        fread(&dir_vec, sizeof(dir_vec), 1, fr);

        //一个j里面存储16个文件夹
        for(int j = 0; j < 16; j++){
            if( strcmp(dir_vec[j].name,"user")==0 || strcmp(dir_vec[j].name, "passwd")==0 || strcmp(dir_vec[j].name, "group") == 0 ) {
                iNode helper;

                fseek(fr, dir_vec[j].inode_addr, SEEK_SET);
                fread(&helper, sizeof(iNode),1,fr);


                //文件名标识别符号判断
                if( ((helper.inode_mode >> 9) & 1 ) ==0 ){

                    if( strcmp(dir_vec[j].name, "user")==0 ){
                        user_inode_Addr = dir_vec[j].inode_addr;
                        user_inode = helper;
                    }
                    else if(strcmp(dir_vec[j].name, "passwd") ==0 ){
                        passwd_inode_Addr = dir_vec[j].inode_addr;
                        passwd_inode = helper;
                    }
                    else if(strcmp(dir_vec[j].name, "group")==0 ){
                        group_inode_Addr = dir_vec[j].inode_addr;
                        group_inode = helper;
                    }
                }
            }
        }
        if(user_inode_Addr != -1 && passwd_inode_Addr != -1)
            break;
    }

    char file_content[100000];
    char buffer[600];



    int pointer = 0;
    for(int i = 0; i < user_inode.inode_size; i++){
        if(i % superBlock->s_BLOCK_SIZE == 0){
            //换新的磁盘块
            fseek(fr, user_inode.inode_dirblock[i/superBlock->s_BLOCK_SIZE], SEEK_SET);
            fread(&buffer, superBlock->s_BLOCK_SIZE, 1, fr);
            pointer = 0;
        }
        file_content[i] = buffer[pointer++];
    }
    buffer[user_inode.inode_size] = '\0';



    if(strstr(buffer, username)!= nullptr){
        cout << "用户已经存在" << endl;
        strcpy( cur_user_name, helper_cur_user_name);
        strcpy(cur_user_dir_name, helper_cur_user_dir_name);
        cur_dir_addr = helper_cur_dir_addr;
        strcpy(cur_dir_name, helper_cur_dir_name);
        return ;
    }

    // 1 -> 普通用户组 buffer + strlen(buffer) -> 定位到末尾
    sprintf(buffer + strlen(buffer), "%s:x:%d:%d\n", username, nextUID++, 1);
    user_inode.inode_size = strlen(buffer);
    WriteFile(user_inode, user_inode_Addr, buffer);


    pointer = 0;
    for(int i = 0;i < passwd_inode.inode_size;i++){
        if(i% superBlock->s_BLOCK_SIZE==0){	//超出了这个磁盘块
            //换新的磁盘块
            fseek(fr, passwd_inode.inode_dirblock[i/superBlock->s_BLOCK_SIZE], SEEK_SET);
            fread(&buffer, superBlock->s_BLOCK_SIZE, 1, fr);
            pointer = 0;
        }
        file_content[i] = buffer[pointer++];
    }
    buffer[passwd_inode.inode_size] = '\0';

    sprintf(buffer + strlen(buffer),"%s:%s\n", username, password);
    passwd_inode.inode_size = strlen(buffer);
    WriteFile(passwd_inode, passwd_inode_Addr, buffer);


    //取出group文件内容
    pointer = 0;
    for(int i = 0; i < group_inode.inode_size; i++){
        if(i% superBlock->s_BLOCK_SIZE == 0){	//超出了这个磁盘块
            //换新的磁盘块
            fseek(fr, group_inode.inode_dirblock[i/superBlock->s_BLOCK_SIZE], SEEK_SET);
            fread(&buffer, superBlock->s_BLOCK_SIZE, 1, fr);
            pointer = 0;
        }
        file_content[i] = buffer[pointer++];
    }
    buffer[group_inode.inode_size] = '\0';


    if(buffer[strlen(buffer)-2]==':')  {                    //当前组内无用户
        sprintf(buffer+strlen(buffer)-1, "%s\n", username);
    }
    else {
        sprintf(buffer+strlen(buffer)-1, ",%s\n",username);
    }
    group_inode.inode_size = strlen(buffer);
    WriteFile(group_inode, group_inode_Addr, buffer);

    //恢复现场，回到原来的目录
    strcpy(cur_user_name, helper_cur_user_name);
    strcpy(cur_user_dir_name, helper_cur_user_dir_name);
    cur_dir_addr = helper_cur_dir_addr;
    strcpy(cur_dir_name, helper_cur_dir_name);
    cout << "用户注册成功" << endl;
    return ;
}

void FileSystem::DeleteUser(char *username) {
    if(strcmp(username, "root") == 0) {
        cout << "无法删除root 用户" << endl;
    }

    FILE* fr = image.get_file_read();
    FILE* fw = image.get_file_write();

    if(strcmp(cur_user_name, "root")!=0){
        cout << "非root用户, 不能添加新用户" << endl;
        return ;
    }


    Dir dir_vec[16];
    int user_inode_Addr = -1;	//用户文件inode地址
    int passwd_inode_Addr = -1;	//用户密码文件inode地址
    int group_inode_Addr = -1;	//用户组文件inode地址
    iNode user_inode;		//用户文件的inode
    iNode passwd_inode;		//用户密码文件的inode
    iNode group_inode;		//用户组文件inode

    //原来的目录


    char helper_cur_user_name[100];
    char helper_cur_user_dir_name[100];
    int  helper_cur_dir_addr;
    char helper_cur_dir_name[100];

    iNode cur_dir_inode;	//当前目录的inode
    int i,j;

    //保存现场
    strcpy(helper_cur_user_name, cur_user_name);
    strcpy(helper_cur_user_dir_name, cur_user_dir_name);
    helper_cur_dir_addr = cur_dir_addr;
    strcpy(helper_cur_dir_name, cur_dir_name);




    //返回根目录
    memset(cur_user_name, 0, sizeof(cur_user_name));
    memset(cur_user_dir_name, 0, sizeof(cur_user_dir_name));
    cur_dir_addr = ROOT_DIR_ADDR;
    strcpy(cur_dir_name, "/");

    //进入用户目录
    FindDir(cur_dir_addr, "etc");


    fseek(fr, cur_dir_addr, SEEK_SET);
    fread(&cur_dir_inode, sizeof(iNode), 1, fr);


    for(int i = 0;i < 10; i++){
        if(cur_dir_inode.inode_dirblock[i] == -1){
            continue;
        }
        //在10个直接块中查询
        fseek(fr, cur_dir_inode.inode_dirblock[i], SEEK_SET);
        fread(&dir_vec, sizeof(dir_vec), 1, fr);

        //一个j里面存储16个文件夹
        for(int j = 0; j < 16; j++){
            if( strcmp(dir_vec[j].name,"user")==0 || strcmp(dir_vec[j].name, "passwd")==0 || strcmp(dir_vec[j].name, "group") == 0 ) {
                iNode helper;

                fseek(fr, dir_vec[j].inode_addr, SEEK_SET);
                fread(&helper, sizeof(iNode),1,fr);


                //文件名标识别符号判断
                if( ((helper.inode_mode >> 9) & 1 ) ==0 ){

                    if( strcmp(dir_vec[j].name, "user")==0 ){
                        user_inode_Addr = dir_vec[j].inode_addr;
                        user_inode = helper;
                    }
                    else if(strcmp(dir_vec[j].name, "passwd") ==0 ){
                        passwd_inode_Addr = dir_vec[j].inode_addr;
                        passwd_inode = helper;
                    }
                    else if(strcmp(dir_vec[j].name, "group")==0 ){
                        group_inode_Addr = dir_vec[j].inode_addr;
                        group_inode = helper;
                    }
                }
            }
        }
        if(user_inode_Addr != -1 && passwd_inode_Addr != -1)
            break;
    }

    char file_content[100000];
    char buffer[600];



    int pointer = 0;
    for(int i = 0; i < user_inode.inode_size; i++){
        if(i % superBlock->s_BLOCK_SIZE == 0){
            //换新的磁盘块
            fseek(fr, user_inode.inode_dirblock[i/superBlock->s_BLOCK_SIZE], SEEK_SET);
            fread(&buffer, superBlock->s_BLOCK_SIZE, 1, fr);
            pointer = 0;
        }
        file_content[i] = buffer[pointer++];
    }
    buffer[user_inode.inode_size] = '\0';



    if(strstr(buffer, username) == nullptr){
        cout << "用户不存在" << endl;
        strcpy( cur_user_name, helper_cur_user_name);
        strcpy(cur_user_dir_name, helper_cur_user_dir_name);
        cur_dir_addr = helper_cur_dir_addr;
        strcpy(cur_dir_name, helper_cur_dir_name);
        return ;
    }

    DeleteUserContent(buffer, username);
    user_inode.inode_size = strlen(buffer);
    WriteFile(user_inode, passwd_inode_Addr, buffer);

    pointer = 0;
    for(int i = 0;i < passwd_inode.inode_size;i++){
        if(i% superBlock->s_BLOCK_SIZE==0){	//超出了这个磁盘块
            //换新的磁盘块
            fseek(fr, passwd_inode.inode_dirblock[i/superBlock->s_BLOCK_SIZE], SEEK_SET);
            fread(&buffer, superBlock->s_BLOCK_SIZE, 1, fr);
            pointer = 0;
        }
        file_content[i] = buffer[pointer++];
    }
    buffer[passwd_inode.inode_size] = '\0';
    DeleteUserContent(buffer, username);
    passwd_inode.inode_size = strlen(buffer);
    WriteFile(passwd_inode, passwd_inode_Addr, buffer);



    //取出group文件内容
    pointer = 0;
    for(int i = 0; i < group_inode.inode_size; i++){
        if(i% superBlock->s_BLOCK_SIZE==0){	//超出了这个磁盘块
            //换新的磁盘块
            fseek(fr, group_inode.inode_dirblock[i/superBlock->s_BLOCK_SIZE], SEEK_SET);
            fread(&buffer, superBlock->s_BLOCK_SIZE, 1, fr);
            pointer = 0;
        }
        file_content[i] = buffer[pointer++];
    }
    buffer[group_inode.inode_size] = '\0';
    DeleteUserContent(buffer, username);
    group_inode.inode_size = strlen(buffer);
    WriteFile(group_inode, group_inode_Addr, buffer);




    char c, User_Dir_to_be_deleted[110] = { 0 }, cmp_Cur_Dir_Name[311];
    strcpy(User_Dir_to_be_deleted, "/home/");
    strcat(User_Dir_to_be_deleted, username);
    strcat(User_Dir_to_be_deleted, "/");

    strcpy(cmp_Cur_Dir_Name, helper_cur_dir_name);
    strcat(cmp_Cur_Dir_Name, "/");

    for (i = 0, j = 0; j < 3; i++) {
        c = cmp_Cur_Dir_Name[i];
        if (c != User_Dir_to_be_deleted[i])
            break;
        else if (c == '/')
            j++;
    }

    if (j != 3) {
        //恢复现场，回到原来的目录
        strcpy(cur_user_name,  "root");
        strcpy(cur_user_dir_name, "root");
        cur_dir_addr = ROOT_DIR_ADDR;
        strcpy(cur_dir_name, "/");
        FindDir(cur_dir_addr, "home");
        DeleteDir(cur_dir_addr, username);

        strcpy(cur_user_name, helper_cur_user_name);
        strcpy(cur_user_dir_name, helper_cur_user_dir_name);
        cur_dir_addr = helper_cur_dir_addr;
        strcpy(cur_dir_name, helper_cur_dir_name);
    }
    else {
        // 在root用户进入a用户目录，删掉a用户，则回到根目录
        strcpy(cur_user_name,  "root");
        strcpy(cur_user_dir_name, "root");
        cur_dir_addr = ROOT_DIR_ADDR;	//当前用户目录地址设为根目录地址
        strcpy(cur_dir_name, "/");		//当前目录设为"/"

        // 删除username目录
        FindDir(cur_dir_addr, "home");
        DeleteDir(cur_dir_addr, username);
        // 回到/home/root
        FindDir(cur_dir_addr, "..");
    }
    cout << "用户删除成功" << endl;

    return ;
}

void FileSystem::WriteFile(iNode inode, int inode_addr, char *buffer) {
    int len = strlen(buffer);
    for(int k = 0; k < len; k += superBlock->s_BLOCK_SIZE){	//最多10次，10个磁盘快，即最多5K
        int cur_block_addr;
        if(inode.inode_dirblock[k/superBlock->s_BLOCK_SIZE] == -1){
            cur_block_addr = BlockAlloc();
            if(cur_block_addr == -1){
                cout << "分配block出现错误" << endl;
                return ;
            }
            inode.inode_dirblock[k/superBlock->s_BLOCK_SIZE] = cur_block_addr;
        }
        else{
            //直接覆盖
            cur_block_addr = inode.inode_dirblock[k / superBlock->s_BLOCK_SIZE];
        }

        fseek(image.get_file_write(), cur_block_addr, SEEK_SET);
        fwrite(buffer+k,superBlock->s_BLOCK_SIZE,1, image.get_file_write());
        fflush(image.get_file_write());
    }
    //更新该文件大小
    inode.inode_size = len;
    fseek(image.get_file_write(), inode_addr, SEEK_SET);
    fwrite(&inode, sizeof(iNode), 1, image.get_file_write());
    fflush(image.get_file_write());
}

void FileSystem::DeleteUserContent(char *buffer, char *username) {
    char* pointer = strstr(buffer, username);
    *pointer = '\0';
    while((*pointer) != '\n') {
        pointer++;
    }

    //pointer + 1 -> 下一个user的开始位置;
    pointer++;
    strcat(buffer, pointer);
}

void FileSystem::Cat(int inode_addr, char name[])
{
    FILE* fr = image.get_file_read();
    //清空缓冲区
    char buf[FILE_BUFFER] = { 0 };

    //查找有无同名文件
    Dir dir_vec[16] = { 0 };

    //从这个地址取出inode
    iNode cur = { 0 }, file_inode = { 0 };
    fseek(fr, inode_addr, SEEK_SET);
    fread(&cur, sizeof(iNode), 1, fr);

    //判断文件模式。6为owner，3为group，0为other
    int filemode;
    if (strcmp(cur_user_name, cur.user_name) == 0)
        filemode = 6;
    else if (strcmp(cur_grop_name, cur.user_group) == 0)
        filemode = 3;
    else
        filemode = 0;

    size_t i = 0, j;
    int dir_id;
    bool isExist = false;	//文件是否已存在
    bool label = false; // 是否停止
    while (i < 160) {
        //160个目录项之内，可以直接在直接块里找
        dir_id = i / 16;	//在第几个直接块里
        if (cur.inode_dirblock[dir_id] == -1) {
            i += 16;
            continue;
        }
        fseek(fr, cur.inode_dirblock[dir_id], SEEK_SET);
        fread(dir_vec, sizeof(dir_vec), 1, fr);
        fflush(fr);

        //输出该磁盘块中的所有目录项
        for (j = 0; j < 16; j++) {
            if (strcmp(dir_vec[j].name, name) == 0) {
                //重名，取出inode，判断是否是文件
                fseek(fr, dir_vec[j].inode_addr, SEEK_SET);
                fread(&file_inode, sizeof(iNode), 1, fr);
                if (((file_inode.inode_mode >> 9) & 1) == 0) {	//是文件且重名，打开这个文件，并编辑
                    isExist = true;
                    label = true;
                }
            }
            if(label) break;
            i++;
        }
        if(label) break;
    }
    int cnt = 0;
    if (isExist) {	//文件已存在，输出文件内容

        //权限判断。判断文件是否可读
        if (((file_inode.inode_mode >> filemode >> 2) & 1) == 0) {
            //不可读
            cout << "Permission denied." << endl;
            return;
        }

        //将文件内容读取出来，显示在，窗口上
        i = 0;
        int sum_len = file_inode.inode_size;	//文件长度
        int len = 0;	//取出来的长度
        for (i = 0; i<10; i++) {
            char fileContent[1000] = { 0 };
            if (file_inode.inode_dirblock[i] == -1) {
                continue;
            }
            //依次取出磁盘块的内容
            fseek(fr, file_inode.inode_dirblock[i], SEEK_SET);
            fread(fileContent, superBlock->s_BLOCK_SIZE, 1, fr);
            fflush(fr);
            //输出字符串
            int cur_len = 0;
            while (cur_len < superBlock->s_BLOCK_SIZE) {
                if (len >= sum_len)	//全部输出完毕
                    break;
                printf("%c", fileContent[cur_len]);	//输出到屏幕
                buf[cnt++] = fileContent[cur_len];	//输出到buf
                cur_len++;
                len++;
            }
            if (len >= sum_len)
                break;
        }
    }
    else {
        printf("cat %s : No such file\n", name);
    }
}

void FileSystem::Help() {
    cout << "Command format : 'command [Necessary parameter] ([Unnecessary parameter])'" << endl;
    cout << "useradd [username] : Add user" << endl;
    cout << "userdel [username] : Delete users" << endl;
    cout << "exit : Exit the current user" << endl;
    cout << "cat [filename] : Output file content" << endl;
    cout << "rm [filename] : Remove file" << endl;
    cout << "mkdir [directoryName] : Create subdirectory" << endl;
    cout << "rmdir [directoryName] : Delete subdirectory" << endl;
    cout << "cd [directoryName] : Change current directory" << endl;
    cout << "ls : List the file directory" << endl;
    cout << "pwd : List the current directory name" << endl;
    cout << "chmod [filename] [permissions] : Change the file permissions" << endl;
    cout << "clear : Clear the terminal" << endl;
    cout << "exit : Exit the System" << endl;
    cout << "touch [filename] : Create a new empty file" << endl;
    cout << "nano [filename] : edit a file" << endl;
    cout << "format : format the system" << endl;
}

void FileSystem::editor(int cur_dir_addr, char file_name[], char buf[])
{
    memset(buf, 0, sizeof(buf));


    auto fr = image.get_file_read();

    if (strlen(file_name) >= MAX_NAME_SIZE) {
        cout << "The filename is too long." << endl;
        return;
    }

    //清空缓冲区
    memset(buf, 0, sizeof(buf));
    buf[0] = '\0';
    int maxlen = 0;	//到达过的最大长度

    //查找有无同名文件，有的话进入编辑模式，没有进入创建文件模式
    Dir dir_vector[16] = { 0 };	//临时目录清单

    //从这个地址取出inode
    iNode cur = { 0 }, fileInode = { 0 };
    fseek(fr, cur_dir_addr, SEEK_SET);
    fread(&cur, sizeof(iNode), 1, fr);

    //判断文件模式。6为owner，3为group，0为other
    int filemode;
    if (strcmp(cur_user_name, cur.user_name) == 0 || strcmp(cur_user_name, "root") == 0)
        filemode = 6;
    else if (strcmp(cur_grop_name, cur.user_group) == 0)
        filemode = 3;
    else
        filemode = 0;

    int i = 0, j;
    int dno;
    int fileInodeAddr = -1;	//文件的inode地址
    bool isExist = false;	//文件是否已存在
    while (i<160)
    {
        //160个目录项之内，可以直接在直接块里找
        dno = i / 16;    //在第几个直接块里

        if (cur.inode_dirblock[dno] == -1)
        {
            i += 16;
            continue;
        }
        fseek(fr, cur.inode_dirblock[dno], SEEK_SET);
        fread(dir_vector, sizeof(dir_vector), 1, fr);
        fflush(fr);

        //输出该磁盘块中的所有目录项
        for (j = 0; j < 16; j++)
        {
            if (strcmp(dir_vector[j].name, file_name) == 0)
            {
                //重名，取出inode，判断是否是文件
                fseek(fr, dir_vector[j].inode_addr, SEEK_SET);
                fread(&fileInode, sizeof(iNode), 1, fr);
                if (((fileInode.inode_mode >> 9) & 1) == 0)
                {    //是文件且重名，打开这个文件，并编辑
                    fileInodeAddr = dir_vector[j].inode_addr;
                    isExist = true;
                    break;
                }
            }
            i++;
        }
        if(isExist)
            break;
    }

    if (isExist) {

        if (((fileInode.inode_mode >> filemode >> 2) & 1) == 0) {
            //不可读
            cout << "Permission denied." << endl;
            return;
        }

        //将文件内容读取出来，显示在，窗口上
        int cnt = 0;
        int sumlen = fileInode.inode_size;	//文件长度
        int getlen = 0;	//取出来的长度
        for (int i = 0; i< 10; i++) {
            char fileContent[1000] = { 0 };
            if (fileInode.inode_dirblock[i] == -1) {
                continue;
            }
            //依次取出磁盘块的内容
            fseek(fr, fileInode.inode_dirblock[i], SEEK_SET);
            fread(fileContent, superBlock->s_BLOCK_SIZE, 1, fr);	//读取出一个磁盘块大小的内容
            fflush(fr);
            //输出字符串
            int curlen = 0;	//当前指针
            while (curlen < superBlock->s_BLOCK_SIZE) {
                if (getlen >= sumlen)	//全部输出完毕
                    break;
                buf[cnt++] = fileContent[curlen];	//输出到buf
                curlen++;
                getlen++;
            }
            buf[cnt++] = 0;
            if (getlen >= sumlen)
                break;
        }

        maxlen = sumlen;
    }

    fstream  bfile;
    bfile.open("bfile.txt", ios::out);


    Page page(buf);
    int curx, cury;
    int c;
    int beg = 0;
    int end = LINES - 2;
    bfile << end;
    bfile.close();
    int y_offset = 0;
    int tab_offset = 0;

    page.debug();

    initscr();
    clear();
    noecho();
    keypad(stdscr, true);
    page.print_page(beg, end);
    getyx(stdscr, cury, curx);

    while(true) {
        int oldy, oldx; getyx(stdscr, oldy, oldx);

        attron(A_REVERSE);
        move(LINES - 1, 0);
        clrtoeol();
        printw("x: %d y: %d o: %d  F4:Exit F5:Save&Exit", curx, cury, y_offset);;
        attroff(A_REVERSE);
        move(oldy, oldx);

        beg = 0 + y_offset;
        end = LINES - 2 + y_offset;

        c = getch();
        switch (c) {
            case KEY_F(4):
                clear();
                echo();
                keypad(stdscr, false);
                endwin();
                return;
            case KEY_F(5):
                page.save_to_buf(buf);
                clear();
                echo();
                keypad(stdscr, false);
                endwin();
                if(isExist) {
                    if (((fileInode.inode_mode >> filemode >> 1) & 1) == 1) {	//可写
                        WriteFile(fileInode, fileInodeAddr, buf);
                    }
                    else {	//不可写
                        cout << "Permission denied." << endl;
                    }
                } else {
                    if (((cur.inode_mode >> filemode >> 1) & 1) == 1) {
                        //可写。可以创建文件
                        Create(cur_dir_addr, file_name, buf);	//创建文件
                    }
                    else {
                        cout << "Permission denied." << endl;
                        return;
                    }
                }
                return;
            case KEY_UP:
                move_up(page, curx, cury, y_offset);
                page.print_page(beg, end);
                move(cury, curx);
                break;
            case KEY_DOWN:
                move_down(page, curx, cury, y_offset);
                page.print_page(beg, end);
                move(cury, curx);
                break;
            case KEY_LEFT:
                move_left(curx, cury);
                page.print_page(beg, end);
                move(cury, curx);
                break;
            case KEY_RIGHT:
                move_right(page, curx, cury, y_offset, tab_offset);
                page.print_page(beg, end);
                move(cury, curx);
                break;
            case KEY_DC:
            case 127:
            case KEY_BACKSPACE:
                if(strlen(page.text[cury + y_offset].line) == 0) {
                    page.remove_line(cury + y_offset);
                    move_up(page, curx, cury, y_offset);
                } else if( curx > 1 ) {
                    page.text[cury + y_offset].remove_char(curx - 2);
                    move_left(curx, cury);
                }
                page.print_page(beg, end);
                move(cury, curx);
                page.debug();
                break;
            case '\t':
                for(int i = 0; i < TAB_WIDTH; i++)
                {
                    page.text[cury + y_offset].insert_char(' ', curx - 1);
                    page.print_page(beg, end);
                    move_right(page, curx, cury, y_offset, tab_offset);
                }
                break;
            case '\n':
                page.insert_line(cury + y_offset + 1);
                page.print_page(beg, end);
                move_down(page, curx, cury, y_offset);
                break;
            default: // all other chars
                if(isprint(c)) {
                    page.text[cury + y_offset].insert_char(c, curx - 1);
                    page.print_page(beg, end);
                    move_right(page, curx, cury, y_offset, tab_offset);
                }
        }
    }

}

