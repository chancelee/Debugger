#include "BPAcc.h"


BPAcc::BPAcc(DbgObject & dbgObj, uaddr uAddr, E_BPType eType, uint uLen)
	:BPObject(dbgObj)
	, m_eType(eType)
	, m_uLen(uLen)
	, m_oldProtect1()
	, m_oldProtect2()
	, m_currentHitAddress()
	, m_currentHitAccType()
{
	m_uAddress = uAddr;
}

BPAcc::~BPAcc()
{
	Remove();
}

bool BPAcc::Install()
{
	if (m_uLen >= 0x1000)
		return false;
	uaddr uPageBase = (m_uAddress & 0xFFFF000);
	DWORD dwTemp = 0;
	if (uPageBase > ToAligentSize(m_uAddress, 0x1000))
	{
		dwTemp = VirtualProtectEx(
			m_dbgObj.m_hCurrProcess,
			(LPVOID)uPageBase,
			0x1000,
			PAGE_NOACCESS,
			&m_oldProtect2
		);
	}
	// �趨�ϵ��ʱ�򣬲��ܳ���������ҳ
	dwTemp = VirtualProtectEx(
		m_dbgObj.m_hCurrProcess,
		(LPVOID)m_uAddress,
		m_uLen,
		PAGE_NOACCESS,
		&m_oldProtect1
	);
	m_bOnce = false;
	return dwTemp == TRUE;
}

bool BPAcc::Remove()
{
	uaddr uPageBase = (m_uAddress & 0xFFFFF000);

	DWORD	dwTemp = 0;
	DWORD	dwOldProtect = 0;
	if (uPageBase > ToAligentSize(m_uAddress, 0x1000))
	{
		dwTemp = VirtualProtectEx(m_dbgObj.m_hCurrProcess,
			(LPVOID)uPageBase,
			0x1000,
			m_oldProtect2,
			&dwOldProtect
		);
	}

	// �趨�ϵ��ʱ�򣬲��ܳ���������ҳ
	dwTemp = VirtualProtectEx(m_dbgObj.m_hCurrProcess,
		(LPVOID)m_uAddress,
		m_uLen,
		m_oldProtect1,
		&dwOldProtect
	);
	return dwTemp == TRUE;
}

bool BPAcc::IsHit() const
{
	// �ж��Ƿ��б���ʽ
	if (m_currentHitAddress != m_uAddress)
		return false;

	switch (m_eType)
	{
	case breakpointType_acc_r:
		if (m_currentHitAccType != 0)
			return false;
		break;
	case breakpointType_acc_w:
		if (m_currentHitAccType != 1)
			return false;
		break;
	case breakpointType_acc_e:
		if (m_currentHitAccType != 8)
			return false;
		break;
	}


	if (!m_condition.IsEmpty()) {
		Expression exp(&m_dbgObj);
		return exp.GetValue(m_condition) != 0;
	}
	return true;
}

E_BPType BPAcc::Type() const
{
	return m_eType;
}

bool BPAcc::IsMe(const EXCEPTION_DEBUG_INFO & exception) const
{
	*(DWORD*)&m_currentHitAccType = exception.ExceptionRecord.ExceptionInformation[0];
	*(DWORD*)&m_currentHitAddress = exception.ExceptionRecord.ExceptionInformation[1];

	// �жϷ����쳣�ĵ�ַ
	if (exception.ExceptionRecord.ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
	{
		//MessageBox(0, "�ڴ���ʶϵ�", "�ϵ���ʾ", 0);
		DWORD dwPageBase = (m_uAddress & 0xFFFFF000);
		if (m_currentHitAddress >= dwPageBase && m_currentHitAddress <= m_uAddress + m_uLen)
			return true;
	}
	return false;
}

bool BPAcc::NeedRemove() const
{
	return m_bOnce;
}