#include "DbgEngine.h"



DbgEngine::DbgEngine()
{
	m_hBreakpointEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	m_hUserInputEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
}


DbgEngine::~DbgEngine()
{
}

E_Status DbgEngine::Exec()
{
	static bool bIsSystemBreakpoint = true;
	DEBUG_EVENT dbgEvent = { 0 };
	DWORD dwStatus = 0;
	DWORD bRet = 0;

	// �ȴ������¼�
	bRet = WaitForDebugEvent(&dbgEvent, 30);
	if (bRet == FALSE)
		return e_s_sucess;

	m_pid = dbgEvent.dwProcessId;
	m_tid = dbgEvent.dwThreadId;

	// �رվɵľ��
	CloseHandle(m_hCurrProcess);
	CloseHandle(m_hCurrThread);
	m_hCurrThread = m_hCurrProcess = 0;
	// ��ȡ�����쳣ʹ�õ��ľ��
	m_hCurrProcess = OpenProcess(PROCESS_ALL_ACCESS,/*����Ȩ��*/
		FALSE,
		m_pid
	);
	m_hCurrThread = OpenThread(THREAD_ALL_ACCESS,
		FALSE,
		m_tid
	);
	//������̾��
	g_hProcess = m_hCurrProcess;
	g_hThread = m_hCurrThread;
	//����
	if (m_hCurrThread)
	{
		context1.ContextFlags = CONTEXT_ALL;
		GetThreadContext(g_hProcess, &context1);
	}
	//�߳�ID
	threadId = m_tid;

	dwStatus = DBG_CONTINUE;
	// ���������¼�
	switch (dbgEvent.dwDebugEventCode)
	{
	case EXCEPTION_DEBUG_EVENT: /*�쳣�����¼�*/
	{
		if (bIsSystemBreakpoint)
		{
			// ��ʼ�����ŷ�����
			//BreakpointEngine::InitSymbol(m_hCurrProcess);
			bIsSystemBreakpoint = false;

			// ��OEP���¶�
			if (m_oep != 0) {

				BPObject* pBp = AddBreakpoint(m_oep, breakpointType_soft);
				if (pBp == nullptr)
				{
					TerminateProcess(m_hCurrProcess, 0);
					return e_s_processQuit;
				}
				pBp->SetCondition(true);

			}


			if (m_bStopOnSystemBreakpoint)
			{
				//���ô����ϵ�Ļص�����
				SetEvent(m_hBreakpointEvent);
				// �ȴ��û��������
				WaitForSingleObject(m_hUserInputEvent, -1);
				dwStatus = DBG_CONTINUE;
			}
			goto _SUCESS;
		}

		// ���°�װ��ʧЧ�Ķϵ�
		ReInstallBreakpoint();

		// �����쳣��Ϣ���Ҷϵ�

		BpItr itr = FindBreakpoint(dbgEvent.u.Exception);
		// �жϵ������Ƿ���Ч, �����Ч,˵��û�ж�Ӧ�Ķϵ�
		if (IsInvalidIterator(itr))
		{
			printf("\n>>>>>>> Ŀ����������������쳣 <<<<<<\n");
			// ���öϵ��¼�Ϊ���ź�״̬
			SetEvent(m_hBreakpointEvent);
			// �ȴ��û�������ɵ��ź�
			WaitForSingleObject(m_hUserInputEvent, -1);
			dwStatus = DBG_EXCEPTION_NOT_HANDLED;
		}
		else
		{
			// �޸��쳣,����ܹ��ɹ������ϵ�, ������û��Ĵ�������
			if (true == FixException(itr))
			{
				// ���öϵ��¼�Ϊ���ź�״̬
				SetEvent(m_hBreakpointEvent);
				// �ȴ��û�������ɵ��ź�
				WaitForSingleObject(m_hUserInputEvent, -1);
			}
		}
		break;
	}


	case CREATE_PROCESS_DEBUG_EVENT: /*���������¼�*/
									 // ����oep�ͼ��ػ�ַ
		m_oep = (uaddr)dbgEvent.u.CreateProcessInfo.lpStartAddress;
		m_imgBase = (uaddr)dbgEvent.u.CreateProcessInfo.lpBaseOfImage;
		imgBase = m_imgBase;
		break;
	case CREATE_THREAD_DEBUG_EVENT:/*�����߳��¼�*/
		break;
	case EXIT_PROCESS_DEBUG_EVENT:/*�����˳��¼�*/
		bIsSystemBreakpoint = true;
		return e_s_processQuit;
		break;
	case EXIT_THREAD_DEBUG_EVENT:
		break;
	case LOAD_DLL_DEBUG_EVENT:
		break;
	case UNLOAD_DLL_DEBUG_EVENT:
		break;
	case OUTPUT_DEBUG_STRING_EVENT:
		break;
	}


_SUCESS:
	ContinueDebugEvent(dbgEvent.dwProcessId,
		dbgEvent.dwThreadId,
		dwStatus);

	return e_s_sucess;
}

void DbgEngine::Close()
{
	DbgObject::Close();
	BreakpointEngine::Clear();
}

BOOL DbgEngine::WaitForBreakpointEvent(DWORD nTime)
{
	// �ȴ��ϵ��¼����ź�
	return WaitForSingleObject(m_hBreakpointEvent, nTime) == WAIT_OBJECT_0;

}

void DbgEngine::FinishBreakpointEvent()
{
	// �����û������¼�����ź�
	SetEvent(m_hUserInputEvent);
}