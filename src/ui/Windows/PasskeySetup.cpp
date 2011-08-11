/*
* Copyright (c) 2003-2011 Rony Shapiro <ronys@users.sourceforge.net>.
* All rights reserved. Use of the code is allowed under the
* Artistic License 2.0 terms, as specified in the LICENSE file
* distributed with this code, or available from
* http://www.opensource.org/licenses/artistic-license-2.0.php
*/
/// \file PasskeySetup.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "PasswordSafe.h"
#include "ThisMfcApp.h"
#include "DboxMain.h"
#include "GeneralMsgBox.h"
#include "Options_PropertySheet.h"

#include "core/PWCharPool.h" // for CheckPassword()
#include "core/PwsPlatform.h"
#include "core/pwsprefs.h"

#include "os/dir.h"

#include "VirtualKeyboard/VKeyBoardDlg.h"

#if defined(POCKET_PC)
#include "pocketpc/resource.h"
#include "pocketpc/PocketPC.h"
#else
#include "resource.h"
#include "resource3.h"  // String resources
#endif

#include "core/util.h"

#include "PasskeySetup.h"

#include <iomanip>  // For setbase and setw

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


//-----------------------------------------------------------------------------
CPasskeySetup::CPasskeySetup(CWnd *pParent)
  : CPKBaseDlg(CPasskeySetup::IDD, pParent), m_pVKeyBoardDlg(NULL),
  m_LastFocus(IDC_PASSKEY)
{
  m_pDbx = static_cast<DboxMain *>(pParent);

  m_verify = L"";
  m_pctlVerify = new CSecEditExtn;
}

CPasskeySetup::~CPasskeySetup()
{
  delete m_pctlVerify;

  if (m_pVKeyBoardDlg != NULL) {
    // Save Last Used Keyboard
    UINT uiKLID = m_pVKeyBoardDlg->GetKLID();
    std::wostringstream os;
    os.fill(L'0');
    os << std::nouppercase << std::hex << std::setw(8) << uiKLID;
    StringX cs_KLID = os.str().c_str();
    PWSprefs::GetInstance()->SetPref(PWSprefs::LastUsedKeyboard, cs_KLID);

    m_pVKeyBoardDlg->DestroyWindow();
    delete m_pVKeyBoardDlg;
  }
}

void CPasskeySetup::DoDataExchange(CDataExchange* pDX)
{
  CPKBaseDlg::DoDataExchange(pDX);
  
  // Can't use DDX_Text for CSecEditExtn
  m_pctlVerify->DoDDX(pDX, m_verify);

  DDX_Control(pDX, IDC_VERIFY, *m_pctlVerify);
}

BEGIN_MESSAGE_MAP(CPasskeySetup, CPKBaseDlg)
  ON_BN_CLICKED(ID_HELP, OnHelp)
  ON_STN_CLICKED(IDC_VKB, OnVirtualKeyboard)
  ON_MESSAGE(PWS_MSG_INSERTBUFFER, OnInsertBuffer)
  ON_EN_SETFOCUS(IDC_PASSKEY, OnPasskeySetfocus)
  ON_EN_SETFOCUS(IDC_VERIFY, OnVerifykeySetfocus)
  ON_BN_CLICKED(IDC_YUBIKEY_BTN, OnYubikeyBtn)
#if defined(POCKET_PC)
  ON_EN_KILLFOCUS(IDC_PASSKEY, OnPasskeyKillfocus)
  ON_EN_KILLFOCUS(IDC_VERIFY, OnPasskeyKillfocus)
#endif
END_MESSAGE_MAP()

BEGIN_DISPATCH_MAP(CPasskeySetup, CPKBaseDlg)
	//{{AFX_DISPATCH_MAP(CPasskeySetup)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_DISPATCH_MAP
	DISP_FUNCTION_ID(CPasskeySetup, "deviceInserted", 1, yubiInserted, VT_EMPTY, VTS_NONE)
	DISP_FUNCTION_ID(CPasskeySetup, "deviceRemoved", 2, yubiRemoved, VT_EMPTY, VTS_NONE)
	DISP_FUNCTION_ID(CPasskeySetup, "operationCompleted", 3, yubiCompleted, VT_EMPTY, VTS_I2)
  DISP_FUNCTION_ID(CPasskeySetup, "userWait", 4, yubiWait, VT_EMPTY, VTS_I2)
END_DISPATCH_MAP()

BOOL CPasskeySetup::OnInitDialog() 
{
  CPKBaseDlg::OnInitDialog();
  ApplyPasswordFont(GetDlgItem(IDC_VERIFY));

  m_pctlVerify->SetPasswordChar(PSSWDCHAR);

  // Only show virtual Keyboard menu if we can load DLL
  if (!CVKeyBoardDlg::IsOSKAvailable()) {
    GetDlgItem(IDC_VKB)->ShowWindow(SW_HIDE);
    GetDlgItem(IDC_VKB)->EnableWindow(FALSE);
  }

  return TRUE;
}

void CPasskeySetup::OnCancel() 
{
  CPKBaseDlg::OnCancel();
}

void CPasskeySetup::OnOK()
{
  UpdateData(TRUE);

  CGeneralMsgBox gmb;
  if (m_passkey != m_verify) {
    gmb.AfxMessageBox(IDS_ENTRIESDONOTMATCH);
    ((CEdit*)GetDlgItem(IDC_VERIFY))->SetFocus();
    return;
  }

  if (m_passkey.IsEmpty()) {
    gmb.AfxMessageBox(IDS_ENTERKEYANDVERIFY);
    ((CEdit*)GetDlgItem(IDC_PASSKEY))->SetFocus();
    return;
  }
  // Vox populi vox dei - folks want the ability to use a weak
  // passphrase, best we can do is warn them...
  // If someone want to build a version that insists on proper
  // passphrases, then just define the preprocessor macro
  // PWS_FORCE_STRONG_PASSPHRASE in the build properties/Makefile
  // (also used in CPasskeyChangeDlg)
#ifndef _DEBUG // for debug, we want no checks at all, to save time
  StringX errmess;
  if (!CPasswordCharPool::CheckPassword(m_passkey, errmess)) {
    CString cs_msg, cs_text;
    cs_msg.Format(IDS_WEAKPASSPHRASE, errmess.c_str());
#ifndef PWS_FORCE_STRONG_PASSPHRASE
    CGeneralMsgBox gmb;
    cs_text.LoadString(IDS_USEITANYWAY);
    cs_msg += cs_text;
    INT_PTR rc = gmb.AfxMessageBox(cs_msg, NULL, MB_YESNO | MB_ICONSTOP);
    if (rc == IDNO)
      return;
#else
    cs_text.LoadString(IDS_TRYANOTHER);
    cs_msg += cs_text;
    gmb.AfxMessageBox(cs_msg, MB_OK | MB_ICONSTOP);
    return;
#endif // PWS_FORCE_STRONG_PASSPHRASE
  }
#endif // _DEBUG

  CPKBaseDlg::OnOK();
}

void CPasskeySetup::OnHelp() 
{
#if defined(POCKET_PC)
  CreateProcess(L"PegHelp.exe", L"pws_ce_help.html#newdatabase", NULL, NULL, FALSE, 0, NULL, NULL, NULL, NULL);
#else
  CString cs_HelpTopic;
  cs_HelpTopic = app.GetHelpFileName() + L"::/html/create_new_db.html";
  HtmlHelp(DWORD_PTR((LPCWSTR)cs_HelpTopic), HH_DISPLAY_TOPIC);
#endif
}

#if defined(POCKET_PC)
/************************************************************************/
/* Restore the state of word completion when the password field loses   */
/* focus.                                                               */
/************************************************************************/
void CPasskeySetup::OnPasskeyKillfocus()
{
  EnableWordCompletion(m_hWnd);
}
#endif

void CPasskeySetup::OnPasskeySetfocus()
{
  m_LastFocus = IDC_PASSKEY;

#if defined(POCKET_PC)
/************************************************************************/
/* When the password field is activated, pull up the SIP and disable    */
/* word completion.                                                     */
/************************************************************************/
  DisableWordCompletion(m_hWnd);
#endif
}

void CPasskeySetup::OnVerifykeySetfocus()
{
  m_LastFocus = IDC_VERIFY;

#if defined(POCKET_PC)
/************************************************************************/
/* When the password field is activated, pull up the SIP and disable    */
/* word completion.                                                     */
/************************************************************************/
  DisableWordCompletion(m_hWnd);
#endif
}

void CPasskeySetup::OnVirtualKeyboard()
{
  // Shouldn't be here if couldn't load DLL. Static control disabled/hidden
  if (!CVKeyBoardDlg::IsOSKAvailable())
    return;

  if (m_pVKeyBoardDlg != NULL && m_pVKeyBoardDlg->IsWindowVisible()) {
    // Already there - move to top
    m_pVKeyBoardDlg->SetWindowPos(&wndTop , 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    return;
  }

  // If not already created - do it, otherwise just reset it
  if (m_pVKeyBoardDlg == NULL) {
    StringX cs_LUKBD = PWSprefs::GetInstance()->GetPref(PWSprefs::LastUsedKeyboard);
    m_pVKeyBoardDlg = new CVKeyBoardDlg(this, cs_LUKBD.c_str());
    m_pVKeyBoardDlg->Create(CVKeyBoardDlg::IDD);
  } else {
    m_pVKeyBoardDlg->ResetKeyboard();
  }

  // Now show it and make it top
  m_pVKeyBoardDlg->SetWindowPos(&wndTop , 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE);

  return;
}

LRESULT CPasskeySetup::OnInsertBuffer(WPARAM, LPARAM)
{
  // Update the variables
  UpdateData(TRUE);

  // Get the buffer
  CSecString vkbuffer = m_pVKeyBoardDlg->GetPassphrase();

  CSecEditExtn *m_pSecCtl(NULL);
  CSecString *m_pSecString;

  switch (m_LastFocus) {
    case IDC_PASSKEY:
      m_pSecCtl = m_pctlPasskey;
      m_pSecString = &m_passkey;
      break;
    case IDC_VERIFY:
      m_pSecCtl = m_pctlVerify;
      m_pSecString = &m_verify;
      break;
    default:
      // Error!
      ASSERT(0);
      return 0L;
  }

  // Find the selected characters - if any
  int nStartChar, nEndChar;
  m_pSecCtl->GetSel(nStartChar, nEndChar);

  // If any characters selected - delete them
  if (nStartChar != nEndChar)
    m_pSecString->Delete(nStartChar, nEndChar - nStartChar);

  // Insert the buffer
  m_pSecString->Insert(nStartChar, vkbuffer);

  // Update the dialog
  UpdateData(FALSE);

  // Put cursor at end of inserted text
  m_pSecCtl->SetSel(nStartChar + vkbuffer.GetLength(),
                    nStartChar + vkbuffer.GetLength());

  return 0L;
}

void CPasskeySetup::OnYubikeyBtn()
{
  UpdateData(TRUE);
  // Check that password and verification are same.
  // unlike non-Yubi usage, here we accept empty passwords,
  // which will give token-based authentication.
  // A non-empty password with Yubikey is 2-factor auth.
  CGeneralMsgBox gmb;
  if (m_passkey != m_verify) {
    gmb.AfxMessageBox(IDS_ENTRIESDONOTMATCH);
    ((CEdit*)GetDlgItem(IDC_VERIFY))->SetFocus();
    return;
  }
  yubiRequestHMACSha1(); // request HMAC of m_passkey
}

void CPasskeySetup::ProcessPhrase()
{
  // OnOK clears the passkey, so we save it
  const CSecString save_passkey = m_passkey;
  TRACE(_T("CPasskeySetup::ProcessPhrase(%s)\n"), m_passkey);
  CPKBaseDlg::OnOK();
  m_passkey = save_passkey;
}