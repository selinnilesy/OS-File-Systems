//
// Created by Selin Yıldırım on 22.06.2021.
//
#include "ext2fs.cpp"
uint32_t inodemap_setter(){
    uint32_t j=0;
    for(int i=0; i<group_count; i++){ // group count
        if(block_size==1024) {
            bitset<1024*8>* inodemap = (bitset<1024*8>*) (((char *) map) + ((bgds[i]->inode_bitmap) * block_size));
            for(j=0; j<1024*8; j++){ if( (*inodemap)[j] == 0) { inodemap->set(j); return j+1; }}
        }
        else if(block_size==2048){
            bitset<2048*8>* inodemap = (bitset<2048*8>*) (((char *) map) + ((bgds[i]->inode_bitmap) * block_size));
            for(j=0; j<2048*8; j++){ if( (*inodemap)[j] == 0) { inodemap->set(j); return j+1; }}
        }
        else if(block_size==4096) {
            bitset<4096*8>* inodemap = (bitset<4096*8>*) (((char *) map) + ((bgds[i]->inode_bitmap) * block_size));
            for(j=0; j<4096*8; j++){ if( (*inodemap)[j] == 0) { inodemap->set(j); return j+1; }}
        }
    }
    return j;
}
uint32_t bitmap_setter(){
    uint32_t j=0;
    for(int i=0; i<group_count; i++){ // group count
        if(block_size==1024) {
            bitset<1024*8>* bitmap = (bitset<1024*8>*) (((char *) map) + ((bgds[i]->block_bitmap) * block_size));
            for(j=0; j<1024*8; j++){ if( (*bitmap)[j] == 0) { bitmap->set(j); return j; }}
        }
        else if(block_size==2048){
            bitset<2048*8>* bitmap = (bitset<2048*8>*) (((char *) map) + ((bgds[i]->block_bitmap) * block_size));
            for(j=0; j<2048*8; j++){ if( (*bitmap)[j] == 0) { bitmap->set(j); return j; }}
        }
        else if(block_size==4096) {
            bitset<4096*8>* bitmap = (bitset<4096*8>*) (((char *) map) + ((bgds[i]->block_bitmap) * block_size));
            for(j=0; j<4096*8; j++){ if( (*bitmap)[j] == 0) { bitmap->set(j); return j; }}
        }
    }
    return j;
}
/*bool bitmap_checker(uint32_t index){ // returns false if the given block is not allocated and ALLOCATES it.
    uint32_t blck_group = index / ((uint32_t) supers[0]->blocks_per_group);
    if(block_size==1024) {
        bitset<1024*8>* bitmap = (bitset<1024*8>*) (((char *) map) + ((bgds[blck_group]->block_bitmap) * block_size));
        if( (*bitmap)[index] == 0) { bitmap->set(index); return false; }
    }
    else if(block_size==2048){
        bitset<2048*8>* bitmap = (bitset<2048*8>*) (((char *) map) + ((bgds[blck_group]->block_bitmap) * block_size));
        if( (*bitmap)[index] == 0) { bitmap->set(index); return false; }
        cout << "block returned to be allocated. " <<  (bool) ((*bitmap)[index]) << endl;
    }
    else if(block_size==4096) {
        bitset<4096*8>* bitmap = (bitset<4096*8>*) (((char *) map) + ((bgds[blck_group]->block_bitmap) * block_size));
       if( (*bitmap)[index] == 0) { bitmap->set(index); return false; }
    }
    return true;
}*/
ext2_inode* inode_photocopy_machine(ext2_inode* source){
    ext2_inode* copy = new ext2_inode;
    memcpy(copy, source, sizeof(ext2_inode));
    cout << "newly created inode has mode: " << copy->mode << endl;
    copy->link_count = 1;
    copy->gid = getgid();
    copy->uid = getuid();
    copy->creation_time=copy->access_time=copy->modification_time=time(nullptr);
    return copy;
}
int dup(int src, int dest){
    ext2_inode* sptr= nullptr;  // directly fetches from memory.
    ext2_inode* dptr= nullptr;  // directly fetches from memory.
    if(src) sptr = inodeFetcher_byIndex((unsigned int) src, 1);
    else{
        sptr = inodeFetcher_recursive(root, 0 , 1);
    }
    if(dest) dptr = inodeFetcher_byIndex((unsigned int) dest, 0);
    else{
        if(destinationPath.size()==0) dptr=root;
        else dptr = inodeFetcher_recursive(root, 0 , 0);
    }

    ext2_inode* copyNode = inode_photocopy_machine(sptr);
    char *padding = new char[(supers[0]->inode_size)-sizeof(ext2_inode)];
    for(int i=0;i<(supers[0]->inode_size)-sizeof(ext2_inode);i++) padding[i]=0;
    // increment refs of data blocks
    for(int i=0; i<12; i++){
        uint32_t blockNo = sptr->direct_blocks[i];  // if that block of source inode is full with information
        if(blockNo){
            uint32_t block_group =blockNo / supers[0]->blocks_per_group;
            uint32_t rel_block_index =blockNo - (supers[0]->blocks_per_group)*block_group;
            uint32_t*  refmap  = (uint32_t*)  (((char *) map) + ((bgds[block_group]->block_refmap) * block_size));
            if(block_size==1024) refmap[rel_block_index-1]++;   // since first byte will index for Block1.
            else refmap[rel_block_index]++;
        }
    }
    // create a new dir entry and fill it
    uint16_t length = ceil(((float)(newFileName.length() + 8 ))/4)*4;   // TO DO !!! PAD TO 4TH MULTIPLICANDS
    cout << "new dir_entry length will be  " << length << endl;
    char*  newEntryCharPtr = new char[length];
    ext2_dir_entry* newEntry =  (ext2_dir_entry*) newEntryCharPtr;
    newEntry->length = length;
    // which inode to allocate ?
    newEntry->inode = inodemap_setter();
    int inode_group = newEntry->inode / supers[0]->inodes_per_group; // inode_group =1 means group index 1, i.e: 15/10 =1
    int rel_inode_index = newEntry->inode - inode_group*(supers[0]->inodes_per_group);
    cout << "Allocated new inode for " << newFileName << " is: " << newEntry->inode <<  " group and rel index: " << inode_group << " " << rel_inode_index<< endl;
    // allocate inode at the right position
    memcpy(((char *) map) + ((bgds[inode_group]->inode_table) * block_size)+(rel_inode_index-1)*(supers[0]->inode_size), copyNode, sizeof(ext2_inode));
    memcpy(((char *) map) + ((bgds[inode_group]->inode_table) * block_size)+(rel_inode_index-1)*(supers[0]->inode_size)+sizeof(ext2_inode), padding, (supers[0]->inode_size)-sizeof(ext2_inode));
    cout << "inode TEST: " << endl;
    cout << copyNode->size << " " << copyNode->mode << endl;
    // decrement new inode's group descriptor's free inode count
    ext2_block_group_descriptor *inode_bgd = (ext2_block_group_descriptor*) (((char *) map) + ((group_initial_blocks[inode_group]+1) * block_size));
    inode_bgd->free_inode_count--;
    bgds[inode_group]->free_inode_count--; // my local one
    cout << "free_inode_count in the group became : " << inode_bgd->free_inode_count << endl;

    //update super blocks' free inode (total) count
    for(int i=0; i<supers.size(); i++) {
        supers[i]->free_inode_count--;
        cout << "super " << i << "s free_inode_count became: " << supers[i]->free_inode_count << endl;
        if(group_initial_blocks[i]==0) memcpy(((char *) map) + group_initial_blocks[i]*block_size+EXT2_SUPER_BLOCK_POSITION, supers[i],  sizeof(ext2_super_block));// set inode map, bitmap for the newly allocated inode.
        else memcpy(((char *) map) + group_initial_blocks[i]*block_size, supers[i],  sizeof(ext2_super_block));
    }
    // complete filling dir. entry
    newEntry->name_length = (uint8_t) (newFileName.length());
    cout << "new dir entry's size is set to be: " << newEntry->length << " and name length: " << (int) newEntry->name_length << endl;
    newEntry->file_type = (uint8_t) 1; // EXT2_FT_REG_FILE
    //strcpy(newEntry->name, name);
    memcpy(newEntry->name, newFileName.c_str(), newFileName.length());
    cout << "new dir entry's name became: " << newEntry->name << endl;
    vector<uint32_t> res = readDirEntries(dptr, "", 1, newEntry->length); // just searches for an empty slots ans returns its block offset IN TERMS OF BYTES from mmap.
    uint32_t  new_block_no =  dptr->direct_blocks[res[0]] ; // 0 if not allocated .
    cout << "block::: " << (int) new_block_no << endl;
    if(  new_block_no==0 ){ // if the data block not allocated, sets its bitmap
        new_block_no = bitmap_setter();
        if(block_size==1024 ) new_block_no++; // since indexed starting from 1.
        uint32_t block_group = new_block_no / supers[0]->blocks_per_group; // inode_group =1 means group index 1, i.e: 15/10 =1
        uint32_t rel_block_index = new_block_no - supers[0]->blocks_per_group*block_group; // inode_group =1 means group index 1, i.e: 15/10 =1

        dptr->direct_blocks[res[0]] = new_block_no;
        dptr->size+=block_size;
        dptr->block_count_512+=block_size/512;
        //memcpy(((char *) map) + (bgds[0]->inode_table)*block_size+(2-1)*128, dptr,  sizeof(ext2_inode));

        uint32_t*  refmap  = (uint32_t*)  (((char *) map) + ((bgds[block_group]->block_refmap) * block_size));
        refmap[rel_block_index]++;

        cout << "wrote here: " << dptr->direct_blocks[res[0]] << endl;
        cout << "Found datablock is not allocated, so  allocated " <<new_block_no <<  endl;
        //update super blocks' free block (total) count
        for(int i=0; i<supers.size(); i++) {
            supers[i]->free_block_count--;
            cout << "super block" << i << "'s free_block_count became: " << supers[i]->free_block_count << endl;
            if(group_initial_blocks[i]==0) memcpy(((char *) map) + group_initial_blocks[i]*block_size+EXT2_SUPER_BLOCK_POSITION, supers[i],  sizeof(ext2_super_block));// set inode map, bitmap for the newly allocated inode.
            else memcpy(((char *) map) + group_initial_blocks[i]*block_size, supers[i],  sizeof(ext2_super_block));
        }
        cout << "Allocated new block for dir entry, block: " << new_block_no << endl;

        // decrement new block's group descriptor's free block count
        ext2_block_group_descriptor *block_bgd = (ext2_block_group_descriptor*) (((char *) map) + ((group_initial_blocks[block_group]+1) * block_size));
        block_bgd->free_block_count--;
        bgds[block_group]->free_block_count--; // my local one
        cout << "free_block_count in that  group(" << block_group << ") became : " << block_bgd->free_block_count << endl;
    }

    cout << "Resulting i: "<< res[0] << " and j: " << res[1] << endl;
    dptr->modification_time=time(nullptr);
    uint16_t  size_to_be_written =  newEntry->length;
    newEntry->length+=block_size-(res[1]+(newEntry->length)); // also cover until the end of the block size
    cout << "the new dir entry became, with padding : " << newEntry->length << endl;
    cout << "writing onto block no: " << new_block_no << " and offset: " << res[1] << endl;
    memcpy(((char *) map) + (new_block_no * block_size) + res[1], newEntryCharPtr,size_to_be_written);
    //copy(newEntryCharPtr, newEntryCharPtr + (newEntry->length), ((char *) map) + (dptr->direct_blocks[res[0]] * block_size) + res[1]);
    cout << "Control after write begins: " << endl;
    cout << ((ext2_dir_entry*)(((char *) map) + (new_block_no * block_size) + res[1]))->length << (int) (((ext2_dir_entry*)(((char *) map) + (new_block_no * block_size) + res[1]))->name_length) << *(((ext2_dir_entry*)(((char *) map) + (new_block_no* block_size) + res[1]))->name) << endl;
    //cout << "new dir physical byte size: " << sizeof(*newEntryCharPtr) << " and written this many: " << newEntry->length << endl;

    cout << "new dir entry is written into mem. " << endl;
    delete [] newEntry;
    delete [] padding;
    return 1;
}


int main(int argc, char **argv){ // driver code reads FS structures upon a call to init
    int fd , sourceNode=0, destinationNode=0;

    if((fd = open(argv[2], O_RDWR)) < 0){       // open image
        handle_error("Cannot open provided image");
    }
    if (fstat(fd, &img_stat) == -1){   // Obtain file size
        handle_error("File Size Error");
    }
    cout << "Img size: " << img_stat.st_size << endl;
    map = mmap(0, img_stat.st_size, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        close(fd);
        handle_error("Error mmapping the file");
    }

    init();     // fetch structs

    if(argc==4){
        cout << "rm function" << endl;
        return 0;
    }
    else if(argc==5){
        cout << "dup function" << endl;
        if (*argv[3] == '/') { // SOURCE = ABS PATH
            string_split(argv[3] , 1,  0);
        }
        else{                           // SOURCE = INODE
            sourceNode = atoi(argv[3]);
        }
        if (*argv[4] == '/') { // DEST = ABS PATH
            string_split(argv[4] , 0, 0);
        }
        else{                               // DEST = INODE/name
            destinationNode = string_split(argv[4] , 0, 1);
        }
        dup(sourceNode, destinationNode);
    }
    //cout << "args: " << sourcePath[0]  << sourcePath.back() << destinationNode << destinationPath[0] << destinationPath.back()   <<  endl;


    // debug printers
    //readInodes();
    //int returnVal = readDirEntries(inodes[0], );
    //cout << "Return val: " <<  returnVal << endl;

    int err = munmap(map, img_stat.st_size);
    if(err != 0){
        printf("UnMapping Failed\n");
        return 1;
    }
    close(fd);
    int temp = deallocate(); // vectors, mmap.
    if(temp) cout << "Problem with deallocation ! Don't upset the boss. " << endl;

    return 0;
}