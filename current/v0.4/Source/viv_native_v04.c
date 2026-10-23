#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdint.h>
#include <wchar.h>
#include <stdlib.h>
#include <string.h>
#include "payload_v04.h"
#define BS (8*1024*1024)
#define WM_STAGE (WM_APP+1)
#define WM_DONE (WM_APP+2)
static HWND mainwin,inbox,outbox,bar,statusbox,startbtn,cancelbtn,binbtn,boutbtn,autobox,helpbox;
static int auto_install=1;
static wchar_t backup_path[32768];
static volatile LONG stopflag=0;
static int busy=0;
static wchar_t inputpath[32768],outputpath[32768];
static const wchar_t* stages[]={L"正在定位资源…",L"正在复制原始 VIV…",L"正在写入并校验资源…"};
static int err(wchar_t* e,const wchar_t* s){wcscpy(e,s);return 0;}
static void progress(int stage,uint64_t done,uint64_t total){if(mainwin)PostMessageW(mainwin,WM_STAGE,stage,total?(LPARAM)(done*100/total):0);}
static int seekto(HANDLE f,uint64_t pos){LARGE_INTEGER n;n.QuadPart=pos;return SetFilePointerEx(f,n,NULL,FILE_BEGIN);}
static int readexact(HANDLE f,void* b,DWORD size){DWORD n;return ReadFile(f,b,size,&n,NULL)&&n==size;}
static int writeexact(HANDLE f,const void* b,DWORD size){DWORD n;return WriteFile(f,b,size,&n,NULL)&&n==size;}
static int samefile(const wchar_t*a,const wchar_t*b){wchar_t x[32768],y[32768];return GetFullPathNameW(a,32768,x,NULL)&&GetFullPathNameW(b,32768,y,NULL)&&_wcsicmp(x,y)==0;}
static uint32_t be32(const unsigned char*p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static void putbe(unsigned char*p,uint32_t n){p[0]=n>>24;p[1]=n>>16;p[2]=n>>8;p[3]=n;}
static uint64_t align2048(uint64_t n){return (n+2047)&~(uint64_t)2047;}
static int merge_file(const wchar_t*src,const wchar_t*dst,wchar_t*error){
 HANDLE in=INVALID_HANDLE_VALUE,out=INVALID_HANDLE_VALUE;unsigned char*buf=NULL,*candidate=NULL;wchar_t*temp=NULL;
 int owns=0,ok=0;LARGE_INTEGER total;DWORD got;uint64_t done=0;unsigned char hdr[16],record[12];
 uint32_t indices[3]={0},offsets[3]={0},stored[3]={0};int counts[3]={0},states[3]={0};
 uint64_t new_offsets[3]={0},archive_sizes[3]={0};
 if(samefile(src,dst))return err(error,L"输出不能与原始文件相同。请选择另存路径。");
 if(GetFileAttributesW(dst)!=INVALID_FILE_ATTRIBUTES)return err(error,L"输出文件已存在，请选择新的文件名。");
 temp=malloc((wcslen(dst)+16)*sizeof(wchar_t));if(!temp)return err(error,L"内存不足。");wcscpy(temp,dst);wcscat(temp,L".partial");
 in=CreateFileW(src,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,NULL);
 if(in==INVALID_HANDLE_VALUE){err(error,L"无法读取原始 VIV，请关闭占用该文件的修改工具。");goto end;}
 if(!GetFileSizeEx(in,&total)||total.QuadPart<24||total.QuadPart>0xffffffffULL||!readexact(in,hdr,16)||memcmp(hdr,"BIGH",4)){
  err(error,L"不是支持的 Visceral BIGH 文件，或超过 32 位长度范围。");goto end;}
 uint32_t count=be32(hdr+8),header_end=be32(hdr+12);
 if(count>2000000||(uint64_t)16+12ULL*count+8>header_end||header_end>(uint64_t)total.QuadPart){err(error,L"VIV 索引边界无效。");goto end;}
 for(uint32_t i=0;i<count;i++){
  if(!readexact(in,record,12)){err(error,L"VIV 索引读取失败。");goto end;}
  uint32_t offset=be32(record),size=be32(record+4),hash=be32(record+8);
  if(offset<header_end||(uint64_t)offset+size>(uint64_t)total.QuadPart){err(error,L"VIV 索引中的资源越界。");goto end;}
  for(int k=0;k<3;k++)if(hash==resource_hashes[k]){counts[k]++;indices[k]=i;offsets[k]=offset;stored[k]=size;}
 }
 DWORD maxsize=0;for(int k=0;k<3;k++){if(counts[k]!=1){err(error,L"VIV 必须包含唯一的英文语言、字体流和配套 TOC 条目。");goto end;}if(stored[k]>maxsize)maxsize=stored[k];}
 /* Bound reads to known payload sizes. No arbitrary allocation from an untrusted archive. */
 for(int a=0;a<3;a++)for(int b=a+1;b<3;b++)if((uint64_t)offsets[a]<((uint64_t)offsets[b]+stored[b])&&(uint64_t)offsets[b]<((uint64_t)offsets[a]+stored[a])){err(error,L"目标资源范围重叠，已停止。");goto end;}
 DWORD knownmax=0;for(int k=0;k<3;k++){if(old_sizes[k]>knownmax)knownmax=old_sizes[k];if(new_sizes_data[k]>knownmax)knownmax=new_sizes_data[k];}
 if(maxsize>knownmax){err(error,L"资源大小不属于支持版本。未修改原文件。");goto end;}
 buf=malloc(BS);candidate=malloc(knownmax);if(!buf||!candidate){err(error,L"内存不足。");goto end;}
 for(int k=0;k<3;k++){
  if(!seekto(in,offsets[k])||!readexact(in,candidate,stored[k])){err(error,L"读取目标资源失败。");goto end;}
  if(stored[k]==new_sizes_data[k]&&!memcmp(candidate,newdata[k],new_sizes_data[k]))states[k]=3;
  else if(stored[k]==old_sizes[k]&&!memcmp(candidate,olddata[k],old_sizes[k]))states[k]=1;
  else if(previous[k]&&stored[k]==old_sizes[k]&&!memcmp(candidate,previous[k],old_sizes[k]))states[k]=2;
  else{err(error,L"目标条目存在，但内容不匹配原版或已知补丁。为避免破坏其他版本，已停止。");goto end;}
  progress(0,k+1,3);
 }
 if(states[0]==3&&states[1]==3&&states[2]==3){err(error,L"此 VIV 已包含当前版本，无需重复合并。");goto end;}
 uint64_t projected=total.QuadPart;
 for(int k=0;k<3;k++){projected=align2048(projected);new_offsets[k]=projected;archive_sizes[k]=align2048(new_sizes_data[k]);projected+=archive_sizes[k];}
 if(projected>0xffffffffULL){err(error,L"生成文件将超过 BIGH/FAT32 单文件范围，已停止。");goto end;}
 out=CreateFileW(temp,GENERIC_READ|GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
 if(out==INVALID_HANDLE_VALUE){err(error,L"无法创建输出文件。请检查权限、空间或已有同名 .partial 文件。");goto end;}owns=1;
 if(!seekto(in,0)){err(error,L"无法重新读取原始文件。");goto end;}
 while(ReadFile(in,buf,BS,&got,NULL)){
  if(!got)break;if(InterlockedCompareExchange(&stopflag,0,0)){err(error,L"已取消，原文件未修改。");goto end;}
  if(!writeexact(out,buf,got)){err(error,L"复制失败，请检查磁盘空间。");goto end;}done+=got;progress(1,done,total.QuadPart);
 }
 if(done!=(uint64_t)total.QuadPart){err(error,L"原文件复制不完整。");goto end;}
 unsigned char zeros[2048]={0};uint64_t at=total.QuadPart;
 for(int k=0;k<3;k++){
  if(InterlockedCompareExchange(&stopflag,0,0)){err(error,L"已取消，原文件未修改。");goto end;}
  if(!seekto(out,at)||!writeexact(out,zeros,(DWORD)(new_offsets[k]-at))||!writeexact(out,newdata[k],new_sizes_data[k])||!writeexact(out,zeros,(DWORD)(archive_sizes[k]-new_sizes_data[k]))){err(error,L"追加汉化资源失败。");goto end;}
  at=new_offsets[k]+archive_sizes[k];putbe(record,(uint32_t)new_offsets[k]);putbe(record+4,(uint32_t)archive_sizes[k]);putbe(record+8,resource_hashes[k]);
  if(!seekto(out,16ULL+12ULL*indices[k])||!writeexact(out,record,12)){err(error,L"VIV 索引更新失败。");goto end;}
 }
 uint32_t length_le=(uint32_t)projected;
 if(!seekto(out,4)||!writeexact(out,&length_le,4)||!FlushFileBuffers(out)){err(error,L"VIV 文件长度或保存失败。");goto end;}
 LARGE_INTEGER actual;if(!GetFileSizeEx(out,&actual)||actual.QuadPart!=(long long)projected){err(error,L"输出文件总长度校验失败。");goto end;}
 for(int k=0;k<3;k++){
  if(!seekto(out,16ULL+12ULL*indices[k])||!readexact(out,record,12)||be32(record)!=new_offsets[k]||be32(record+4)!=archive_sizes[k]||be32(record+8)!=resource_hashes[k]){err(error,L"输出 VIV 索引校验失败。");goto end;}
  /* Full, byte-for-byte verification of every appended payload. */
  uint64_t checked=0;if(!seekto(out,new_offsets[k])){err(error,L"无法校验输出资源。");goto end;}
  while(checked<new_sizes_data[k]){DWORD n=(DWORD)((new_sizes_data[k]-checked)>BS?BS:(new_sizes_data[k]-checked));if(!readexact(out,buf,n)||memcmp(buf,newdata[k]+checked,n)){err(error,L"输出汉化资源校验失败。");goto end;}checked+=n;}
 }
 if(InterlockedCompareExchange(&stopflag,0,0)){err(error,L"已取消，原文件未修改。");goto end;}
 CloseHandle(out);out=INVALID_HANDLE_VALUE;
 if(!MoveFileExW(temp,dst,MOVEFILE_WRITE_THROUGH)){err(error,L"输出发布失败，请检查文件占用或同名文件。");goto end;}owns=0;ok=1;progress(2,100,100);
end:
 if(out!=INVALID_HANDLE_VALUE)CloseHandle(out);if(in!=INVALID_HANDLE_VALUE)CloseHandle(in);if(owns)DeleteFileW(temp);free(temp);free(buf);free(candidate);return ok;
}

static int install_generated(const wchar_t*src,const wchar_t*dst,wchar_t*backup,wchar_t*error){
 SYSTEMTIME t;GetLocalTime(&t);wchar_t suffix[128];int available=0;
 if(wcslen(src)>32500)return err(error,L"原文件路径过长，无法生成备份名称。已保留生成的输出。");
 for(int i=0;i<1000;i++){
  swprintf(suffix,128,L".original.%04d%02d%02d_%02d%02d%02d_%03d.bak",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,i);
  wcscpy(backup,src);wcscat(backup,suffix);
  if(GetFileAttributesW(backup)==INVALID_FILE_ATTRIBUTES){available=1;break;}
 }
 if(!available)return err(error,L"无法分配唯一备份文件名。原文件与生成文件均未修改。");
 if(!MoveFileExW(src,backup,MOVEFILE_WRITE_THROUGH))return err(error,L"原文件自动改名失败。请检查目录权限或文件占用；原文件和生成文件均已保留。");
 if(!MoveFileExW(dst,src,MOVEFILE_WRITE_THROUGH)){
  if(MoveFileExW(backup,src,MOVEFILE_WRITE_THROUGH))return err(error,L"自动替换失败，已恢复原文件名称。生成文件已保留；请将输出位置选在原文件同一磁盘后重试。");
  swprintf(error,4096,L"自动替换失败，原文件已保留在以下备份位置，但无法恢复原名：\n%ls\n请手动将备份恢复为原文件名。生成文件仍保留。",backup);return 0;
 }
 return 1;
}
static DWORD WINAPI worker(LPVOID unused){
 wchar_t*message=calloc(1,4096*sizeof(wchar_t));WIN32_FILE_ATTRIBUTE_DATA before,after;
 int have_before=GetFileAttributesExW(inputpath,GetFileExInfoStandard,&before);
 int ok=merge_file(inputpath,outputpath,message);backup_path[0]=0;
 if(ok&&auto_install){
  if(InterlockedCompareExchange(&stopflag,0,0))ok=err(message,L"已取消自动安装。原文件未修改，已生成的修改版保留在输出路径。");
  else if(!have_before||!GetFileAttributesExW(inputpath,GetFileExInfoStandard,&after)||
    before.nFileSizeHigh!=after.nFileSizeHigh||before.nFileSizeLow!=after.nFileSizeLow||
    CompareFileTime(&before.ftLastWriteTime,&after.ftLastWriteTime)!=0)
    ok=err(message,L"源文件信息发生变化，已停止自动安装；原文件和生成文件均保留。");
  else ok=install_generated(inputpath,outputpath,backup_path,message);
 }
 PostMessageW(mainwin,WM_DONE,ok,(LPARAM)message);return 0;
}
static void choosein(void){OPENFILENAMEW o={0};wchar_t p[32768]={0};o.lStructSize=sizeof(o);o.hwndOwner=mainwin;o.lpstrFilter=L"VIV 文件\0*.viv\0所有文件\0*.*\0";o.lpstrFile=p;o.nMaxFile=32768;o.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
 if(GetOpenFileNameW(&o)){if(wcslen(p)>32500){MessageBoxW(mainwin,L"路径过长，请将文件移至较短路径。",L"路径检查",MB_OK);return;}SetWindowTextW(inbox,p);wchar_t*d=wcsrchr(p,L'.');if(d)wcscpy(d,L"_zh.viv");else wcscat(p,L"_zh.viv");SetWindowTextW(outbox,p);}}
static void chooseout(void){OPENFILENAMEW o={0};wchar_t p[32768]=L"bigfile0_zh.viv";o.lStructSize=sizeof(o);o.hwndOwner=mainwin;o.lpstrFilter=L"VIV 文件\0*.viv\0";o.lpstrFile=p;o.nMaxFile=32768;o.lpstrDefExt=L"viv";o.Flags=OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;if(GetSaveFileNameW(&o))SetWindowTextW(outbox,p);}
static HWND control(const wchar_t*cls,const wchar_t*txt,DWORD style,int x,int y,int w,int h,int id){HWND a=CreateWindowExW(0,cls,txt,WS_CHILD|WS_VISIBLE|style,x,y,w,h,mainwin,(HMENU)(INT_PTR)id,GetModuleHandle(NULL),NULL);SendMessageW(a,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);return a;}
static void setbusy(int b){busy=b;EnableWindow(inbox,!b);EnableWindow(outbox,!b);EnableWindow(binbtn,!b);EnableWindow(boutbtn,!b);EnableWindow(startbtn,!b);EnableWindow(autobox,!b);EnableWindow(cancelbtn,b);}
static LRESULT CALLBACK proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){switch(msg){
 case WM_CREATE:
  mainwin=w;control(L"STATIC",L"Switch 死亡空间2 xbox360版 汉化工具",0,24,18,740,28,0);
  control(L"STATIC",L"工具 v0.4  |  全局译稿 6894 条 + 中文字库  |  待实机验证",0,24,50,740,24,0);
  control(L"STATIC",L"原始 VIV",0,24,88,740,20,0);inbox=control(L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL,24,112,630,27,0);binbtn=control(L"BUTTON",L"选择…",0,666,112,90,27,101);
  control(L"STATIC",L"生成文件（自动安装时用于暂存；需与原文件位于同一磁盘）",0,24,153,740,20,0);outbox=control(L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL,24,177,630,27,0);boutbtn=control(L"BUTTON",L"选择…",0,666,177,90,27,102);
  autobox=control(L"BUTTON",L"自动备份并安装：原文件改名保留，修改版使用原文件名",BS_AUTOCHECKBOX,24,220,730,24,105);SendMessageW(autobox,BM_SETCHECK,BST_CHECKED,0);
  bar=control(PROGRESS_CLASSW,L"",0,24,258,732,20,0);SendMessageW(bar,PBM_SETRANGE,0,MAKELPARAM(0,100));
  statusbox=control(L"STATIC",L"请选择与上传资源一致的 Disc 1 或 Disc 2 原始 bigfile0.viv。",0,24,291,732,35,0);
  startbtn=control(L"BUTTON",L"开始合并",0,24,335,160,32,103);cancelbtn=control(L"BUTTON",L"取消",0,196,335,85,32,104);EnableWindow(cancelbtn,FALSE);
  control(L"STATIC",L"详细使用说明（可滚动、选择和复制）",0,24,389,732,22,0);
  helpbox=control(L"EDIT",L"",WS_BORDER|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|WS_VSCROLL,24,417,732,230,0);
  SetWindowTextW(helpbox,
   L"【本版内容与状态】\r\n"
   L"已接入用户最终译稿：6894条索引，5253条与原文不同，1641条署名、标识等保留原文。不是旧版仅改“新游戏”的补丁。本版加入实际Unicode中文字形，不借用俄文字形。文本完整性与资源重建检查已通过；完整游戏覆盖、文字排版及扩展字库仍待实机验证。\r\n"
   L"\r\n"
   L"【一、选择原始文件】\r\n"
   L"选择对应光盘完整的bigfile0.viv。不能选择ZIP、单独的8CA715C3、STR或LH2。本版接受与已提供资源一致的原版，以及旧版“新游戏”补丁。遇到不匹配版本会停止。\r\n"
   L"\r\n"
   L"【二、生成与自动备份】\r\n"
   L"保持“自动备份并安装”勾选，选择同一磁盘上的新输出路径，点击“开始合并”。程序读取BIGH索引并校验语言、字体和TOC三项资源，然后复制原文件，追加资源，更新索引及文件长度，最后逐字节验证新资源。成功后才将原文件改名为bigfile0.viv.original.日期_时间_序号.bak，再将生成文件改成原文件名。备份不会覆盖。需要额外空间容纳一整份VIV。\r\n"
   L"\r\n"
   L"【三、仅另存修改版】\r\n"
   L"取消自动备份并安装勾选，只生成输出文件。手动使用前，请备份原版，再将输出文件改名为bigfile0.viv并放回对应光盘目录。\r\n"
   L"\r\n"
   L"【四、两张光盘】\r\n"
   L"Disc 1和Disc 2分别选择各自的bigfile0.viv，各执行一次。目标语言与字体资源相同，但整份VIV不可跨盘互相覆盖。程序仅接受三项资源均匹配的文件。\r\n"
   L"\r\n"
   L"【五、运行游戏】\r\n"
   L"游戏语言保持English。new game应显示“新游戏”。检查主菜单、物品栏、商店、任务日志及字幕是否显示中文。若方框、缺字、错位或崩溃，请保留错误日志与画面，并恢复原版；文件合并成功不等于实机显示已经验证。\r\n"
   L"\r\n"
   L"【六、恢复原版】\r\n"
   L"先关闭游戏。将当前修改版另行改名保留，再把同盘对应的.original.日期_时间_序号.bak恢复为bigfile0.viv。不要删除原版备份。\r\n"
   L"\r\n"
   L"【七、错误和取消】\r\n"
   L"不匹配资源、重复索引、越界资源、输出已存在或已安装当前版本时停止。生成阶段可取消。安装失败时尝试恢复原名，请按错误提示处理。不会覆盖现有输出或备份。超过BIGH/FAT32的32位文件长度范围时停止。\r\n"
   L"\r\n"
   L"【验证范围】\r\n"
   L"本版引擎经构造BIGH文件测试，验证索引、资源内容与自动备份。尚无完整原始VIV进行端到端验证，也未在本环境运行Windows界面或Switch游戏。附带检查报告、译稿、资源及源码，便于核查。\r\n");return 0;
 case WM_COMMAND:switch(LOWORD(wp)){
  case 101:choosein();break;case 102:chooseout();break;
  case 103:GetWindowTextW(inbox,inputpath,32768);GetWindowTextW(outbox,outputpath,32768);
   if(!*inputpath||!*outputpath){MessageBoxW(w,L"请先选择原始 VIV 和输出位置。",L"选择文件",MB_OK);break;}
   auto_install=SendMessageW(autobox,BM_GETCHECK,0,0)==BST_CHECKED;InterlockedExchange(&stopflag,0);setbusy(1);SendMessageW(bar,PBM_SETPOS,0,0);SetWindowTextW(statusbox,L"开始检查资源…");
   HANDLE t=CreateThread(NULL,0,worker,NULL,0,NULL);if(t)CloseHandle(t);else{setbusy(0);MessageBoxW(w,L"无法启动工作线程。",L"错误",MB_ICONERROR);}break;
  case 104:InterlockedExchange(&stopflag,1);SetWindowTextW(statusbox,L"正在取消，请稍候…");break;}return 0;
 case WM_STAGE:if(busy){SendMessageW(bar,PBM_SETPOS,lp,0);SetWindowTextW(statusbox,stages[wp<3?wp:0]);}return 0;
 case WM_DONE:{setbusy(0);wchar_t*e=(wchar_t*)lp;if(wp){
   SendMessageW(bar,PBM_SETPOS,100,0);wchar_t text[4096];
   if(auto_install){SetWindowTextW(statusbox,L"合并并安装成功。原文件已自动改名备份。");swprintf(text,4096,L"合并并安装成功。\n当前文件：\n%ls\n\n原版备份：\n%ls\n\n已接入全局译稿与中文字库。请进行实机显示验证。",inputpath,backup_path);}
   else{SetWindowTextW(statusbox,L"合并成功。原文件保持原名，修改版已另存。");swprintf(text,4096,L"已生成：\n%ls\n\n请按界面说明备份并替换对应盘文件。请按附带说明进行实机验证。",outputpath);}
   MessageBoxW(w,text,L"完成",MB_OK|MB_ICONINFORMATION);
  }else{SetWindowTextW(statusbox,e);MessageBoxW(w,e,L"操作未完成",MB_OK|MB_ICONERROR);}free(e);return 0;}
 case WM_CLOSE:if(busy){InterlockedExchange(&stopflag,1);SetWindowTextW(statusbox,L"正在取消，完成后可关闭窗口。");}else DestroyWindow(w);return 0;
 case WM_DESTROY:PostQuitMessage(0);return 0;
 }return DefWindowProcW(w,msg,wp,lp);}
int WINAPI wWinMain(HINSTANCE h,HINSTANCE prev,LPWSTR cmd,int show){INITCOMMONCONTROLSEX ic={sizeof(ic),ICC_PROGRESS_CLASS};InitCommonControlsEx(&ic);SetProcessDPIAware();WNDCLASSW wc={0};wc.lpfnWndProc=proc;wc.hInstance=h;wc.lpszClassName=L"DS2VivMerge";wc.hCursor=LoadCursor(NULL,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);RegisterClassW(&wc);HWND w=CreateWindowW(wc.lpszClassName,L"Switch 死亡空间2 xbox360版 汉化工具",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,800,700,NULL,NULL,h,NULL);ShowWindow(w,show);MSG m;while(GetMessageW(&m,NULL,0,0)>0){TranslateMessage(&m);DispatchMessageW(&m);}return 0;}
