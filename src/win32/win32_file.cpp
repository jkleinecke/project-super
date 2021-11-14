

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

    // add 1 back to preserve the forward slash
    Copy(pathIndex+1, filepath, state.EXEFolder);
    Copy(strPath.size-pathIndex+1, &filepath[pathIndex+1], state.EXEFilename);
}

internal void
Win32LoadCode(win32_state& state, win32_loaded_code& code)
{
    char szSourceLibraryPath[WIN32_STATE_FILE_NAME_COUNT] = {};
    FormatString(WIN32_STATE_FILE_NAME_COUNT, szSourceLibraryPath, "%s%s", state.EXEFolder, code.pszDLLName);
    char szTempLibraryPath[WIN32_STATE_FILE_NAME_COUNT] = {};
    FormatString(WIN32_STATE_FILE_NAME_COUNT, szTempLibraryPath, "%s%s", state.EXEFolder, code.pszTransientDLLName);
    
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