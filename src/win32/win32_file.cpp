
inline internal FILETIME
Win32GetFileWriteTime(const char* szFilepath)
{
    WIN32_FILE_ATTRIBUTE_DATA attribs;
    GetFileAttributesExA(szFilepath, GetFileExInfoStandard, (LPVOID)&attribs);
    return attribs.ftLastWriteTime;
}

internal void
Win32GetExecutablePath(win32_state& state)
{
    char filepath[WIN32_STATE_FILE_NAME_COUNT];
    DWORD dwPathSize = GetModuleFileNameA(0, filepath, WIN32_STATE_FILE_NAME_COUNT);

    string strPath = MakeString((umm)dwPathSize, filepath);
    umm pathIndex = ReverseIndexOf(strPath, '\\');
    ASSERT(pathIndex != INDEX_NOT_FOUND);

    // add 1 back to preserve the slash
    Copy(pathIndex+1, filepath, state.EXEFolder);
    Copy(strPath.size-pathIndex+1, &filepath[pathIndex+1], state.EXEFilename);
}

internal void
Win32LoadCode(win32_state& state, win32_loaded_code& code)
{
    char szSourceLibraryPath[WIN32_STATE_FILE_NAME_COUNT] = {};
    FormatString(szSourceLibraryPath, WIN32_STATE_FILE_NAME_COUNT, "%s%s", state.EXEFolder, code.pszDLLName);
    char szTempLibraryPath[WIN32_STATE_FILE_NAME_COUNT] = {};
    FormatString(szTempLibraryPath, WIN32_STATE_FILE_NAME_COUNT, "%s%s", state.EXEFolder, code.pszTransientDLLName);
    
    code.ftLastFileWriteTime = Win32GetFileWriteTime(szSourceLibraryPath);
    CopyFileA(szSourceLibraryPath, szTempLibraryPath, FALSE);
    HMODULE hLibrary = LoadLibraryA(szTempLibraryPath);
    code.hDLL = hLibrary;
    if(hLibrary)
    {
        code.isValid = true;
        for(u32 index = 0; index < code.nFunctionCount; ++index)
        {
            void* pFunc = GetProcAddress(hLibrary, code.ppszFunctionNames[index]);
            code.ppFunctions[index] = pFunc;
            
            if(!pFunc)
            {
                code.isValid = false;
            }
        }
    }
    else
    {
        code.isValid = false;
    }
}

internal void
Win32UnloadGameCode(win32_loaded_code& code)
{
    for(u32 index = 0; index < code.nFunctionCount; ++index)
    {
        code.ppFunctions[index] = 0;
        
    }
    code.isValid = false;
    FreeLibrary(code.hDLL);
    code.hDLL = 0;
    code.ftLastFileWriteTime = {};
}

internal void
Win32WriteMemoryArena(HANDLE hFile, const MemoryArena& arena)
{
    // TODO(james): handle writing out buffers larger than 32-bits
    
    DWORD dwBytesWritten = 0;
    DWORD dwArenaSize = (DWORD)arena.size;
    ASSERT(arena.size <= 0xFFFFFFFF);
    u64 freePointerOffset = (u8*)arena.freePointer - (u8*)arena.basePointer;
    WriteFile(hFile, &arena.size, sizeof(u64), &dwBytesWritten, 0);
    WriteFile(hFile, &freePointerOffset, sizeof(u64), &dwBytesWritten, 0);
    WriteFile(hFile, arena.basePointer, dwArenaSize, &dwBytesWritten, 0);
}

internal void
Win32ReadMemoryArena(HANDLE hFile, MemoryArena& arena)
{
    // TODO(james): handle reading in buffers larger than 32-bits
    
    DWORD dwBytesRead = 0;
    u64 freePointerOffset = 0;
    DWORD memorySize = 0;
    ReadFile(hFile, &memorySize, sizeof(u64), &dwBytesRead, 0);
    ReadFile(hFile, &freePointerOffset, sizeof(u64), &dwBytesRead, 0);
    ASSERT(arena.size >= (u64)memorySize); // allocated size MUST be at least bigger than what we are reading in...
    ReadFile(hFile, arena.basePointer, memorySize, &dwBytesRead, 0);

    arena.freePointer = (u8*)arena.basePointer + freePointerOffset;
}

internal platform_file
Win32OpenFile(FileLocation location, const char* filename, FileUsage usage)
{
    platform_file result { .error = 1 };
    DWORD desiredAccess = 0;
    DWORD shareMode = FILE_SHARE_READ;
    DWORD creation = OPEN_EXISTING;

    char filepath[WIN32_STATE_FILE_NAME_COUNT];
    FormatString(filepath, WIN32_STATE_FILE_NAME_COUNT, "%s%s", FileLocationsTable[(u32)location].szFolder, filename);

    if((usage & FileUsage::Read) == FileUsage::Read) { desiredAccess |= GENERIC_READ; }
    if((usage & FileUsage::Write) == FileUsage::Write) { desiredAccess |= GENERIC_WRITE; }

    if(usage == FileUsage::Write) {
        creation = CREATE_ALWAYS;
        shareMode = 0;
    }

    WIN32_FILE_ATTRIBUTE_DATA attribs;
    if(GetFileAttributesExA(filepath, GetFileExInfoStandard, (LPVOID)&attribs))
    {
        result.size = ((u64)attribs.nFileSizeHigh << 32) | (u64)attribs.nFileSizeLow; 
        HANDLE hFile = CreateFileA(filepath, desiredAccess, shareMode, 0, creation, 0, 0);

        if(hFile != INVALID_HANDLE_VALUE)
        {
            result.error = 0;
            result.platform = hFile;
        }
    }

    return result;
}

internal u64
Win32ReadFile(platform_file& file, void* buffer, u64 size)
{
    if(file.error)
    {
        ASSERT(false);
        return 0;
    }

    u64 amountRead = 0;
    u32 readSize = SafeTruncateToU32(size);
    HANDLE hFile = (HANDLE)file.platform;

    // have to loop to handle sizes too big to fit into a DWORD
    while(size > 0)
    {
        DWORD dwReadBytes = 0;

        if(ReadFile(hFile, buffer, readSize, &dwReadBytes, 0))
        {
            size -= readSize;
            amountRead += readSize;
        }
        else
        {
            break;
        }        
    }

    return amountRead;
}

internal u64
Win32WriteFile(platform_file& file, const void* buffer, u64 size)
{
    if(file.error)
    {
        ASSERT(false);
        return 0;
    }

    u64 amountWritten = 0;
    u32 writeSize = SafeTruncateToU32(size);
    HANDLE hFile = (HANDLE)file.platform;

    while(size > 0)
    {
        DWORD dwWriteBytes = 0;

        if(WriteFile(hFile, buffer, writeSize, &dwWriteBytes, 0))
        {
            size -= writeSize;
            amountWritten += dwWriteBytes;
        }
        else
        {
            break;
        }
    }

    return amountWritten;
}

internal void
Win32CloseFile(platform_file& file)
{
    if(!file.error)
    {
        CloseHandle((HANDLE)file.platform);
    }
}