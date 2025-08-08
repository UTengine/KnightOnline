﻿// UIQuestTalk.h: interface for the CUIQuestTalk class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_UIQUESTTALK_H__DB9A4C59_4BE8_4698_9462_CF036C8D834D__INCLUDED_)
#define AFX_UIQUESTTALK_H__DB9A4C59_4BE8_4698_9462_CF036C8D834D__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <N3Base/N3UIBase.h>

#define MAX_STRING_TALK		10

class Packet;
class CUIQuestTalk : public CN3UIBase
{
protected:
	CN3UIString*	m_pTextTalk;
	CN3UIButton*	m_pBtnOk;
	CN3UIButton*	m_pBtnClose;
	CN3UIButton*	m_pBtnUpperEvent;
	CN3UIButton*	m_pBtnNext;
	CN3UIButton*	m_pBtnOkRight;
	CN3UIButton*	m_pBtnPre;
	CN3UIScrollBar* m_pScrollBar;

	std::string		m_szTalk[MAX_STRING_TALK];
	int				m_iNumTalk;
	int				m_iCurTalk;

public:
	void Release() override;
	void SetVisible(bool bVisible) override;
	bool OnKeyPress(int iKey) override;
	bool Load(HANDLE hFile) override;
	bool ReceiveMessage(CN3UIBase* pSender, uint32_t dwMsg) override;
	void Open(Packet& pkt);
	CUIQuestTalk();
	~CUIQuestTalk() override;

protected:
	void UpdateTextForScroll();
};

#endif // !defined(AFX_UIQUESTTALK_H__DB9A4C59_4BE8_4698_9462_CF036C8D834D__INCLUDED_)
