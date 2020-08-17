
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



FARPROC func_resolve(LPCSTR dll, LPCSTR func);
BreakPoint *find_bp(LPVOID addr, BreakPoints *bps);
int bp_set(LPVOID addr, BreakPoints *bps);
void init_bps(BreakPoints *bps);
int bp_del(BreakPoint *bp, BreakPoints *bps);
