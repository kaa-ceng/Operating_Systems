#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "ext2fs.h"
#include "ext2fs_print.h"

// setting to 300, max path count is around 130 in example 3
#define PATH_MAX 500

// global var
int fd;
struct ext2_super_block mySuperblock;
struct ext2_block_group_descriptor *bgdTable;
uint32_t blockSize;
FILE *stateOutputFile;
FILE *historyOutputFile;
int firstHistoryLine = 1;  // track first line fro empty line
int firstStateLine = 1;  // track first line

// function decl
uint32_t delParentInode(uint32_t targetInode);

// index loc
uint32_t inodeLocationCalculation(uint32_t inodeNum) {

    uint32_t blockGrIndex = (inodeNum - 1) / mySuperblock.inodes_per_group;
    uint32_t indexInGroup = (inodeNum - 1) % mySuperblock.inodes_per_group;
    uint32_t inodeLocation = bgdTable[blockGrIndex].inode_table * blockSize + (indexInGroup * mySuperblock.inode_size);
    
    return inodeLocation;
}

// name = entry->name
void copyEntryName(const struct ext2_dir_entry *entry, char *name) {
    
    int ii;

    for (ii = 0; ii < entry->name_length; ii++) {
        name[ii] = entry->name[ii];
    }
    name[ii] = '\0';
}


// check inode is directory
int isDirectory(uint32_t inodeNum) {
    if (inodeNum == 0) {
        return 0;
    }

    // inode location
    uint32_t indexLoc = inodeLocationCalculation(inodeNum);
    
    // inode data
    struct ext2_inode inodeData;
    pread(fd, &inodeData, sizeof(inodeData), indexLoc);
    
    // check mode
    // from ext2fs.h
    if (inodeData.mode & EXT2_I_DTYPE) {
        return 1;  // directory
    }
    else {
        return 0;  // not directory
    }
}




// display the file hierarchy starting from the given inode number
void displayHierarchy(uint32_t inodeNum, int depth) 
{ 
    // if inodeNum is 0 return
    if(inodeNum==0){
        return;
    }
    
    // inode location
    uint32_t inodeLoc = inodeLocationCalculation(inodeNum);

    //inode data
    struct ext2_inode inode;
    pread(fd, &inode, sizeof(inode), inodeLoc);

    // for each block
    // printf("iterating over blocks for inode %u\n", inodeNum);
    for (int ii = 0; ii < EXT2_NUM_DIRECT_BLOCKS; ii++) {

        // if block is 0, skip it
        if (inode.direct_blocks[ii] == 0) {
            continue; 
        }

        // calculate the block pos
        uint32_t blockPos = inode.direct_blocks[ii] * blockSize;

        // data holds directory block data
        char *data = malloc(blockSize);

        // read the directory block
        pread(fd, data, blockSize, blockPos);

        // offset to process directory entries
        uint32_t position = 0;

        // process directory entries in the bl//printf("processing dir entries\n");ck
        //printf("processing dir entries\n");
        while (position < blockSize) {
            struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(data + position);
            
            // check entry 
            if (entry->length == 0 || entry->length < EXT2_DIR_ENTRY_HEADER_SIZE) {
                printf("error: invalid entry length \n");
                break;
            }

            /////////////// GHOST part

            // checking ghost entries, looking at space
            uint32_t expectedLength = EXT2_DIR_LENGTH(entry->name_length);

            // if expectedLength is larger than entry length, skip ghost detection
            if (expectedLength > entry->length || expectedLength < EXT2_DIR_ENTRY_HEADER_SIZE) {
                position += entry->length;
                continue;
            }

            // length is larger than expected, might be ghost entry
            if (entry->length > expectedLength + EXT2_DIR_ENTRY_HEADER_SIZE) {

                //printf("looking for ghost entries\n");
                
                uint32_t ghostPos = position + expectedLength;
                
                // inode 4 bytes, 4 byte alignment
                // move to next multiple of 4
                while (ghostPos % 4 != 0) {
                    ghostPos++;// printf("looking for ghost entries starting at offset %u\n", ghostOffset);

                }


                // checking for ghost entries
                // printf("looking for ghost entries starting at offset %u\n", ghostOffset);
                while (ghostPos + EXT2_DIR_ENTRY_HEADER_SIZE <= position + entry->length && ghostPos < blockSize) {
                    struct ext2_dir_entry *ghostEntry = (struct ext2_dir_entry *)(data + ghostPos);
                    
                    // check if valid ghost entry - simplified validation
                    if (ghostEntry->inode > 0 && ghostEntry->inode <= mySuperblock.inode_count && ghostEntry->name_length > 0) {

                        char ghostName[EXT2_MAX_NAME_LENGTH + 1]; // +1 for null terminator

                        // fill ghost name
                        copyEntryName(ghostEntry, ghostName);

                        // skipping . and .. entries and empty names
                        if (ghostName[0] != '.' || (ghostEntry->name_length != 1 && 
                            !(ghostEntry->name_length == 2 && ghostName[1] == '.'))) {

                            
                            if (firstStateLine) {
                                // dashes for depth
                                // no newline at beginning for first line
                                for (int iii = 0; iii < depth; iii++) {
                                    fprintf(stateOutputFile, "-");
                                }
                                firstStateLine = 0;  // not first line anymore
                            } else {
                                // newline at beginning for other lines
                                fprintf(stateOutputFile, "\n");
                                for (int iiii = 0; iiii < depth; iiii++) {
                                    fprintf(stateOutputFile, "-");
                                }
                            }

                            // check is directory or file
                            if (isDirectory(ghostEntry->inode)) {
                                fprintf(stateOutputFile, " (%u:%s/)", ghostEntry->inode, ghostName);
                            } 
                            // file
                            else {
                                fprintf(stateOutputFile, " (%u:%s)", ghostEntry->inode, ghostName);
                            }
                        }
                        ghostPos += ghostEntry->length;
                    }

                    else {
                        //debug
                        //printf("invalid ghost entry #2 \n");
                        
                        // next 4 byte position for other ghost entry
                        ghostPos += 4;
                    }
                }
            }

            /////////////// end ghost part



            // regular entries
            if (entry->inode != 0) {

                char name[EXT2_MAX_NAME_LENGTH + 1]; // for null terminator
                //int ii;

                // copy to name
                copyEntryName(entry, name);

                //null terminate
                //name[ii] = '\0';
                
                // if . or .. skip 
                if (name[0] == '.' && (entry->name_length == 1 || (entry->name_length == 2 && name[1] == '.'))) {
                    position += entry->length;
                    continue;
                }
                
                if (firstStateLine) {
                    // dashes for depth
                    // no newline at beginning for first line
                    for (int kk = 0; kk < depth; kk++) {
                        fprintf(stateOutputFile, "-");
                    }
                    firstStateLine = 0;  // not first line anymore
                } 
                else {
                    // newline at beginning for other lines
                    fprintf(stateOutputFile, "\n");
                    for (int kk = 0; kk < depth; kk++) {
                        fprintf(stateOutputFile, "-");
                    }
                }
                
                // check if directory or file
                if (isDirectory(entry->inode)) 
                {

                    fprintf(stateOutputFile, " %u:%s/", entry->inode, name);

                    //recursive
                    displayHierarchy(entry->inode, depth +1);
                } 
                // file
                else {
                    fprintf(stateOutputFile, " %u:%s", entry->inode , name);
                }
            }
            position +=entry->length;
        }
        
        //debug
        //printf("finished processing block \n");
        free(data);
    }
    
    ///debug
    //printf("finished processing inode \n");

}




////////////////////////////// history part

// delete structure
struct deletion {
    uint32_t inodeNum;
    uint32_t deletionTime;
    int isDirectory;
    char path[PATH_MAX]; // path to the deleted file or directory
    uint32_t parentInode;
    int foundLocation;
};


// structure for mkdir and touch
struct mkdirTouch {
    uint32_t inodeNum;
    uint32_t creationTime;
    int isDirectory;
    char path[PATH_MAX];
    uint32_t parentInode;
    int foundLocation;
};


// find directory path from inode
void findPath(uint32_t targetInode, uint32_t currentInode, char* currentPath, char* resultPath, int* found) {

    // check doing this in directoryHierarchy ??
    
    if (*found || currentInode == 0) {
        return;
    }
    
    // calculate inode location
    uint32_t inodeLocation = inodeLocationCalculation(currentInode);
    
    // read inode
    struct ext2_inode inode;
    pread(fd, &inode, sizeof(inode), inodeLocation);
    
    // only directories
    if (!(inode.mode & EXT2_I_DTYPE)) {
        return;
    }
    
    // for each block in directory
    for (int ii = 0; ii < EXT2_NUM_DIRECT_BLOCKS; ii++) {
        
        if (inode.direct_blocks[ii] == 0) {
            continue;
        }
        
        uint32_t directoryBlockPosition = inode.direct_blocks[ii] * blockSize;
        char *data = malloc(blockSize);
        pread(fd, data, blockSize, directoryBlockPosition);
        
        uint32_t position = 0;
        
        while (position < blockSize && !*found) {
            struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(data + position);
            
            if (entry->length == 0 || entry->length < EXT2_DIR_ENTRY_HEADER_SIZE) {
                break;
            }
            
            // check ghost entries
            uint32_t expectedLength = EXT2_DIR_LENGTH(entry->name_length);
            
            if (expectedLength <= entry->length && entry->length > expectedLength + EXT2_DIR_ENTRY_HEADER_SIZE) {
                
                uint32_t ghostPos = position + expectedLength;
                
                // align to 4 bytes
                while (ghostPos % 4 != 0) {
                    ghostPos++;
                }
                
                // search for ghost entries
                while (ghostPos + EXT2_DIR_ENTRY_HEADER_SIZE <= position + entry->length && ghostPos < blockSize) {
                    struct ext2_dir_entry *ghostEntry = (struct ext2_dir_entry *)(data + ghostPos);
                    
                    if (ghostEntry->inode == targetInode && ghostEntry->inode > 0 && 
                        ghostEntry->inode <= mySuperblock.inode_count && 
                        ghostEntry->name_length > 0 && ghostEntry->name_length <= EXT2_MAX_NAME_LENGTH) {
                        
                        char ghostName[EXT2_MAX_NAME_LENGTH + 1];
                        // int j ;
                        
                        // cahnged to helper function
                        copyEntryName(ghostEntry, ghostName);
                        
                        // skip . and .. entries
                        if (ghostName[0] != '.' || (ghostEntry->name_length != 1 && !(ghostEntry->name_length == 2 && ghostName[1] == '.'))) {
                            
                            // found target inode
                            // building path with ghost name
                            if (strlen(currentPath) == 0) {
                                // if current path is empty, im at root
                                // path is just /filename
                                sprintf(resultPath, "/%s", ghostName);
                            } 
                            else {
                                //path is not empty, add ghost name
                                // current path + / + filename
                                sprintf(resultPath, "%s/%s", currentPath, ghostName);
                            }

                            *found = 1;
                            free(data);
                            return;
                        }
                    }
                    
                    if (ghostEntry->length > 0) {
                        ghostPos += ghostEntry->length;
                    } else {
                        ghostPos += 4;
                    }
                }
            }
            
            // check regular entries
            if (entry->inode != 0) {
                
                char name[EXT2_MAX_NAME_LENGTH + 1];
                
                // copy to name
                copyEntryName(entry, name);
                

                // skip . and .. entries
                if (name[0] == '.' && (entry->name_length == 1 || (entry->name_length == 2 && name[1] == '.'))) {
                    position += entry->length;
                    continue;
                }
                
                // if found target inode in regular entries
                if (entry->inode == targetInode) {

                    // do complete file path
                    // directory path with file name
                    if (strlen(currentPath) == 0) {
                        // at root, path is just /filename
                        sprintf(resultPath, "/%s", name);
                    } 
                    else {
                        // in a subdirectory, add path
                        sprintf(resultPath, "%s/%s", currentPath, name);
                    }
                    *found = 1;
                    free(data);
                    return;
                }
                
                // if directory, recurse
                if (isDirectory(entry->inode)) {
                    // found a subdirectory, search inside
                    // build  path to this subdirectory
                    char newPath[PATH_MAX];
                    
                    if (strlen(currentPath) == 0) {
                        // at root, new path is /directoryName
                        sprintf(newPath, "/%s", name);
                    } 
                    else {
                        // path is not empty, add ghost name
                        sprintf(newPath, "%s/%s", currentPath, name);
                    }
                    
                    // search inside this subdirectory recursively
                    findPath(targetInode, entry->inode, newPath, resultPath, found);
                }
            }
            position += entry->length;
        }
        
        free(data);
    }
    
    // checking ghost directories, all deleted directory inodes
    for (uint32_t checkInode = 1; checkInode <= mySuperblock.inode_count; checkInode++) {
        
        if (checkInode == currentInode) {
            continue;
        }
        
        // calculate inode location for ghost dir
        uint32_t checkInodeLocation = inodeLocationCalculation(checkInode);
        
        struct ext2_inode checkInodeData;
        pread(fd, &checkInodeData, sizeof(checkInodeData), checkInodeLocation);
        
        // check if deleted directory was in current directory
        if (checkInodeData.deletion_time > 0 && (checkInodeData.mode & EXT2_I_DTYPE)) {
            
            uint32_t foundParent = delParentInode(checkInode);
            if (foundParent == currentInode) {
                
                // deleted directory name
                char deletedDirName[EXT2_MAX_NAME_LENGTH + 1];
                int foundName = 0;
                
                // check current directory for ghost entry of deleted directory
                for (int i3 = 0; i3 < EXT2_NUM_DIRECT_BLOCKS && !foundName; i3++) {
                    
                    if (inode.direct_blocks[i3] == 0) {
                        continue;
                    }
                    
                    uint32_t directoryBlockPosition = inode.direct_blocks[i3] * blockSize;
                    char *data = malloc(blockSize);
                    pread(fd, data, blockSize, directoryBlockPosition);
                    
                    uint32_t position = 0;
                    
                    while (position < blockSize && !foundName) {
                        
                        struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(data + position);
                        
                        if (entry->length == 0 || entry->length < EXT2_DIR_ENTRY_HEADER_SIZE) {
                            break;
                        }
                        
                        // check for ghost entries
                        uint32_t expectedLength = EXT2_DIR_LENGTH(entry->name_length);
                        
                        if (expectedLength <= entry->length && entry->length > expectedLength + EXT2_DIR_ENTRY_HEADER_SIZE) {
                            
                            uint32_t ghostPos = position + expectedLength;
                            
                            // align to 4 bytes
                            while (ghostPos % 4 != 0) {
                                ghostPos++;
                            }
                            
                            // search for ghost entries
                            while (ghostPos + EXT2_DIR_ENTRY_HEADER_SIZE <= position + entry->length && ghostPos < blockSize) {
                                
                                struct ext2_dir_entry *ghostEntry = (struct ext2_dir_entry *)(data + ghostPos);
                                
                                if (ghostEntry->inode == checkInode && ghostEntry->inode > 0 && 
                                    ghostEntry->inode <= mySuperblock.inode_count && 
                                    ghostEntry->name_length > 0 && ghostEntry->name_length <= EXT2_MAX_NAME_LENGTH) {
                                    
                                    char ghostName[EXT2_MAX_NAME_LENGTH + 1];
                                    
                                    // changed to helper function
                                    copyEntryName(ghostEntry, ghostName);
                                    
                                    // skip . and .. entries
                                    if (ghostName[0] != '.' || (ghostEntry->name_length != 1 && 
                                        !(ghostEntry->name_length == 2 && ghostName[1] == '.'))) {
                                        
                                        // copy ghostName to deletedDirName
                                        int i5;
                                        for (i5 = 0; i5 < ghostEntry->name_length; i5++) {
                                            deletedDirName[i5] = ghostName[i5];
                                        }
                                        deletedDirName[i5] = '\0'; // null terminate
                                        
                                        foundName = 1;
                                        break;
                                    }
                                }
                                
                                if (ghostEntry->length > 0) {
                                    ghostPos += ghostEntry->length;
                                } else {
                                    ghostPos += 4;
                                }
                            }
                        }
                        
                        position += entry->length;
                    }
                    
                    free(data);
                }
                
                // name found, recurse deleted directory
                if (foundName) {

                    char newPath[PATH_MAX];

                    if (strlen(currentPath) == 0) {
                        sprintf(newPath, "/%s", deletedDirName);
                    } 
                    else {
                        sprintf(newPath, "%s/%s", currentPath, deletedDirName);
                    }

                    findPath(targetInode, checkInode, newPath, resultPath, found);
                }
            }
        }
    }
}

// find parent directory inode for deleted entry
uint32_t delParentInode(uint32_t targetInode) {

    // find inode location
    for (uint32_t inodeNum = 1; inodeNum <= mySuperblock.inode_count; inodeNum++) {
        if (!isDirectory(inodeNum)) {
            continue;
        }
        
        // calculate inode location
        uint32_t inodeLocation = inodeLocationCalculation(inodeNum);
        
        struct ext2_inode inode;
        pread(fd, &inode, sizeof(inode), inodeLocation);
        
        // search for ghost entries
        for (int i6 = 0; i6 < EXT2_NUM_DIRECT_BLOCKS; i6++) {
            
            if (inode.direct_blocks[i6] == 0) {
                continue;
            }
            
            uint32_t directoryBlockPosition = inode.direct_blocks[i6] * blockSize;
            char *data = malloc(blockSize);
            pread(fd, data, blockSize, directoryBlockPosition);
            
            uint32_t position = 0;
            
            while (position < blockSize) {
                
                struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(data + position);
                
                if (entry->length == 0 || entry->length < EXT2_DIR_ENTRY_HEADER_SIZE) {
                    break;
                }
                
                // check for ghost entries
                uint32_t expectedLength = EXT2_DIR_LENGTH(entry->name_length);
                
                if (expectedLength <= entry->length && entry->length > expectedLength + EXT2_DIR_ENTRY_HEADER_SIZE) {
                    
                    uint32_t ghostPos = position + expectedLength;
                    
                    // align to 4 bytes
                    while (ghostPos % 4 != 0) {
                        ghostPos++;
                    }
                    
                    // search for ghost entries
                    while (ghostPos + EXT2_DIR_ENTRY_HEADER_SIZE <= position + entry->length && ghostPos < blockSize) {
                        
                        struct ext2_dir_entry *ghostEntry = (struct ext2_dir_entry *)(data + ghostPos);
                        
                        if (ghostEntry->inode == targetInode && ghostEntry->inode > 0 && 
                            ghostEntry->inode <= mySuperblock.inode_count && 
                            ghostEntry->name_length > 0 && ghostEntry->name_length <= EXT2_MAX_NAME_LENGTH) {
                            
                            free(data);
                            return inodeNum;
                        }
                        
                        if (ghostEntry->length > 0) {
                            ghostPos += ghostEntry->length;
                        } 
                        else {
                            ghostPos += 4;
                        }
                    }
                }
                
                position += entry->length;
            }
            
            free(data);
        }
    }
    
    return 0; // not found
}

// find deleted files and their locations
void deletedFiles() {
    
    // store deletion info
    // check 1000 later ???
    struct deletion deletions[PATH_MAX]; 
    int deletionCount = 0;
    
    // find all deleted inodes
    for (uint32_t inodeNum = 1; inodeNum <= mySuperblock.inode_count; inodeNum++) {
        // calculate inode location
        uint32_t inodeLoc = inodeLocationCalculation(inodeNum);
        
        // read inode
        struct ext2_inode tempInode;
        pread(fd, &tempInode, sizeof(tempInode), inodeLoc);
        
        // check if was deleted
        if (tempInode.deletion_time > 0 && deletionCount < PATH_MAX) {
            
            // store deletion info
            deletions[deletionCount].inodeNum = inodeNum;
            deletions[deletionCount].deletionTime = tempInode.deletion_time;
            
            // check if directory or file
            if (tempInode.mode & EXT2_I_DTYPE) {
                deletions[deletionCount].isDirectory = 1;
            } 
            else {
                deletions[deletionCount].isDirectory = 0;
            }
            
            deletions[deletionCount].foundLocation = 0;
            deletions[deletionCount].parentInode = 0;
            
            deletions[deletionCount].path[0] = '?';
            deletions[deletionCount].path[1] = '\0';
            
            deletionCount++;
        }
    }
    
    // locations of deleted inodes
    for (int i = 0; i < deletionCount; i++) {
        
        // printf("processing deletion %d\n", i); // debug
        
        // parent directory
        uint32_t parentInode = delParentInode(deletions[i].inodeNum);
        
        if (parentInode > 0) {
            deletions[i].parentInode = parentInode;
            deletions[i].foundLocation = 1;
            
            // full path
            char resultPath[PATH_MAX];
            int found = 0;
            findPath(deletions[i].inodeNum, EXT2_ROOT_INODE, "", resultPath, &found);
            
            if (found) {
                
                int i7;
                for (i7 = 0; resultPath[i7] != '\0'; i7++) {
                    deletions[i].path[i7] = resultPath[i7];
                }
                deletions[i].path[i7] = '\0'; // null terminate
            }
        }
    }
    
    // output deletion history
    for (int i = 0; i < deletionCount; i++) {
    
        if (deletions[i].isDirectory) {
            // directory (rmdir)
            if (deletions[i].foundLocation) {
                if (firstHistoryLine) {
                    fprintf(historyOutputFile, "%u rmdir [%s] [%u] [%u]", deletions[i].deletionTime, deletions[i].path,  deletions[i].parentInode, deletions[i].inodeNum);
                    firstHistoryLine = 0;  // not first line anymore
                } else {
                    fprintf(historyOutputFile, "\n%u rmdir [%s] [%u] [%u]", deletions[i].deletionTime, deletions[i].path,  deletions[i].parentInode, deletions[i].inodeNum);
                }
            } else {
                if (firstHistoryLine) {
                    fprintf(historyOutputFile, "%u rmdir [?] [?] [%u]", deletions[i].deletionTime, deletions[i].inodeNum);
                    firstHistoryLine = 0; 
                } else {
                    fprintf(historyOutputFile, "\n%u rmdir [?] [?] [%u]", deletions[i].deletionTime, deletions[i].inodeNum);
                }
            }
        } else {
            // file (rm)
            if (deletions[i].foundLocation) {
                if (firstHistoryLine) {
                    fprintf(historyOutputFile, "%u rm [%s] [%u] [%u]", deletions[i].deletionTime, deletions[i].path, deletions[i].parentInode, deletions[i].inodeNum);
                    firstHistoryLine = 0;
                } else {
                    fprintf(historyOutputFile, "\n%u rm [%s] [%u] [%u]", deletions[i].deletionTime, deletions[i].path, deletions[i].parentInode, deletions[i].inodeNum);
                }
            } else {
                if (firstHistoryLine) {
                    fprintf(historyOutputFile, "%u rm [?] [?] [%u]", deletions[i].deletionTime, deletions[i].inodeNum);
                    firstHistoryLine = 0;
                } else {
                    fprintf(historyOutputFile, "\n%u rm [?] [?] [%u]", deletions[i].deletionTime, deletions[i].inodeNum);
                }
            }
        }
    }
}

// created files and directories
void funcCreated() {
    
    // 1000??? check later
    struct mkdirTouch creations[PATH_MAX];
    int creationCount = 0;
    
    // cchecking creation times
    // starting from inode 11
    for (uint32_t inodeNum = 11; inodeNum <= mySuperblock.inode_count; inodeNum++) {
        // calculate inode location
        uint32_t inodeLoc = inodeLocationCalculation(inodeNum);
        
        // read inode
        struct ext2_inode tempInode;
        pread(fd, &tempInode, sizeof(tempInode), inodeLoc);
        

        if (tempInode.access_time > 0 && creationCount < PATH_MAX) {
            
            // process all inodes starting from 11
            if (creationCount < PATH_MAX) {
                
                // store creation info
                creations[creationCount].inodeNum = inodeNum;
                creations[creationCount].creationTime = tempInode.access_time;
                
                // check if directory or file
                // from ext2fs.h
                if (tempInode.mode & EXT2_I_DTYPE) {
                    creations[creationCount].isDirectory = 1;
                } else {
                    creations[creationCount].isDirectory = 0;
                }
                
                creations[creationCount].foundLocation = 0;
                creations[creationCount].parentInode = 0;
                
                creations[creationCount].path[0] = '?';
                creations[creationCount].path[1] = '\0';
                
                creationCount++;
            }
        }
    }
    
    // paths created inodes
    for (int i = 0; i < creationCount; i++) {
        
        //  path from root
        char resultPath[PATH_MAX];
        int found = 0;
        findPath(creations[i].inodeNum, EXT2_ROOT_INODE, "", resultPath, &found);
        
        if (found) {
            int kk;
            for (kk = 0; resultPath[kk] != '\0'; kk++) {
                creations[i].path[kk] = resultPath[kk];
            }
            creations[i].path[kk] = '\0';
            creations[i].foundLocation = 1;
            

            // parent path 
            char *lastSlash = NULL;

            for (int j = 0; resultPath[j] != '\0'; j++) {
                if (resultPath[j] == '/') {
                    lastSlash = &resultPath[j];
                }
            }
            
            if (lastSlash && lastSlash != resultPath) {
                // parent path 
                char parentPath[PATH_MAX];
                int parentLen = lastSlash - resultPath;

                for (int k = 0; k < parentLen; k++) {
                    parentPath[k] = resultPath[k];
                }
                parentPath[parentLen] = '\0';
                
                // find parent inode by searching for exact path match
                for (uint32_t checkInode = 1; checkInode <= mySuperblock.inode_count; checkInode++) {
                    
                    if (isDirectory(checkInode)) {

                        char checkPath[PATH_MAX];
                        int checkFound = 0;

                        findPath(checkInode, EXT2_ROOT_INODE, "", checkPath, &checkFound);
                        
                        if (checkFound) {
                            
                            // compare paths
                            if (strcmp(parentPath, checkPath) == 0) {
                                creations[i].parentInode = checkInode;
                                break;
                            }
                        }
                    }
                }
            } 
            else {
                // no parent path found, direct child of root
                creations[i].parentInode = EXT2_ROOT_INODE;
            }
        }
    }
    
    // output creation history
    for (int i = 0; i < creationCount; i++) {
        
        if (creations[i].foundLocation) {

            if (creations[i].isDirectory) {
                
                if (firstHistoryLine) {
                    fprintf(historyOutputFile, "%u mkdir [%s] [%u] [%u]", creations[i].creationTime, creations[i].path,creations[i].parentInode, creations[i].inodeNum);
                    firstHistoryLine = 0;  // not first line anymore
                } 
                else {
                    fprintf(historyOutputFile, "\n%u mkdir [%s] [%u] [%u]", creations[i].creationTime, creations[i].path,creations[i].parentInode, creations[i].inodeNum);
                }
            } 
            else {
                if (firstHistoryLine) {
                    fprintf(historyOutputFile, "%u touch [%s] [%u] [%u]", creations[i].creationTime, creations[i].path, creations[i].parentInode, creations[i].inodeNum);
                    firstHistoryLine = 0; 
                } 
                else {
                    fprintf(historyOutputFile, "\n%u touch [%s] [%u] [%u]", creations[i].creationTime, creations[i].path, creations[i].parentInode, creations[i].inodeNum);
                }
            }
        }
    }
}

// move action
/*
void findMv() {
    
}
    */


//////////////////////////////////////////////////////////// end history
///////////////////////


int main(int argc, char *argv[]) {

    char *image = argv[1];              
    char *stateOutput = argv[2];       
    char *historyOutput = argv[3];     

    // open filesystem image
    fd = open(image, O_RDONLY); 

    // read superblock
    pread(fd, &mySuperblock, sizeof(mySuperblock), EXT2_SUPER_BLOCK_POSITION);

    // blockSize = 2^(10 + logBlockSize)
    blockSize = EXT2_UNLOG(mySuperblock.log_block_size);

    uint32_t blockGroupCount = (mySuperblock.block_count + mySuperblock.blocks_per_group - 1) / mySuperblock.blocks_per_group;

    // block group descriptors
    uint32_t bgdBlock = mySuperblock.first_data_block + 1;
    uint32_t bgdOffset = bgdBlock * blockSize;

    bgdTable = malloc(blockGroupCount * sizeof(struct ext2_block_group_descriptor));
    pread(fd, bgdTable, blockGroupCount * sizeof(struct ext2_block_group_descriptor), bgdOffset);

    // open file
    stateOutputFile = fopen(stateOutput, "w");
    
    // new line flag
    firstStateLine = 1;
    
    fprintf(stateOutputFile, "- %u:root/", EXT2_ROOT_INODE);
    firstStateLine = 0; 
    
    displayHierarchy(EXT2_ROOT_INODE, 2);
    fprintf(stateOutputFile, "\n"); 
    fclose(stateOutputFile);

    historyOutputFile = fopen(historyOutput, "w");
    
    
    firstHistoryLine = 1;
    funcCreated();
    //findMv(); // 
    deletedFiles();

    fclose(historyOutputFile);

    free(bgdTable);
    close(fd);

    return 0;
}