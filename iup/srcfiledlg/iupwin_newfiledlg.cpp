/** \file
 * \brief IupFileDlg pre-defined dialog
 *
 * See Copyright Notice in "iup.h"
 */

#define UNICODE

#include <windows.h>      // For common windows data types and function headers
#include <shobjidl.h>     // for IFileDialogEvents and IFileDialogControlEvents
#include <shlwapi.h>
#include <new>
#include <stdio.h>

#include "iup.h"
#include "iupcbs.h"
#include "iupfiledlg.h"

#include "iup_object.h"
#include "iup_attrib.h"
#include "iup_str.h"
#include "iup_array.h"
#include "iup_dialog.h"
#include "iup_drvinfo.h"
#include "iup_key.h"
#include "iup_register.h"

#include "iupwin_str.h"


enum { IUP_DIALOGOPEN, IUP_DIALOGSAVE, IUP_DIALOGDIR };


class winNewFileDlgEventHandler : public IFileDialogEvents, public IFileDialogControlEvents
{
public:
  // IUnknown methods
  IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv)
  {
    static const QITAB qit[] = {
      QITABENT(winNewFileDlgEventHandler, IFileDialogEvents),
      QITABENT(winNewFileDlgEventHandler, IFileDialogControlEvents),
      { 0 },
    };
    return QISearch(this, qit, riid, ppv);
  }

  IFACEMETHODIMP_(ULONG) AddRef()
  {
    return InterlockedIncrement(&_cRef);
  }

  IFACEMETHODIMP_(ULONG) Release()
  {
    long cRef = InterlockedDecrement(&_cRef);
    if (!cRef)
      delete this;
    return cRef;
  }

  // IFileDialogEvents methods
  IFACEMETHODIMP OnFileOk(IFileDialog *);
  IFACEMETHODIMP OnFolderChange(IFileDialog *) { return S_OK; };
  IFACEMETHODIMP OnFolderChanging(IFileDialog *, IShellItem *) { return S_OK; };
  IFACEMETHODIMP OnSelectionChange(IFileDialog *);
  IFACEMETHODIMP OnShareViolation(IFileDialog *, IShellItem *, FDE_SHAREVIOLATION_RESPONSE *) { return S_OK; };
  IFACEMETHODIMP OnTypeChange(IFileDialog *pfd);
  IFACEMETHODIMP OnOverwrite(IFileDialog *, IShellItem *, FDE_OVERWRITE_RESPONSE *) { return S_OK; };

  // IFileDialogControlEvents methods
  IFACEMETHODIMP OnItemSelected(IFileDialogCustomize*, DWORD, DWORD) { return S_OK; }
  IFACEMETHODIMP OnButtonClicked(IFileDialogCustomize *, DWORD) { return S_OK; };
  IFACEMETHODIMP OnCheckButtonToggled(IFileDialogCustomize *, DWORD, BOOL) { return S_OK; };
  IFACEMETHODIMP OnControlActivating(IFileDialogCustomize *, DWORD) { return S_OK; };

  winNewFileDlgEventHandler(Ihandle* _ih) : _cRef(1), ih(_ih) {};

private:
  ~winNewFileDlgEventHandler() { };
  Ihandle *ih;
  long _cRef;
};

IFACEMETHODIMP winNewFileDlgEventHandler::OnFileOk(IFileDialog *pfd)
{
  char * filename = NULL;
  int ret;
  IFnss cb = (IFnss)IupGetCallback(ih, "FILE_CB");
  if (cb)
  {
    IFileOpenDialog *pfod;
    HRESULT hr = pfd->QueryInterface(IID_PPV_ARGS(&pfod));
    if (SUCCEEDED(hr))
    {
      IShellItem *psi;
      if (!iupAttribGetBoolean(ih, "MULTIPLEFILES"))
      {
        if (SUCCEEDED(pfd->GetResult(&psi)))
        {
          PWSTR pszFilePath = NULL;
          HRESULT hr = psi->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
          if (SUCCEEDED(hr))
          {
            filename = iupwinStrFromSystemFilename(pszFilePath);
            CoTaskMemFree(pszFilePath);
          }
        }
      }
      else
      {
        IShellItemArray *psiaResult;

        hr = pfod->GetResults(&psiaResult);
        if (SUCCEEDED(hr))
        {
          PWSTR pszFilePath = NULL;
          IShellItem *psi = NULL;
          hr = psiaResult->GetItemAt(0, &psi); // get a selected item from the IShellItemArray
          if (SUCCEEDED(hr))
          {
            HRESULT hr = psi->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
            if (SUCCEEDED(hr))
            {
              filename = iupwinStrFromSystemFilename(pszFilePath);
              CoTaskMemFree(pszFilePath);
            }
          }
        }
      }
      pfod->Release();
    }
    else
    {
      IFileSaveDialog *pfsd;
      HRESULT hr = pfd->QueryInterface(IID_PPV_ARGS(&pfsd));
      if (SUCCEEDED(hr))
      {
        IShellItem *psi;
        if (SUCCEEDED(pfd->GetResult(&psi)))
        {
          PWSTR pszFilePath = NULL;
          HRESULT hr = psi->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
          if (SUCCEEDED(hr))
          {
            filename = iupwinStrFromSystemFilename(pszFilePath);
            CoTaskMemFree(pszFilePath);
          }
        }
      }
      pfsd->Release();
    }

    if (!filename)
      return S_OK;

    ret = cb(ih, filename, "OK");

    if (ret == IUP_IGNORE || ret == IUP_CONTINUE)
    {
      if (ret == IUP_CONTINUE)
      {
        char* value = iupAttribGet(ih, "FILE");
        /* TODO: Se o nome selecionado n�o for uns dos tipos especificados para o di�logo uma extens�o ser� concatenada */
        if (value)
          pfd->SetFileName(iupwinStrToSystem(value));
      }
      return S_FALSE;
    }
  }

  return S_OK;
}

IFACEMETHODIMP winNewFileDlgEventHandler::OnSelectionChange(IFileDialog *pfd)
{
  char *filename = NULL;
  int ret;
  IFnss cb = (IFnss)IupGetCallback(ih, "FILE_CB");
  if (cb)
  {
    IFileOpenDialog *pfod;
    HRESULT hr = pfd->QueryInterface(IID_PPV_ARGS(&pfod));
    if (SUCCEEDED(hr))
    {
      IShellItem *psi;
      if (!iupAttribGetBoolean(ih, "MULTIPLEFILES"))
      {
        if (SUCCEEDED(pfd->GetCurrentSelection(&psi)))
        {
          PWSTR pszFilePath = NULL;
          SFGAOF attr;
          hr = psi->GetAttributes(SFGAO_FOLDER, &attr);
          if (SUCCEEDED(hr) && (SFGAO_FOLDER == attr))
          {
            HRESULT hr = psi->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
            if (SUCCEEDED(hr))
            {
              filename = iupwinStrFromSystemFilename(pszFilePath);
              CoTaskMemFree(pszFilePath);
            }
          }
        }
      }
      else
      {
        IShellItemArray *psiaResult;

        hr = pfod->GetSelectedItems(&psiaResult);
        if (SUCCEEDED(hr))
        {
          PWSTR pszFilePath = NULL;
          IShellItem *psi = NULL;
          hr = psiaResult->GetItemAt(0, &psi); // get a selected item from the IShellItemArray
          SFGAOF attr;
          hr = psi->GetAttributes(SFGAO_FOLDER, &attr);
          if (SUCCEEDED(hr) && (SFGAO_FOLDER == attr))
          {
            if (SUCCEEDED(hr))
            {
              HRESULT hr = psi->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
              if (SUCCEEDED(hr))
              {
                filename = iupwinStrFromSystemFilename(pszFilePath);
                CoTaskMemFree(pszFilePath);
              }
            }
          }
        }
      }
      pfod->Release();
    }
    else
    {
      IFileSaveDialog *pfsd;
      HRESULT hr = pfd->QueryInterface(IID_PPV_ARGS(&pfsd));
      if (SUCCEEDED(hr))
      {
        IShellItem *psi;
        if (SUCCEEDED(pfd->GetCurrentSelection(&psi)))
        {
          PWSTR pszFilePath = NULL;
          SFGAOF attr;
          hr = psi->GetAttributes(SFGAO_FOLDER, &attr);
          if (SUCCEEDED(hr) && (SFGAO_FOLDER == attr))
          {
            HRESULT hr = psi->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
            if (SUCCEEDED(hr))
            {
              filename = iupwinStrFromSystemFilename(pszFilePath);
              CoTaskMemFree(pszFilePath);
            }
          }
        }
      }
      pfsd->Release();
    }

    if (filename == NULL)
      return S_OK;

    ret = cb(ih, filename, "OK");

    if (ret == IUP_IGNORE || ret == IUP_CONTINUE)
      return S_FALSE;
  }

  return S_OK;
}

static int winNewFileDlgGetFirstFile(TCHAR* filename)
{
  /* check if there are more than 1 files and return only the first one */
  int found = 0;
  while (*filename != 0)
  {
    if (*filename == TEXT('"'))
    {
      if (!found)
        found = 1;
      else
      {
        *(filename - 1) = 0;
        return 1;
      }
    }
    if (found)
      *filename = *(filename + 1);
    filename++;
  }

  return 1;
}

IFACEMETHODIMP winNewFileDlgEventHandler::OnTypeChange(IFileDialog *pfd)
{
  char *filename = NULL;
  char *pathname = NULL;
  char* buffer = NULL;
  int size = 0;
  int ret;
  IFnss cb = (IFnss)IupGetCallback(ih, "FILE_CB");
  if (cb)
  {
    HRESULT hr;
    IShellItem *folder = NULL;
    PWSTR pszFileName = NULL;
    PWSTR pszFolder = NULL;
    UINT index;
    hr = pfd->GetFolder(&folder);
    if (SUCCEEDED(hr))
    {
      hr = folder->GetDisplayName(SIGDN_FILESYSPATH, &pszFolder);
      if (SUCCEEDED(hr))
      {
        pathname = iupwinStrFromSystemFilename(pszFolder);
        size = (int)strlen(pathname);
        hr = pfd->GetFileName(&pszFileName);
        if (SUCCEEDED(hr))
        {
          if (iupAttribGetBoolean(ih, "MULTIPLEFILES"))
            winNewFileDlgGetFirstFile(pszFileName);
          filename = iupwinStrFromSystemFilename(pszFileName);
          size += (int)strlen(filename);
          CoTaskMemFree(pszFileName);
        }
        CoTaskMemFree(pszFolder);
      }
    }

    if (filename == NULL || filename[0] == 0 || size == 0)
      return S_OK;

    buffer = (char *)malloc((size + 2)*sizeof(char));
    strcpy(buffer, pathname);
    strcat(buffer, "\\");
    strcat(buffer, filename);
    pfd->GetFileTypeIndex(&index);
    iupAttribSetInt(ih, "FILTERUSED", index - 1);
    ret = cb(ih, buffer, "FILTER");
    free(buffer);
    if (ret == IUP_CONTINUE)
    {
      char* value = iupAttribGet(ih, "FILE");
      /* TODO: Se o nome selecionado n�o for uns dos tipos especificados para o di�logo uma extens�o ser� concatenada */
      if (value)
        pfd->SetFileName(iupwinStrToSystem(value));
    }
  }

  return S_OK;
}

// Instance creation helper
static HRESULT winNewFileDlgEventHandler_CreateInstance(REFIID riid, void **ppv, Ihandle *ih)
{
  *ppv = NULL;
  winNewFileDlgEventHandler *pDialogEventHandler = new (std::nothrow) winNewFileDlgEventHandler(ih);  // TODO: should we keep nothrow?
  HRESULT hr = pDialogEventHandler ? S_OK : E_OUTOFMEMORY;
  if (SUCCEEDED(hr))
  {
    hr = pDialogEventHandler->QueryInterface(riid, ppv);
    pDialogEventHandler->Release();
  }
  return hr;
}

static COMDLG_FILTERSPEC *winNewFileDlgCreateFilterSpecs(char *name, int *size)
{
  int i = 0;
  int buffSize = 50;
  COMDLG_FILTERSPEC* filters = (COMDLG_FILTERSPEC*)malloc((buffSize)*sizeof(COMDLG_FILTERSPEC));

  /* replace symbols "|" by terminator "\0" */

  while (*name)
  {
    char *filter;
    filter = name;
    while (*filter)
    {
      if (*filter == '|')
      {
        *filter = 0;
        filters[i].pszName = (LPCWSTR)iupwinStrToSystem(name);
        break;
      }
      filter++;
    }

    if (!filter)
      break;

    name = ++filter;

    while (*filter)
    {
      if (*filter == '|')
      {
        *filter = 0;
        filters[i].pszSpec = (LPCWSTR)iupwinStrToSystem(name);
        break;
      }
      filter++;
    }

    if (!filter)
      break;

    i++;
    name = ++filter;

    if (i == 50)
      break;
  }

  *size = i;
  return filters;
}

static void winNewFileDlgStrReplacePathSlash(TCHAR* name)
{
  /* check for "/" */
  int i, len = lstrlen(name);
  for (i = 0; i < len; i++)
  {
    if (name[i] == TEXT('/'))
      name[i] = TEXT('\\');
  }
}

static IShellItem *winNewFileDlgParseName(LPCWSTR path)
{
  IShellItem *psi;
  HRESULT hr = SHCreateItemFromParsingName(path, NULL, IID_IShellItem, (void**)&psi);
  if (SUCCEEDED(hr))
    return psi;
  return NULL;
}

static int winIsFile(const TCHAR* name)
{
  DWORD attrib = GetFileAttributes(name);
  if (attrib == INVALID_FILE_ATTRIBUTES)
    return 0;
  if (attrib & FILE_ATTRIBUTE_DIRECTORY)
    return 0;
  return 1;
}

static void winNewFileDlgGetFolder(Ihandle *ih)
{
  InativeHandle* parent = iupDialogGetNativeParent(ih);
  IFileDialog *pfd;

  /* if NOT set will NOT be Modal */
  /* anyway it will be modal only relative to its parent */
  if (!parent)
    parent = GetActiveWindow();

  if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd))))
  {
    DWORD dwOptions;
    if (SUCCEEDED(pfd->GetOptions(&dwOptions)))
      pfd->SetOptions(dwOptions | FOS_PICKFOLDERS);

    char* value = iupAttribGet(ih, "TITLE");
    if (value)
      pfd->SetTitle(iupwinStrToSystem(value));

    if (SUCCEEDED(pfd->Show(parent)))
    {
      IShellItem *psi;
      if (SUCCEEDED(pfd->GetResult(&psi)))
      {
        PWSTR pszFilePath = NULL;
        HRESULT hr = psi->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
        if (SUCCEEDED(hr))
        {
          iupAttribSetStr(ih, "VALUE", iupwinStrFromSystemFilename(pszFilePath));
          iupAttribSet(ih, "STATUS", "0");
          CoTaskMemFree(pszFilePath);
        }
        else
        {
          iupAttribSet(ih, "VALUE", NULL);
          iupAttribSet(ih, "STATUS", "-1");
        }
        psi->Release();
      }
    }
    pfd->Release();
  }

  iupAttribSet(ih, "FILEEXIST", NULL);
  iupAttribSet(ih, "FILTERUSED", NULL);
}

static int winNewFileDlgPopup(Ihandle *ih, int x, int y)
{
  InativeHandle* parent = iupDialogGetNativeParent(ih);
  int dialogtype;
  char *value, *directory;

  iupAttribSetInt(ih, "_IUPDLG_X", x);   /* used in iupDialogUpdatePosition */
  iupAttribSetInt(ih, "_IUPDLG_Y", y);

  value = iupAttribGetStr(ih, "DIALOGTYPE");
  if (iupStrEqualNoCase(value, "SAVE"))
    dialogtype = IUP_DIALOGSAVE;
  else if (iupStrEqualNoCase(value, "DIR"))
    dialogtype = IUP_DIALOGDIR;
  else
    dialogtype = IUP_DIALOGOPEN;

  if (dialogtype == IUP_DIALOGDIR)
  {
    winNewFileDlgGetFolder(ih);
    return IUP_NOERROR;
  }

  /* if NOT set will NOT be Modal */
  /* anyway it will be modal only relative to its parent */
  if (!parent)
    parent = GetActiveWindow();

  // CoCreate the File Open Dialog object.
  IFileDialog *pfd = NULL;
  IFileOpenDialog *opfd = NULL;
  IFileSaveDialog *spfd = NULL;
  HRESULT hr;

  if (dialogtype == IUP_DIALOGOPEN)
  {
    hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&opfd));
    if (SUCCEEDED(hr))
      hr = opfd->QueryInterface(IID_IFileDialog, reinterpret_cast<void**>(&pfd));
  }
  else
  {
    hr = CoCreateInstance(CLSID_FileSaveDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&spfd));
    if (SUCCEEDED(hr))
      hr = spfd->QueryInterface(IID_IFileDialog, reinterpret_cast<void**>(&pfd));
  }

  if (!SUCCEEDED(hr))
  {
    if (pfd)
      pfd->Release();
    if (opfd)
      opfd->Release();
    if (spfd)
      spfd->Release();

    return IUP_ERROR;
  }

  // Create an event handling object, and hook it up to the dialog.
  IFileDialogEvents *pfde = NULL;
  hr = winNewFileDlgEventHandler_CreateInstance(IID_PPV_ARGS(&pfde), ih);
  DWORD dwCookie = 0;
  if (SUCCEEDED(hr))
    hr = pfd->Advise(pfde, &dwCookie); // Hook up the event handler.

  if (!SUCCEEDED(hr))
  {
    if (pfde)
      pfde->Release();
    if (pfd)
      pfd->Release();
    if (opfd)
      opfd->Release();
    if (spfd)
      spfd->Release();

    return IUP_ERROR;
  }

  value = iupAttribGet(ih, "EXTDEFAULT");
  if (value)
    pfd->SetDefaultExtension(iupwinStrToSystem(value));

  // Before setting, always get the options first in order not to override existing options.
  DWORD dwFlags;
  hr = pfd->GetOptions(&dwFlags);
  if (SUCCEEDED(hr))
  {
    value = iupAttribGet(ih, "ALLOWNEW");
    if (!value)
    {
      if (dialogtype == IUP_DIALOGSAVE)
        value = "YES";
      else
        value = "NO";
    }
    if (iupStrBoolean(value))
      dwFlags |= FOS_CREATEPROMPT;
    else
      dwFlags |= FOS_FILEMUSTEXIST;

    if (iupAttribGetBoolean(ih, "NOCHANGEDIR"))
      dwFlags |= FOS_NOCHANGEDIR;

    if (iupAttribGetBoolean(ih, "MULTIPLEFILES"))
      dwFlags |= FOS_ALLOWMULTISELECT;

    if (!iupAttribGetBoolean(ih, "NOOVERWRITEPROMPT"))
      dwFlags |= FOS_OVERWRITEPROMPT;

    if (iupAttribGetBoolean(ih, "SHOWHIDDEN"))
      dwFlags |= FOS_FORCESHOWHIDDEN;

    // In this case, get shell items only for file system items.
    pfd->SetOptions(dwFlags | FOS_FORCEFILESYSTEM);
  }

  char *extFilter;
  extFilter = iupStrDup(iupAttribGet(ih, "EXTFILTER"));
  if (extFilter)
  {
    int index, size;
    COMDLG_FILTERSPEC *filterSpecs = winNewFileDlgCreateFilterSpecs(extFilter, &size);

    hr = pfd->SetFileTypes(size, filterSpecs);
    if (SUCCEEDED(hr))
    {
      value = iupAttribGet(ih, "FILTERUSED");
      if (iupStrToInt(value, &index))
        pfd->SetFileTypeIndex(index);
    }
    free(extFilter);
    free(filterSpecs);
  }
  else
  {
    value = iupAttribGet(ih, "FILTER");
    if (value)
    {
      COMDLG_FILTERSPEC filterSpecs[1];
      char* info = iupAttribGet(ih, "FILTERINFO");
      if (!info)
        info = value;

      filterSpecs[0].pszName = (LPCWSTR)iupwinStrToSystemFilename(info);
      filterSpecs[0].pszSpec = (LPCWSTR)iupwinStrToSystemFilename(value);

      hr = pfd->SetFileTypes(1, filterSpecs);
      if (SUCCEEDED(hr))
        pfd->SetFileTypeIndex(1);
    }
  }

  value = iupAttribGet(ih, "FILE");
  directory = iupStrDup(iupAttribGet(ih, "DIRECTORY"));
  if (value)
  {
    char name[4096] = "";
    static char dir[4096] = "";
    IShellItem *si;
    TCHAR *fileName, *directory;
    iupStrFileNameSplit(value, dir, name);
    fileName = iupwinStrToSystemFilename(name);
    directory = iupwinStrToSystemFilename(dir);
    winNewFileDlgStrReplacePathSlash(directory);
    pfd->SetFileName((LPCWSTR)fileName);
    si = winNewFileDlgParseName((LPCWSTR)directory);
    if (si)
      pfd->SetFolder(si);
  }
  else if (directory)
  {
    IShellItem *si;
    winNewFileDlgStrReplacePathSlash((TCHAR*)directory);
    si = winNewFileDlgParseName((LPCWSTR)directory);
    if (si)
      hr = pfd->SetFolder(si);
    free(directory);
  }

  value = iupAttribGet(ih, "TITLE");
  if (value)
    pfd->SetTitle(iupwinStrToSystem(value));

  // Show the dialog
  hr = pfd->Show(parent);

  if (SUCCEEDED(hr))
  {
    if (!iupAttribGetBoolean(ih, "MULTIPLEFILES"))
    {
      // Obtain the result, once the user clicks the 'Open' button.
      // The result is an IShellItem object.
      IShellItem *psiResult;
      hr = pfd->GetResult(&psiResult);
      if (SUCCEEDED(hr))
      {
        PWSTR pszFilePath = NULL;
        hr = psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
        if (SUCCEEDED(hr))
        {
          char* filename = iupwinStrFromSystemFilename(pszFilePath);
          char* dir = iupStrFileGetPath(filename);
          int dir_len = (int)strlen(dir);
          iupAttribSetStr(ih, "DIRECTORY", dir);

          iupAttribSetStrId(ih, "MULTIVALUE", 0, dir);  /* same as directory, includes last separator */

          if (iupAttribGetBoolean(ih, "MULTIVALUEPATH"))
            dir_len = 0;

          iupAttribSetStrId(ih, "MULTIVALUE", 1, filename + dir_len);

          iupAttribSetStr(ih, "VALUE", filename);  /* here value is not separated by '|' */

          iupAttribSetInt(ih, "MULTIVALUECOUNT", 2);
          free(dir);
          CoTaskMemFree(pszFilePath);

          if (winIsFile(pszFilePath))  /* check if file exists */
          {
            iupAttribSet(ih, "FILEEXIST", "YES");
            iupAttribSet(ih, "STATUS", "0");
          }
          else
          {
            iupAttribSet(ih, "FILEEXIST", "NO");
            iupAttribSet(ih, "STATUS", "1");
          }
        }
        else
          iupAttribSet(ih, "STATUS", "-1");

        psiResult->Release();
      }
    }
    else
    {
      IShellItemArray *psiaResult;

      hr = opfd->GetResults(&psiaResult);
      if (SUCCEEDED(hr))
      {
        PWSTR pszFilePath = NULL;
        DWORD dwNumItems = 0; // number of items in multiple selection

        hr = psiaResult->GetCount(&dwNumItems);  // get number of selected items

        if (dwNumItems == 1)
        {
          IShellItem *psi = NULL;
          hr = psiaResult->GetItemAt(0, &psi); // get a selected item from the IShellItemArray
          if (SUCCEEDED(hr))
          {
            hr = psi->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
            if (SUCCEEDED(hr))
            {
              /* if there is only one file selected the returned value is different
              and includes just that file */
              char* filename = iupwinStrFromSystemFilename(pszFilePath);
              char* dir = iupStrFileGetPath(filename);
              int dir_len = (int)strlen(dir);
              iupAttribSetStr(ih, "DIRECTORY", dir);

              iupAttribSetStrId(ih, "MULTIVALUE", 0, dir);  /* same as directory, includes last separator */

              if (iupAttribGetBoolean(ih, "MULTIVALUEPATH"))
                dir_len = 0;

              iupAttribSetStrId(ih, "MULTIVALUE", 1, filename + dir_len);

              iupAttribSetStr(ih, "VALUE", filename);  /* here value is not separated by '|' */

              iupAttribSetInt(ih, "MULTIVALUECOUNT", 2);

              free(dir);
              CoTaskMemFree(pszFilePath);
            }
          }
        }
        else
        {
          char* dir = NULL;
          DWORD i;
          for (i = 0; i < dwNumItems; i++)
          {
            IShellItem *psi = NULL;

            hr = psiaResult->GetItemAt(i, &psi); // get a selected item from the IShellItemArray
            if (SUCCEEDED(hr))
            {
              hr = psi->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
              if (SUCCEEDED(hr))
              {
                char* filename = iupwinStrFromSystemFilename(pszFilePath);

                if (i == 0)
                {
                  dir = iupStrFileGetPath(filename);

                  if (iupAttribGetBoolean(ih, "MULTIVALUEPATH"))
                    iupAttribSetStrf(ih, "VALUE", "%s|", dir);

                  iupAttribSetStrf(ih, "DIRECTORY", "%s", dir);  /* add the last separator */

                  iupAttribSetStrId(ih, "MULTIVALUE", 0, dir);  /* same as directory, includes last separator */
                }

                if (iupAttribGetBoolean(ih, "MULTIVALUEPATH"))
                {
                  char* value = iupAttribGet(ih, "VALUE");
                  char nameid[100];
                  char *fname = iupStrFileGetTitle(filename);
                  sprintf(nameid, "MULTIVALUE%d", i + 1);
                  iupAttribSetStrf(ih, nameid, "%s%s", dir, fname);

                  iupAttribSetStrf(ih, "VALUE", "%s%s|", value, iupAttribGetId(ih, "MULTIVALUE", i + 1));
                }
                else
                {
                  iupAttribSetStrId(ih, "MULTIVALUE", i + 1, filename);

                  if (i == 0)
                    iupAttribSetStrf(ih, "VALUE", "%s|", filename);
                  else
                  {
                    char* value = iupAttribGet(ih, "VALUE");
                    iupAttribSetStrf(ih, "VALUE", "%s%s|", value, filename);
                  }
                }

                CoTaskMemFree(pszFilePath);
              }

              psi->Release();
            }
            iupAttribSetInt(ih, "MULTIVALUECOUNT", i + 2);
          }

          iupAttribSet(ih, "STATUS", "0");
          iupAttribSet(ih, "FILEEXIST", "YES");
        }
        psiaResult->Release();
      }
    }
  }
  else
  {
    iupAttribSet(ih, "FILTERUSED", NULL);
    iupAttribSet(ih, "VALUE", NULL);
    iupAttribSet(ih, "DIRECTORY", NULL);
    iupAttribSet(ih, "FILEEXIST", NULL);
    iupAttribSet(ih, "STATUS", "-1");
  }
        
  // Unhook the event handler.
  pfd->Unadvise(dwCookie);
  pfde->Release();

  pfd->Release();

  if (opfd)
    opfd->Release();
  if (spfd)
    spfd->Release();

  return IUP_NOERROR;
}

extern "C" {
/* the only exported function, 
   once called it will replace regular IupFileDlg */
int IupNewFileDlgOpen(void)
{
  if (IupGetGlobal("_IUP_NEWFILEDLG_OPEN"))
    return IUP_OPENED;

  Iclass* ic = iupRegisterFindClass("filedlg");
  ic->DlgPopup = winNewFileDlgPopup;

  IupSetGlobal("_IUP_NEWFILEDLG_OPEN", "1");
  return IUP_NOERROR;
}
}

// TODO:
// SHOWPREVIEW + Preview Callbacks
// HELP_CB
