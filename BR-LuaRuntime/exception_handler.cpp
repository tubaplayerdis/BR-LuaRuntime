#include <iomanip>
#include <ios>
#include <iosfwd>
#include <iostream>
#include <string>
#include <unordered_map>
#include <windows.h>
#include <sstream>

//#define SHIPPING

static std::string Hex(DWORD code) {
    std::ostringstream ss;
    ss << "0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << code;
    return ss.str();
}

std::string GetExceptionName(DWORD code) {
    static const std::unordered_map<DWORD, std::string> ntstatusMap = {
        // Success / Informational
        {0x00000000, "STATUS_SUCCESS"},
        {0x00000001, "STATUS_WAIT_0"},
        {0x00000080, "STATUS_ABANDONED"},
        {0x00000102, "STATUS_TIMEOUT"},
        {0x00000103, "STATUS_PENDING"},
        {0x40000000, "STATUS_OBJECT_NAME_EXISTS"},
        {0x40000001, "STATUS_THREAD_WAS_SUSPENDED"},
        {0x40000004, "STATUS_RXACT_STATE_CREATED"},
        {0x40000015, "STATUS_FATAL_APP_EXIT"},
        {0x4000001A, "STATUS_NO_MORE_ENTRIES"},
        {0x40010001, "DBG_EXCEPTION_HANDLED"},
        {0x40010002, "DBG_CONTINUE"},
        // Warning / Informational (examples)
        {0x80000001, "STATUS_GUARD_PAGE_VIOLATION"},
        {0x80000002, "STATUS_DATATYPE_MISALIGNMENT"},
        {0x80000003, "STATUS_BREAKPOINT"},
        {0x80000004, "STATUS_SINGLE_STEP"},
        {0x80000005, "STATUS_BUFFER_OVERFLOW"},
        {0x80000006, "STATUS_NO_MORE_FILES"},
        {0x8000000E, "STATUS_DEVICE_PAPER_EMPTY"},
        {0x80000010, "STATUS_DEVICE_OFF_LINE"},
        {0x80000011, "STATUS_DEVICE_BUSY"},
        // Common Errors
        {0xC0000001, "STATUS_UNSUCCESSFUL"},
        {0xC0000002, "STATUS_NOT_IMPLEMENTED"},
        {0xC0000003, "STATUS_INVALID_INFO_CLASS"},
        {0xC0000004, "STATUS_INFO_LENGTH_MISMATCH"},
        {0xC0000005, "STATUS_ACCESS_VIOLATION"},
        {0xC0000006, "STATUS_IN_PAGE_ERROR"},
        {0xC0000008, "STATUS_INVALID_HANDLE"},
        {0xC000000D, "STATUS_INVALID_PARAMETER"},
        {0xC000000F, "STATUS_NO_SUCH_FILE"},
        {0xC0000017, "STATUS_NO_MEMORY"},
        {0xC0000019, "STATUS_NOT_MAPPED_VIEW"},
        {0xC000001E, "STATUS_ILLEGAL_INSTRUCTION"},
        {0xC0000022, "STATUS_ACCESS_DENIED"},
        {0xC0000023, "STATUS_BUFFER_TOO_SMALL"},
        {0xC0000025, "STATUS_ALREADY_COMMITTED"},
        {0xC0000034, "STATUS_OBJECT_NAME_NOT_FOUND"},
        // File / I/O
        {0xC000003A, "STATUS_OBJECT_PATH_NOT_FOUND"},
        {0xC0000035, "STATUS_OBJECT_PATH_SYNTAX_BAD"},
        {0xC000003C, "STATUS_NOT_A_DIRECTORY"},
        {0xC000003E, "STATUS_DIRECTORY_NOT_EMPTY"},
        {0xC0000043, "STATUS_TOO_MANY_OPENED_FILES"},
        {0xC00000BB, "STATUS_NOT_SUPPORTED"},
        // Memory / Virtual Memory
        {0xC000009A, "STATUS_INSUFFICIENT_RESOURCES"},
        {0xC00000BB, "STATUS_NOT_SUPPORTED"},
        // Security / Privileges
        {0xC0000061, "STATUS_PRIVILEGE_NOT_HELD"},
        {0xC0000062, "STATUS_TOO_MANY_SESSIONS"},
        // RPC / Network
        {0xC0000202, "STATUS_REMOTE_NOT_LISTENING"},
        {0xC0000225, "STATUS_NOT_FOUND"},
        // Device / Driver
        {0xC00000E9, "STATUS_DEVICE_NOT_CONNECTED"},
        {0xC00000EA, "STATUS_DEVICE_POWER_FAILURE"},
        // Debug / Special
        {0xC000013A, "STATUS_CONTROL_C_EXIT"},
        {0xC0000142, "STATUS_DLL_INIT_FAILED"},
        // Add more mappings here as needed...
        // The user's original exception constants (Windows SEH)
        {EXCEPTION_ACCESS_VIOLATION, "EXCEPTION_ACCESS_VIOLATION (STATUS_ACCESS_VIOLATION)"},
        {EXCEPTION_ARRAY_BOUNDS_EXCEEDED, "EXCEPTION_ARRAY_BOUNDS_EXCEEDED"},
        {EXCEPTION_BREAKPOINT, "EXCEPTION_BREAKPOINT"},
        {EXCEPTION_DATATYPE_MISALIGNMENT, "EXCEPTION_DATATYPE_MISALIGNMENT"},
        {EXCEPTION_FLT_DENORMAL_OPERAND, "EXCEPTION_FLT_DENORMAL_OPERAND"},
        {EXCEPTION_FLT_DIVIDE_BY_ZERO, "EXCEPTION_FLT_DIVIDE_BY_ZERO"},
        {EXCEPTION_INT_DIVIDE_BY_ZERO, "EXCEPTION_INT_DIVIDE_BY_ZERO"},
        {EXCEPTION_ILLEGAL_INSTRUCTION, "EXCEPTION_ILLEGAL_INSTRUCTION"},
        {EXCEPTION_IN_PAGE_ERROR, "EXCEPTION_IN_PAGE_ERROR"},
        {EXCEPTION_STACK_OVERFLOW, "EXCEPTION_STACK_OVERFLOW"},
    };

    auto it = ntstatusMap.find(code);
    if (it != ntstatusMap.end()) {
        return it->second;
    }

    // Fallback: return hex + generic label
    return std::string("UNKNOWN NTSTATUS ") + Hex(code);
}

#include <fstream>

LONG WINAPI UpgradedExceptionHandler(PEXCEPTION_POINTERS ExceptionInfo) {
    PEXCEPTION_RECORD record = ExceptionInfo->ExceptionRecord;

    #ifdef SHIPPING
    std::cerr << "DLL EXCEPTION!: ";

    // 1. Basic Exception Info
    std::cerr << "Exception Code: 0x" << std::hex << std::uppercase << record->ExceptionCode
        << " -> " << GetExceptionName(record->ExceptionCode) << "\n";
    #endif

    //DUMP THE CONSOLE
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole == INVALID_HANDLE_VALUE) return EXCEPTION_CONTINUE_SEARCH;
    // 2. Query the current size and layout of the console window
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(hConsole, &csbi)) {
        // Calculate total characters up to the current cursor position
        // This avoids dumping thousands of trailing empty lines
        DWORD totalChars = csbi.dwCursorPosition.Y * csbi.dwSize.X + csbi.dwCursorPosition.X;

        if (totalChars > 0) {
            std::vector<char> buffer(totalChars);
            COORD readCoord = { 0, 0 };
            DWORD charsRead = 0;

            // 3. Read the actual characters from the OS console buffer
            if (ReadConsoleOutputCharacterA(hConsole, buffer.data(), totalChars, readCoord, &charsRead)) {
                // 4. Write the raw screen text directly to a crash log file
                std::ofstream dumpFile("console_crash_dump.txt", std::ios::out | std::ios::binary);
                if (dumpFile.is_open()) {
                    // Loop through data and insert newlines based on console width
                    for (DWORD i = 0; i < charsRead; ++i) {
                        dumpFile.put(buffer[i]);
                        if ((i + 1) % csbi.dwSize.X == 0) {
                            dumpFile.put('\n'); // Maintain line breaks
                        }
                    }
                    dumpFile.close();
                }
            }
        }
    }

    return EXCEPTION_CONTINUE_SEARCH;
}