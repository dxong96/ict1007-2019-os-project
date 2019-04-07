#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define ENTRIES 128
#define ALLOC_CONTIGUOUS -101
#define ALLOC_LINKED -102
#define ALLOC_INDEXED -103
#define ALLOC_LINKEDCONTIG -104
#define ALLOC_LINKEDCONTIG -104
#define CSV_NAME "fulltest.csv"

// block representation
typedef struct block
{
    int *entries;
    int index;
} Block;

// represent a file entry
typedef struct fileEntry
{
    int allocationType;
    int fileName;
    int params[2];
} FileEntry;

// vcb representation
typedef struct volumeControlBlock
{
    int blockSize;
    int freeBlockNum;
    Block **freeBlocks;
} VolumeControlBlock;

// physical store representation
typedef struct store
{
    VolumeControlBlock *vcb;
    FileEntry *fileEntry;
    int fileEntrySize;
    Block *blocks;
    int numBlocks;
} Store;

typedef struct instruction
{
    char *action;
    int fileName;
    int *fileContent;
    int fileSize;
} Instruction;

Store *createStore(int block_size, int allocationType)
{
    Store *store = malloc(sizeof(Store));

    int entrySize = ENTRIES;
    int leftovers = entrySize % block_size;
    int numBlocks = entrySize / block_size;
    int numFileSupported = leftovers - 1;

    // calculate supported number of files such that supported > block
    while (numBlocks > numFileSupported)
    {
        numFileSupported += block_size;
        numBlocks -= 1;
    }

    // allocate space for entries used for files
    int dataEntrySize = numBlocks * block_size;
    int *dataEntries = malloc(sizeof(int) * dataEntrySize);
    // set default value for file entries to -1
    for (int i = 0; i < dataEntrySize; i++)
    {
        dataEntries[i] = -1;
    }

    // assign block size to each block
    Block *blocks = malloc(sizeof(Block) * numBlocks);
    // assign freeblocks all new block pointers to freeblocks
    Block **freeBlocks = malloc(sizeof(Block *) * numBlocks);
    // assign entries and index to each block
    for (int i = 0; i < numBlocks; i++)
    {
        blocks[i].index = i;
        blocks[i].entries = (dataEntries + i * block_size);
        freeBlocks[i] = (blocks + i);
    }

    VolumeControlBlock *vcb = malloc(sizeof(VolumeControlBlock));
    vcb->freeBlocks = freeBlocks;
    vcb->blockSize = block_size;
    vcb->freeBlockNum = numBlocks;

    store->fileEntrySize = numFileSupported;
    store->blocks = blocks;
    store->numBlocks = numBlocks;
    store->fileEntry = malloc(numFileSupported * sizeof(FileEntry));
    store->vcb = vcb;

    for (int i = 0; i < store->fileEntrySize; i++)
    {
        store->fileEntry[i].fileName = 0;
        store->fileEntry[i].allocationType = allocationType;
        store->fileEntry[i].params[0] = 0;
        store->fileEntry[i].params[1] = 0;
    }

    return store;
}

void freeStore(Store *store)
{
    free(store->vcb->freeBlocks);
    free(store->vcb);
    free(store->blocks[0].entries);
    free(store->blocks);
    free(store->fileEntry);
}

Block *store_getBlock(Store *store, int index)
{
    return store->blocks + index;
}

void block_clear(Block *block, int blockSize)
{
    for (int i = 0; i < blockSize; i++)
    {
        block->entries[i] = -1;
    }
}

void vcb_freeBlock(VolumeControlBlock *vcb, Block *block)
{
    block_clear(block, vcb->blockSize);
    vcb->freeBlocks[block->index] = block;
    vcb->freeBlockNum += 1;
}

void vcb_useBlock(VolumeControlBlock *vcb, int index)
{
    vcb->freeBlocks[index] = NULL;
    vcb->freeBlockNum -= 1;
}

Block *store_findFreeBlock(Store *store)
{
    VolumeControlBlock *vcb = store->vcb;
    for (int i = 0; i < store->numBlocks; i++)
    {
        if (vcb->freeBlocks[i] != NULL)
        {
            printf("B%d found in %d traversals\n", vcb->freeBlocks[i]->index, i + 1);
            return vcb->freeBlocks[i];
        }
    }
    return NULL;
}

FileEntry *store_findFreeFileEntry(Store *store)
{
    for (int i = 0; i < store->fileEntrySize; i++)
    {
        if (store->fileEntry[i].fileName == 0)
        {
            return store->fileEntry + i;
        }
    }
    return NULL;
}

void store_add(Store *s, int allocationType, int fileName, int fileSize, int *fileContents)
{
    //if file exists then then don't add
    int exists = 0;
    for (int i = 0; i < s->fileEntrySize; i++)
    {
        if (s->fileEntry[i].fileName == fileName)
        {
            exists = 1;
            break;
        }
    }

    if (exists)
    {
        printf("File %d already exists\n", fileName);
    }
    else
    {
        if (allocationType == ALLOC_CONTIGUOUS)
        {
            // find the blocks required
            int blocksRequired = fileSize / s->vcb->blockSize;

            // round up the blocks required
            if (fileSize % s->vcb->blockSize > 0)
            {
                blocksRequired++;
            }

            //if file size is too big, return
            if (s->vcb->freeBlockNum < blocksRequired)
            {
                printf("\nFile size too big");
                return;
            }

            int temp = 0;
            int i = 0;
            int count = 0;

            //find the index of the free block
            for (i = 0; i < s->numBlocks; ++i)
            {
                if (s->vcb->freeBlocks[i] != NULL)
                {
                    count++;
                    //if found the right file size, break
                    if (count >= blocksRequired)
                    {
                        break;
                    }
                }
                if (s->vcb->freeBlocks[i] == NULL)
                {
                    count = 0;
                }
            }
            printf("%d Traversals to find blocks\n", i);
            if (count < blocksRequired)
            {
                printf("No contiguous space found");
                return;
            }

            temp = i;
            // i refers to the index of the block where the file is to be inserted
            i -= blocksRequired - 1;

            printf("Adding file %d and found free ", fileName);
            //allocate the blocks
            for (int j = 0; j <= blocksRequired; ++j)
            {
                vcb_useBlock(s->vcb, i + j);
                printf("B%d ", i + j);
            }
            printf("\n");

            int prevBlock = -1;
            printf("Adding file%d at", fileName);
            for (int j = 0; j < fileSize; ++j)
            {
                int blockOffset = j / s->vcb->blockSize;

                if (prevBlock != blockOffset)
                {
                    if (prevBlock != -1)
                    {
                        printf(")");
                    }
                    printf(" B%d(", i + blockOffset);
                    prevBlock = blockOffset;
                }
                s->blocks[i + blockOffset].entries[j % s->vcb->blockSize] = fileContents[j];
                printf("%d ", fileContents[j]);
            }
            if (fileSize == 0)
            {
                printf(" B%d(", i);
            }
            printf(")\n");

            //allocate the file entry appropriately
            for (int k = 0; k < s->fileEntrySize; ++k)
            {
                if (s->fileEntry[k].fileName == 0)
                {
                    s->fileEntry[k].allocationType = allocationType;
                    s->fileEntry[k].fileName = fileName;
                    s->fileEntry[k].params[0] = i;
                    s->fileEntry[k].params[1] = blocksRequired;
                    break;
                }
            }
        }
        else if (allocationType == ALLOC_LINKED)
        {
            // Get File Entry if none available, End
            FileEntry *fe = store_findFreeFileEntry(s);
            if (fe == NULL)
            {
                printf("No File Entry available\n");
            }
            else
            {
                // Get Number of blocks needed
                int blockSize = s->vcb->blockSize;
                int blocksNeeded = 0;
                int entriesLeft = fileSize;
                while (entriesLeft > blockSize)
                {
                    entriesLeft -= (blockSize - 1);
                    blocksNeeded++;
                }
                blocksNeeded++;

                // If there is not enoguh space, End. Else start file allocation
                if (blocksNeeded > s->vcb->freeBlockNum)
                {
                    printf("Not enough space for file\n");
                }
                else
                {
                    Block **blocks = malloc(sizeof(Block *) * blocksNeeded);
                    //find free block(s)
                    for (int i = 0; i < blocksNeeded; i++)
                    {
                        blocks[i] = store_findFreeBlock(s);
                        vcb_useBlock(s->vcb, blocks[i]->index);
                    }
                    printf("Adding File%d, found blocks: ", fileName);
                    for (int i = 0; i < blocksNeeded; i++)
                    {
                        printf("B%d ", blocks[i]->index);
                    }
                    printf("\n");
                    //set file entry to values
                    fe->fileName = fileName;
                    fe->params[0] = blocks[0]->index;
                    fe->params[1] = blocks[blocksNeeded - 1]->index;

                    //allocate the file content to the blocks
                    printf("Added File%d at: ", fileName);
                    int filePos = 0;
                    for (int i = 0; i < blocksNeeded; i++)
                    {
                        printf("B%d(", blocks[i]->index);
                        int count = 0;
                        //Fill upto blocksize - 1 or if fileSize - 1
                        while (count < blockSize - 1 && filePos < fileSize - 1)
                        {
                            blocks[i]->entries[count] = fileContents[filePos];
                            printf("%d ", fileContents[filePos]);
                            count++;
                            filePos++;
                        }
                        //if there is leftover point to the next block. else fill the slot with file entry
                        if (i < blocksNeeded - 1)
                        {
                            blocks[i]->entries[count] = blocks[i + 1]->index;
                            printf("), ");
                        }
                        else if (filePos < fileSize)
                        {
                            blocks[i]->entries[count] = fileContents[filePos];
                            printf("%d)\n", fileContents[filePos]);
                        }
                        else if (fileSize == 0)
                        {
                            printf(")\n");
                        }
                    }
                    free(blocks);
                }
            }
        }
        else if (allocationType == ALLOC_INDEXED)
        {
            // minimum 1 for block containing the indices
            int blocksRequired = 1;
            // round up the filesize / blocksize
            // because any remainder means extra block is needed
            blocksRequired += ceil(fileSize / (double)s->vcb->blockSize);
            int entriesSupported = s->vcb->blockSize * s->vcb->blockSize;

            // when fileSize is more than entries supported by blockSize
            // or when blocksRequired is more than numbers of free block
            if (fileSize > entriesSupported || blocksRequired > s->vcb->freeBlockNum)
            {
                printf("Not enough space\n");
                return;
            }
            //For Printing
            char out[320];
            char added[1000];
            char blockIndex[7];
            char entryIndex[7];
            sprintf(out, "Adding file%d and found free ", fileName);
            sprintf(added, "Added file%d at ", fileName);

            Block *indexBlock = store_findFreeBlock(s);
            sprintf(blockIndex, "B%d, ", indexBlock->index);
            strcat(out, blockIndex);
            vcb_useBlock(s->vcb, indexBlock->index);
            FileEntry *entry = store_findFreeFileEntry(s);
            entry->fileName = fileName;
            entry->params[0] = indexBlock->index;

            blocksRequired--;
            for (int i = 0; i < blocksRequired; i++)
            {
                // find the next free block
                Block *contentBlock = store_findFreeBlock(s);
                //Printing
                sprintf(blockIndex, "B%d, ", contentBlock->index);
                strcat(out, blockIndex);
                sprintf(blockIndex, "B%d(", contentBlock->index);
                strcat(added, blockIndex);
                vcb_useBlock(s->vcb, contentBlock->index);
                // update the content block
                int offset = s->vcb->blockSize * i;
                for (int y = 0; y < s->vcb->blockSize && y + offset < fileSize; y++)
                {
                    contentBlock->entries[y] = fileContents[y + offset];
                    sprintf(entryIndex, "%d, ", fileContents[y + offset]);
                    strcat(added, entryIndex);
                }
                int a = strlen(added);
                added[a - 2] = ')';
                // update the index block
                indexBlock->entries[i] = contentBlock->index;
            }

            int n = strlen(out);
            out[n - 2] = '\0';
            printf("%s\n", out);
            printf("%s\n", added);
        }
        else if (allocationType == ALLOC_LINKEDCONTIG)
        {
            //get the amount of entries required
            int blockSize = s->vcb->blockSize;
            int entriesRequired = fileSize;
            if (fileSize == 0)
            {
                entriesRequired = 1;
            }
            int availableBlocks = 0;
            int prevBlock = 1;
            //get available file entry
            FileEntry *fe = store_findFreeFileEntry(s);
            if (fe == NULL)
            {
                printf("No File Entry available\n");
                return;
            }
            //get the number of blocks needed
            while (blockSize * availableBlocks < entriesRequired)
            {
                for (int i = 0; i < s->numBlocks; i++)
                {
                    if (s->blocks[i].entries[0] == -1)
                    {
                        prevBlock = 0;
                        availableBlocks++;
                    }
                    else
                    {
                        if (prevBlock == 0)
                        {
                            entriesRequired++;
                        }
                        prevBlock = 1;
                    }
                    if (blockSize * availableBlocks >= entriesRequired)
                    {
                        break;
                    }
                }
                //if not enough break
                if (blockSize * availableBlocks < entriesRequired)
                {
                    printf("Not enough space found\n");
                    return;
                }
            }

            //allocate pointers to store the block
            Block **blocks = malloc(sizeof(Block *) * availableBlocks);
            for (int i = 0; i < availableBlocks; i++)
            {
                blocks[i] = store_findFreeBlock(s);
                vcb_useBlock(s->vcb, blocks[i]->index);
            }
            printf("Adding File%d, found blocks: ", fileName);
            for (int i = 0; i < availableBlocks; i++)
            {
                printf("B%d ", blocks[i]->index);
            }
            printf("\n");
            //set the file pointer to the blocks needed
            fe->fileName = fileName;
            fe->params[0] = blocks[0]->index;
            fe->params[1] = blocks[availableBlocks - 1]->index;
            int currBlock = 0;
            int filePos = 0;
            printf("Adding to ");
            //while not the end of file
            while (filePos < fileSize)
            {
                printf("B%d(", blocks[currBlock]->index);
                int i;
                //insert to blocksize - 1
                for (i = 0; i < blockSize - 1 && filePos < fileSize; i++)
                {
                    blocks[currBlock]->entries[i] = fileContents[filePos];
                    printf("%d ", fileContents[filePos]);
                    filePos++;
                }
                //If next block is null means it is the last block
                if (currBlock < availableBlocks - 1)
                {
                    //if nextBlock index not currBlock index+1, it is not contiguous set entry as the pointer to next block else set to content
                    if (blocks[currBlock + 1]->index != blocks[currBlock]->index + 1)
                    {
                        blocks[currBlock]->entries[i] = blocks[currBlock + 1]->index;
                        printf("%d ", blocks[currBlock + 1]->index);
                    }
                    else
                    {
                        blocks[currBlock]->entries[i] = fileContents[filePos];
                        printf("%d ", fileContents[filePos]);
                        filePos++;
                    }
                    printf("), ");
                    currBlock++;
                }
                else if (filePos < fileSize)
                {
                    blocks[currBlock]->entries[i] = fileContents[filePos];
                    printf("%d ", fileContents[filePos]);
                }
            }
            printf(")\n");
            free(blocks);
        }
    }
}

void store_read(Store *s, int allocationType, int fileName)
{
    if (allocationType == ALLOC_CONTIGUOUS)
    {
        int blockSize = -1;
        int blockIndex = -1;
        int reads = 0;

        //find the block Index and block Size
        for (int i = 0; i < s->vcb->blockSize; ++i)
        {
            reads++;
            if (s->fileEntry[i].fileName == fileName)
            {
                blockIndex = s->fileEntry[i].params[0];
                blockSize = s->fileEntry[i].params[1];
            }
        }

        // if block size and block index does not exist
        if (blockSize == -1 && blockIndex == -1)
        {
            //loop through the entire program entries
            for (int i = 0; i < s->numBlocks * s->vcb->blockSize; ++i)
            {
                //if entries is fileName
                if (s->blocks[0].entries[i] == fileName)
                {
                    blockIndex = i / s->vcb->blockSize;
                    if (i % s->vcb->blockSize > 0)
                    {
                        blockIndex++;
                    }
                    //find the block in range in file entry
                    for (int j = 0; j < s->fileEntrySize; ++j)
                    {
                        reads++;
                        if (blockIndex >= s->fileEntry[j].params[0] && blockIndex <= s->fileEntry[j].params[0] + s->fileEntry[j].params[1])
                        {
                            printf("Read %d(%d) from %d\n", s->fileEntry[j].fileName, fileName, blockIndex - 1);
                            break;
                        }
                    }
                    break;
                }
            }
        }
        for (int i = 0; i < blockSize; ++i)
        {
            Block *readBlock = store_getBlock(s, i + blockIndex);
            for (int j = 0; j < sizeof(readBlock[i].entries); ++j)
            {
                printf("Block read: %d \n", readBlock[i].entries[j]);
            }
        }
        printf("Time = %d reads\n", reads);
    }
    else if (allocationType == ALLOC_LINKED)
    {
        //Find actual file name
        int fileActual = (fileName / 100) * 100;
        int start = -1;
        int end = -1;
        int reads = 0;
        for (int i = 0; i < s->fileEntrySize; i++)
        {
            reads++;
            if (s->fileEntry[i].fileName == fileActual)
            {
                start = s->fileEntry[i].params[0];
                end = s->fileEntry[i].params[1];
                break;
            }
        }
        if (start == -1)
        {
            printf("File%d not found\n", fileName);
        }
        else if (fileName == fileActual)
        {
            printf("File %d found", fileName);
        }
        else
        {
            Block b = s->blocks[start];
            int found = 0;
            int blockSize = s->vcb->blockSize;

            //find block that contains filename
            while (1)
            {
                //search block for filename
                printf("Reading B%d(", b.index);
                for (int i = 0; i < blockSize; i++)
                {
                    reads++;
                    if (b.entries[i] != -1)
                    {
                        printf("%d ", b.entries[i]);
                    }
                    else
                    {
                        break;
                    }
                    if (b.entries[i] == fileName)
                    {
                        printf(")\n");
                        printf("File%d(%d) found in B%d\n", fileActual, fileName, b.index);
                        found = 1;
                        break;
                    }
                }
                if (found == 1)
                    break;
                if (b.index != end)
                {
                    printf("), ");
                    b = s->blocks[b.entries[blockSize - 1]];
                    reads++;
                }
                else
                    break;
            }
            if (found == 0)
            {
                printf(")\nFile%d not found\n", fileName);
            }
            printf("Time = %d reads\n", reads);
        }
    }
    else if (allocationType == ALLOC_INDEXED)
    {
        // try to find the fileEntry with fileName
        FileEntry *fileEntry = NULL;
        for (int i = 0; i < s->fileEntrySize; i++)
        {
            if (s->fileEntry[i].fileName == fileName)
            {
                fileEntry = s->fileEntry + i;
                break;
            }
        }

        // fileName found
        if (fileEntry != NULL)
        {
            printf("Read file %d(%d) from directory structure\n", fileName, fileName);
            return;
        }

        // not found try to find from content
        int entriesSize = s->numBlocks * s->vcb->blockSize;
        const int *entries = s->blocks[0].entries;
        int entryIndex = -1;
        int reads = 0;
        for (int i = 0; i < entriesSize; i++)
        {
            reads++;
            if (entries[i] == fileName)
            {
                entryIndex = i;
                break;
            }
        }

        if (entryIndex == -1)
        {
            printf("File with name and content of %d is not found\n", fileName);
            printf("Time = %d reads\n", reads);
            return;
        }

        int blockIndex = entryIndex / (double)s->vcb->blockSize;
        fileEntry = NULL;
        for (int i = 0; i < s->fileEntrySize; i++)
        {
            fileEntry = s->fileEntry + i;
            Block indexBlock = s->blocks[fileEntry->params[0]];
            // check each fileEntry
            int entryPosition = -1;
            // find index block with content block index
            for (int y = 0; y < s->vcb->blockSize; y++)
            {
                reads++;
                if (indexBlock.entries[y] == blockIndex)
                {
                    entryPosition = y;
                    break;
                }
            }
            // when index block is found

            if (entryPosition != -1)
            {
                // Read file100(106) from <you will decide how it can be processed>
                printf("Read file %d(%d) from block %d\n", fileEntry->fileName, fileName, blockIndex);
                break;
            }
        }
        printf("Time = %d reads\n", reads);
    }
    else if (allocationType == ALLOC_LINKEDCONTIG)
    {
        int fileActual = (fileName / 100) * 100;
        int start = -1;
        int end = -1;
        int reads = 0;
        for (int i = 0; i < s->fileEntrySize; i++)
        {
            reads++;
            if (s->fileEntry[i].fileName == fileActual)
            {
                start = s->fileEntry[i].params[0];
                end = s->fileEntry[i].params[1];
                break;
            }
        }
        if (start == -1)
        {
            printf("File%d not found\n", fileName);
            return;
        }
        else if (fileName == fileActual)
        {
            printf("File %d found\n", fileName);
        }
        else
        {
            Block b = s->blocks[start];
            int found = 0;
            int blockSize = s->vcb->blockSize;

            //find block that contains filename
            while (1)
            {
                //search block for filename
                printf("Reading B%d(", b.index);
                for (int i = 0; i < blockSize; i++)
                {
                    reads++;
                    if (b.entries[i] != -1)
                    {
                        printf("%d ", b.entries[i]);
                    }
                    else
                    {
                        break;
                    }
                    if (b.entries[i] == fileName)
                    {
                        printf(")\n");
                        printf("File%d(%d) found in B%d\n", fileActual, fileName, b.index);
                        found = 1;
                        break;
                    }
                }
                if (found == 1)
                    break;
                if (b.index != end)
                {
                    printf("), ");
                    if (b.entries[blockSize - 1] < fileActual)
                    {
                        b = s->blocks[b.entries[blockSize - 1]];
                    }
                    else
                    {
                        b = s->blocks[b.index + 1];
                    }
                    reads++;
                }
                else
                    break;
            }
            if (found == 0)
            {
                printf(")\nFile%d not found\n", fileName);
            }
            printf("Time = %d reads\n", reads);
        }
    }
}

void store_delete(Store *s, int allocationType, int fileName)
{
    if (fileName % 100 != 0)
    {
        fileName = (fileName / 100) * 100;
    }
    if (allocationType == ALLOC_CONTIGUOUS)
    {

        int blockIndex = -1;
        int requiredBlocks = -1;
        FileEntry *fileEntry = NULL;
        //find the file entry where file is stored
        for (int i = 0; i < s->vcb->blockSize; ++i)
        {
            fileEntry = s->fileEntry + i;
            if (fileEntry->fileName == fileName)
            {
                blockIndex = fileEntry->params[0];
                requiredBlocks = fileEntry->params[1];
                break;
            }
        }

        if (fileEntry != NULL)
        {
            if (blockIndex != -1 && requiredBlocks != -1 && blockIndex < s->numBlocks)
            {
                //delete block
                Block *deleteBlock = store_getBlock(s, blockIndex);
                int totalEntries = requiredBlocks * s->fileEntrySize;

                VolumeControlBlock *deleteVcb = s->vcb;
                //free block
                vcb_freeBlock(deleteVcb, deleteBlock);
            }
            //set the file entry parameters back to 0
            fileEntry->fileName = 0;
            fileEntry->allocationType = ALLOC_CONTIGUOUS;
            fileEntry->params[0] = 0;
            fileEntry->params[1] = 0;
            printf("Deleted file %d and freed B%d \n", fileName, blockIndex);
        }
        else
        {
            printf("No file found \n");
        }
    }
    else if (allocationType == ALLOC_LINKED)
    {
        FileEntry *fe = NULL;
        int start = -1;
        int end = -1;
        //Find file entry that contains filename and set the start and end
        for (int i = 0; i < s->fileEntrySize; i++)
        {
            if (s->fileEntry[i].fileName == fileName)
            {
                fe = &s->fileEntry[i];
                start = s->fileEntry[i].params[0];
                end = s->fileEntry[i].params[1];
                break;
            }
        }
        if (start == -1)
        {
            printf("File%d not found", fileName);
        }
        else
        {
            //clear the filentry data
            fe->fileName = 0;
            fe->params[0] = 0;
            fe->params[1] = 0;
            printf("Deleted file %d and freed ", fileName);
            int blockSize = s->vcb->blockSize;
            int temp = start;
            Block *b = NULL;
            //free the blocks from start to end
            do
            {
                b = &s->blocks[temp];
                temp = s->blocks[temp].entries[blockSize - 1];
                vcb_freeBlock(s->vcb, b);
                printf("B%d ", b->index);
            } while (b->index != end);
            printf("\n");
        }
    }
    else if (allocationType == ALLOC_INDEXED)
    {
        int indexBlock = -1;
        FileEntry *fileEntry = NULL;
        int found = 0;
        for (int i = 0; i < s->vcb->blockSize; ++i)
        {
            fileEntry = s->fileEntry + i;
            //finds the filename
            if (fileEntry->fileName == fileName)
            {
                found = 1;
                indexBlock = fileEntry->params[0];
                if (indexBlock != -1)
                {
                    Block *deleteBlockIndex = store_getBlock(s, indexBlock); //get the block that contains the index
                    for (int y = 0; y < s->vcb->blockSize; y++)
                    {
                        int contentBlockIndex = deleteBlockIndex->entries[y];
                        Block *deleteBlock = store_getBlock(s, contentBlockIndex); //get the block that contais the data
                        if (contentBlockIndex == -1)
                        {
                            break;
                        }
                        else
                        {
                            vcb_freeBlock(s->vcb, deleteBlock);
                        }
                    }
                    //clearing the Volume Control Block
                    VolumeControlBlock *deleteVCB = s->vcb;
                    vcb_freeBlock(deleteVCB, deleteBlockIndex);

                    fileEntry->fileName = 0;
                    fileEntry->allocationType = ALLOC_INDEXED;
                    fileEntry->params[0] = 0;
                    fileEntry->params[1] = 0;
                    printf("Deleted file %d and freed B%d \n", fileName, indexBlock);
                }
                break;
            }
        }
        if (!found)
        {
            printf("File %d is not found!\n", fileName);
        }
    }
    else if (allocationType == ALLOC_LINKEDCONTIG)
    {
        FileEntry *fe = NULL;
        int start = -1;
        int end = -1;
        //Find file entry that contains filename and set the start and end
        for (int i = 0; i < s->fileEntrySize; i++)
        {
            if (s->fileEntry[i].fileName == fileName)
            {
                fe = &s->fileEntry[i];
                start = s->fileEntry[i].params[0];
                end = s->fileEntry[i].params[1];
                break;
            }
        }
        if (start == -1)
        {
            printf("File%d not found\n", fileName);
            return;
        }
        fe->fileName = 0;
        fe->params[0] = 0;
        fe->params[1] = 0;
        printf("Deleted file %d and freed ", fileName);
        int blockSize = s->vcb->blockSize;
        int temp = start;
        Block *b = NULL;
        do
        {
            b = &s->blocks[temp];
            if (b->entries[blockSize - 1] < fileName)
            {
                temp = b->entries[blockSize - 1];
            }
            else
            {
                temp = b->index + 1;
            }
            vcb_freeBlock(s->vcb, b);
            printf("B%d ", b->index);
        } while (b->index != end);
        printf("\n");
    }
}

void store_print(Store *store)
{
    printf("%20s%20s%20s\n", "Index", "Block", "File Data");
    printf("%20s%20s%20s\n", "0", "-", "<v.ctrl B>");
    int i, y;
    char fileEntryStr[100] = "\0";
    for (i = 0; i < store->fileEntrySize; i++)
    {
        if (store->fileEntry[i].allocationType == ALLOC_INDEXED)
        {
            sprintf(fileEntryStr, "%d,%d", store->fileEntry[i].fileName, store->fileEntry[i].params[0]);
        }
        else
        {
            sprintf(fileEntryStr, "%d,%d,%d", store->fileEntry[i].fileName, store->fileEntry[i].params[0], store->fileEntry[i].params[1]);
        }
        printf("%20d%20s%20s\n", 1 + i, "-", fileEntryStr);
    }

    for (i = 0; i < store->numBlocks; i++)
    {
        for (y = 0; y < store->vcb->blockSize; y++)
        {
            int index = store->fileEntrySize + 1 + y + i * store->vcb->blockSize;
            printf("%20d%20d%20d\n", index, store->blocks[i].index, store->blocks[i].entries[y]);
        }
    }
}

char *readCsv()
{
    FILE *f = fopen(CSV_NAME, "rb");
    // find out the number of chars of the text
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    // + 1 for null character
    char *text = malloc(sizeof(char) * (fsize + 1));
    fread(text, fsize, 1, f);
    text[fsize] = '\0';
    fclose(f);
    return text;
}

char **splitString(char *text, const char *delimiter)
{
    int resultCount = 0;
    int resultSize = 5;
    char **result = malloc(sizeof(char *) * resultSize);
    char *iterator = text;
    char *lineBreak = strstr(iterator, delimiter);
    char *line;
    size_t length;
    while (lineBreak != NULL)
    {
        length = lineBreak - iterator;
        line = malloc(sizeof(char) * (length + 1));
        strncpy(line, iterator, length);
        line[length] = '\0';
        iterator = lineBreak + 1;
        result[resultCount] = line;
        resultCount++;
        lineBreak = strstr(iterator, delimiter);

        // printf("%s\n", line);

        if (resultCount == resultSize)
        {
            resultSize = resultSize * 2;
            result = realloc(result, sizeof(char *) * resultSize);
        }
    }

    length = strlen(iterator);
    line = malloc(sizeof(char) * (length + 1));
    strncpy(line, iterator, length);
    line[length] = '\0';
    result[resultCount] = line;
    resultCount++;

    if (resultCount == resultSize)
    {
        result = realloc(result, sizeof(char *) * (resultSize + 1));
    }
    result[resultCount] = NULL;
    // printf("%s\n", line);
    // printf("end split\n");
    return result;
}

void freeSplittedString(char **splittedStr)
{
    for (int x = 0; splittedStr[x] != NULL; x++)
        free(splittedStr[x]);
    free(splittedStr);
}

int main()
{
    char *csvText = readCsv();
    // start with 5
    int instructionSize = 5;
    int instructionCount = 0;
    Instruction *instructions = calloc(instructionSize, sizeof(Instruction));

    // printf("csv content, \n%s\n", csvText);

    char **lines = splitString(csvText, "\n");
    int x;
    for (x = 0; lines[x] != NULL; x++)
    {
        if (x == instructionSize)
        {
            int tmp = instructionSize;
            instructionSize += 5;
            instructions = realloc(instructions, sizeof(Instruction) * instructionSize);
            for (; tmp < instructionSize; tmp++)
            {
                instructions[tmp].fileSize = 0;
            }
        }

        char *line = lines[x];
        char **parts = splitString(line, ",");

        char *action = strdup(parts[0]);
        int fileName = atoi(parts[1]);
        instructions[x].action = action;
        instructions[x].fileName = fileName;

        if (parts[2] == NULL)
        {
            freeSplittedString(parts);
            // skip if this is a read/delete action
            continue;
        }

        int fileSize = 0;
        for (int y = 2; parts[y] != NULL; y++)
        {
            fileSize++;
        }

        int *fileContent = malloc(sizeof(int) * fileSize);
        for (int y = 0; y < fileSize; y++)
        {
            fileContent[y] = atoi(parts[y + 2]);
        }
        instructions[x].fileSize = fileSize;
        instructions[x].fileContent = fileContent;
        // skip others
        freeSplittedString(parts);
    }
    freeSplittedString(lines);
    free(csvText);
    instructionCount = x;
    if (instructionCount == instructionSize)
    {
        instructionSize++;
        instructions = realloc(instructions, sizeof(Instruction) * instructionSize);
    }

    int block_size;
    printf("Enter block size: ");
    scanf("%d", &block_size);
    while (1)
    {
        if (block_size < 2 || block_size > 126)
        {
            if (block_size < 0)
            {
                printf("Block size cannot be a negative number.\nEnter block size: ");
                scanf("%d", &block_size);
            }
            else
            {
                printf("Block size must be more 2 and less than 126.\nEnter block size: ");
                scanf("%d", &block_size);
            }
        }
        else
        {
            break;
        }
    }
    char alloctype[4][18] = {"Contiguous", "Linked", "Indexed", "Linked Contiguous"};

    for (int i = ALLOC_CONTIGUOUS; i >= ALLOC_LINKEDCONTIG; i--)
    {
        printf("\nAllocation type: %s\n", alloctype[(-i) - 101]);

        Store *s = createStore(block_size, i);
        // printf("Block size: %d\n", s->vcb->blockSize);
        // printf("File Entries: %d\n", s->fileEntrySize);
        // printf("Blocks %d\n", s->numBlocks);
        // printf("Free Blocks %d\n", s->vcb->freeBlockNum);
        for (x = 0; x < instructionCount; x++)
        {
            printf("\naction: %s, fileName: %d \n", instructions[x].action, instructions[x].fileName);
            if (strcmp("read", instructions[x].action) == 0)
            {
                store_read(s, i, instructions[x].fileName);
            }
            else if (strcmp("delete", instructions[x].action) == 0)
            {
                store_delete(s, i, instructions[x].fileName);
            }
            else if (strcmp("add", instructions[x].action) == 0)
            {
                store_add(s, i, instructions[x].fileName, instructions[x].fileSize, instructions[x].fileContent);
                // printf("fileContent: \n");
                // for (int y = 0; y < instructions[x].fileSize; y++)
                // {
                //     printf("%d ", instructions[x].fileContent[y]);
                // }
                // printf("\n");
            }
        }

        store_print(s);

        freeStore(s);
        s = NULL;
    }
    for (x = 0; x < instructionCount; x++)
    {
        free(instructions[x].action);
        free(instructions[x].fileContent);
    }
    free(instructions);
}
