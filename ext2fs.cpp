#include "ext2fs.h"
#include "helper.cpp" // custom deallocators

#define handle_error(msg) {perror(msg); exit(1);}

vector<uint32_t> readDirEntries(ext2_inode* inode, string keyword, bool emptySlot, uint16_t storage){ // direct blocks are searched for a specific filename or an empty slot.
    vector<uint32_t> dirVec;
    uint32_t total_length=0;
    ext2_dir_entry* dir_entry;
    for(int i=0; i<12; i++) {
        for(uint32_t j=0; j<block_size; j+=total_length){ // read full struct with name each time
            dir_entry = new ext2_dir_entry;
            memcpy(dir_entry, ((char *) map) + ((inode->direct_blocks[i]) * block_size) + j, sizeof(ext2_dir_entry)); // read to see total length.
            total_length = sizeof(ext2_dir_entry); // in case it is an empty dir. entry
            if(emptySlot && dir_entry->inode==0){ // if empty read
                delete dir_entry; // skip only
                cout << "found empty dir entry on block no " << i << " and j " << j << endl;
                dirVec.push_back(i); dirVec.push_back(j);
                return dirVec;
            }
            else if(!emptySlot && dir_entry->inode==0){
                delete dir_entry; // skip only
                break; // are directory entries holey : TO DO ???
            }
            total_length= dir_entry->length; // else fetch the size
            delete dir_entry; // deallocate the struct totally to block flexible array members collapse
            dir_entry = (ext2_dir_entry*) new char[total_length];
            memcpy(dir_entry, ((char *) map) + ((inode->direct_blocks[i]) * block_size) + j, total_length);
            if(emptySlot){
                cout << "full dir_entry is: " << dir_entry->name <<  " and pointed inode is: " << dir_entry->inode << " and its size is: " << dir_entry->length << " and name size: " << (int) dir_entry->name_length << " and file type: " << (int) dir_entry->file_type << endl;
                uint16_t estimated_size = ceil(((float)(dir_entry->name_length + 8 ))/4)*4;
                if((dir_entry->length > estimated_size) && ((block_size - (j+estimated_size)) >= storage)){
                    ext2_dir_entry *origin = (ext2_dir_entry *) (((char *) map) + ((inode->direct_blocks[i]) * block_size) + j);
                    origin->length=estimated_size; // trim the length
                    cout << "new length after trimmed: " << origin->length << endl;
                    delete [] dir_entry;
                    dirVec.push_back(i);
                    dirVec.push_back(j+(origin->length));
                    return dirVec;
                }
                delete [] dir_entry;
                continue;
            }  // total length updated here, go search other dir. entries for an empty one.
            //cout << "file type of dir entry " << (int) dir_entry->file_type << endl;
            //cout << "length of dir entry " <<  dir_entry->length << endl;
            //cout << "name length of dir entry " << (int) dir_entry->name_length << endl;
            cout << "Length name: " << (int) dir_entry->name_length << " and name: " << dir_entry->name <<  " and pointed inode is: " << dir_entry->inode << " and its size is: " << dir_entry->length << endl;
            cout << "Searched keyword is: " << keyword << endl;
            bool flag=true;
            if(dir_entry->name_length==keyword.length()) {
                for (int i = 0; i < dir_entry->name_length; i++) {
                    if ((dir_entry->name)[i] != keyword[i]) {
                        flag = false;
                        break;
                    }
                }
            }
            if(dir_entry->name_length==keyword.length() && flag) { uint32_t res = dir_entry->inode; delete [] dir_entry; dirVec.push_back(res); return dirVec;}
            delete [] dir_entry;
        }
    }
    return dirVec;
}
ext2_inode* inodeFetcher_byIndex(unsigned int index, bool source){
    if(supers[0]->inode_count < index) {cout << "Erroneous inode index." << endl; return nullptr;}
    unsigned int group_index  = index / supers[0]->inodes_per_group; // 15 / 10 = 1. grup !!! index 1
    if(source) source_node_group = group_index;
    else dest_node_group =  group_index;
    ext2_inode *res = (ext2_inode*) (((char *) map)+((bgds[group_index]->inode_table)*block_size)+(index-1)*(supers[0]->inode_size));
    return res;
}
ext2_inode* inodeFetcher_recursive(ext2_inode* currNode, int pathIndex, bool source) { // reads all the root inodes, inode_number=2 !!!
    uint32_t pointedInode=0;
    if(source) pointedInode = (readDirEntries(currNode, sourcePath[pathIndex], 0, 0))[0];  // only token search purpose
    else pointedInode = (readDirEntries(currNode, destinationPath[pathIndex], 0, 0))[0];    // only token search purpose
    if(pointedInode == 0 ){ handle_error("searched inode is not found under root.");}
    else{
        int group_index = pointedInode / supers[0]->inodes_per_group;; // i.e : 3. gruptaki 88. inode , 3. gruptaki inode indexi = 88 - (2)*group_per_inode, 2 = group_index
        if(source) source_node_group = group_index; // overwrite group number of the node that may be returned as final now
        else dest_node_group =  group_index;
        int relative_inode_index = pointedInode - (group_index) * (supers[0]->inodes_per_group);
        ext2_inode *newInode = (ext2_inode*) (((char *) map) + ((bgds[group_index]->inode_table) * block_size) + (relative_inode_index - 1) * (supers[0]->inode_size));

        if (source && pathIndex+1 == sourcePath.size()) { return newInode;} // final file inode
        else if(!source && pathIndex+1 == destinationPath.size()) {return newInode;} // final dir inode
        else {      // recursive part
            cout << "recursing. " << endl;
            return inodeFetcher_recursive(newInode, ++pathIndex, source);
        }
    }
}
/*void readInodes() { // reads all the root inodes, inode_number=2 !!!
    // read inode table

    // reads only file inode's data blocks
    for(int i=0; i<supers[0]->inode_count; i++){
        memcpy(inodes[0], ((char *) map)+((bgds[0]->inode_table)*block_size)+i*128, sizeof(ext2_inode));
        if(initInode->mode & 0x8000) {
            cout << "This is a file inode: " << i+1 << endl;
            if (initInode->direct_blocks[0]) {
                char *dataPtr =  new char[2048];
                memcpy(dataPtr, ((char *) map)+((initInode->direct_blocks[0])*block_size), block_size);
                cout << *dataPtr << endl;
            }
        }
    }
}*/

void init(){
    int i;
    //  sneak-peak to 0th group's super block to fetch overall group count
    supers.push_back(new ext2_super_block);
    memcpy(supers[0], ((char *) map)+EXT2_SUPER_BLOCK_POSITION, sizeof(ext2_super_block)); // map is the source
    if(supers[0]->magic != EXT2_SUPER_MAGIC){
        handle_error("Super 0 not an ext2 filesystem");
    }
    // block size and group count calculations
    block_size = pow(2, 10 + (supers[0]->log_block_size)); // in bytes
    if(block_size!=1024) group_count =  ceil((float) supers[0]->block_count/supers[0]->blocks_per_group);
    else group_count =  ceil((float) (supers[0]->block_count-1)/supers[0]->blocks_per_group); //ignore boot block in group calculation
    cout << "group count: " << group_count << endl ;
    cout << "block_size: " << block_size << endl ;

    // groups' inital block numbers, (super block numbers of each group)
    if(block_size!=1024) group_initial_blocks.push_back((uint32_t) 0);
    else group_initial_blocks.push_back((uint32_t) 1); // since boot data covers 1 block
    for( i=0; i<group_count-1; i++){ // end of the data blocks are set to initial offset for the next group block.
        cout <<  "The firdt data block for this group is: " << supers[i]->first_data_block << endl;
        cout << "Super group block for the next group is: " << supers[i]->first_data_block + supers[i]->blocks_per_group << endl;
        group_initial_blocks.push_back((uint32_t) (supers[i]->first_data_block + supers[i]->blocks_per_group)); // calculate for next group
    }

    // now we can use group count and pointers. rest of the super blocks, for each group, found here.
    for( i=1; i<group_count; i++){
        supers.push_back(new ext2_super_block);
        memcpy(supers[i], ((char *) map)+group_initial_blocks[i]*block_size, sizeof(ext2_super_block)); // map is the source
        if(supers[i]->magic != EXT2_SUPER_MAGIC){
            cout << "Super " << i << " not an ext2 filesystem";
            exit(1);
        }
    }
    cout << "bgd size: " <<  sizeof(ext2_block_group_descriptor) << endl;
    //  read BGDs using group block pointers
    bgds.push_back(new ext2_block_group_descriptor);
    if(block_size==1024) memcpy(bgds[0], ((char *) map)+2*block_size, sizeof(ext2_block_group_descriptor));
    else if(block_size==2048 || (block_size==4096)) memcpy(bgds[0], ((char *) map)+block_size, sizeof(ext2_block_group_descriptor));
    for( i=1; i<group_count; i++){
        bgds.push_back(new ext2_block_group_descriptor);
        // each group has its bgd's on Block1 (second block) for all sizes ?? VERIFY !
        memcpy(bgds[i], ((char *) map)+group_initial_blocks[i]*block_size+block_size , sizeof(ext2_block_group_descriptor));
        cout << "block_bitmap block is at: " << (int) bgds[i]->block_bitmap << endl ;
        cout << "inode_bitmap  is : " << (int) bgds[i]->inode_bitmap << endl ;
        cout << "inode_table  is : " << (int) bgds[i]->inode_table << endl ;
        cout << "first empty_inode  is : " << (int) supers[i]->first_inode << endl ;
    }
    //  read root dir
    root = (ext2_inode*) (((char *) map)+((bgds[0]->inode_table)*block_size)+(supers[0]->inode_size));
    cout << "initial inode's mode  is : " << root->mode << endl;
    cout << "initial inode's size  is : " << root->size << endl;
    cout << "initial inode's creation_time  is : " << root->creation_time << endl;

}

