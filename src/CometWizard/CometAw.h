#if !defined(AFX_COMETAW_H__75218EE7_56E4_45D6_BECF_294381FF7E21__INCLUDED_)
#define AFX_COMETAW_H__75218EE7_56E4_45D6_BECF_294381FF7E21__INCLUDED_

// cometaw.h : header file
//

class CDialogChooser;

// All function calls made by mfcapwz.dll to this custom AppWizard (except for
//  GetCustomAppWizClass-- see comet.cpp) are through this class.  You may
//  choose to override more of the CCustomAppWiz virtual functions here to
//  further specialize the behavior of this custom AppWizard.
class CCometAppWiz : public CCustomAppWiz
{
public:
	virtual CAppWizStepDlg* Next(CAppWizStepDlg* pDlg);
		
	virtual void InitCustomAppWiz();
	virtual void ExitCustomAppWiz();
	virtual void CustomizeProject(IBuildProject* pProject);
};

// This declares the one instance of the CCometAppWiz class.  You can access
//  m_Dictionary and any other public members of this class through the
//  global Cometaw.  (Its definition is in cometaw.cpp.)
extern CCometAppWiz Cometaw;

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_COMETAW_H__75218EE7_56E4_45D6_BECF_294381FF7E21__INCLUDED_)
