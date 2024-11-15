#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <ShlObj.h>
#include <Windows.h>
#include <commdlg.h>

#undef near
#undef far

#include "opal/container/string.h"

#include "types.h"

Opal::StringUtf8 OpenFileDialog()
{
    OPENFILENAME ofn;
    char file_name[MAX_PATH] = "";
    ZeroMemory(&ofn, sizeof(ofn));

    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = NULL;  // If you have a window to center over, put its HANDLE here
    ofn.lpstrFilter = "Any File\0*.*\0";
    ofn.lpstrFile = file_name;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Select a File";
    ofn.Flags = OFN_DONTADDTORECENT | OFN_FILEMUSTEXIST;

    GetOpenFileName(&ofn);

    Opal::StringUtf8 file_name_utf8;
    file_name_utf8.Append(reinterpret_cast<const char8*>(file_name));
    return file_name_utf8;
}

Opal::StringUtf8 OpenFolderDialog()
{
    BROWSEINFO bi = {0};
    bi.lpszTitle = "Browse for folder...";
    LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
    Opal::StringUtf8 ret;
    if (pidl != 0)
    {
        // get the name of the folder
        char path[MAX_PATH];
        if (SHGetPathFromIDList(pidl, path))
        {
            ret.Append(reinterpret_cast<const char8*>(path));
        }

        // free memory used
        IMalloc* imalloc = 0;
        if (SUCCEEDED(SHGetMalloc(&imalloc)))
        {
            imalloc->Free(pidl);
            imalloc->Release();
        }
    }

    return ret;
}