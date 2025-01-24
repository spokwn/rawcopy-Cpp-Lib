#define UNICODE
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <winioctl.h>
#include <string>
typedef struct {
    ULONG Type;
    USHORT UsaOffset;
    USHORT UsaCount;
    ULONGLONG Lsn;
} NTFS_RECORD_HEADER, * PNTFS_RECORD_HEADER;
typedef struct {
    NTFS_RECORD_HEADER RecHdr;
    USHORT SequenceNumber;
    USHORT LinkCount;
    USHORT AttributeOffset;
    USHORT Flags;
    ULONG BytesInUse;
    ULONG BytesAllocated;
    ULONGLONG BaseFileRecord;
    USHORT NextAttributeNumber;
    USHORT Pading;
    ULONG MFTRecordNumber;
    USHORT UpdateSeqNum;
} FILE_RECORD_HEADER, * PFILE_RECORD_HEADER;
typedef enum {
    AttributeStandardInformation = 0x10,
    AttributeAttributeList = 0x20,
    AttributeFileName = 0x30,
    AttributeObjectId = 0x40,
    AttributeSecurityDescriptor = 0x50,
    AttributeVolumeName = 0x60,
    AttributeVolumeInformation = 0x70,
    AttributeData = 0x80,
    AttributeIndexRoot = 0x90,
    AttributeIndexAllocation = 0xA0,
    AttributeBitmap = 0xB0,
    AttributeReparsePoint = 0xC0,
    AttributeEAInformation = 0xD0,
    AttributeEA = 0xE0,
    AttributePropertySet = 0xF0,
    AttributreLoggedUtilityStream = 0x100
} ATTRIBUTE_TYPE, * PATTRIBUTE_TYPE;
typedef struct {
    ATTRIBUTE_TYPE AttributeType;
    ULONG Length;
    BOOLEAN Nonresident;
    UCHAR NameLength;
    USHORT NameOffset;
    USHORT Flags;
    USHORT AttributeNumber;
} ATTRIBUTE, * PATTRIBUTE;
typedef struct {
    ATTRIBUTE Attribute;
    ULONG ValueLength;
    USHORT ValueOffset;
    UCHAR Flags;
} RESIDENT_ATTRIBUTE, * PRESIDENT_ATTRIBUTE;
inline unsigned long RawCopy(std::wstring src, std::wstring dst)
{
    unsigned long result = ERROR_SUCCESS;
    HANDLE hFile = CreateFile(src.c_str(), FILE_READ_ATTRIBUTES,
        (FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE),
        NULL, OPEN_EXISTING, 0, NULL);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        std::wstring szDrive = src.substr(0, 2);
        DWORD dwSectorPerCluster;
        DWORD dwBytesPerSector;
        if (GetDiskFreeSpace(szDrive.c_str(), &dwSectorPerCluster, &dwBytesPerSector, NULL, NULL))
        {
            LARGE_INTEGER liFileSize;
            DWORD dwRead, dwWritten;
            DWORD dwClusterSize = (dwBytesPerSector * dwSectorPerCluster);
            liFileSize.LowPart = GetFileSize(hFile, (LPDWORD)&liFileSize.HighPart);
            DWORD dwClusters = (DWORD)(liFileSize.QuadPart / dwClusterSize);
            DWORD dwPointsSize = sizeof(RETRIEVAL_POINTERS_BUFFER) +
                (dwClusters * (sizeof(LARGE_INTEGER) * 2));
            PRETRIEVAL_POINTERS_BUFFER pPoints =
                (PRETRIEVAL_POINTERS_BUFFER) new BYTE[dwPointsSize];
            STARTING_VCN_INPUT_BUFFER vcnStart = { 0 };
            if (DeviceIoControl(hFile,
                FSCTL_GET_RETRIEVAL_POINTERS,
                &vcnStart,
                sizeof(vcnStart),
                pPoints,
                dwPointsSize,
                &dwWritten,
                NULL))
            {
                szDrive = L"\\\\.\\" + szDrive;
                HANDLE hDrive = CreateFile(szDrive.c_str(),
                    GENERIC_READ,
                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                    NULL,
                    OPEN_EXISTING,
                    0,
                    NULL);
                if (hDrive != INVALID_HANDLE_VALUE)
                {
                    HANDLE hDst = CreateFile(dst.c_str(),
                        GENERIC_WRITE,
                        0,
                        NULL,
                        CREATE_NEW,
                        0,
                        NULL);
                    if (hDst != INVALID_HANDLE_VALUE)
                    {
                        if (SetFilePointer(hDst,
                            liFileSize.LowPart,
                            &liFileSize.HighPart,
                            FILE_BEGIN) != INVALID_SET_FILE_POINTER)
                        {
                            if (SetEndOfFile(hDst))
                            {
                                const DWORD CHUNK_CLUSTERS = 1024;
                                const DWORD MAX_CHUNK_SIZE = CHUNK_CLUSTERS * dwClusterSize;
                                LPBYTE lpChunkBuffer = new BYTE[MAX_CHUNK_SIZE];
                                LARGE_INTEGER vcnPrev = pPoints->StartingVcn;
                                for (DWORD dwExtent = 0; dwExtent < pPoints->ExtentCount; dwExtent++)
                                {
                                    LARGE_INTEGER liSrcPos;
                                    LARGE_INTEGER liDstPos;
                                    DWORD dwLength = (DWORD)(pPoints->Extents[dwExtent].NextVcn.QuadPart -
                                        vcnPrev.QuadPart);
                                    liSrcPos.QuadPart = pPoints->Extents[dwExtent].Lcn.QuadPart *
                                        dwClusterSize;
                                    liDstPos.QuadPart = vcnPrev.QuadPart * dwClusterSize;
                                    DWORD clustersRemaining = dwLength;
                                    while (clustersRemaining > 0)
                                    {
                                        DWORD currentChunkClusters = (clustersRemaining > CHUNK_CLUSTERS)
                                            ? CHUNK_CLUSTERS
                                            : clustersRemaining;
                                        DWORD bytesToRead = currentChunkClusters * dwClusterSize;
                                        if (SetFilePointer(hDrive,
                                            liSrcPos.LowPart,
                                            &liSrcPos.HighPart,
                                            FILE_BEGIN) == INVALID_SET_FILE_POINTER
                                            && GetLastError() != NO_ERROR)
                                        {
                                            result = GetLastError();
                                            break;
                                        }
                                        if (!ReadFile(hDrive,
                                            lpChunkBuffer,
                                            bytesToRead,
                                            &dwRead,
                                            NULL))
                                        {
                                            result = GetLastError();
                                            break;
                                        }
                                        if (SetFilePointer(hDst,
                                            liDstPos.LowPart,
                                            &liDstPos.HighPart,
                                            FILE_BEGIN) == INVALID_SET_FILE_POINTER
                                            && GetLastError() != NO_ERROR)
                                        {
                                            result = GetLastError();
                                            break;
                                        }
                                        if (!WriteFile(hDst,
                                            lpChunkBuffer,
                                            dwRead,
                                            &dwWritten,
                                            NULL))
                                        {
                                            result = GetLastError();
                                            break;
                                        }
                                        liSrcPos.QuadPart += dwRead;
                                        liDstPos.QuadPart += dwRead;
                                        clustersRemaining -= currentChunkClusters;
                                        if (result != ERROR_SUCCESS)
                                            break;
                                    }
                                    if (result != ERROR_SUCCESS)
                                        break;
                                    vcnPrev = pPoints->Extents[dwExtent].NextVcn;
                                }
                                delete[] lpChunkBuffer;
                            }
                            else
                            {
                                result = GetLastError();
                            }
                        }
                        else
                        {
                            result = GetLastError();
                        }
                        CloseHandle(hDst);
                    }
                    else
                    {
                        result = GetLastError();
                    }
                    CloseHandle(hDrive);
                }
                else
                {
                    result = GetLastError();
                }
            }
            else
            {
                result = GetLastError();
                if (result == ERROR_HANDLE_EOF)
                {
                }
            }
            delete[] pPoints;
        }
        else
        {
            result = GetLastError();
        }
        CloseHandle(hFile);
    }
    else
    {
        result = GetLastError();
    }
    return result;
}