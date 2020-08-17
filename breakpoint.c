#include <stdio.h>
#include <stdlib.h>
#include <windows.h>


extern int _log(char *fmt, ...);
extern int _err(char *msg);

extern HANDLE h_process;


typedef struct _BreakPoint {
    struct _BreakPoint *prev;
    LPVOID addr;
    WCHAR  original_byte[1];
    int    restore;
    struct _BreakPoint *next;
}BreakPoint;


typedef struct _BreakPoints {
    BreakPoint *head;
    BreakPoint *tail;
}BreakPoints;




FARPROC func_resolve(LPCSTR dll, LPCSTR func)
{
HANDLE   handle;
LPCVOID  address;

    if((handle = LoadLibraryA(dll)) == 0){
        _err("LoadLibraryA");
        return NULL;
    }
    if((address = GetProcAddress(handle, func)) == 0){
        _err("GetProcAddress");
        return NULL;
    }
    FreeLibrary(handle);
    return address;
}



BreakPoint *find_bp(LPVOID addr, BreakPoints *bps)
{
BreakPoint *current = bps -> head;

    while (1) {
        if(current -> addr == addr)
            return current;

        else if(current -> next != NULL)
            current = current -> next;

        else
            break;

    }
    return NULL;
}




void init_bps(BreakPoints *bps)
{
    bps -> head = NULL;
    bps -> tail = NULL;
}



int bp_del(BreakPoint *bp, BreakPoints *bps)
{

    if(bp == bps -> head){
        bps -> head = bp -> next;

        if(bp -> next != NULL)
            bp -> next -> prev = NULL;
    }

    else if(bp == bps -> tail){
        bps -> tail = bp -> prev;
        bp -> prev -> next = NULL;
    }
    else{
        bp -> prev -> next = bp -> next;
        bp -> next -> prev = bp -> prev;
    }
    free(bp);

    return 0;
}


int bp_set(LPVOID addr, BreakPoints *bps)
{

BreakPoint *bp = (BreakPoint *)malloc(sizeof(BreakPoint));

    if(!ReadProcessMemory(h_process, addr, bp -> original_byte, 1, NULL)){
        _err("ReadProcessMemory");
        free(bp);
        return 1;
    }

    if(!WriteProcessMemory(h_process, addr, "\xCC", 1, NULL)){
        _err("WriteProcessMemory");
        free(bp);
        return 1;
    }

    bp -> addr = addr;
    bp -> next = NULL;
    bp -> restore = 1;

    if(bps -> head == NULL){
        bp -> prev  = NULL;
        bps -> head = bp;
    }
    else{
        bp -> prev = bps -> tail;
        bps -> tail -> next = bp;
    }

    bps -> tail = bp;

    return 0;

}
