//
// Created by Selin Yıldırım on 21.06.2021.
//
#include "ext2fs.h"

int deallocate(){
    for (auto p : supers) { if(p) delete p;}
    supers.clear();
    for (auto p : bgds) { if(p) delete p;}
    bgds.clear();
    for (auto p : dir_entries) { if(p) delete p;}
    dir_entries.clear();

    if (munmap(map, img_stat.st_size) == -1) {
        perror("Error unmmapping the file"); exit(1);
    }
    return 0;
}

int string_split(char *ptr, bool sourceMode,  bool destinationNode){ // if sourceMode== 1 fill source path vector, else fill destination path.
    // if destinationNode!=NULL : inode repres of dest.
    int dest=0;
    string source = ptr;           //  overloaded '=' operator from string lib
    string delimiter = "/";
    size_t pos = 0;
    string token;
    if(!destinationNode) source.erase(0, delimiter.length());    // absolute path's root discarded before start.
    while ((pos = source.find(delimiter)) != string::npos) {
        token = source.substr(0, pos);
        if(sourceMode) sourcePath.push_back(token);
        else{
            if(destinationNode) dest = stoi(token);
            else destinationPath.push_back(token);
        }
        source.erase(0, pos + delimiter.length());
        //cout << token << endl;
    }
    // last piece is not processed ! if destination, do  not save the last file name since it will be created later on.
    if(sourceMode) sourcePath.push_back(source);
    else if(!sourceMode) newFileName = source;
    //cout << source << endl;

    return dest; // return 0 if the mode is otherwise
}