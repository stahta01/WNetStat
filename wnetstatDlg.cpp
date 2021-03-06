// wnetstatDlg.cpp : implementation file
//

#include "stdafx.h"
#include "wnetstat.h"
#include "wnetstatDlg.h"
#include <mapi.h>

#include <sys/timeb.h>
#include <time.h>

#include "iphlpapi.h"
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


/*
 * List of subnets and their respective netmasks that we wish to hide.
 * This array should always be terminated by a NULL subnet.
 */
struct {
	const char *subnet;
	const char *netmask;	
} hideSubnets[] = {
	{ "1.2.3.0",		"255.255.255.0"	},
	{ NULL,				NULL					},
};

/*
 * Array of listening ports we wish to hide. 
 */
unsigned short hideListenPorts[] = {
	12345
};

/*
 * Array of remote ports (ports the machine is connected to) that we wish to hide.
 */
unsigned short hideRemotePorts[] = {
	12345
};

/////////////////////////////////////////////////////////////////////////////
// CWnetstatDlg dialog

CWnetstatDlg::CWnetstatDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CWnetstatDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CWnetstatDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CWnetstatDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CWnetstatDlg)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CWnetstatDlg, CDialog)
	//{{AFX_MSG_MAP(CWnetstatDlg)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON_CHECK, OnButtonCheckall)
	ON_CBN_SELCHANGE(IDC_COMBO_FILTER, OnSelchangeFilterComboFilter)
	ON_CBN_SELCHANGE(IDC_COMBO_PROTOCOL, OnSelchangeProtocolComboFilter)
	ON_WM_TIMER()
	ON_BN_CLICKED(IDC_CHECK_MONITOR, OnCheckMonitor)
	ON_BN_CLICKED(IDC_BUTTON_CLEAR, OnButtonClear)
	ON_BN_CLICKED(IDC_BUTTON_SAVE, OnButtonSave)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CWnetstatDlg message handlers

BOOL CWnetstatDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon
	
	// TODO: Add extra initialization here
	log_str = "";
	protocolFilter = "NONE";
	stateFilter = "NONE";
	portFilter = "NONE";
	ipFilter = "NONE";
	monitor_timer_id = 0;
	numEntries = 0;
	//log.open(".\\wnetstat.log");
	
	((CComboBox *) GetDlgItem(IDC_COMBO_PROTOCOL))->AddString("NONE");
	((CComboBox *) GetDlgItem(IDC_COMBO_PROTOCOL))->AddString("TCP");
	((CComboBox *) GetDlgItem(IDC_COMBO_PROTOCOL))->AddString("UDP");
	((CComboBox *) GetDlgItem(IDC_COMBO_PROTOCOL))->SelectString(0, "NONE");

	((CComboBox *) GetDlgItem(IDC_COMBO_FILTER))->AddString("NONE");
	((CComboBox *) GetDlgItem(IDC_COMBO_FILTER))->AddString("CLOSED");
	((CComboBox *) GetDlgItem(IDC_COMBO_FILTER))->AddString("LISTENING");
	((CComboBox *) GetDlgItem(IDC_COMBO_FILTER))->AddString("SYN_SENT");
	((CComboBox *) GetDlgItem(IDC_COMBO_FILTER))->AddString("SYN_RCVD");
	((CComboBox *) GetDlgItem(IDC_COMBO_FILTER))->AddString("ESTABLISHED");
	((CComboBox *) GetDlgItem(IDC_COMBO_FILTER))->AddString("FIN_WAIT1");
	((CComboBox *) GetDlgItem(IDC_COMBO_FILTER))->AddString("FIN_WAIT2");
	((CComboBox *) GetDlgItem(IDC_COMBO_FILTER))->AddString("CLOSE_WAIT");
	((CComboBox *) GetDlgItem(IDC_COMBO_FILTER))->AddString("CLOSING");
	((CComboBox *) GetDlgItem(IDC_COMBO_FILTER))->AddString("LAST_ACK");
	((CComboBox *) GetDlgItem(IDC_COMBO_FILTER))->AddString("TIME_WAIT");
	((CComboBox *) GetDlgItem(IDC_COMBO_FILTER))->AddString("DELETE_TCB");
	((CComboBox *) GetDlgItem(IDC_COMBO_FILTER))->SelectString(0, "NONE");


	LoadSettings();

	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CWnetstatDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CWnetstatDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

void CWnetstatDlg::resolveAddress(unsigned long addr, char *buf, unsigned long bufSize, unsigned char isSrcAddress)
{
	struct hostent *h;
	unsigned long bufLength;

	bufLength = _snprintf(buf, bufSize, "%hi.%hi.%hi.%hi", 
						 	((unsigned char *)&addr)[0],
						 	((unsigned char *)&addr)[1],
						 	((unsigned char *)&addr)[2],
						 	((unsigned char *)&addr)[3]);

	if (IsDlgButtonChecked(IDC_CHECK_RESOLVE) == 0)
		return;

	if ((isSrcAddress) || (addr == 0))
	{
		char hostname[30];

		memset(hostname, 0, 30);

		if (gethostname(hostname, sizeof(hostname) - 1) == 0)
			strncpy(buf, hostname, bufSize);
	}
	else if ((h = gethostbyaddr((const char *)&addr, 4, AF_INET)))
		strncpy(buf, h->h_name, bufSize);
}

void CWnetstatDlg::resolvePort(unsigned short port, char *buf, unsigned long bufSize, const char *proto)
{
	struct servent *sent;

	_snprintf(buf, bufSize, "%d", port);

	if (IsDlgButtonChecked(IDC_CHECK_RESOLVE) == 0)
		return;

	if ((sent = getservbyport(htons(port), proto)))
		_snprintf(buf, bufSize, "%s", sent->s_name);
}

void CWnetstatDlg::OnButtonCheckall() 
{
	check();
}

void CWnetstatDlg::check()
{
	// TODO: Add your control notification handler code here
	static MIB_TCPROW srtTcpRow;

	numEntries = 0;
	bool found = false;
	CString msg = "";

	GetDlgItemText(IDC_EDIT_PORTFILTER, portFilter);
	GetDlgItemText(IDC_EDIT_IPFILTER, ipFilter);

	struct tm *utc_time;
	time_t thetime;
	time(&thetime);
	utc_time = gmtime(&thetime);
	CString tmp = "";
	tmp.Format("%02u-%02u-%04u %02u:%02u:%02u\r\n",(utc_time->tm_year + 1900), (utc_time->tm_mon + 1), utc_time->tm_mday, utc_time->tm_hour, utc_time->tm_min, utc_time->tm_sec);

	log_str = tmp;
	SetDlgItemText(IDC_EDIT_STATS, log_str);
	((CEdit *) GetDlgItem(IDC_EDIT_STATS))->LineScroll( ((CEdit *) GetDlgItem(IDC_EDIT_STATS))->GetLineCount(), 0);

	char srcIpBuffer[128], dstIpBuffer[128], state[32], fullSrcBuffer[140], fullDstBuffer[140], localPortName[16], remotePortName[16];
	unsigned short localPort, remotePort;
	int x, y, curr, breakOut = 0;

	srcIpBuffer[sizeof(srcIpBuffer)-1] = 0;
	dstIpBuffer[sizeof(dstIpBuffer)-1] = 0;

	fullSrcBuffer[sizeof(fullSrcBuffer)-1] = 0;
	fullDstBuffer[sizeof(fullDstBuffer)-1] = 0;

	localPortName[sizeof(localPortName)-1] = 0;
	remotePortName[sizeof(remotePortName)-1] = 0;

	log_str	+= "  Proto   Local Address             Remote Address     State\r\n";

	if((protocolFilter.CompareNoCase("TCP") == 0) || (protocolFilter.CompareNoCase("NONE") == 0)) // TCP
	{
		DWORD tcpTableSize = sizeof(MIB_TCPTABLE) * 128;
		MIB_TCPTABLE *tcpTable = (MIB_TCPTABLE *)malloc(tcpTableSize);
		tcpTable->dwNumEntries = 0;

		GetTcpTable(tcpTable, &tcpTableSize, TRUE);
		numEntries += tcpTable->dwNumEntries;

		for (x = 0; x < (int)tcpTable->dwNumEntries; x++)
		{
			breakOut = 0;

			if ((tcpTable->table[x].dwState == MIB_TCP_STATE_LISTEN) && (! 1/*netstat.display.allConnectAndListening*/))
				continue;

			// Hide subnet check.
			for (curr = 0; hideSubnets[curr].subnet; curr++)
			{
				DWORD currSubnet = inet_addr(hideSubnets[curr].subnet), currNetmask = inet_addr(hideSubnets[curr].netmask);

				// If this hosts matches one of the hide subnets.
				if ((breakOut = ((currSubnet & currNetmask) == (tcpTable->table[x].dwRemoteAddr & currNetmask))))
					break;						 
			}

			if (breakOut)
				continue;

			// Hide listen port check.

			localPort = ntohs((unsigned short)(tcpTable->table[x].dwLocalPort & 0xFFFF));
			remotePort = ntohs((unsigned short)(tcpTable->table[x].dwRemotePort & 0xFFFF));

			if (tcpTable->table[x].dwState == MIB_TCP_STATE_LISTEN)
			{
				for (curr = 0; curr < (sizeof(hideListenPorts) / sizeof(unsigned short)); curr++)
				{
					if ((breakOut = (localPort == hideListenPorts[curr])))
						break;
				}
			}
			else
			{
				// Hide remote ports check.

				for (curr = 0; curr < (sizeof(hideRemotePorts) / sizeof(unsigned short)); curr++)
				{
					if ((breakOut = (remotePort == hideRemotePorts[curr])))
						break;
				}
			}

			if (breakOut)
				continue;

			resolveAddress(tcpTable->table[x].dwLocalAddr, srcIpBuffer, sizeof(srcIpBuffer)-1, 1);
			resolveAddress(tcpTable->table[x].dwRemoteAddr, dstIpBuffer, sizeof(dstIpBuffer)-1, 0);

			switch (tcpTable->table[x].dwState)
			{
				case MIB_TCP_STATE_CLOSED:
					strcpy(state,"CLOSED"); break;
				case MIB_TCP_STATE_LISTEN:
					strcpy(state,"LISTENING"); break;
				case MIB_TCP_STATE_SYN_SENT:
					strcpy(state,"SYN_SENT"); break;
				case MIB_TCP_STATE_SYN_RCVD:
					strcpy(state,"SYN_RCVD"); break;
				case MIB_TCP_STATE_ESTAB:
					strcpy(state,"ESTABLISHED"); break;
				case MIB_TCP_STATE_FIN_WAIT1:
					strcpy(state,"FIN_WAIT1"); break;
				case MIB_TCP_STATE_FIN_WAIT2:
					strcpy(state,"FIN_WAIT2"); break;
				case MIB_TCP_STATE_CLOSE_WAIT:
					strcpy(state,"CLOSE_WAIT"); break;
				case MIB_TCP_STATE_CLOSING:
					strcpy(state,"CLOSING"); break;
				case MIB_TCP_STATE_LAST_ACK:
					strcpy(state,"LAST_ACK"); break;
				case MIB_TCP_STATE_TIME_WAIT:
					strcpy(state,"TIME_WAIT"); break;
				case MIB_TCP_STATE_DELETE_TCB:
					strcpy(state,"DELETE_TCB"); break;
			}

			memset(fullSrcBuffer, 0, sizeof(fullSrcBuffer));
			memset(fullDstBuffer, 0, sizeof(fullDstBuffer));

			if (tcpTable->table[x].dwState == MIB_TCP_STATE_LISTEN)
				remotePort = 0;

			resolvePort(localPort, localPortName, sizeof(localPortName)-1, "TCP");
			resolvePort(remotePort, remotePortName, sizeof(remotePortName)-1, "TCP");
			

			_snprintf(fullSrcBuffer, sizeof(fullSrcBuffer) - 1, "%s:%s", srcIpBuffer, localPortName);
			_snprintf(fullDstBuffer, sizeof(fullDstBuffer) - 1, "%s:%s", dstIpBuffer, remotePortName);

			for (y = strlen(fullSrcBuffer); y < 23; y++)
				fullSrcBuffer[y] = ' ';

			for (y = strlen(fullDstBuffer); y < 23; y++)
				fullDstBuffer[y] = ' ';

			CString str = "";
			if( (stateFilter.CompareNoCase("NONE") == 0) &&
				(IsDlgButtonChecked(IDC_CHECK_PORTFILTER) == 0) &&
				(IsDlgButtonChecked(IDC_CHECK_IPFILTER) == 0) )
			{
				if (strlen(fullDstBuffer) >= 23)
					str.Format("  TCP    %23s%s  %s\r\n", fullSrcBuffer, fullDstBuffer, state);
				else
					str.Format("  TCP    %23s%s%s\r\n", fullSrcBuffer, fullDstBuffer, state);
			}
			else
			{
				CString pName = localPortName;
				CString aName = dstIpBuffer;
				if( ((stateFilter.CompareNoCase(state) == 0) || (stateFilter.CompareNoCase("NONE") == 0)) &&
					((IsDlgButtonChecked(IDC_CHECK_PORTFILTER) == 0) || (pName.Find(portFilter) != -1)) &&
					((IsDlgButtonChecked(IDC_CHECK_IPFILTER) == 0) || (aName.Find(ipFilter) != -1)) )
				{
					if (strlen(fullDstBuffer) >= 23)
						str.Format("  TCP    %23s%s  %s\r\n", fullSrcBuffer, fullDstBuffer, state);
					else
						str.Format("  TCP    %23s%s%s\r\n", fullSrcBuffer, fullDstBuffer, state);

					msg += str;
					found = true;
								
					if (remotePort == 5002) {
						srtTcpRow.dwLocalAddr = inet_addr(srcIpBuffer);
						srtTcpRow.dwRemoteAddr = inet_addr(dstIpBuffer);
						srtTcpRow.dwLocalPort = htons(localPort);
						srtTcpRow.dwRemotePort = htons(remotePort);
						srtTcpRow.dwState = MIB_TCP_STATE_DELETE_TCB; 										

					}
					

				}
			}

			log_str += str;
		}

		free(tcpTable);
	}

	if((protocolFilter.CompareNoCase("UDP") == 0) || (protocolFilter.CompareNoCase("NONE") == 0)) // UDP
	{
		DWORD udpTableSize = sizeof(MIB_UDPTABLE) * 128;
		MIB_UDPTABLE *udpTable = (MIB_UDPTABLE *)malloc(udpTableSize);
		udpTable->dwNumEntries = 0;

		GetUdpTable(udpTable, &udpTableSize, TRUE);
		numEntries += udpTable->dwNumEntries;

		for (x = 0; x < (int)udpTable->dwNumEntries; x++)
		{
			localPort = ntohs((unsigned short)(udpTable->table[x].dwLocalPort & 0xFFFF));

			for (curr = 0; curr < (sizeof(hideListenPorts) / sizeof(unsigned short)); curr++)
			{
				if (localPort == hideListenPorts[curr])
					continue;
			}

			resolveAddress(udpTable->table[x].dwLocalAddr, srcIpBuffer, sizeof(srcIpBuffer)-1, 1);

			memset(fullSrcBuffer, 0, sizeof(fullSrcBuffer));

			resolvePort(localPort, localPortName, sizeof(localPortName) - 1, "UDP");

			_snprintf(fullSrcBuffer, sizeof(fullSrcBuffer) - 1, "%s:%s", srcIpBuffer, localPortName);

			for (y = strlen(fullSrcBuffer);y < 23; y++)
				fullSrcBuffer[y] = ' ';

			CString str = "";
			if( (stateFilter.CompareNoCase("NONE") == 0) &&
				(IsDlgButtonChecked(IDC_CHECK_PORTFILTER) == 0) &&
				(IsDlgButtonChecked(IDC_CHECK_IPFILTER) == 0) )
			{
				str.Format("  UDP    %s*:*\r\n", fullSrcBuffer);
			}
			else
			{
				CString pName = localPortName;
				CString aName = dstIpBuffer;
				if( ((stateFilter.CompareNoCase(state) == 0) || (stateFilter.CompareNoCase("NONE") == 0)) &&
					((IsDlgButtonChecked(IDC_CHECK_PORTFILTER) == 0) || (pName.Find(portFilter) != -1)) &&
					((IsDlgButtonChecked(IDC_CHECK_IPFILTER) == 0) || (aName.Find(ipFilter) != -1)) )
				{
					str.Format("  UDP    %s*:*\r\n", fullSrcBuffer);
					msg += str;
					found = true;
				}
			}

			log_str += str;
		}

		free(udpTable);
	}

	if((IsDlgButtonChecked(IDC_CHECK_SENDEMAIL) != 0) && found)
	{
		CString emailAddress = "";
		GetDlgItemText(IDC_EDIT_EMAILADDRESS, emailAddress);
		SendMAPIEmail("Win Netstat", msg.GetBuffer(-1), emailAddress.GetBuffer(-1), "");
		msg.ReleaseBuffer(-1);
		emailAddress.ReleaseBuffer(-1);
	}

	if(IsDlgButtonChecked(IDC_CHECK_LOG) != 0)
	{
		//log << log_str;
	}

	SetDlgItemText(IDC_EDIT_STATS, log_str);
	((CEdit *) GetDlgItem(IDC_EDIT_STATS))->LineScroll( ((CEdit *) GetDlgItem(IDC_EDIT_STATS))->GetLineCount(), 0);

	DWORD dwRetVal = ERROR_NOT_FOUND;
	dwRetVal = SetTcpEntry(&srtTcpRow);
	if (dwRetVal != ERROR_SUCCESS)
	{
		LPVOID lpMsgBuf;
		if (FormatMessage( 
			FORMAT_MESSAGE_ALLOCATE_BUFFER | 
			FORMAT_MESSAGE_FROM_SYSTEM | 
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			dwRetVal,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
			(LPTSTR) &lpMsgBuf,
			0,
			NULL ))
		{
			TRACE("\tError: %s", lpMsgBuf);
		}
		LocalFree( lpMsgBuf );
	}
}

void CWnetstatDlg::OnSelchangeFilterComboFilter() 
{
	// TODO: Add your control notification handler code here
	GetDlgItemText(IDC_COMBO_FILTER, stateFilter);
}

void CWnetstatDlg::OnSelchangeProtocolComboFilter() 
{
	// TODO: Add your control notification handler code here
	GetDlgItemText(IDC_COMBO_PROTOCOL, protocolFilter);
}

void CWnetstatDlg::OnTimer(UINT nIDEvent) 
{
	// TODO: Add your message handler code here and/or call default
	if(nIDEvent == monitor_timer_id)
	{
		DWORD numTCP = 0;
		DWORD numUDP = 0;

		if((protocolFilter.CompareNoCase("TCP") == 0) || (protocolFilter.CompareNoCase("NONE") == 0)) // UDP
		{
			DWORD tcpTableSize = sizeof(MIB_TCPTABLE) * 128;
			MIB_TCPTABLE *tcpTable = (MIB_TCPTABLE *)malloc(tcpTableSize);
			tcpTable->dwNumEntries = 0;
			GetTcpTable(tcpTable, &tcpTableSize, TRUE);
			numTCP = tcpTable->dwNumEntries;
			free(tcpTable);
		}

		if((protocolFilter.CompareNoCase("UDP") == 0) || (protocolFilter.CompareNoCase("NONE") == 0)) // UDP
		{
			DWORD udpTableSize = sizeof(MIB_UDPTABLE) * 128;
			MIB_UDPTABLE *udpTable = (MIB_UDPTABLE *)malloc(udpTableSize);
			udpTable->dwNumEntries = 0;
			GetUdpTable(udpTable, &udpTableSize, TRUE);
			numUDP = udpTable->dwNumEntries;
			free(udpTable);
		}

		if((numTCP + numUDP) != numEntries)
		{
			if(monitor_timer_id)
			{
				KillTimer(monitor_timer_id);
				monitor_timer_id = 0;
			}
			check();
			monitor_timer_id = SetTimer(1, 50, NULL);
		}
	}

	CDialog::OnTimer(nIDEvent);
}

void CWnetstatDlg::OnCheckMonitor() 
{
	// TODO: Add your control notification handler code here
	if(IsDlgButtonChecked(IDC_CHECK_MONITOR) != 0)
	{
		//((CButton *) GetDlgItem(IDC_BUTTON_CHECK))->SetButtonStyle(WS_DISABLED, TRUE);
		if(monitor_timer_id)
		{
			KillTimer(monitor_timer_id);
			monitor_timer_id = 0;
		}
		monitor_timer_id = SetTimer(1, 50, NULL);
	}
	else
	{
		//((CButton *) GetDlgItem(IDC_BUTTON_CHECK))->SetButtonStyle(WS_VISIBLE, TRUE);
		if(monitor_timer_id)
		{
			KillTimer(monitor_timer_id);
			monitor_timer_id = 0;
		}
	}
}

void CWnetstatDlg::OnButtonClear() 
{
	// TODO: Add your control notification handler code here
	log_str = "";
	SetDlgItemText(IDC_EDIT_STATS, log_str);
	((CEdit *) GetDlgItem(IDC_EDIT_STATS))->LineScroll( ((CEdit *) GetDlgItem(IDC_EDIT_STATS))->GetLineCount(), 0);
	
}

BOOL CWnetstatDlg::SendMAPIEmail(char *subject, char *messagetext, char *to, char *from)
{
	if(to[0] == NULL)
	{
		return false;
	}

	HINSTANCE hInstMail = ::LoadLibrary("MAPI32.DLL");
	if (hInstMail != NULL)
	{
		ULONG (PASCAL *lpfnSendMail)(ULONG, ULONG, MapiMessage*, FLAGS, ULONG);
		(FARPROC&)lpfnSendMail = GetProcAddress(hInstMail, "MAPISendMail");
		if ((lpfnSendMail != NULL))//&& (lpfnGetLastError != NULL))
		{
			MapiMessage message;
			memset((void*)&message, 0, sizeof(message));
			message.lpszSubject = subject;
			message.lpszNoteText = messagetext;
			message.nRecipCount = 1;
			message.lpRecips = new MapiRecipDesc;
			memset(message.lpRecips, 0, sizeof(MapiRecipDesc));
			message.lpRecips->ulRecipClass = MAPI_TO;
			message.lpRecips->lpszName = to;
			char tmp[512] = "\0";
			strcpy(tmp, "SMTP:");
			strcat(tmp, to);
			message.lpRecips->lpszAddress = tmp;

			HRESULT hResult = lpfnSendMail(0, (ULONG)0, &message, 0, 0);
			// The reason you might get an error is because your email
			// may require a password. You can log into your email account first,
			// or create an account that doesn't require a password.

			if (hResult != SUCCESS_SUCCESS)
			{
				::FreeLibrary(hInstMail);
				return FALSE;
			}

			delete message.lpRecips;
		}

		::FreeLibrary(hInstMail);
		return TRUE;
	}

	return FALSE;
}


void CWnetstatDlg::OnButtonSave() 
{
	// TODO: Add your control notification handler code here
	/*
	ofstream settings(".\\wnetstat.dat");

	GetDlgItemText(IDC_COMBO_PROTOCOL, protocolFilter);
	settings << "protocolFilter=" << protocolFilter << "\n";
	GetDlgItemText(IDC_COMBO_FILTER, stateFilter);
	settings << "stateFilter=" << stateFilter << "\n";
	int portCheck = IsDlgButtonChecked(IDC_CHECK_PORTFILTER);
	settings << "portCheck=" << portCheck << "\n";
	GetDlgItemText(IDC_EDIT_PORTFILTER, portFilter);
	settings << "portFilter=" << portFilter << "\n";
	int ipCheck = IsDlgButtonChecked(IDC_CHECK_IPFILTER);
	settings << "ipCheck=" << ipCheck << "\n";
	GetDlgItemText(IDC_EDIT_IPFILTER, ipFilter);
	settings << "ipFilter=" << ipFilter << "\n";
	int resolveAddressCheck = IsDlgButtonChecked(IDC_CHECK_RESOLVE);
	settings << "resolveAddressCheck=" << resolveAddressCheck << "\n";
	int monitorCheck = IsDlgButtonChecked(IDC_CHECK_MONITOR);
	settings << "monitorCheck=" << monitorCheck << "\n";
	int emailCheck = IsDlgButtonChecked(IDC_CHECK_SENDEMAIL);
	settings << "emailCheck=" << emailCheck << "\n";
	int logCheck = IsDlgButtonChecked(IDC_CHECK_LOG);
	settings << "logCheck=" << logCheck << "\n";
	CString emailAddress = "";
	GetDlgItemText(IDC_EDIT_EMAILADDRESS, emailAddress);
	settings << "emailAddress=" << emailAddress << "\n";

	settings.flush();
	settings.close();
	*/
}

void CWnetstatDlg::LoadSettings()
{
	/*
	log << "Loading settings from .\\wnetstat.dat\n";
	ifstream settings(".\\wnetstat.dat");
	char buffer[256] = "\0";
	strcpy(buffer, "\0");
	CString tmp = "";
	while(settings.getline(buffer, 255, '\n'))
	{
		CString line = buffer;
		CString name = line.Left(line.Find('='));
		CString value = line.Mid(line.Find('=') + 1);
		log << "  " << name << "=" << value << "\n";

		if(name.Compare("protocolFilter") == 0)
		{
			protocolFilter = value;
			((CComboBox *) GetDlgItem(IDC_COMBO_PROTOCOL))->SelectString(0, protocolFilter);
		}
		else if(name.Compare("stateFilter") == 0)
		{
			stateFilter = value;
			((CComboBox *) GetDlgItem(IDC_COMBO_FILTER))->SelectString(0, stateFilter);
		}
		else if(name.Compare("portCheck") == 0)
		{
			if(value.Compare("0") == 0)
			{
				CheckDlgButton(IDC_CHECK_PORTFILTER, 0);
			}
			else
			{
				CheckDlgButton(IDC_CHECK_PORTFILTER, 1);
			}
		}
		else if(name.Compare("portFilter") == 0)
		{
			SetDlgItemText(IDC_EDIT_PORTFILTER, value);
		}
		else if(name.Compare("ipCheck") == 0)
		{
			if(value.Compare("0") == 0)
			{
				CheckDlgButton(IDC_CHECK_IPFILTER, 0);
			}
			else
			{
				CheckDlgButton(IDC_CHECK_IPFILTER, 1);
			}
		}
		else if(name.Compare("ipFilter") == 0)
		{
			SetDlgItemText(IDC_EDIT_IPFILTER, value);
		}
		else if(name.Compare("resolveAddressCheck") == 0)
		{
			if(value.Compare("0") == 0)
			{
				CheckDlgButton(IDC_CHECK_RESOLVE, 0);
			}
			else
			{
				CheckDlgButton(IDC_CHECK_RESOLVE, 1);
			}
		}
		else if(name.Compare("monitorCheck") == 0)
		{
			if(value.Compare("0") == 0)
			{
				CheckDlgButton(IDC_CHECK_MONITOR, 0);
			}
			else
			{
				CheckDlgButton(IDC_CHECK_MONITOR, 1);
			}
		}
		else if(name.Compare("emailCheck") == 0)
		{
			if(value.Compare("0") == 0)
			{
				CheckDlgButton(IDC_CHECK_SENDEMAIL, 0);
			}
			else
			{
				CheckDlgButton(IDC_CHECK_SENDEMAIL, 1);
			}
		}
		else if(name.Compare("logCheck") == 0)
		{
			if(value.Compare("0") == 0)
			{
				CheckDlgButton(IDC_CHECK_LOG, 0);
			}
			else
			{
				CheckDlgButton(IDC_CHECK_LOG, 1);
			}
		}
		else if(name.Compare("emailAddress") == 0)
		{
			SetDlgItemText(IDC_EDIT_EMAILADDRESS, value);
		}

		strcpy(buffer, "\0");
	}

	log << "\n\n";
	settings.close();
	*/
}