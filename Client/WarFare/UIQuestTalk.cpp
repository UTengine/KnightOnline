﻿// UIQuestTalk.cpp: implementation of the CUIQuestTalk class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "UIQuestTalk.h"
#include "GameDef.h"
#include "GameProcedure.h"
#include "UIManager.h"
#include "APISocket.h"

#include <N3Base/N3UIScrollBar.h>
#include <N3Base/N3UIString.h>
#include <N3Base/N3UIButton.h>

#include <algorithm>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CUIQuestTalk::CUIQuestTalk()
{
	m_pTextTalk			= nullptr;
	m_pBtnOk			= nullptr;
	m_pBtnClose			= nullptr;
	m_pBtnUpperEvent	= nullptr;
	m_pBtnNext			= nullptr;
	m_pBtnOkRight		= nullptr;
	m_pBtnPre			= nullptr;
	m_pScrollBar		= nullptr;
	m_iNumTalk			= 0;
	m_iCurTalk			= 0;
}

CUIQuestTalk::~CUIQuestTalk()
{
}

void CUIQuestTalk::Open(Packet& pkt)
{
	m_iNumTalk = 0;
	m_iCurTalk = 0;

	// NOTE(srmeier): two -1s before text ids
	int index = pkt.read<uint32_t>();
	index = pkt.read<uint32_t>();

	for(int i=0;i<MAX_STRING_TALK;i++)
	{
		m_szTalk[i] = "";

		index = pkt.read<uint32_t>();
		__TABLE_QUEST_TALK* pTbl_Quest_Talk = CGameBase::s_pTbl_QuestTalk.Find(index);
		if(pTbl_Quest_Talk)
		{
			m_szTalk[i] = pTbl_Quest_Talk->szTalk;
			CGameBase::ConvertPipesToNewlines(m_szTalk[i]);
			m_iNumTalk++;
		}
	}

	m_pTextTalk->SetString(m_szTalk[m_iCurTalk]);

	// reset scrollbar position
	if (m_pScrollBar != nullptr)
		m_pScrollBar->SetCurrentPos(0);

	SetVisible(true);
}

bool CUIQuestTalk::ReceiveMessage(CN3UIBase* pSender, uint32_t dwMsg)
{
	if (dwMsg == UIMSG_BUTTON_CLICK)
	{
		if (pSender == m_pBtnOk)
		{
			m_iCurTalk++;
			if (m_iCurTalk >= m_iNumTalk)
			{
				m_iCurTalk = 0;
				SetVisible(false);
			}
			else
			{
				CGameBase::ConvertPipesToNewlines(m_szTalk[m_iCurTalk]);
				m_pTextTalk->SetString(m_szTalk[m_iCurTalk]);
			}
		}
		else if (pSender == m_pBtnClose)
		{
			SetVisible(false);
		}
	}
	else if (dwMsg == UIMSG_SCROLLBAR_POS)
	{
		if (pSender == m_pScrollBar)
		{
			UpdateTextForScroll();
			return true;
		}
	}

	return true;
}

bool CUIQuestTalk::Load(HANDLE hFile)
{
	if (!CN3UIBase::Load(hFile))
		return false;

	N3_VERIFY_UI_COMPONENT(m_pTextTalk,			(CN3UIString*) GetChildByID("Text_Talk"));
	N3_VERIFY_UI_COMPONENT(m_pBtnOk,			(CN3UIButton*) GetChildByID("btn_Ok_center"));

	// NOTE(srmeier): new stuff:
	N3_VERIFY_UI_COMPONENT(m_pBtnClose,			(CN3UIButton*) GetChildByID("btn_close"));
	N3_VERIFY_UI_COMPONENT(m_pBtnUpperEvent,	(CN3UIButton*) GetChildByID("btn_UpperEvent"));
	N3_VERIFY_UI_COMPONENT(m_pBtnNext,			(CN3UIButton*) GetChildByID("btn_Next"));
	N3_VERIFY_UI_COMPONENT(m_pBtnOkRight,		(CN3UIButton*) GetChildByID("btn_Ok_right"));
	N3_VERIFY_UI_COMPONENT(m_pBtnPre,			(CN3UIButton*) GetChildByID("btn_Pre"));
	N3_VERIFY_UI_COMPONENT(m_pScrollBar,		(CN3UIScrollBar*) GetChildByID("scroll"));
	
	if (m_pBtnUpperEvent != nullptr)
		m_pBtnUpperEvent->SetVisible(false);

	if (m_pBtnNext != nullptr)
		m_pBtnNext->SetVisible(false);

	if (m_pBtnOkRight != nullptr)
		m_pBtnOkRight->SetVisible(false);

	if (m_pBtnPre != nullptr)
		m_pBtnPre->SetVisible(false);

	return true;
}

bool CUIQuestTalk::OnKeyPress(int iKey)
{
	switch(iKey)
	{
	case DIK_ESCAPE:
		SetVisible(false);
		return true;
	case DIK_RETURN:
		ReceiveMessage(m_pBtnOk, UIMSG_BUTTON_CLICK);
		return true;
	}

	return CN3UIBase::OnKeyPress(iKey);
}

void CUIQuestTalk::SetVisible(bool bVisible)
{
	CN3UIBase::SetVisible(bVisible);
	if(bVisible)
		CGameProcedure::s_pUIMgr->SetVisibleFocusedUI(this);
	else
		CGameProcedure::s_pUIMgr->ReFocusUI();//this_ui
}

void CUIQuestTalk::Release()
{
	CN3UIBase::Release();

	m_pTextTalk			= nullptr;
	m_pBtnOk			= nullptr;
	m_pBtnClose			= nullptr;
	m_pBtnUpperEvent	= nullptr;
	m_pBtnNext			= nullptr;
	m_pBtnOkRight		= nullptr;
	m_pBtnPre			= nullptr;
	m_pScrollBar		= nullptr;
	m_iNumTalk			= 0;
	m_iCurTalk			= 0;
}

void CUIQuestTalk::UpdateTextForScroll()
{
	if (m_pTextTalk == nullptr
		|| m_pScrollBar == nullptr)
		return;

	// scrollbar's current position
	const int iScrollPosition = m_pScrollBar->GetCurrentPos();

	// total number of lines of text
	const int iTotalLineCount = m_pTextTalk->GetLineCount();

	// max number of lines visible in text area
	const int iVisibleLineCount = 8;

	const int iMaxScrollableLines = iTotalLineCount - iVisibleLineCount;
	m_pScrollBar->SetRangeMax(iMaxScrollableLines);

	// return if text is shorter than or equal to the visible line count
	if (iTotalLineCount <= iVisibleLineCount)
		return;

	// limit check for the line which displayed first, topline
	int iTopLine = std::clamp(
		iScrollPosition,
		0,
		iTotalLineCount - iVisibleLineCount);

	m_pTextTalk->SetStartLine(iTopLine);
}
