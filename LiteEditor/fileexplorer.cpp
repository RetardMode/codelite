//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
// copyright            : (C) 2008 by Eran Ifrah                            
// file name            : fileexplorer.cpp              
//                                                                          
// -------------------------------------------------------------------------
// A                                                                        
//              _____           _      _     _ _                            
//             /  __ \         | |    | |   (_) |                           
//             | /  \/ ___   __| | ___| |    _| |_ ___                      
//             | |    / _ \ / _  |/ _ \ |   | | __/ _ )                     
//             | \__/\ (_) | (_| |  __/ |___| | ||  __/                     
//              \____/\___/ \__,_|\___\_____/_|\__\___|                     
//                                                                          
//                                                  F i l e                 
//                                                                          
//    This program is free software; you can redistribute it and/or modify  
//    it under the terms of the GNU General Public License as published by  
//    the Free Software Foundation; either version 2 of the License, or     
//    (at your option) any later version.                                   
//                                                                          
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
 #include "wx/xrc/xmlres.h"
#include "fileexplorer.h"
#include "fileexplorertree.h"
#include "wx/sizer.h"
#include "wx/tokenzr.h"

#include "macros.h" 
#include "globals.h"
#include "plugin.h"
#include "editor_config.h"
#include "manager.h"
#include "workspace_pane.h"
#include "frame.h"

FileExplorer::FileExplorer(wxWindow *parent, const wxString &caption)
: wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(250, 300))
, m_caption(caption)
, m_isLinkedToEditor(false)
#ifdef __WXMSW__
, m_thread(this)
#endif
{
	long link(1);
	EditorConfigST::Get()->GetLongValue(wxT("LinkFileExplorerToEditor"), link);
	m_isLinkedToEditor = link ? true : false;
	CreateGUIControls();
}

FileExplorer::~FileExplorer()
{
}

void FileExplorer::CreateGUIControls()
{
	wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);
	SetSizer(mainSizer);
	
#ifdef __WXMSW__
	wxArrayString volumes;
	Connect(wxEVT_THREAD_VOLUME_COMPLETED, wxCommandEventHandler(FileExplorer::OnVolumes), NULL, this);
	
	m_thread.Create();
	m_thread.Run();
	
	m_volumes = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, volumes, 0 );
	mainSizer->Add(m_volumes, 0, wxEXPAND|wxALL, 1);
#endif

	m_fileTree = new FileExplorerTree(this, wxID_ANY);
	mainSizer->Add(m_fileTree, 1, wxEXPAND|wxALL, 1);
	
	wxToolBar *tb = new wxToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTB_FLAT|wxTB_HORIZONTAL);
	mainSizer->Add(tb, 0, wxEXPAND);
	
	tb->AddTool(XRCID("link_editor"), wxEmptyString, wxXmlResource::Get()->LoadBitmap(wxT("link_editor")), wxT("Link Editor"), wxITEM_CHECK);
	tb->ToggleTool(XRCID("link_editor"), m_isLinkedToEditor);
	tb->AddTool(XRCID("collapse_all"), wxEmptyString, wxXmlResource::Get()->LoadBitmap(wxT("collapse")), wxT("Collapse All"), wxITEM_NORMAL);
	tb->AddTool(XRCID("go_home"), wxEmptyString, wxXmlResource::Get()->LoadBitmap(wxT("gohome")), wxT("Goto Current Directory"), wxITEM_NORMAL);
	tb->Realize();
	
	Connect( XRCID("link_editor"), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( FileExplorer::OnLinkEditor ));
	Connect( XRCID("collapse_all"), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( FileExplorer::OnCollapseAll ));
	Connect( XRCID("go_home"), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( FileExplorer::OnGoHome ));

	mainSizer->Layout();

#ifdef __WXMSW__
	m_fileTree->Connect(wxVDTC_ROOT_CHANGED, wxCommandEventHandler(FileExplorer::OnRootChanged), NULL, this);
	ConnectChoice(m_volumes, FileExplorer::OnVolumeChanged);
#endif
}

void FileExplorer::Scan()
{
	wxString cwd = wxGetCwd();
	cwd << wxT("/");
	wxFileName fn(cwd);
	if(fn.HasVolume()){
		wxString root;
		root << fn.GetVolume() << wxT(":\\");
		m_fileTree->SetRootPath(root, false, wxVDTC_DEFAULT);
#ifdef __WXMSW__
		if(m_volumes->FindString(fn.GetVolume() + wxT(":\\")) == wxNOT_FOUND) {
			m_volumes->AppendString(fn.GetVolume() + wxT(":\\"));
		}
		m_volumes->SetStringSelection(fn.GetVolume() + wxT(":\\"));
#endif
	}else{
		m_fileTree->SetRootPath(wxT("/"), false, wxVDTC_DEFAULT);
	}
	
	m_fileTree->ExpandToPath(fn);
}

#ifdef __WXMSW__
void FileExplorer::OnVolumeChanged(wxCommandEvent &e)
{
	wxUnusedVar(e);
	//Get the selection
	wxString newRoot = m_volumes->GetStringSelection();
	m_fileTree->SetRootPath(newRoot);
}

void FileExplorer::OnRootChanged(wxCommandEvent &e)
{
	wxTreeItemId root = m_fileTree->GetRootItem();
	if(root.IsOk()){
		wxString vol = m_fileTree->GetItemText(root);
		this->m_volumes->SetStringSelection(vol);
	}
	e.Skip();
}

void FileExplorer::OnVolumes(wxCommandEvent &e)
{
	wxString curvol = m_volumes->GetStringSelection();
	wxArrayString volumes = wxStringTokenize(e.GetString(), wxT(";"), wxTOKEN_STRTOK);
	int where = volumes.Index(curvol);
	if(where != wxNOT_FOUND){
		volumes.RemoveAt((size_t)where);
	}
	m_volumes->Append(volumes);
}

#endif

void FileExplorer::OnCollapseAll(wxCommandEvent &e)
{
	wxUnusedVar(e);
    
	wxTreeItemId root = m_fileTree->GetRootItem();
	if(root.IsOk() == false) {
		return;
	}
	
	if(m_fileTree->ItemHasChildren(root) == false) {
		return;
	}
	
	m_fileTree->Freeze();
	
	//iterate over all the projects items and collapse them all
	wxTreeItemIdValue cookie;
	wxTreeItemId child = m_fileTree->GetFirstChild(root, cookie);
	while( child.IsOk() ) {
		m_fileTree->CollapseAllChildren(child);
		child = m_fileTree->GetNextChild(root, cookie);
	}
	
	m_fileTree->Thaw();
    
	wxTreeItemId sel = m_fileTree->GetSelection();
	if (sel.IsOk())
		m_fileTree->EnsureVisible(sel);
}

void FileExplorer::OnGoHome(wxCommandEvent &e)
{
	wxUnusedVar(e);
	ManagerST::Get()->ShowWorkspacePane(WorkspacePane::EXPLORER);
	this->Freeze();
	Scan();
	this->Thaw();
}

void FileExplorer::OnLinkEditor(wxCommandEvent &e)
{
	wxUnusedVar(e);
	m_isLinkedToEditor = !m_isLinkedToEditor;
	// save the value
	EditorConfigST::Get()->SaveLongValue(wxT("LinkFileExplorerToEditor"), m_isLinkedToEditor ? 1 : 0);
    if (m_isLinkedToEditor) {
        wxCommandEvent event(wxEVT_COMMAND_MENU_SELECTED, XRCID("show_in_explorer"));
        Frame::Get()->AddPendingEvent(event);
    }
}
