#include <iostream>
#include <vector>
#include <map>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

using namespace std;

#define DISK_SIZE 256

//Convert Integer Number To Binary Base Represent By Chars Number.
void decToBinary(int n, char &c)
{
    // Array To Store Binary Number
    int binaryNum[8];

    // Counter For Binary Array
    int i = 0;
    while (n > 0)
    {
        // Storing Remainder In Binary Array
        binaryNum[i] = n % 2;
        n = n / 2;
        i++;
    }

    // Printing Binary Array In Reverse Order
    for (int j = i - 1; j >= 0; j--)
    {
        if (binaryNum[j] == 1)
            c = c | 1u << j;
    }
}

// #define SYS_CALL
// ============================================================================
class fsInode
{
    int fileSize;
    int block_in_use;
    int *directBlocks;  // Array Of Direct Block Those Pointing To DATA.
    int singleInDirect; // Contain NUMBER (Convert To Binary Requierment) That Pointing To Block With Blocks Numbers
    int num_of_direct_blocks;
    int block_size;
    int maxSize;   //File Max Size
    int maxBlocks; // File Max Blocks Available.

public:
    // Constructor
    fsInode(int _block_size, int _num_of_direct_blocks)
    {
        fileSize = 0;
        block_in_use = 0;
        block_size = _block_size;
        maxBlocks = _num_of_direct_blocks + block_size;
        maxSize = maxBlocks * block_size;
        num_of_direct_blocks = _num_of_direct_blocks;
        directBlocks = new int[num_of_direct_blocks]; // INIT array of blocks.
        assert(directBlocks);
        for (int i = 0; i < num_of_direct_blocks; i++)
        {
            directBlocks[i] = -1;
        }
        singleInDirect = -1;
    }
    // Set and Get to File Attributes.
    void SetFileSize(int size)
    {
        fileSize += size;
    }
    void SetBlockInUse(int blocks)
    {
        block_in_use += blocks;
    }
    void SetSingle(int n)
    {
        singleInDirect = n;
    }
    int GetFileSize() { return fileSize; }
    int GetBlocksInUse() { return block_in_use; }
    int *GetDirectBlock() { return directBlocks; }
    int GetMaxFileSize() { return maxSize; }
    int GetMaxBlocks() { return maxBlocks; }
    int GetSingle() { return singleInDirect; }
    int GetNumOfDirect() { return num_of_direct_blocks; }

    ~fsInode()
    {
        delete[] directBlocks;
    }
};

// ============================================================================
class FileDescriptor
{
    pair<string, fsInode *> file;
    bool inUse;

public:
    //Constructor
    FileDescriptor(string FileName, fsInode *fsi)
    {
        file.first = FileName;
        file.second = fsi;
        inUse = true;
    }
    //SET and GET to FD Attributes.
    string getFileName()
    {
        return file.first;
    }

    fsInode *getInode()
    {
        return file.second;
    }

    bool isInUse()
    {
        return (inUse);
    }
    void setInUse(bool _inUse)
    {
        inUse = _inUse;
    }
    void setFileName(string name)
    {
        file.first = name;
    }
};

#define DISK_SIM_FILE "DISK_SIM_FILE.txt"
// ============================================================================
class fsDisk
{
    FILE *sim_disk_fd;

    bool is_formated;

    // BitVector - "bit" (int) vector, indicate which block in the disk is free
    //              or not.  (i.e. if BitVector[0] == 1 , means that the
    //             first block is occupied.
    int BitVectorSize;
    int *BitVector;

    // Unix directories are lists of association structures,
    // each of which contains one filename and one inode number.
    map<string, fsInode *> MainDir;

    // OpenFileDescriptors --  when you open a file,
    // the operating system creates an entry to represent that file
    // This entry number is the file descriptor.
    vector<FileDescriptor> OpenFileDescriptors;

    //Optional:
    int direct_enteris; // Set To 3 On Our System by Defualt
    int block_size;
    int _usedBlocks; // Total used blocks on the disk.

public:
    // ------------------------------------------------------------------------
    //Init Disk.
    fsDisk()
    {
        sim_disk_fd = fopen(DISK_SIM_FILE, "r+");
        assert(sim_disk_fd);
        for (int i = 0; i < DISK_SIZE; i++)
        {
            int ret_val = fseek(sim_disk_fd, i, SEEK_SET);
            ret_val = fwrite("\0", 1, 1, sim_disk_fd);
            assert(ret_val == 1);
        }
        is_formated = false;
        fflush(sim_disk_fd);
    }

    // ------------------------------------------------------------------------
    //Print File List And Disk Contents.
    void listAll()
    {
        int i = 0;
        for (auto it = begin(OpenFileDescriptors); it != end(OpenFileDescriptors); ++it)
        {
            cout << "index: " << i << ": FileName: " << it->getFileName() << " , isInUse: " << it->isInUse() << endl;
            i++;
        }
        char bufy;
        cout << "Disk content: '";
        for (i = 0; i < DISK_SIZE; i++)
        {
            int ret_val = fseek(sim_disk_fd, i, SEEK_SET);
            ret_val = fread(&bufy, 1, 1, sim_disk_fd);
            cout << bufy;
        }
        cout << "'" << endl;
    }

    // ------------------------------------------------------------------------
    // Format the disk.
    void fsFormat(int blockSize = 4, int direct_Enteris_ = 3)
    {
        //If Disk Already Formatted - Return.
        if (is_formated)
        {
            cout << "Disk Has Been Formatted Already." << endl;
            return;
        }

        is_formated = true;

        //Init Parameters.
        block_size = blockSize;
        direct_enteris = direct_Enteris_;
        BitVectorSize = DISK_SIZE / block_size;
        _usedBlocks = 0;

        //Init BitVector.
        BitVector = new int[BitVectorSize];
        assert(BitVector);
        for (size_t i = 0; i < BitVectorSize; i++)
            BitVector[i] = 0; // All blocks are unused.

        cout << "Format Disk: number of blocks: " << BitVectorSize << endl;
    }

    // ------------------------------------------------------------------------
    //Create New File. Return File Descriptor Number.
    int CreateFile(string fileName)
    {
         if (!is_formated)
        {
            cout << "Disk Has Not Been Formatted Yet." << endl;
            return -1;
        }
        //Check if File Name Already Exist.
        if (MainDir.find(fileName) != MainDir.end())
        {
            cout << "File Name Already Exist." << endl;
            return -1;
        }
        // Not Enough Blocks To Allocate. 
        if((BitVectorSize - _usedBlocks) < 1 ){
            cout<< "Not Enough Blocks To Allocate."<<endl;
            return -1;
        }

        //Create fsInode
        fsInode *fsi = new fsInode(block_size, direct_enteris);
        assert(fsi);

        //Create File Descriptor
        FileDescriptor fd(fileName, fsi);

        //Update MainDir - Create New Pair Of <fileName, fsi> On The Map
        MainDir.insert(make_pair(fileName, fsi));

        //Update FD Table - Insert New FD Into Vector.
        OpenFileDescriptors.push_back(fd);

        int _fd = OpenFileDescriptors.size() - 1;

        return _fd;
    }

    // ------------------------------------------------------------------------
    // Open Closed File Only. Return File Descriptor Number.
    int OpenFile(string fileName)
    {
         if (!is_formated)
        {
            cout << "Disk Has Not Been Formatted Yet." << endl;
            return -1;
        }

        //Find The File Descriptor And Check If Not Open.
        for (auto it = begin(OpenFileDescriptors); it != end(OpenFileDescriptors); ++it)
        {
            if (fileName.compare(it->getFileName()) == 0)
            {
                if (it->isInUse() == false)
                {
                    it->setInUse(true);
                    //The Distance Bitween Begin And It Is That Location Of It.
                    return distance(OpenFileDescriptors.begin(), it);
                }
                cout << "File Has Been Opened Already." << endl;
                break;
            }
        }
        return -1;
    }

    // ------------------------------------------------------------------------
    // Close Open File Only. Return File Name If Succeed.
    string CloseFile(int fd)
    {
          if (!is_formated)
        {
            cout << "Disk Has Not Been Formatted Yet." << endl;
            return "-1";
        }
        // File with this FD Not Create Before.
        if (fd > OpenFileDescriptors.size())
        {
            cout << "File Has Not Been Created Yet." << endl;
            return "-1";
        }
        //File Is Open- Close It.
        if (OpenFileDescriptors[fd].isInUse() == true)
            OpenFileDescriptors[fd].setInUse(false);
        else{
            //File Already Close.
            cout<<"File Already Closed."<<endl;
            return "-1"; 
        }

        return OpenFileDescriptors[fd].getFileName();
    }

    // ------------------------------------------------------------------------
    // Write LEN Char of *BUF into FD.
    // This Function Write Data Into The Disk, Allocate New Blocks If Needed.
    // File Can Be Write Into Direct Blocks Immediatly, Or Write By Using Single Block.
    // Single InDirect Block Can Contain More SizeBlock Blocks.
    // Blocks Only Allocate When Required, To Fill the Demand Of Minimize Number Of Block Per File.
    int WriteToFile(int fd, char *buf, int len)
    {
        //If Disk Has Not Been Formatted OR FD Not Exist OR File is Close.
        if (!is_formated)
        {
            cout << "Disk Has Not Been Formatted Yet." << endl;
            return -1;
        }
        //FD Not Exist At BitVector
        if (fd >= OpenFileDescriptors.size())
        {
            cout << "File Has Not Been Created Yet." << endl;
            return -1;
        }
        //File Closed - Can Not Write.
        if (OpenFileDescriptors[fd].isInUse() == false)
        {
            cout << "File Has Not Been Opened Yet." << endl;
            return -1;
        }

        fsInode *fsi = OpenFileDescriptors[fd].getInode();

        // Check If There More Place To Insert The Data Into the File.
        // This Test Cover The Case Of FILE Itself Have Not Enough More Blocks To Allocate
        if (len > (fsi->GetMaxFileSize() - fsi->GetFileSize())){
            cout<< "Not Enough Free Space On The File."<<endl;
            return -1;
        }

        int *arr = fsi->GetDirectBlock();

        //Check If There Is A Space In The Allocated Blocks.
        int _free_space = fsInodeBlocksUsed(fd);
        int rest_char = len - _free_space;
        int blocks_req;

        if (rest_char > 0)
        {
            //Calculate How Many Blocks Requierds.
            blocks_req = rest_char / block_size;
            if ((rest_char % block_size) != 0)
                blocks_req++;

            //Check If There Enough Blocks On The Disk To Be Allocate
            if ((blocks_req > (BitVectorSize - _usedBlocks)) ||
                (blocks_req > fsi->GetMaxBlocks() - fsi->GetBlocksInUse())){
                cout<<"Not Enough Free Space On The Disk."<< endl;
                return -1;
                }
                

            //Check If Single Indirect Required.
            if ((blocks_req + fsi->GetBlocksInUse()) > direct_enteris)
                if (fsi->GetSingle() == -1)
                    fsi->SetSingle(allocateBlocks());
        }
        int _min_size = _free_space;
        if (_free_space != 0)
        { //Check If Indirect Blocks Required.
            if (len < _free_space)
                _min_size = len;

            if (fsi->GetBlocksInUse() > direct_enteris)
            {
                // Single Indirect Block.
                //Calculate The INDEX Of The Block At The Block Of Single Indirect.
                int block_ind = fsi->GetBlocksInUse() - direct_enteris;
                //Get The Char Represent Block Number.
                char c;
                int ret_val = readDisk((fsi->GetSingle() * block_size) + block_ind - 1, 1, 1, &c); //-1 #####
                int n = (int)c;
                //Write Into The Disk
                ret_val = writeDisk(n * block_size + (block_size - _free_space), buf, 1, _min_size);
            }
            else //Direct Block.
            {
                //Calculate The Index On The Disk To Be Written, And Write Into It.
                int _block_ind = arr[fsi->GetBlocksInUse() - 1] * block_size + (block_size - _free_space);
                int ret_val = writeDisk(_block_ind, buf, 1, _min_size);
            }
            fsi->SetFileSize(_min_size);
        }
        // Check If All The User Chars Has Been Inserted Into The Free Space Those Already Allocated.
        if (rest_char <= 0)
        {
            memset(buf, '\0', len);
            return 1;
        }

        //Fill the Blocks.
        for (size_t i = 0, k = 0; i < blocks_req; i++)
        {
            //Check If Single Indirect Required.
            if (fsi->GetBlocksInUse() >= direct_enteris)
            {
                //Convert Allocate Block Number To Char
                char c = '\0';
                int n = allocateBlocks();
                decToBinary(n, c);

                //Write The Converted Char To Single Block.
                int ind = (fsi->GetSingle() * block_size) + fsi->GetBlocksInUse() - fsi->GetNumOfDirect(); // -1?
                int ret_val = writeDisk(ind, &c, 1, 1);

                for (size_t j = 0; j < block_size; j++)
                {
                    //Write Data To InDirect Block ### CHECK THIS LINE!!! ####
                    ret_val = writeDisk((n * block_size) + j, buf + _free_space + j + (i * block_size), 1, 1);
                    rest_char--;
                    // No More Chars To Be Insert Into The Disk.
                    if (rest_char == 0)
                        break;
                }
            }
            //Only Direct Blocks Needed For Now.
            else
            { //Allocate New Direct Block.
                arr[fsi->GetBlocksInUse()] = allocateBlocks();

                //Write Data Into The Block.
                for (size_t j = 0; j < block_size; j++)
                {
                    int ret_val = writeDisk((arr[fsi->GetBlocksInUse()] * block_size) + j, buf + _free_space + j + (i * block_size), 1, 1);
                    rest_char--;
                    if (rest_char == 0)
                        break;
                }
            }
            //Increase File BlockInUsed Number By 1.
            fsi->SetBlockInUse(1);
        }
        //Increase File Size By LEN.
        fsi->SetFileSize(len - _min_size);
        //Clear BUF To Avoid
        memset(buf, '\0', len);
    }
    // ------------------------------------------------------------------------
    //Delete File From Disk.
    int DelFile(string FileName)
    {
          if (!is_formated)
        {
            cout << "Disk Has Not Been Formatted Yet." << endl;
            return -1;
        }
       
        //Find The File By Name.
        auto fd = OpenFileDescriptors.begin();
        while (fd != OpenFileDescriptors.end())
        {
            if (FileName.compare(fd->getFileName()) == 0)
                break;
            fd++;
        }
        // If The File Is Open - Delete Unallowed.
        if (fd->isInUse()){
            cout<< "File Still Open. Close It And Try Again."<<endl;
            return -1;
        }
        
        //Init Parameters.
        int res = distance(OpenFileDescriptors.begin(), fd);
        int *arr = fd->getInode()->GetDirectBlock();
        int file_size = fd->getInode()->GetFileSize();
        char c = '\0', zero = '\0';
        bool flag = false;
        int single = fd->getInode()->GetSingle();
        int ind = 0, ret_val = 0, block = 0;

        //Delete Data From Both Direct and Indirect blocks.
        for (size_t i = 0, j = 0, k = 0; i < file_size; i++)
        {
            if ((direct_enteris * block_size) <= i)
            {   // Single InDirect Blocks.
                if (!flag) //First Single InDirect Block.
                {
                    k = 0;
                    flag = true;
                }
                ind = (single * block_size) + k;
                ret_val = readDisk(ind, 1, 1, &c);
                block = (int)c;
                ret_val = writeDisk(block * block_size + j, &zero, 1, 1);
            }
            else //Direct Blocks.
                ret_val = writeDisk((arr[k] * block_size) + j, &zero, 1, 1);
            j++;
            if (j == block_size)
            {
                if (flag)
                {
                    //Delete InDirect Block.
                    ret_val = writeDisk(ind, &zero, 1, 1);
                    BitVector[block] = 0;
                }
                //Set BitVector Block As Assingable.
                else
                    BitVector[arr[k]] = 0;

                j = 0;
                k++;
            }
        }
        if (flag)
            BitVector[single] = 0;
      
        //Delete Name From Vector.
        fd->setFileName("\0");
        //Decrease Disk Total Used Blocks.
        _usedBlocks -= fd->getInode()->GetBlocksInUse();
        fd->setInUse(false);

        //Delete From MainDir (Map).
        auto map = MainDir.find(FileName);
        delete map->second;
        MainDir.erase(map);

        return res;
    }
    // ------------------------------------------------------------------------
    //Read LEN Chars From File FD. If LEN > File Size Return The File Only.
    int ReadFromFile(int fd, char *buf, int len)
    {
        memset(buf, 0, 256);
           if (!is_formated)
        {
            cout << "Disk Has Not Been Formatted Yet." << endl;
            return -1;
        }
        if (fd > OpenFileDescriptors.size())
        {
            cout << "File Has Not Been Created Yet." << endl;
            return -1;
        }
        if (OpenFileDescriptors[fd].isInUse() == false)
        {
            cout << "File Has Not Been Opened Yet." << endl;
            return -1;
        }

        fsInode *fsi = OpenFileDescriptors[fd].getInode();
        int *arr = fsi->GetDirectBlock();
        int single = fsi->GetSingle();
        bool flag = false;
        int _size = len;
        
        if (fsi->GetFileSize() < len)
            _size = fsi->GetFileSize();

        for (size_t i = 0, j = 0, k = 0; i < _size; i++)
        {
            if ((direct_enteris * block_size) <= i)
            { // Single Indirect.
                if (!flag)
                {
                    k = 0;
                    flag = true;
                }
                int ind = (single * block_size) + k;
                char c;
                int ret_val = readDisk(ind, 1, 1, &c);
                int block = (int)c;
                ret_val = readDisk(block * block_size + j, 1, 1, buf + i);
            }
            else
                int ret_val = readDisk((arr[k] * block_size) + j, 1, 1, buf + i);
            j++;
            if (j == block_size)
            {
                j = 0;
                k++;
            }
        }
        return 1;
    }

    //Private Help Functions.

    //Calculate How Many Free Space In The Allocated Blocks.
    int fsInodeBlocksUsed(int fd)
    {
        fsInode *fsi = OpenFileDescriptors[fd].getInode();

        int res = (fsi->GetBlocksInUse() * block_size) - fsi->GetFileSize();

        return res;
    }

    //Find Unused Block And Allocate It.
    int allocateBlocks()
    {
        for (size_t i = 0; i < BitVectorSize; i++)
        {
            if (BitVector[i] == 0)
            {
                BitVector[i] = 1;
                _usedBlocks++;
                return i;
            }
        }
        return -1;
    }

    //Read From Disk
    int readDisk(int _ind, int __size, int __n, char *buf)
    {
        int ret_val = fseek(sim_disk_fd, _ind, SEEK_SET);
        ret_val = fread(buf, __size, __n, sim_disk_fd);
        return ret_val;
    }
    //Write To Disk.
    int writeDisk(int ind, char *buf, int __size, int __n)
    {
        int ret_val = fseek(sim_disk_fd, ind, SEEK_SET);
        ret_val = fwrite(buf, __size, __n, sim_disk_fd);
        return ret_val;
    }
    ~fsDisk(){

        for (auto it = MainDir.begin(); it != MainDir.end(); it++)
            delete it->second;

        MainDir.clear();
        OpenFileDescriptors.clear();
        delete[] BitVector;
        fclose(sim_disk_fd);
      
    }
};

int main()
{
    int blockSize;
    int direct_entries;
    string fileName;
    char str_to_write[DISK_SIZE];
    char str_to_read[DISK_SIZE];
    int size_to_read;
    int _fd;

    fsDisk *fs = new fsDisk();
    int cmd_;
    while (1)
    {
        cin >> cmd_;
        switch (cmd_)
        {
        case 0: // exit
            delete fs;
            exit(0);
            break;

        case 1: // list-file
            fs->listAll();
            break;

        case 2: // format
            cin >> blockSize;
            cin >> direct_entries;
            fs->fsFormat(blockSize, direct_entries);
            break;

        case 3: // creat-file
            cin >> fileName;
            _fd = fs->CreateFile(fileName);
            cout << "CreateFile: " << fileName << " with File Descriptor #: " << _fd << endl;
            break;

        case 4: // open-file
            cin >> fileName;
            _fd = fs->OpenFile(fileName);
            cout << "OpenFile: " << fileName << " with File Descriptor #: " << _fd << endl;
            break;

        case 5: // close-file
            cin >> _fd;
            fileName = fs->CloseFile(_fd);
            cout << "CloseFile: " << fileName << " with File Descriptor #: " << _fd << endl;
            break;

        case 6: // write-file
            cin >> _fd;
            cin >> str_to_write;
            fs->WriteToFile(_fd, str_to_write, strlen(str_to_write));
            break;

        case 7: // read-file
            cin >> _fd;
            cin >> size_to_read;
            fs->ReadFromFile(_fd, str_to_read, size_to_read);
            cout << "ReadFromFile: " << str_to_read << endl;
            break;

        case 8: // delete file
            cin >> fileName;
            _fd = fs->DelFile(fileName);
            cout << "DeletedFile: " << fileName << " with File Descriptor #: " << _fd << endl;
            break;
        default:
            break;
        }
    }
}