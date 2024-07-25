#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>

#include "rndr/core/containers/string.h"

Rndr::String OpenFileDialog()
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
    return file_name;
}

Rndr::String OpenFolderDialog()
{
    BROWSEINFO bi = { 0 };
    bi.lpszTitle = "Browse for folder...";
    LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
    Rndr::String ret;
    if (pidl != 0)
    {
        // get the name of the folder
        char path[MAX_PATH];
        if (SHGetPathFromIDList(pidl, path))
        {
            ret = path;
        }

        // free memory used
        IMalloc *imalloc = 0;
        if (SUCCEEDED(SHGetMalloc(&imalloc)))
        {
            imalloc->Free(pidl);
            imalloc->Release();
        }
    }

    return ret;
}