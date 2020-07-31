// ConsoleApplication8.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//


#include <iostream>
#include <fstream>
#include <Windows.h>
#include <iomanip>
#include <string>
using namespace std;

#define IN
#define OUT




//根据文件路径，返回文件大小
DWORD getFileSize(IN const CHAR* Filepath)
{
    //将ascii转为unicode
    WCHAR wszClassName[256];
    MultiByteToWideChar(CP_ACP, 0, Filepath, strlen(Filepath) + 1, wszClassName,
        sizeof(wszClassName) / sizeof(wszClassName[0]));
    //创建文件对象
    HANDLE hFile = CreateFile(wszClassName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    //判断文件是否创建成功
    if (INVALID_HANDLE_VALUE == hFile)
    {
        //文件创建失败,返回0
        return 0;
    }
    DWORD dwFileSize = 0;
    //获得文件大小
    dwFileSize = GetFileSize(hFile, NULL);
    //关闭文件句柄
    CloseHandle(hFile);
    //文件创建成功，返回文件大小
    return dwFileSize;
}

//将文件的信息读入缓冲区，
//filepath 文件路径
//pBuffer  指向缓冲区的指针
//BufferSize 缓冲区大小
BOOL readFileToBuffer(IN const CHAR* Filepath,OUT BYTE* &pBuffer,OUT int &BufferSize)
{
    //获取文件大小
    int size = getFileSize(Filepath);
    //判断文件是否存在
    if (size == 0)
    {
        //文件不存在
        cout << "打开文件失败";
        return false;
    }
    //申请缓冲区
    PBYTE l_byte = (PBYTE)malloc(size);
    ifstream l_FileIfstream;
    l_FileIfstream.open(Filepath, ios::binary);
    l_FileIfstream.seekg(0);
    l_FileIfstream.read((CHAR *)l_byte, size);
    l_FileIfstream.close();
    //使指针指向缓冲区
    pBuffer = (PBYTE)l_byte;
    //获取缓冲区的大小
    BufferSize = size;
    return true;
}

//将缓冲区的数据转为PE文件
//pBuffer 缓冲区指针
//Buffersize 缓冲区的大小
//FilePath 改写文件路径
BOOL  BufferToFile(IN const CHAR* FilePath, IN BYTE* pBuffer, IN int Buffersize) 
{
    //获取文件大小
    int size = getFileSize(FilePath);
    //判断文件是否存在
    if (size == 0)
    {
        //文件不存在
        cout << "打开文件失败";
        return false;
    }

    //判断是否为空指针
    if (pBuffer == NULL) 
    {
        return false;
    }
    ofstream l_FileIfstream;
    //改写文件
    l_FileIfstream.open(FilePath, ios::binary | ios::out );
    l_FileIfstream.seekp(0,ios::beg);
    l_FileIfstream.write((char *)pBuffer,Buffersize);
    l_FileIfstream.close();
    return true;
}



//添加节区
//pBuffer 缓冲区的指针
//pNewBuffer 新缓冲区的指针
//BufferSize 缓冲区的大小
//NewBufferSize 新缓冲区的大小
//AddSize  增加的节区的大小
void AddSection(IN PBYTE pBuffer, OUT PBYTE &pNewBuffer ,IN int BufferSize,OUT int &NewBufferSize,IN int AddSize) 
{
    //申请 原空间大小加新增加的节区大小的内存空间
    PBYTE l_pByte = (PBYTE)malloc(BufferSize+AddSize);
    //初始化内存空间
    memset((void *)l_pByte, 0, BufferSize + AddSize);

    //将原信息复制到新的内存空间
    memcpy(l_pByte, pBuffer, BufferSize);

    //修改FILE头的节区数量
    PIMAGE_DOS_HEADER l_pDosHeader = (PIMAGE_DOS_HEADER)(l_pByte);
    PIMAGE_FILE_HEADER l_pFileHeader = (PIMAGE_FILE_HEADER)(l_pByte + l_pDosHeader->e_lfanew+0x4);
    //节区数量加 1
    l_pFileHeader->NumberOfSections += 1;
    PIMAGE_OPTIONAL_HEADER l_pOptional = (PIMAGE_OPTIONAL_HEADER)(l_pByte + l_pDosHeader->e_lfanew+0x18);
    

    PIMAGE_SECTION_HEADER l_pSection = (PIMAGE_SECTION_HEADER)(l_pByte + l_pDosHeader->e_lfanew + 0x18 + l_pFileHeader->SizeOfOptionalHeader);
    //判断是否有空间加入一个节区头
    if ((l_pOptional->SizeOfHeaders - (l_pDosHeader->e_lfanew + 0x18 + l_pFileHeader->SizeOfOptionalHeader + 40 * (l_pFileHeader->NumberOfSections-1))) < 80) 
    {
        cout << "文件空间不够";
        return;
    }
    
    //对节区头进行赋值
    l_pSection = l_pSection + l_pFileHeader->NumberOfSections - 1;
    //对节区的名称赋值
    char name[8] = { 'n','e','w','\0','\0','\0','\0','\0' };
    memcpy(l_pSection->Name, name, 8);
    //设置节区对齐前的内存大小属性
    l_pSection->Misc.VirtualSize =AddSize;
    //设置节区的内存偏移
    l_pSection->VirtualAddress = l_pOptional->SizeOfImage;
    //设置节区对应文件的大小
    l_pSection->SizeOfRawData = AddSize;
    //设置节区对应的文件偏移
    l_pSection->PointerToRawData = BufferSize;
    //设置节区的读写权限
    l_pSection->Characteristics = 0x60000020;
    //改变映像大小
    l_pOptional->SizeOfImage += AddSize;

    pNewBuffer = l_pByte;
    NewBufferSize = BufferSize + AddSize;

}


int main()
{

    string str;
    char Filepath[256];//最大路径长度256个字节
    int index;
    cout << "输入PE文件路径:";
    getline(cin, str);
    strcpy_s(Filepath, str.c_str());

    //C:\\Users\\admin\\Desktop\\ipmsg.exe
    //没有添加节区前的缓冲区
    PBYTE l_Buffer = (PBYTE)malloc(sizeof(BYTE));
    //添加节区后的缓冲区
    PBYTE l_NewBuffer = (PBYTE)malloc(sizeof(BYTE));
    //缓冲区的大小
    int BufferSize,NewBufferSize;
    //将PE读入缓冲区
    if (readFileToBuffer(Filepath, l_Buffer, BufferSize) == false) 
    {
        cout << "文件路径错误";
        return 0;
    }
    //增加一个节区,写入节区大小为0x1000 节区的大小为0x1000的整数倍
    AddSection(l_Buffer, l_NewBuffer, BufferSize, NewBufferSize,0x1000);
    //写回原文件
    BufferToFile(Filepath, l_NewBuffer, NewBufferSize);
    
    free(l_Buffer);
    free(l_NewBuffer);
}