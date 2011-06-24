/* 
Copyright (c) 2011 Wangdera Corporation (hobocopy@wangdera.com)

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

#include "CCopyFilter.h"
#include "CDirectoryAction.h"
#include "OutputWriter.h"
#include "CHoboCopyException.h"

class CCopyAction : public CDirectoryAction
{
private: 
    LONGLONG _byteCount; 
    LPCTSTR _destination; 
    int _directoryCount; 
    int _fileCount; 
    vector<CCopyFilter*> _filters; 
	LPCTSTR _lockCommand;
    int _skipCount; 
    bool _skipDenied; 
    LPCTSTR _source; 

public: 
    CCopyAction::CCopyAction(LPCTSTR source, LPCTSTR destination, bool skipDenied, LPCTSTR lockCommand, vector<CCopyFilter*> filters) : _filters(filters)
    {
        _source = source; 
        _destination = destination; 
        _skipDenied = skipDenied; 
        _fileCount = 0; 
        _directoryCount = 0; 
		_lockCommand = lockCommand;
        _skipCount = 0; 
        _byteCount = 0; 
    }

    LONGLONG get_ByteCount(void)
    {
        return _byteCount; 
    }

    int get_DirectoryCount(void)
    {
        return _directoryCount; 
    }

    int get_FileCount(void)
    {
        return _fileCount; 
    }

    int get_SkipCount(void)
    {
        return _skipCount; 
    }

    void VisitDirectoryFinal(LPCTSTR path)
    {
        CString message; 
        message.AppendFormat(TEXT("Copied directory %s"), path); 
        OutputWriter::WriteLine(message, VERBOSITY_THRESHOLD_NORMAL); 
        ++_directoryCount; 
    }

    void VisitDirectoryInitial(LPCTSTR path)
    {
        CString destDir;
        Utilities::CombinePath(_destination, path, destDir);
		Utilities::CreateDirectory(destDir);
		CString message; 
		message.AppendFormat(TEXT("Created directory %s"), destDir); 
		OutputWriter::WriteLine(message, VERBOSITY_THRESHOLD_IF_VERBOSE); 
    }

	void VisitFile(LPCTSTR path) 
    {
        CString sourceFile;
        Utilities::CombinePath(_source, path, sourceFile);

        CString destinationFile; 
        Utilities::CombinePath(_destination, path, destinationFile);
        Utilities::FixLongFilenames(destinationFile); 
        if (IsFileMatch(sourceFile))
        {
            BOOL worked = ::CopyFile(sourceFile, destinationFile, false);
#ifdef TESTING
			if (true)
#else
			if (!worked)
#endif
            {
                DWORD error = ::GetLastError();

                if ((error == 5 || error == 32) && _skipDenied)
                {
                    CString message; 
                    message.Format(TEXT("Error %d accessing file %s. Skipping."), error, sourceFile); 
                    OutputWriter::WriteLine(message, VERBOSITY_THRESHOLD_NORMAL); 
                    ++_skipCount; 
                }
#ifdef TESTING
				else if (true)
#else
				else if ((error == 32 || error == 33) && _lockCommand != NULL)
#endif
				{
					CString lockCommand; 
					lockCommand.Format(TEXT("\"%s\" \"%s\""), _lockCommand, path);
					LPTSTR szCmdLine = _tcsdup(lockCommand);

					CString message;
					message.Format(TEXT("Encountered error %d accessing file %s."), error, sourceFile);
					OutputWriter::WriteLine(message, VERBOSITY_THRESHOLD_UNLESS_SILENT);
										
					STARTUPINFO startupInfo; 
					memset(&startupInfo, 0, sizeof(startupInfo));
					startupInfo.cb = sizeof(startupInfo);

					PROCESS_INFORMATION processInformation;

					CString notification;
					notification.Format(TEXT("Launching %s and terminating."), lockCommand);
					OutputWriter::WriteLine(notification, VERBOSITY_THRESHOLD_UNLESS_SILENT);

					::CreateProcess(NULL, szCmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &startupInfo, &processInformation);

					::WaitForSingleObject(processInformation.hProcess, INFINITE);

					// TODO: we ought to clean up here, but this is just temporary diagnostic code, and we're going to terminate 
					// the process anyway.

					throw new CHoboCopyException(message);
				}
                else
                {
                    CString errorMessage; 
                    Utilities::FormatErrorMessage(error, errorMessage); 
                    CString message; 
                    message.AppendFormat(TEXT("Copy of file failed with error %s on file %s"), 
                        errorMessage, sourceFile); 
                    throw new CHoboCopyException(message);
                }
            }
            else
            {
                CString message; 
                message.AppendFormat(TEXT("Copied file %s to %s"), sourceFile, destinationFile); 
                OutputWriter::WriteLine(message, VERBOSITY_THRESHOLD_IF_VERBOSE); 
                ++_fileCount; 

                try
                {
                    _byteCount += Utilities::GetFileSize(sourceFile); 
                }
                catch (CHoboCopyException* x)
                {
                    CString message; 
                    message.AppendFormat(TEXT("Unable to calculate size of file. Size calculations may be incorrect. Message was: %s"), 
                        x->get_Message()); 
                    OutputWriter::WriteLine(message, VERBOSITY_THRESHOLD_UNLESS_SILENT); 
                    delete x; 
                }
            }
        }
        else
        {
            CString message;
            message.AppendFormat(TEXT("Skipping file %s because it doesn't meet filter criteria."), path); 
            OutputWriter::WriteLine(message); 
            ++_skipCount; 
        }

    }

private:
    bool IsFileMatch(CString& file)
    {
        for (unsigned int iFilter = 0; iFilter < _filters.size(); ++iFilter)
        {
            if (!_filters[iFilter]->IsFileMatch(file))
            {
                return false; 
            }
        }

        return true; 

    }

};