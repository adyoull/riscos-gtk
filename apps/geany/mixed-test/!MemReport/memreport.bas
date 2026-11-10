10 REM MemReport: tasks' slots and dynamic areas' sizes, to <Wimp$ScrapDir>.MemReport
20 DIM n% 64
30 o%=OPENOUT("<Wimp$ScrapDir>.MemReport")
40 BPUT#o%,"MemReport at "+TIME$
50 BPUT#o%,"-- tasks (slot)"
60 h%=0
70 REPEAT
80   SYS "TaskManager_EnumerateTasks",h%,n%,16 TO h%,r%
90   IF r%>n% THEN BPUT#o%,FNs(n%!4)+": "+STR$(n%!8 DIV 1024)+"K"
100 UNTIL h%<0
110 BPUT#o%,"-- dynamic areas (size)"
120 h%=-1
130 REPEAT
140   SYS "OS_DynamicArea",3,h% TO ,h%
150   IF h%<>-1 THEN SYS "OS_DynamicArea",2,h% TO ,,sz%,,,,,,nm%:BPUT#o%,FNs(nm%)+": "+STR$(sz% DIV 1024)+"K"
160 UNTIL h%=-1
170 CLOSE#o%
180 SYS "OS_File",18,"<Wimp$ScrapDir>.MemReport",&FFF
190 DIM c% 512
200 SYS "OS_FSControl",37,"<Wimp$ScrapDir>.MemReport",c%,0,0,512
210 OSCLI "Filer_Run "+FNs(c%)
220 END
230 DEF FNs(p%)
240 LOCAL s$:WHILE ?p%>=32:s$+=CHR$?p%:p%+=1:ENDWHILE
250 =s$
