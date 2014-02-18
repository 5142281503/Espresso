#include "stdafx.h"
#include <FCI.H>					// C:\Program Files\Microsoft SDKs\Windows\v7.0\Include\Fci.h
#include <io.h>						// C  _open(), _read(), _write(), etc...
#include <fcntl.h>					// _O_BINARY, _O_RDONLY, etc...
#include <shlobj.h>					// SHGetSpecialFolderPath
#include <shlwapi.h>				// PathIsDirectory
#include <string>	
#include <crtdbg.h>
#include <vector>
#include <list>
#include <algorithm>				// find
#define SECURITY_WIN32
#include <Security.h>				// GetUserNameEx
#include <time.h>

/*
	(release) Runtime dependencies
	- shlwapi.dll,	PathIsDirectory
	- shell32.dll,	SHGetSpecialFolderPath
	- secur32.dll,	GetUserNameEx
*/


#define TRACE0(sz)					_RPT0( _CRT_WARN, sz );
#define TRACE1(sz, p1)				_RPT1( _CRT_WARN, sz, p1 );
#define TRACE2(sz, p1, p2)			_RPT2( _CRT_WARN, sz, p1, p2 );
#define TRACE3(sz, p1, p2, p3)		_RPT3( _CRT_WARN, sz, p1, p2, p3 );

#define TRACEW0(sz)					_RPTW0( _CRT_WARN, sz );
#define TRACEW1(sz, p1)				_RPTW1( _CRT_WARN, sz, p1 );
#define TRACEW2(sz, p1, p2)			_RPTW2( _CRT_WARN, sz, p1, p2 );

std::string GetExeDir()
{
	char strFullPath[MAX_PATH] = { 0 } ;
	::GetModuleFileNameA( NULL, strFullPath, _MAX_PATH );

	char strDrive[MAX_PATH] = {0};
	char strDir[MAX_PATH] = {0};
	_splitpath_s( strFullPath, strDrive, MAX_PATH, strDir, MAX_PATH, NULL, 0, NULL, 0 );

	const std::string result = std::string( strDrive ) + strDir;

	TRACE1("Here is where CAB files will be created [%s]\n", result.c_str());

	return result;
}



inline std::string
GetCurrentTimeFormatted(const std::string& in_Format)
{
	char timeString[ 100 ] = { 0 };
	time_t timeNow = 0;
	time( &timeNow );
	tm currentTimeTm = { 0 };
	localtime_s(&currentTimeTm, &timeNow);
	strftime( timeString, sizeof(timeString), in_Format.c_str(), &currentTimeTm);
	return timeString;
}

std::string 
MakeValidFileName(const std::string& in_strHint)
{
	static const char invalid_chars[] = {'\\','/',':','*','?','\"','<','>','|'}; //characters not allowed
	static const size_t num_invalid_chars = _countof(invalid_chars);

	//substitute invalid characters
	std::string valid_name = in_strHint;
	std::string::size_type pos = 0;
	while( pos != std::string::npos )
	{
		pos = valid_name.find_first_of( invalid_chars, pos, num_invalid_chars );
		if( pos != std::string::npos )
			valid_name.replace( pos, 1, 1, '_' );
	}

	return valid_name;
}

std::string
MakeValidFileNameUsingUserName(const std::string& in_strHint)
{
	char szUserName[2048] = { 0 };
	unsigned long userNameSize = _countof( szUserName );
	if( GetUserNameExA( NameSamCompatible, szUserName, &userNameSize ) )
	{
		return MakeValidFileName(szUserName + in_strHint);
	}

	return MakeValidFileName("unknown"+ in_strHint);
}

std::string
MakeFileNameUsingTimeAndUser(const std::string& in_strHint)
{

	return MakeValidFileNameUsingUserName( in_strHint + GetCurrentTimeFormatted("%A %B-%d-%y %H.%M.%S") );
}

char *return_fci_error_string(FCIERROR err)
{
	switch (err)
	{
		case FCIERR_NONE:
			return "No error";

		case FCIERR_OPEN_SRC:
			return "Failure opening file to be stored in cabinet";
		
		case FCIERR_READ_SRC:
			return "Failure reading file to be stored in cabinet";
		
		case FCIERR_ALLOC_FAIL:
			return "Insufficient memory in FCI";

		case FCIERR_TEMP_FILE:
			return "Could not create a temporary file";

		case FCIERR_BAD_COMPR_TYPE:
			return "Unknown compression type";

		case FCIERR_CAB_FILE:
			return "Could not create cabinet file";

		case FCIERR_USER_ABORT:
			return "Client requested abort";

		case FCIERR_MCI_FAIL:
			return "Failure compressing data";

		default:
			return "Unknown error";
	}
}

/*
 * When a CAB file reaches this size, a new CAB will be created
 * automatically.  This is useful for fitting CAB files onto disks.
 *
 * If you want to create just one huge CAB file with everything in
 * it, change this to a very very large number.
 */
#define MEDIA_SIZE			300000000

/*
 * When a folder has this much compressed data inside it,
 * automatically flush the folder.
 *
 * Flushing the folder hurts compression a little bit, but
 * helps random access significantly.
 */
#define FOLDER_THRESHOLD	900000000


/*
 * Compression type to use
 */

#define COMPRESSION_TYPE    tcompTYPE_MSZIP


/*
 * Our internal state
 *
 * The FCI APIs allow us to pass back a state pointer of our own
 */
typedef struct
{
    long    total_compressed_size;      /* total compressed size so far */
	long	total_uncompressed_size;	/* total uncompressed size so far */
} client_state;


/*
 * Memory allocation function
 */
FNFCIALLOC(mem_alloc)
{
	return malloc(cb);
}


/*
 * Memory free function
 */
FNFCIFREE(mem_free)
{
	free(memory);
}


/*
 * File i/o functions
 */
FNFCIOPEN(fci_open)
{
    const int result = _open(pszFile, oflag, pmode);

    if (result == -1)
        *err = errno;

    return result;
}

FNFCIREAD(fci_read)
{
    const unsigned int result = (unsigned int) _read(hf, memory, cb);

    if (result != cb)
        *err = errno;

    return result;
}

FNFCIWRITE(fci_write)
{
    const unsigned int result = _write(hf, memory, cb);

    if (result != cb)
        *err = errno;

    return result;
}

FNFCICLOSE(fci_close)
{
    const int result = _close(hf);

    if (result != 0)
        *err = errno;

    return result;
}

FNFCISEEK(fci_seek)
{
    const long result = _lseek(hf, dist, seektype);

    if (result == -1)
        *err = errno;

    return result;
}

FNFCIDELETE(fci_delete)
{
    int result;

    result = remove(pszFile);

    if (result != 0)
        *err = errno;

    return result;
}


/*
 * File placed function called when a file has been committed
 * to a cabinet
 */
FNFCIFILEPLACED(file_placed)
{
#ifdef _VERBOSE_OUTPUT

	printf(
		"   placed file '%s' (size %d) on cabinet '%s'\n",
		pszFile, 
		cbFile, 
		pccab->szCab
	);

	if (fContinuation)
		printf("      (Above file is a later segment of a continued file)\n");
#endif

	return 0;
}


/*
 * Function to obtain temporary files
 */
FNFCIGETTEMPFILE(get_temp_file)
{
	CHAR tempFileName[MAX_PATH] = {0};
	GetTempFileNameA( GetExeDir().c_str(), "ES", 0, tempFileName) ;

    if ((tempFileName != NULL) && (strlen(tempFileName) < (unsigned)cbTempName)) 
	{
        strcpy(pszTempName, tempFileName);  // Copy to caller's buffer
//        free(psz);							// Free temporary name buffer

		DeleteFileA(pszTempName);			// GetTempFileNameA creates the File, and FCI seems to dislike that.

        return TRUE;						// Success
    }
    
	/* Failed
    if (psz) 
	{
        free(psz);
    }
	*/

    return FALSE;
}


int get_percentage(unsigned long a, unsigned long b)
{
	while (a > 10000000)
	{
		a >>= 3;
		b >>= 3;
	}

	if (b == 0)
		return 0;

	return ((a*100)/b);
}


/*
 * Progress function

 The typeStatus parameter may take on values of statusFile, statusFolder, or statusCabinet.  
 If typeStatus equals statusFile then it means that FCI is compressing data blocks into a folder.  
 In this case, cb1 is either zero, or the compressed size of the most recently compressed block, and cb2 is either zero, 
 or the uncompressed size of the most recently read block (which is usually 32K, except for the last block in a folder, 
 which may be smaller).  There is no direct relation between cb1 and cb2; FCI may read several blocks of uncompressed data 
 before emitting any compressed data; if this happens, some statusFile messages may contain, for example, cb1 = 0 and cb2 = 32K, 
 followed later by other messages which contain cb1 = 20K and cb2 = 0.

If typeStatus equals statusFolder then it means that FCI is copying a folder to a cabinet, and cb1 is the amount copied so far, 
and cb2 is the total size of the folder.  Finally, if typeStatus equals statusCabinet, then it means that FCI is writing out a 
completed cabinet, and cb1 is the estimated cabinet size that was previously passed to GetNextCab, and cb2 is the actual 
resulting cabinet size.

The progress function should return 0 for success, or -1 for failure, with an exception in the case of statusCabinet messages, 
where the function should return the desired cabinet size (cb2), or possibly a value rounded up to slightly higher than that.

 */
FNFCISTATUS(progress)
{
	client_state	*cs;

	cs = (client_state *) pv;

	if (typeStatus == statusFile)
	{
        cs->total_compressed_size += cb1;
		cs->total_uncompressed_size += cb2;

		/*
		 * Compressing a block into a folder
		 *
		 * cb2 = uncompressed size of block
		 */
		printf(
            "Compressing: %9ld -> %9ld             \r",
            cs->total_uncompressed_size,
            cs->total_compressed_size
		);
		
		fflush(stdout);
	}
	else if (typeStatus == statusFolder)
	{
		int	percentage;

		/*
		 * Adding a folder to a cabinet
		 *
		 * cb1 = amount of folder copied to cabinet so far
		 * cb2 = total size of folder
		 */
		percentage = get_percentage(cb1, cb2);

		printf("Copying folder to cabinet: %d%%      \r", percentage);
		fflush(stdout);
	}

	return 0;
}


std::string g_CurrentDescription;

void store_cab_name(char *cabname, int iCab)
{
	sprintf(cabname, "%s_EspressoResults%d.CAB", g_CurrentDescription.c_str(), iCab);
}


FNFCIGETNEXTCABINET(get_next_cabinet)
{
	/*
	 * Cabinet counter has been incremented already by FCI
	 */

	/*
	 * Store next cabinet name
	 */
	store_cab_name(pccab->szCab, pccab->iCab);

	/*
	 * You could change the disk name here too, if you wanted
	 */

	return TRUE;
}


/*
The function will receive five parameters; pszName, the complete pathname of the file to open; pdate, a memory location to 
return a FAT-style date code; ptime, a memory location to return a FAT-style time code; pattribs, a memory location to return 
FAT-style attributes; and pv, the application-specific context pointer originally passed to FCICreate.  The function should open 
the file using a file open function compatible with those passed in to FCICreate, and return the resulting file handle, or -1 if 
unsuccessful.

I commented most of it out thus allowing index.dat to be added, otherwsie it cannot
*/
FNFCIGETOPENINFO(get_open_info)
{
	BY_HANDLE_FILE_INFORMATION	finfo;
	FILETIME					filetime;
//	HANDLE						handle;
    DWORD                       attrs;
//    int                         hf;

    /*
     * Need a Win32 type handle to get file date/time
     * using the Win32 APIs, even though the handle we
     * will be returning is of the type compatible with
     * _open
     */
	HANDLE handle = CreateFileA(
		pszName,
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
		NULL
	);
   
	if (handle != INVALID_HANDLE_VALUE)
	{
		if (GetFileInformationByHandle(handle, &finfo) )
		{
			FileTimeToLocalFileTime(&finfo.ftLastWriteTime, &filetime);

			FileTimeToDosDateTime(&filetime, pdate, ptime);

			attrs = GetFileAttributesA(pszName);

			if (attrs == 0xFFFFFFFF)
			{
				/* failure */
				*pattribs = 0;
			}
			else
			{
				/*
				 * Mask out all other bits except these four, since other
				 * bits are used by the cabinet format to indicate a
				 * special meaning.
				 */
				*pattribs = (int) (attrs & (_A_RDONLY | _A_SYSTEM | _A_HIDDEN | _A_ARCH));
			}
		}
		CloseHandle(handle);
    }    


    /*
     * Return handle using _open
     */
	//const int hf = _open( pszName, _O_RDONLY | _O_BINARY );

	//if (hf == -1)
	//	return -1; // abort on error
 //  
	//return hf;

	return _open( pszName, _O_RDONLY | _O_BINARY );
}




void set_cab_parameters(PCCAB cab_parms)
{
	// initialize it yourself using = { 0 };
	// memset(cab_parms, 0, sizeof(CCAB));

	/*
	The cb field, the media size, specifies the maximum size of a cabinet which will be created by FCI.  
	If necessary, multiple cabinets will be created.  To ensure that only one cabinet is created, a sufficiently large 
	number should be used for this parameter.
	*/
	cab_parms->cb = MEDIA_SIZE;

	/*
	The cbFolderThresh field specifies the maximum number of compressed bytes which may reside in a folder before a 
	new folder is created.  A higher folder threshold improves compression performance (since creating a new folder 
	resets the compression history), but increases random access time to the folder.
	*/
	cab_parms->cbFolderThresh = FOLDER_THRESHOLD;

	/*
	 * Don't reserve space for any extensions
	 */
	cab_parms->cbReserveCFHeader = 0;
	cab_parms->cbReserveCFFolder = 0;
	cab_parms->cbReserveCFData   = 0;

	/*
	 * We use this to create the cabinet name
	 */
	cab_parms->iCab = 1;

	/*
	 * If you want to use disk names, use this to
	 * count disks
	 */
	cab_parms->iDisk = 0;

	/*
	 * Choose your own number
	 */
	cab_parms->setID = 39;

	/*
	 * Only important if CABs are spanning multiple
	 * disks, in which case you will want to use a
	 * real disk name.
	 *
	 * Can be left as an empty string.
	 */
	strcpy(cab_parms->szDisk, "Espresso");

	/* 
		where to store the created CAB files 

		The szCabPath field should contain the complete path of where to create the cabinet (e.g. “C:\MYFILES\”).
	
	*/

	strncpy(cab_parms->szCabPath, GetExeDir().c_str(), CB_MAX_CAB_PATH);

	/* 
		store name of first CAB file 

		The szCab field should contain a string which contains the name of the first cabinet to be created (e.g. “APP1.CAB”).  
	
	*/
	store_cab_name(cab_parms->szCab, cab_parms->iCab);
}

//This function returns true if the path points to the current or the parent directory
bool
IsDots(const std::wstring& in_pathW)
{
	//If the filename is ".." or ".", then the function returns true
	if( ( in_pathW.rfind(L"..") == 0 ) || ( in_pathW.rfind(L".") == 0 ) )
		return true;
	else 
		return false;
}

std::wstring
ExtractDirectoryW( const std::wstring& in_strFile )
{
	wchar_t strDrive[_MAX_DRIVE+1] = {0};
	wchar_t strDir[_MAX_DIR+1] = {0};
	_wsplitpath_s( in_strFile.c_str(), strDrive, _MAX_DRIVE, strDir, _MAX_DIR, NULL, 0, NULL, 0 );

    std::wstring out_strDirectory = strDrive;
	out_strDirectory += strDir;
    return out_strDirectory;
}

std::wstring 
getExtensionW(const std::wstring& in_String)
{
	wchar_t szFileExt[_MAX_EXT+1] = { 0 };
	_wsplitpath_s( in_String.c_str(), NULL, 0, NULL, 0, NULL, 0, szFileExt, _MAX_EXT+1 );
	return szFileExt;
}

bool 
GetFileCount(
	const LPCWSTR& in_lpszPathW, 
	DWORD& io_iCount,
	std::vector<std::wstring>& io_VecFiles,
	std::list<std::wstring>&   in_SkippedExtensions,
	const bool in_bDontRecurse = false)
{
	_ASSERT(in_lpszPathW);

	std::wstring strPathW(in_lpszPathW);
	
	_ASSERT( strPathW.length() );

	if(!PathIsDirectoryW(in_lpszPathW)) return false;

	if( strPathW.length())
	{
		if( strPathW[ strPathW.length() - 1 ] == L'\\' )
		{
			strPathW += L"*.*";
		}
		else
		{
			strPathW += L"\\*.*";
		}
	}
	
	TRACEW2(L"Doing a findfile on [%s] count is [%d]\n", strPathW.c_str(), io_iCount);

	WIN32_FIND_DATAW fileData={0};	
	
	//The first handle returns a valid file handle which is not the case with CFileFind::FindFile() 
	HANDLE hFind = FindFirstFileW(strPathW.c_str(), &fileData);

	if(hFind != INVALID_HANDLE_VALUE)
	{
		BOOL bWorking = true;
		std::wstring filePath = ExtractDirectoryW(strPathW);
		while(bWorking)
		{
			const std::wstring fileName(fileData.cFileName);

			if( ( (fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && IsDots(fileName) ) || (fileData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
			{
				bWorking = FindNextFileW(hFind, &fileData);
				continue;
			}

			if(fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				if(!in_bDontRecurse)
				{
					TRACEW2(L"Doing a recursive GetFileCount on [%s] count is [%d]\n", fileData.cFileName, io_iCount);
					GetFileCount( (filePath + fileName).c_str(), io_iCount, io_VecFiles, in_SkippedExtensions);
				}
				else
				{
					TRACE0("Not doing a recurse, in_bDontRecurse is true");
				}
			}
			else
			{
				if(in_SkippedExtensions.size())
				{
					// There's something in the list
					// Look for the current extension in the list, if it's there do not increment the counter

					const std::wstring extension (getExtensionW(fileName).c_str());

					TRACEW2(L"Gonna look for extension [%s] of file [%s] in the list of excluded extension\n", extension.c_str(), fileName.c_str());
					
					std::list<std::wstring>::const_iterator i = std::find(in_SkippedExtensions.begin(), in_SkippedExtensions.end(), extension);

					if(i == in_SkippedExtensions.end()) // extension not in list
					{
						io_VecFiles.push_back(filePath + fileName);
						++io_iCount;
					}
					else
					{
						TRACEW1(L"Skipped file [%s], it matched a skipped extension\n", fileName.c_str());
					}
				}
				else
				{
					io_VecFiles.push_back(filePath + fileName);
					++io_iCount;
				}
			}			
			bWorking = FindNextFileW(hFind,&fileData);
		}
		FindClose(hFind);
	}

	return true;
}




std::string
wide2Ansi(const std::wstring& in_str)
{
	std::string temp(in_str.length(), ' ');
	std::copy(in_str.begin(), in_str.end(), temp.begin());
	return temp; 
}


std::string
getFileNameOnly(const std::wstring& in_strFullPath)
{
	wchar_t strFileName[_MAX_FNAME] = { 0 };	
	wchar_t strExtensionName[_MAX_EXT] = { 0 };	

	_wsplitpath_s( in_strFullPath.c_str(), NULL, 0, NULL, 0, strFileName, _MAX_FNAME, strExtensionName, _MAX_EXT );

	return wide2Ansi(strFileName) + wide2Ansi(strExtensionName);
}


/*
	instead of using just "test.exe" as the file name
	(LPSTR)getFileNameOnly(s).c_str()
	I use the whole path.

	Looks much better in Winrar and with the MS Cab viewer as one ca see the entier file paths

	Except that it doesn't always work....*sigh*. So you'll have to use the filename
	to determine what the full path was.

*/
bool 
MakeCab(const std::wstring& in_Where, 
		const std::string& in_Why, 
		DWORD& o_NumFilesInCab, 
		const std::wstring& in_StrThisFileOnly,
		const bool in_bDontRecurse = false) 
{
	g_CurrentDescription			= in_Why;
	ERF							erf = { 0 };
	CCAB			 cab_parameters = { 0 };	
	client_state				 cs = { 0 };

	set_cab_parameters(&cab_parameters);

	HFCI hfci = FCICreate(
		&erf,
		file_placed,
		mem_alloc,
		mem_free,
        fci_open,
        fci_read,
        fci_write,
        fci_close,
        fci_seek,
        fci_delete,
		get_temp_file,
        &cab_parameters,
        &cs
	);

	if (hfci == NULL)
	{
		printf("FCICreate() failed: code %d [%s]\n", erf.erfOper, return_fci_error_string( (FCIERROR)erf.erfOper));
		return false;
	}

	o_NumFilesInCab = 0;
	std::vector<std::wstring>	VecFiles;
	std::list<std::wstring>	    SkippedExtensions;

	if(in_StrThisFileOnly.empty())
	{
		if( GetFileCount(in_Where.c_str(), o_NumFilesInCab, VecFiles, SkippedExtensions, in_bDontRecurse) && o_NumFilesInCab)
		{
			wprintf(L"Ready to create a CAB using [%s] with [%d] files in it...\r\n", in_Where.c_str(),  o_NumFilesInCab);		
			printf("Destination CAB: [%s]\r\n", GetExeDir().c_str());
		}
		else
		{
			wprintf(L"Cannot perform under [%s]...Specify a valid, non-empty folder\r\n", in_Where.c_str());
			printf("Destination CAB: [%s]\r\n", GetExeDir().c_str());
			return false;
		}
	}
	else
	{
		++o_NumFilesInCab;
		VecFiles.push_back(in_StrThisFileOnly);
	}


	// For each file in 'in_Where' folder and subfolder, unless 'in_StrThisFileOnly' param was specified

	for each(std::wstring s in VecFiles)	
	{
		if (FALSE == FCIAddFile(
				hfci,
				(LPSTR)wide2Ansi(s).c_str(),		// file to add, can't be a folder, needs full path 
				(LPSTR)getFileNameOnly(s).c_str(),	// file name in cabinet file, and should not include any path information (e.g. “TEST.EXE”). 
				FALSE,						 //  specifies whether the file should be executed automatically when the cabinet is extracted 
				get_next_cabinet,
				progress,		// should point to a function which is called periodically by FCI so that the application may send a progress report to the user
				get_open_info,	// point to a function which opens a file and returns its datestamp, timestamp, and attributes
				COMPRESSION_TYPE))
			{
				printf("FCIAddFile(%s) failed: code %d [%s]\n", wide2Ansi(s).c_str(), erf.erfOper, return_fci_error_string( (FCIERROR)erf.erfOper));
				TRACE3("FCIAddFile(%s) failed: code %d [%s]\n", wide2Ansi(s).c_str(), erf.erfOper, return_fci_error_string( (FCIERROR)erf.erfOper));
				//FCIDestroy(hfci);
				//return false;
				// Try to keep a possible good file created so far...
				o_NumFilesInCab--;
			}
	}

	/*
	The FCIFlushCabinet API forces the current cabinet under construction to be completed immediately and written to disk.  
	Further calls to FCIAddFile will cause files to be added to another cabinet.  It is also possible that there exists pending 
	data in FCI’s internal buffers that will may require spillover into another cabinet, if the current cabinet has reached the 
	application-specified media size limit.
	*/

	if(o_NumFilesInCab > 0)
	{
		if (FALSE == FCIFlushCabinet(
			hfci,
			FALSE,				// The fGetNextCab flag determines whether the function pointed to by the supplied GetNextCab parameter, will be called.  
			get_next_cabinet,	// If fGetNextCab is TRUE, then GetNextCab will be called to obtain continuation information
			progress))
		{
			printf("FCIFlushCabinet() failed: code %d [%s]\n", erf.erfOper, return_fci_error_string( (FCIERROR)erf.erfOper));
			FCIDestroy(hfci);
			return false;
		}
	}

    if (FCIDestroy(hfci) != TRUE)
	{
		printf("FCIDestroy() failed: code %d [%s]\n", erf.erfOper, return_fci_error_string( (FCIERROR)erf.erfOper));
		return false;
	}

	return o_NumFilesInCab > 0;
}


// This is what is used to construct the CAB filename
// I make them both unique and descriptive
std::vector<std::string> g_descvector; 

bool
GetFoldersToScan(std::vector<std::wstring>& out_VecFolders)
{		
	wchar_t personal[MAX_PATH] = {0};
	SHGetSpecialFolderPathW(NULL, personal, CSIDL_PERSONAL, FALSE);			// C:\Users\mario\Documents

	wchar_t roaming[MAX_PATH] = {0};
	SHGetSpecialFolderPathW( NULL, roaming, CSIDL_APPDATA, FALSE );			// C:\Users\mario\AppData\Roaming

	wchar_t local[MAX_PATH] = {0};
	SHGetSpecialFolderPathW( NULL, local, CSIDL_LOCAL_APPDATA, FALSE );		// C:\Users\mario\AppData\Local 

	wchar_t docs[MAX_PATH] = {0};
	SHGetSpecialFolderPathW( NULL, docs, CSIDL_COMMON_DOCUMENTS, FALSE );	// C:\Users\Public\Documents

	wchar_t programData[MAX_PATH] = {0};
	SHGetSpecialFolderPathW( NULL, programData, CSIDL_COMMON_APPDATA, FALSE );	// C:\ProgramData

	wchar_t ieFavs[MAX_PATH] = {0};
	SHGetSpecialFolderPathW( NULL, ieFavs, CSIDL_FAVORITES, FALSE );			// C:\Users\mario\Favorites


	// Add unique files below...

	std::wstring strJungleDiskFile(programData);
	strJungleDiskFile += L"\\JungleDisk\\jungledisk-settings.xml";
	DWORD NumFilesInCab(0);
	MakeCab(L"nowhere, 1 file only", MakeFileNameUsingTimeAndUser("_jungledisk_"), NumFilesInCab, strJungleDiskFile);	

	// Add folders without recursing as follows...

	std::wstring strBOINC(programData);
	strBOINC += L"\\BOINC";	
	MakeCab(strBOINC, MakeFileNameUsingTimeAndUser("_Boinc_"), NumFilesInCab, L"", true);	


	// Add all other folders below...

	{
	std::wstring strIeTemp(local);
	strIeTemp += L"\\Microsoft\\Windows\\Temporary Internet Files";
	g_descvector.push_back( MakeFileNameUsingTimeAndUser( "_IE Temp Files_") );
	out_VecFolders.push_back(strIeTemp);
	
	strIeTemp += L"\\Low";
	g_descvector.push_back( MakeFileNameUsingTimeAndUser( "_IE Temp Low Files_") );
	out_VecFolders.push_back(strIeTemp);
	}

	{
	std::wstring strPersonal(personal);
	strPersonal += L"\\My Received Files";
	g_descvector.push_back( MakeFileNameUsingTimeAndUser("_My Received Files_") );
	out_VecFolders.push_back(strPersonal);
	}

	{
	std::wstring strLocal(local);
	strLocal += L"\\Microsoft\\Windows Live Mail";
	g_descvector.push_back( MakeFileNameUsingTimeAndUser( "_Windows Live Mail_") );
	out_VecFolders.push_back(strLocal);
	}

	{
	std::wstring strRoaming(roaming);
	strRoaming += L"\\Skype";
	g_descvector.push_back( MakeFileNameUsingTimeAndUser( "_Skype_") );
	out_VecFolders.push_back(strRoaming);
	}

	{
	const std::wstring strieFavs(ieFavs);
	g_descvector.push_back( MakeFileNameUsingTimeAndUser( "_IE Favs_") );
	out_VecFolders.push_back(strieFavs);
	}

	{
	std::wstring strFFCookies(roaming);
	g_descvector.push_back( MakeFileNameUsingTimeAndUser( "_FireFox Cookies_") );
	strFFCookies += L"\\Mozilla\\Firefox\\profiles";
	out_VecFolders.push_back(strFFCookies);		
	}


	return out_VecFolders.size() > 0;
}

int _tmain(int argc, _TCHAR* argv[])
{
	DWORD NumFilesInCab = 0;	

	std::vector<std::wstring>	VecFolders;
	if(GetFoldersToScan(VecFolders))
	{
		int i(0);
		for each(std::wstring s in VecFolders)
		{			
			MakeCab(s, g_descvector[i++], NumFilesInCab, L"");	// Using folder name as part of CAB file name
		}
	}

	printf("Visit my blog for latest changes: http://securitymario.spaces.live.com/ \r\n");
	printf("Email questions to: marioc@computer.org\r\n");
	printf("Full readme.txt file with details at http://www.superconfigure.com/\r\n");		
	return 0;
}




/*
void
PrintHowitWorks(const std::wstring& in_appName)
{
	wprintf(L"[%s]\r\n", in_appName.c_str());
	wprintf(L"For LE Officials to quickly and quietly capture potentially compromising data\r\n");
	wprintf(L"Full readme.txt file with details at http://www.superconfigure.com/\r\n");		
}

wide2Ansi(PathSkipRoot(s.c_str()))



	char    *psz = _tempnam( PathRemoveBackslashA((LPSTR)GetExeDir().c_str()), "espresso_temp_file_");

	char    *psz2 = _tempnam( "C:\\temp\\Espresso\\release", "espresso_temp_file_"); 

	char    *psz3 = _tempnam( "C:\\temp\\Espresso\\release\\", "espresso_temp_file_"); 

	int hf = _open( "C:\\Users\\mario\\AppData\\Local\\Microsoft\\Windows\\Temporary Internet Files\\Low\\Content.IE5\\index.dat", _O_RDONLY | _O_BINARY );

	if (hf == -1)
		return -1; // abort on error

			DWORD NumFilesInCab = 0;	
	MakeCab(L"nowhere, 1 file only", MakeFileNameUsingTimeAndUser("_index.dat_"), NumFilesInCab, L"C:\\Users\\mario\\AppData\\Local\\Microsoft\\Windows\\Temporary Internet Files\\Low\\Content.IE5\\index.dat");
 

 //	MakeCab(L"nowhere, 1 file only", MakeFileNameUsingTimeAndUser("_index.dat_"), NumFilesInCab, L"C:\\Users\\mario\\AppData\\Local\\Microsoft\\Windows\\Temporary Internet Files\\Content.IE5\\index.dat");	

 L"C:\\Users\\mario\\AppData\\Local\\Microsoft\\Windows\\Temporary Internet Files\\Low");

 L"C:\\Users\\mario\\Documents\\My Received Files");

 L"C:\\Users\\mario\\AppData\\Local\\Microsoft\\Windows Live Mail");	

 L"C:\\Users\\mario\\AppData\\Roaming\\Skype"


 std::string getrawMac()
{
	std::string sRet;
	UUID Id = { 0 };

	if( RPC_S_UUID_NO_ADDRESS != UuidCreateSequential(&Id) )
	{
		PUCHAR GuidString;

		if( RPC_S_OK ==  UuidToString( &Id, &GuidString ) )
		{
			sRet = (char*)GuidString;
			RpcStringFree( &GuidString );
		}
	}
	else
	{

	}
	return sRet;
}

std::string getMac()
{
	std::string x( getrawMac() );
	return x.substr(24);
}
*/