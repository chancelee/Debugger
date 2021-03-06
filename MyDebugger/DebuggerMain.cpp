#include "DebuggerMain.h"
/**
* 利用DbgEngine类的Open()打开一个进程进行调试.
* 利用DbgEngine类的Exec()接收被调试进程的调试事件
* 利用DbgEngine类的AddBreakpoint()设置断点
* 利用WaitForBreakpointEvent()函数等待断点事件
* 利用DbgEngine类的GetRegInfo()获取寄存器信息
* 利用DbgEngine类的SetRegInfo()设置寄存器信息
* 利用DbgEngine类的ReadMomory()获取指定地址的内存
* 利用DbgEngine类的WriteMomory()设置指定地址的内存
* 利用DisAssambly的DiAsm()来将一段opcode转换成汇编代码
* 利用XEDParse的XEDParseAssemble()来将一段汇编代码转换成opcode
*/
#include <Windows.h>
#include "DbgEngine.h"         // 调试引擎
#include "DbgUi.h"             // 用户界面
#include "Expression.h"        // 表达式模块
#include "DisAsmEngine.h"      // 反汇编引擎
#include "XEDParse.h"          // 汇编引擎
#include "Pectrl.h"

#pragma comment(lib,"XEDParse.lib")

#include "AddPlugin.h"

#include <iostream>
#include <conio.h>
using namespace std;


HANDLE g_hProcess;
HANDLE g_hThread;
unsigned int threadId;
CONTEXT context1;
//进程加载基址
uaddr imgBase;
PIMAGE_DOS_HEADER pDosH;	//DOS头
char* buff;
CString file_Name;

// 显示调试器命令行帮助信息
void ShowHelp();
// 将字符串分割成两个字符串(将第一次遇到的空格替换成字符串结束符)
char* GetSecondArg(char* pBuff);
inline char* SkipSpace(char* pBuff);
// 获取命令行中的参数
void GetCmdLineArg(char* pszCmdLine, int nArgCount, ...);
// 设置断点
void SetBreakpoint(DbgEngine* pDbg, DbgUi* pUi, char* szCmdLine, DisAsmEngine* disAsm);
//通过创建远程线程给其他进程加载Dll
BOOL LoadRometeDll(DWORD dwProcessId, LPTSTR lpszLibName);

DWORD	g_dwProcessStatus = 0;



//处理断点的回调函数
unsigned int __stdcall DbgBreakpointEvent(void* uParam);
// 提升为调试权限
BOOL WINAPI EnablePrivileges()
{
	HANDLE hToken;
	// 令牌结构体
	TOKEN_PRIVILEGES tkp;
	// 以修改权限的方式打开进程的令牌
	if (!OpenProcessToken(GetCurrentProcess(),
		TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
		return(FALSE);
	// 获得LUID
	LookupPrivilegeValue(NULL, SE_DEBUG_NAME,
		&tkp.Privileges[0].Luid);

	tkp.PrivilegeCount = 1;
	tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	// 修改权限
	AdjustTokenPrivileges(hToken, FALSE, &tkp, 0,
		(PTOKEN_PRIVILEGES)NULL, 0);

	if (GetLastError() != ERROR_SUCCESS)
		return FALSE;

	return TRUE;
}


int main()
{
	//提升权限
	EnablePrivileges();
	printf("--调试器--\n");

	DbgEngine dbgEng; // 调试器引擎对象

	char szPath[MAX_PATH];
	bool bCreateThread = false;
	unsigned int taddr = 0;
	uintptr_t	 tid = 0;
	uint processId = 0;

	int mFlag = 0;
	while (true)
	{
		printf("->请选择调试方式\n");
		printf("->1.创建进程；2.附加进程\n");
		printf("->");
		scanf_s("%d", &mFlag);

		bool bFlag = false;
		// 根据选择使用调试方式
		if (mFlag == 1)
		{
			printf("请输入文件路径或将文件拖拽到此进行调试：");
			scanf_s("%s", szPath, MAX_PATH);
			AnalysisPE(szPath);
			bFlag = dbgEng.Open(szPath);
		}
		else if (mFlag == 2)
		{
			int mPid = 0;
			printf("请输入附加进程PID：");
			scanf_s("%d", &mPid);
			bFlag = dbgEng.Open(mPid);
		}
		else
		{
			continue;
		}
		/*AnalysisPE("CRACKME.EXE");
		bFlag = dbgEng.Open("CRACKME.EXE");
*/
		if (bFlag)
		{
			printf("调试进程创建成功，可以开始调试");

			g_dwProcessStatus = 0;
			// 开启接收用户输入的线程
			tid = _beginthreadex(0, 0, DbgBreakpointEvent, &dbgEng, 0, &taddr);
			while (1)
			{
				// 运行调试器,Exec不处于阻塞状态,因此需要放在while循环中.
				if (e_s_processQuit == dbgEng.Exec()) {
					dbgEng.Close();
					system("cls");
					cout << "被调试进程已退出\n";
					g_dwProcessStatus = 1;
					break;
				}
			}

		}

	}
}



// 显示反汇编
void ShowAsm(DbgEngine& dbgEngine,
	DbgUi& ui,
	DisAsmEngine& disAsmEng,
	int nLine = 30,
	SIZE_T Addr = 0)
{
	static CONTEXT ct = { CONTEXT_CONTROL };
	if (Addr == 0)
	{
		dbgEngine.GetRegInfo(ct);
		Addr = ct.Eip;
	}

	vector<DISASMSTRUST> vecDisAsm;
	disAsmEng.DiAsm(Addr, vecDisAsm, nLine);

	for (vector<DISASMSTRUST>::iterator i = vecDisAsm.begin();
		i != vecDisAsm.end();
		++i)
	{
		ui.ShowAsm(Addr,
			(*i).strOpCode,
			(*i).strAsm,
			(*i).strCom
		);
		Addr += (*i).dwCodeLen;
	}
}



//修改内存 e
DWORD CmdModifyData(CString& str1)
{
	DWORD	dwModifyAddr;
	DWORD	dwOldProtect;
	DWORD	dwReadCount;
	DWORD	dwInputValue;
	byte	bBuffer;
	char	szBuffer[10];

	if (str1.IsEmpty())
	{
		printf("没有输入要修改的内存地址\r\n");
	}
	//转换参数
	char	*pRet = NULL;
	USES_CONVERSION;

	dwModifyAddr = strtoul(T2CA(str1), &pRet, 16);
	//转换失败
	if (*pRet != NULL)
	{
		printf("要修改的内存地址输入错误\r\n");
		return 0;
	}
	//
	while (1)
	{
		//修改内存保护属性
		VirtualProtectEx(g_hProcess, (LPVOID)dwModifyAddr, 1, PAGE_READWRITE, &dwOldProtect);
		if (!ReadProcessMemory(g_hProcess, (LPVOID)dwModifyAddr, &bBuffer, 1, &dwReadCount)) {

			printf("要修改的内存地址无效\r\n");
			VirtualProtectEx(g_hProcess, (LPVOID)dwModifyAddr, 1, dwOldProtect, &dwReadCount);
			return 0;
		}


		//显示原值
		printf("%p  %02X\r\n", dwModifyAddr, bBuffer);
		//获取用户输入修改后的值
		fflush(stdin);
		memset(szBuffer, 0, sizeof(szBuffer));
		////scanf_s("%2[^\n]", szBuffer);
		//cin >> szBuffer;
		scanf_s("%s", szBuffer,10);
		fflush(stdin);
		//输入为空则退出e命令
		if (!_stricmp(szBuffer,"quit")) {
			VirtualProtectEx(g_hProcess, (LPVOID)dwModifyAddr, 1, dwOldProtect, &dwReadCount);
			return TRUE;
		}
		//输入转换为数值类型
		dwInputValue = strtoul(szBuffer, &pRet, 16);
		if (*pRet != NULL)
		{
			printf("输入的值错误\r\n");
			VirtualProtectEx(g_hProcess, (LPVOID)dwModifyAddr, 1, dwOldProtect, &dwReadCount);
			return 0;
		}
		bBuffer = dwInputValue;

		//写入修改后的值
		if (!WriteProcessMemory(g_hProcess, (LPVOID)dwModifyAddr, &bBuffer, 1, &dwReadCount))
		{
			printf("修改后的值写入失败\r\n");
			VirtualProtectEx(g_hProcess, (LPVOID)dwModifyAddr, 1, dwOldProtect, &dwReadCount);
			return 0;
		}

		dwModifyAddr++;
		//还原内存属性
		VirtualProtectEx(g_hProcess, (LPVOID)dwModifyAddr, 1, dwOldProtect, &dwReadCount);
	}

}



// 断点事件回调函数
unsigned int __stdcall DbgBreakpointEvent(void * uParam)
{
	DbgEngine* pDbg = (DbgEngine*)uParam;
	DbgUi ui(pDbg);
	Expression exp(pDbg);
	DisAsmEngine disAsm(pDbg);

	char szCmdLine[64];
	CONTEXT ct = { CONTEXT_ALL };
	vector<DISASMSTRUST> vecDisAsm;
	char* pCmdLine = 0;


	DWORD	dwStatus = 0;
	while (1)
	{
		if (pDbg->WaitForBreakpointEvent(30))
		{
			// 清屏
			system("cls");
			// 获取寄存器信息
			pDbg->GetRegInfo(ct);
			// 使用ui模块将寄存器信息输出
			ui.ShowReg(ct);
			// 输出反汇编
			ShowAsm(*pDbg, ui, disAsm, 20, ct.Eip);
			dwStatus = 1;
		}

		if (dwStatus)
		{
			//printf("%s>", dwStatus == 1 ? "暂停中" : "运行中");
			printf(">");
			dwStatus = 0;
		}
		// 接收用户输入的命令
		if (_kbhit())
		{
			do
			{
				gets_s(szCmdLine, 64);
			} while (*szCmdLine == '\0');
			dwStatus = 2;
		}
		else
			continue;
		// 跳过行头空格
		pCmdLine = SkipSpace(szCmdLine);
		// 判断是否需要退出调试器
		if (*(DWORD*)pCmdLine == 'tixe' || g_dwProcessStatus == 1) {
			pDbg->Close();
			pDbg->FinishBreakpointEvent();
			return 0;
		}
		// 解析用户输入的命令
		switch (*pCmdLine)
		{
		/*查看汇编代码*/
		case 'u':
		{
			dwStatus = 1;
			char *pAddr = 0;
			char* pLineCount = 0;
			GetCmdLineArg(pCmdLine + 1, 2, &pAddr, &pLineCount);
			if (pAddr == nullptr)
				pAddr = "eip";
			if (pLineCount == nullptr)
				pLineCount = "20";
			// 显示反汇编
			ShowAsm(*pDbg, ui, disAsm, exp.GetValue(pLineCount), exp.GetValue(pAddr));
			break;
		}
		/*修改汇编代码*/
		case 'a':
		{
			dwStatus = 1;
			// 获取开始地址
			XEDPARSE xpre = { 0 };
			xpre.x64 = false; // 是否转换成64位的opCode
			memset(xpre.dest, 0x90, XEDPARSE_MAXASMSIZE);
			pCmdLine = SkipSpace(pCmdLine + 1);
			if (*pCmdLine == 0)
			{
				printf("指令格式错误, 格式为: a 地址\n");
				continue;// 结束本次while循环
			}
			printf("输入quit退出汇编模式\n");
			uaddr address = exp.GetValue(pCmdLine);
			if (address == 0)
				continue;// 结束本次while循环

			while (true)
			{
				printf("%08X: ", address);
				cin.getline(xpre.instr, XEDPARSE_MAXBUFSIZE);
				if (strcmp(xpre.instr, "quit") == 0)
					break;
				DWORD uLen = disAsm.GetCodeLen(address);
				xpre.cip = address;// 指令所在的地址
				if (false == XEDParseAssemble(&xpre))
				{
					printf("%s\n", xpre.error);
					continue;// 结束本次while循环
				}
				// 将代码写入到目标进程
				if (!pDbg->WriteMemory(address, xpre.dest, uLen))
					continue;// 结束本次while循环
							 // 地址++
				address += xpre.dest_size;
			}
			break;
		}
		/*查看内存*/
		case 'd':
		{
			dwStatus = 1;
			char *p = &szCmdLine[1];
			// 筛选数据格式
			switch (*p)
			{
			case 'u':/*unicode字符串*/
			{
				SSIZE_T uAddr = exp.GetValue(szCmdLine + 2);
				BYTE lpBuff[16 * 6];
				pDbg->ReadMemory(uAddr, lpBuff, 16 * 6);
				ui.ShowMem(uAddr, lpBuff, 16 * 6, 1);
			}
			break;

			case 'a':/*ansi字符串*/
				p = &szCmdLine[2];

			default:
			{
				SSIZE_T uAddr = exp.GetValue(p);
				BYTE lpBuff[16 * 6];
				pDbg->ReadMemory(uAddr, lpBuff, 16 * 6);
				ui.ShowMem(uAddr, lpBuff, 16 * 6, 0);
			}
			break;

			}
			break;
		}
		//修改内存
		case 'x':
		{
			dwStatus = 1;
			pCmdLine = SkipSpace(pCmdLine + 1);

			//printf("%s",pCmdLine);
			CString str(pCmdLine);
			CmdModifyData(str);
			break;
		}
		/*查看栈*/
		case 'k':
		{
			dwStatus = 1;
			pDbg->GetRegInfo(ct);
			BYTE	buff[sizeof(SIZE_T) * 20];
			pDbg->ReadMemory(ct.Esp, buff, sizeof(SIZE_T) * 20);
			ui.ShowStack(ct.Esp, buff, sizeof(SIZE_T) * 20);
			break;
		}
		/*查看和修改寄存器*/
		case 'r':
		{
			dwStatus = 1;
			// 获取寄存器的值:
			// r 寄存器名 
			// 设置寄存器的值
			// r 寄存器名 = 表达式
			char* p = szCmdLine + 1;
			p = SkipSpace(p);
			if (*p == 0)
			{
				ct.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
				pDbg->GetRegInfo(ct);
				ui.ShowReg(ct);
				break;
			}
			SSIZE_T nValue = exp.GetValue(szCmdLine + 1);

			char* pSecArg = SkipSpace(szCmdLine + 1);
			pSecArg = GetSecondArg(pSecArg);
			if (*pSecArg == 0)
				printf("%s = 0x%X\n", szCmdLine + 1, nValue);
			break;
		}
		/*查看调试程序的模块信息*/
		case 'm':
		{
			dwStatus = 1;
			if (*SkipSpace(pCmdLine + 1) == 'l')
			{
				list<MODULEFULLINFO> modList;
				pDbg->GetModuleList(modList);
				printf("+------------------+----------+----------------------------------------------------+\n");
				printf("|     加载基址     + 模块大小 |                    模块名                          |\n");
				printf("+------------------+----------+----------------------------------------------------+\n");
				for (auto &i : modList)
				{
					printf("| %016I64X | %08X | %-50s |\n", i.uStart, i.uSize, (LPCSTR)i.name);
				}
				printf("+------------------+----------+----------------------------------------------------+\n");
			}
			continue;// 结束本次while循环
		}
		/*单步步入*/
		case 't':
		{
			// 使用调试器引擎的函数来添加一个TF断点
			BPObject *pBp = pDbg->AddBreakpoint(0, breakpointType_tf);
			if (pBp == nullptr)
				return 0;
			char* pCondition = 0;
			GetCmdLineArg(pCmdLine + 1, 1, &pCondition);
			// 设置断点的中断条件
			if (pCondition != 0)
			{
				pBp->SetCondition(pCondition);
			}
			else
				pBp->SetCondition(true);
			pDbg->FinishBreakpointEvent();
			break;
		}
		/*单步步过*/
		case 'p':
		{
			BPObject *pBp = nullptr;

			// 判断当前是否是call指令
			pDbg->GetRegInfo(ct);
			SIZE_T uEip = ct.Eip;
			BYTE c[2] = { 0 };
			pDbg->ReadMemory(uEip, c, 2);
			DWORD dwCodeLen = 5;
			/**
			　* call 的机器码有:
			 　* 0xe8 : 5byte,
			  　* 0x9a : 7byte,
			   　* 0xff :
				　*	 0x10ff ~ 0x1dff
				 　* rep 前缀的指令也可以步过
				  　*/
			if (c[0] == 0xe8/*call*/
				|| c[0] == 0xf3/*rep*/
				|| c[0] == 0x9a/*call*/
				|| (c[0] == 0xff && 0x10 <= c[1] && c[1] <= 0x1d)/*call*/
				)
			{
				dwCodeLen = disAsm.GetCodeLen(uEip);

				pBp = pDbg->AddBreakpoint(uEip + dwCodeLen, breakpointType_soft);

			}
			else
				pBp = pDbg->AddBreakpoint(0, breakpointType_tf);
			// 如果断点没有添加成功1
			if (pBp == nullptr)
			{
				pDbg->FinishBreakpointEvent();
				break;
			}
				
			// 获取条件
			char* pCondition = SkipSpace(pCmdLine + 1);
			if (*pCondition != 0)
			{
				pBp->SetCondition(pCondition);
			}
			else
				pBp->SetCondition(true);

			pDbg->FinishBreakpointEvent();
			break;
		}
		/*设置断点*/
		case 'b':
		{
			dwStatus = 1;
			SetBreakpoint(pDbg, &ui, pCmdLine, &disAsm);
			break;
		}
		/*运行程序*/
		case 'g':
		{
			pDbg->FinishBreakpointEvent();
			break;
		}

		/*插件*/
		case 'c':
		{
			PluginInit();
			break;
		}

		/*查看帮助*/
		case 'h':
		{
			dwStatus = 1;
			ShowHelp();
			break;
		}
		/*查看导入表*/
		case 'i':
		{
			GetPEImpTab(pDosH);
			cout << "请输入要查看第几个DLL：" << endl;
			int numIndex;
			cin >> numIndex;
			ShowImpFunc(numIndex);
			break;
		}
		/*查看导出表*/
		case 'o':
		{
			GetPEExpTab(pDosH);
			ShowExpFunc();
			break;
		}
		}
	}


	return 0;
}


// 显示帮助信息
void ShowHelp()
{
	printf("----------------------------------------------------\n");
	printf("h : 查看帮助\n");
	// 基础功能
	// 显示/修改汇编代码
	printf("u : 查看反汇编\n");
	printf("    格式为 : u 开始地址 指令条数\n");
	//printf("    例如   : u eip\n");
	//printf("    例如   : u eax 100\n");
	//printf("    例如   : u 0x401000 100\n");
	printf("a : 修改汇编代码\n");
	printf("    格式为 : a 开始地址（地址格式：0x数字）\n");
	printf("    输入quit退出汇编代码编辑\n");
	// 查看/修改内存数据，查看栈
	printf("d : 查看内存数据\n");
	printf("    格式为 : d 开始地址\n");
	//printf("    格式为 : da 开始地址(显示字符串时使用ANSIII字符)\n");
	//printf("    格式为 : du 开始地址(显示字符串时使用Unicode字符)\n");
	//printf("    格式为 : dp DUMP功能\n");
	printf("x : 修改内存数据\n");
	printf("    格式为 : x 开始地址(后续操作直接输入修改值)\n");
	printf("k : 查看栈\n");
	// 查看/修改寄存器
	printf("r : 查看/修改寄存器\n");
	printf("    查看格式为 : r 寄存器名\n");
	printf("    修改寄存器 r eax = 0x1000\n");
	// 查看调用程序模块信息
	printf("ml: 查看调试程序模块信息\n");
	// 断点功能
	printf("b : 设置断点\n");
	printf("    格式:\n");
	printf("    bp 地址 =>软件断点\t");
	//printf("    例如: bp 0x401000 eax==0 && byte[0x403000]==97\n");
	printf("    bh 地址 =>硬件断点\t");
	//printf("    例如: bh 0x401000 e \n");
	printf("    bm 地址 =>内存断点\n");
	//printf("    例如: bm 0x401000 e \n");
	printf("    bl 列出所有断点\n");
	//printf("    bc 序号 删除指定序号的断点\n");
	printf("t : 单步步入\n");
	printf("p : 单步步过\n");
	printf("g : 运行程序\n");
	// 附加功能
	printf("i : 查看被调试程序的导入表\n");
	printf("o : 查看被调试程序的导出表\n");
	printf("c : 加载插件\n");
	//printf("exit: 退出调试会话\n");
	printf("----------------------------------------------------\n");

}

// 获取第二个参数(参数之间以空格间隔开
char* GetSecondArg(char* pBuff)
{
	for (; *pBuff != 0; ++pBuff)
	{
		if (*pBuff == ' ')//找到第一个空格
		{
			*pBuff = 0; // 把空格变成字符串结束符,分隔两个参数
			return pBuff + 1;//返回第二个参数的开始地址
		}
	}
	return pBuff;
}

// 跳过空格(包括换行符,tab符)
inline char* SkipSpace(char* pBuff)
{
	for (; *pBuff == ' ' || *pBuff == '\t' || *pBuff == '\r' || *pBuff == '\n'; ++pBuff);
	return pBuff;
}


void GetCmdLineArg(char* pszCmdLine, int nArgCount, ...)
{
	va_list argptr;
	va_start(argptr, nArgCount);

	while (nArgCount-- > 0 && *pszCmdLine != 0)
	{
		if (*pszCmdLine == ' ' || *pszCmdLine == '\t')
			*pszCmdLine++ = 0;

		pszCmdLine = SkipSpace(pszCmdLine);
		if (*pszCmdLine == 0)
			break;
		DWORD*& dwArg = va_arg(argptr, DWORD*);
		*dwArg = (DWORD)pszCmdLine;

		for (; *pszCmdLine != 0 && *pszCmdLine != ' ' && *pszCmdLine != '\t'; ++pszCmdLine);
	}
	va_end(argptr);
}

// 设置断点
void SetBreakpoint(DbgEngine* pDbg, DbgUi* pUi, char* szCmdLine, DisAsmEngine* disAsm)
{
	char* pAddr = 0;//断点地址
	char* pType = 0;//断点类型
	char* pLen = 0;//断点长度
	char* pRule = 0; // 断点命中规则
	Expression exp(pDbg);

	char  cType = *(SkipSpace(szCmdLine + 1));// 断点类型
	E_BPType   bpType = e_bt_none;
	SIZE_T   uAddr = 0; // 下断地址
	uint		uBPLen = 1;
	switch (cType)
	{
	case 'p':/*普通断点*/
	{
		char* pAddrr = 0;
		bpType = breakpointType_soft;
		GetCmdLineArg(szCmdLine + 2, 2, &pAddrr, &pRule);
		if (pAddrr == nullptr)
			pAddrr = "eip";

		// 得到地址
		uAddr = exp.GetValue(pAddrr);
		// 得到长度
		uBPLen = disAsm->GetCodeLen(uAddr);
		break;
	}
	case 'l':/*断点列表*/
	{
		pUi->ShowBreakPointList(pDbg->GetBPListBegin(), pDbg->GetBPListEnd());
		return;
	}
	case 'c':/*删除断点*/
	{
		DWORD	dwIndex = 0;
		sscanf_s(szCmdLine + 2, "%d", &dwIndex);

		pDbg->DeleteBreakpoint(dwIndex);

		return;
	}

	//硬件断点在下面的m分支中找
	case 'h':/*硬件断点*/
	case 'm': /*内存访问断点*/
	{
		GetCmdLineArg(szCmdLine + 2, 4, &pAddr, &pType, &pLen, &pRule);
		if (pAddr == 0 || pType == 0)
		{
			printf("bm/bh 地址 类型(r/w/e) 长度(1/2/4)(可选) 条件(可选)\n");
			return;
		}

		uAddr = exp.GetValue(pAddr);
		switch (*pType) // 筛选断点的类型
		{
		case 'r':bpType = cType == 'm' ? breakpointType_acc_r : breakpointType_hard_r; break;
		case 'w':bpType = cType == 'm' ? breakpointType_acc_w : breakpointType_hard_w; break;
		case 'e':bpType = cType == 'm' ? breakpointType_acc_e : breakpointType_hard_e; break;
		default:
			printf("断点类型设置错误,访问断点的类型有: r(读),w(写),e(执行)\n");
			return;
		}

		uBPLen = exp.GetValue(pLen);
		if (uBPLen == 0)
			uBPLen = 1;

		if (pLen == 0 && cType == 'h') //如果是硬件断点,则将长度设为0
			pLen = "0";
		else if (pLen > 0 && cType == 'h') // 检测硬件断点地址和长度的对应关系
		{
			if (*pType == 'e') // 如果是执行断点,长度只能为0
				uBPLen = 0;
			else // 如果是读写断点,断点地址和长度必须满足对应关系
			{
				// 默认长度为4个字节
				uBPLen = 3;
				// 如果长度是4 , 但地址并不是4的倍数,则断点长度最多是2个字节
				if (*pLen == '4' && uAddr % 4 != 0)
					uBPLen = 1;
				// 如果长度是2 , 但地址并不是2的倍数,则断点长度最多是1个字节
				if (*pLen == '2' && uAddr % 2 != 0)
					uBPLen = 0;
			}
		}
		break;
	}
	default:
		cout << "没有该类型的断点\n";
		return;
	}

	// 获取完断点的地址,类型,条件后, 进行下断.
	BPObject* pBp = pDbg->AddBreakpoint(uAddr, bpType, uBPLen);
	if (pBp == nullptr)
	{
		printf("设置断点失败\n");
		return;
	}

	// 如果断点携带表达式, 则把表达式设置到断点上
	if (pRule != nullptr)
		BreakpointEngine::SetExp(pBp, pRule);
	return;
}

