#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <Windows.h>
#include "debug.h"

void _log(char *fmt, ...)
{
int log = 1;
    if(log){
        va_list args;
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
    }
}


void _err(char *msg)
{
int err = 1;
    if(err){
        fprintf(stderr, "error: %s erno = %ld\n", msg, GetLastError());
    }
}


int get_debug_privileges()
{
HANDLE            h_token;
LUID              luid;
TOKEN_PRIVILEGES  token_state;

    if(OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &h_token) == 0){
        _err("OpenProcessToken");
        return 1;
    }

    if(LookupPrivilegeValueA(0, "SeDebugPrivilege", &luid) == 0){
        _err("LookupPrivilegeValueA");
        CloseHandle(h_token);
        return 1;
    }

    token_state.PrivilegeCount = 1;
    token_state.Privileges[0].Luid = luid;
    token_state.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if(AdjustTokenPrivileges(h_token, 0, &token_state, 0, 0, 0) == 0){
        _err("AdjustTokenPrivileges");
        CloseHandle(h_token);
        return 1;
    }
    return 0;
}



int main(int argc, char **argv)
{

    if(argc < 2){
        fprintf(stderr, "pid?\n");
        return 1;
    }

    get_debug_privileges();
    do_debug(atoi(argv[1]));

    return 0;
}
