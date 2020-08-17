#include <Windows.h>
#include <stdio.h>
#include "breakpoint.h"


extern int _log(char *fmt, ...);
extern int _err(char *msg);


int          debugger_active = 1;
int          first_break_hit = 0;

DWORD        pid  = 0;
HANDLE       h_process;
HANDLE       h_thread;
CONTEXT      context;
BreakPoints *bps;
LPVOID       restore = NULL;



DWORD exception_handler_breakpoint(EXCEPTION_DEBUG_INFO *info)
{
LPVOID exception_addr = info -> ExceptionRecord.ExceptionAddress;
BreakPoint *current;

    if((current = find_bp(exception_addr, bps)) != NULL){
        _log("our breakpoint\n");
        _log("restoring original byte\n");

        if(!WriteProcessMemory(h_process, current -> addr, current -> original_byte, 1, NULL))
            _err("WriteProcessMemory");

        context.Rip -= 1;
        context.EFlags = 256;

        if(SetThreadContext(h_thread, &context) == 0){
            _err("SetThreadContext");
        }

        if(current -> restore){
            restore = current -> addr;
        }

        bp_del(current, bps);

    }
    else{
        if(!first_break_hit){
            _log("First Windows Breakpoint\n");
            first_break_hit = 1;
        }
        else{
            _log("unknown breakpoint detected\n");
        }
    }


    return DBG_CONTINUE;
}



DWORD exception_handler_single_step(EXCEPTION_DEBUG_INFO *info)
{
    if(restore != NULL){
        bp_set(restore, bps);
        restore = NULL;
    }
    return DBG_CONTINUE;
}



DWORD event_handler_exception(DEBUG_EVENT *dbg)
{
DWORD   continue_status = DBG_CONTINUE;
EXCEPTION_DEBUG_INFO info = dbg -> u.Exception;
    _log("[EVENT] Exception at0x %p\n", info.ExceptionRecord.ExceptionAddress);

    //printf("Exception at %p   first-chance = %ld\n",
    //info.ExceptionRecord.ExceptionAddress, info.dwFirstChance);

    switch(info.ExceptionRecord.ExceptionCode){
        case EXCEPTION_ACCESS_VIOLATION:
            _log("    [!] Access violation.\n");
            continue_status = DBG_EXCEPTION_NOT_HANDLED;
            break;

        case EXCEPTION_BREAKPOINT:
            _log("    [*] Breakpoint.\n");
            continue_status = exception_handler_breakpoint(&info);
            break;

        case EXCEPTION_GUARD_PAGE:
            _log("    [!] Guard page.\n");
            continue_status = DBG_EXCEPTION_NOT_HANDLED;
            break;

        case EXCEPTION_SINGLE_STEP:
            _log("    [*] Single step.\n");
            continue_status = exception_handler_single_step(&info);
            break;

        default:
            _log("    [!] not handled exception\n");
            continue_status = DBG_EXCEPTION_NOT_HANDLED;
            break;
    }

    return continue_status;

}




DWORD event_handler_create_thread(DEBUG_EVENT *dbg)
{
CREATE_THREAD_DEBUG_INFO info = dbg->u.CreateThread;

    _log("[EVENT] Thread (handle: 0x%p  id: %ld) created at: 0x%p\n", info.hThread, dbg->dwThreadId, info.lpStartAddress);

    return DBG_CONTINUE;
}



DWORD event_handler_create_process(DEBUG_EVENT *dbg)
{
CREATE_PROCESS_DEBUG_INFO info = dbg->u.CreateProcessInfo;

    _log("[EVENT] Create Process: at 0x%p\n", info.lpStartAddress);
    return DBG_CONTINUE;
}



DWORD event_handler_exit_thread(DEBUG_EVENT *dbg)
{
EXIT_THREAD_DEBUG_INFO info = dbg->u.ExitThread;
    _log("[EVENT] Thread %ld exited with code: %ld\n", dbg->dwThreadId, info.dwExitCode);
    return DBG_CONTINUE;
}



DWORD event_handler_exit_process(DEBUG_EVENT *dbg)
{
EXIT_PROCESS_DEBUG_INFO info = dbg-> u.ExitProcess;
    _log("[EVENT] Process exited with code: 0x%lx\n", info.dwExitCode);
    debugger_active = 0;
    return DBG_CONTINUE;
}



DWORD event_handler_load_dll(DEBUG_EVENT *dbg)
{
LOAD_DLL_DEBUG_INFO info = dbg-> u.LoadDll;

    _log("[EVENT] Load DLL: at 0x%p\n", info.lpBaseOfDll);
    return DBG_CONTINUE;
}



DWORD event_handler_unload_dll(DEBUG_EVENT *dbg)
{
//UNLOAD_DLL_DEBUG_INFO info = dbg->u.UnloadDll;
    _log("[EVENT] Unload DLL:\n");
    return DBG_CONTINUE;
}



DWORD event_handler_output_debug_string(DEBUG_EVENT *dbg)
{
OUTPUT_DEBUG_STRING_INFO  info = dbg->u.DebugString;
WCHAR msg[info.nDebugStringLength];

    ReadProcessMemory(h_process, info.lpDebugStringData, msg, info.nDebugStringLength, NULL);

    _log("[EVENT] Output string.\n");

    if(info.fUnicode)
        printf("Output String: %hs\n", msg);

    return DBG_CONTINUE;
}



DWORD event_handler_rip_event(DEBUG_EVENT *dbg)
{
//RIP_INFO info = dbg ->u.RipInfo;
    _log("[EVENT] Rip event\n");
    return DBG_CONTINUE;
}


int get_debug_event(void)
{
DEBUG_EVENT   dbg;
DWORD         continue_status = DBG_CONTINUE;


    if(WaitForDebugEvent(&dbg, INFINITE) == 0)
        return 1;

    h_thread = OpenThread(THREAD_ALL_ACCESS, 0, dbg.dwThreadId);

    context.ContextFlags = CONTEXT_FULL;
    if(GetThreadContext(h_thread, &context) == 0){
        _err("GetThreadContext");
        return 1;
    }

    //printf("ExceptionCode = %ld  Thread ID = %ld\n", dbg.dwDebugEventCode, dbg.dwThreadId);
    switch(dbg.dwDebugEventCode){
        case EXCEPTION_DEBUG_EVENT:
            continue_status = event_handler_exception(&dbg);
            break;

        case CREATE_THREAD_DEBUG_EVENT:
            continue_status = event_handler_create_thread(&dbg);
            break;

        case CREATE_PROCESS_DEBUG_EVENT:
            continue_status = event_handler_create_process(&dbg);
            break;

        case EXIT_THREAD_DEBUG_EVENT:
            continue_status = event_handler_exit_thread(&dbg);
            break;

        case EXIT_PROCESS_DEBUG_EVENT:
            continue_status = event_handler_exit_process(&dbg);
            break;

        case LOAD_DLL_DEBUG_EVENT:
            continue_status = event_handler_load_dll(&dbg);
            break;

        case UNLOAD_DLL_DEBUG_EVENT:
            continue_status = event_handler_unload_dll(&dbg);
            break;

        case OUTPUT_DEBUG_STRING_EVENT:
            continue_status = event_handler_output_debug_string(&dbg);
            break;

        case RIP_EVENT:
            continue_status = event_handler_rip_event(&dbg);
            break;

        default :  break;
    }

    CloseHandle(h_thread);
    FlushInstructionCache(h_process, 0, 0);
    ContinueDebugEvent(dbg.dwProcessId, dbg.dwThreadId, continue_status);
    return 0;
}


int do_debug(DWORD num){

    pid = num;

    if((h_process = OpenProcess(PROCESS_ALL_ACCESS, 0, pid)) == 0){
        _err("OpenProcess");
        return 1;
    }

    if(DebugActiveProcess(pid) == 0){
        _err("DebugActiveProcess");
    }
    _log("attach pid = %ld\n", pid);


    bps = (BreakPoints *)malloc(sizeof(BreakPoints));
    init_bps(bps);

    FARPROC addr = func_resolve("msvcrt.dll", "printf");
    bp_set(addr, bps);


    while(debugger_active){
        if(get_debug_event() == 1)
            break;
    }

    DebugActiveProcessStop(pid);
    _log("Detach from pid = %ld\n", pid);
    free(bps);

    return 0;

}
