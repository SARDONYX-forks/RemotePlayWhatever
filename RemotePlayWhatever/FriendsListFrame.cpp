#include <algorithm>
#include <cstring>
#include <vector>
#include <wx/clipbrd.h>
#include <wx/srchctrl.h>
#include <wx/time.h>
#include <wx/wx.h>
#ifdef __WXMSW__
#include <wx/msw/wrapwin.h>
#endif
#include "ControlFrame.h"
#include "FriendPanel.h"
#include "FriendsListFrame.h"
#include "GameInfo.h"
#include "TrayIcon.h"
#include "UiHelpers.h"

static const wxColour kReadyGreen(30, 130, 50);
static const wxColour kNoticeOrange(190, 100, 0);

FriendsListFrame::FriendsListFrame(RemotePlayInviteHandler *handler)
    : wxFrame(NULL, wxID_ANY, _("Remote Play Whatever"), wxPoint(50, 50),
              wxSize(275, 650)),
      m_onlineSizer(nullptr), m_offlineSizer(nullptr), m_onlineLabel(nullptr),
      m_offlineLabel(nullptr), m_tray(nullptr), m_search(nullptr),
      m_status(nullptr), m_copyLinkButton(nullptr), m_endButton(nullptr),
      m_noticeUntil(0), m_copiedUntil(0), m_tick(this),
      m_askingGroupFallback(false), m_inviteQueued(false),
      m_remoteInviteResultCb(this, &FriendsListFrame::OnRemotePlayInviteResult),
      m_personaStateCb(this, &FriendsListFrame::OnPersonaStateChange) {
  SetSizeHints(wxDefaultSize, wxDefaultSize);

  SetMinSize(wxSize(275, 300));
  SetMaxSize(wxSize(350, -1));
  SetIcons(AppIcons());

  m_pRemoteInvite = handler;

  wxBoxSizer *frameSizer = new wxBoxSizer(wxVERTICAL);

  // What will be shared, so it's clear before inviting anyone.
  wxPanel *statusPanel = new wxPanel(this, wxID_ANY);
  statusPanel->SetBackgroundColour(
      wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
  wxBoxSizer *statusSizer = new wxBoxSizer(wxVERTICAL);
  m_status = new wxStaticText(statusPanel, wxID_ANY, wxEmptyString);
  m_status->SetFont(m_status->GetFont().Bold());
  statusSizer->Add(m_status, 0, wxALL | wxEXPAND, 10);
  wxBoxSizer *sessionButtons = new wxBoxSizer(wxHORIZONTAL);
  m_copyLinkButton = new wxButton(statusPanel, wxID_ANY, "Copy guest link");
  m_copyLinkButton->SetToolTip("Copy this session's guest link again.");
  m_copyLinkButton->Bind(wxEVT_BUTTON, &FriendsListFrame::OnCopyLink, this);
  m_copyLinkButton->Hide();
  sessionButtons->Add(m_copyLinkButton, 1, wxRIGHT, 6);
  m_endButton = new wxButton(statusPanel, wxID_ANY, "End session");
  m_endButton->SetToolTip("Disconnect everyone, cancel unused invites and "
                          "guest links, and stop sharing your screen.");
  m_endButton->Bind(wxEVT_BUTTON, &FriendsListFrame::OnEndSession, this);
  m_endButton->Hide();
  sessionButtons->Add(m_endButton, 1);
  statusSizer->Add(sessionButtons, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND,
                   10);
  statusPanel->SetSizer(statusSizer);
  frameSizer->Add(statusPanel, 0, wxEXPAND | wxBOTTOM, 1);

  m_friendsListContainer = new wxScrolledWindow(
      this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL);
  m_friendsListContainer->SetScrollRate(5, 5);
  m_friendsListSizer = new wxBoxSizer(wxVERTICAL);

  BtnPanel *guestBtn = new BtnPanel(m_friendsListContainer, rpwID_INVITE_GUEST,
                                    "Create guest invite link");
  m_friendsListSizer->Add(guestBtn, 0, wxALL | wxEXPAND, 0);

  wxStaticText *flLabel = new wxStaticText(
      m_friendsListContainer, wxID_ANY, "or invite a friend", wxDefaultPosition,
      wxDefaultSize, wxALIGN_CENTER_HORIZONTAL);
  m_friendsListSizer->Add(flLabel, 0, wxALL | wxEXPAND, 10);

  m_search = new wxSearchCtrl(m_friendsListContainer, wxID_ANY);
  m_search->SetDescriptiveText("Search friends");
  m_search->ShowCancelButton(true);
  m_friendsListSizer->Add(m_search, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND,
                          8);
  m_search->Bind(wxEVT_TEXT, [this](wxCommandEvent &) { ApplyFilter(); });
  m_search->Bind(wxEVT_SEARCHCTRL_CANCEL_BTN, [this](wxCommandEvent &) {
    m_search->Clear();
    ApplyFilter();
  });

  // Online friends, then the ones who appear offline (they may just be
  // invisible, and an invite link reaches them in Steam chat anyway).
  wxColour headerColour = wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT);
  m_onlineLabel = new wxStaticText(m_friendsListContainer, wxID_ANY, "Online");
  m_onlineLabel->SetForegroundColour(headerColour);
  m_friendsListSizer->Add(m_onlineLabel, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);
  m_onlineSizer = new wxBoxSizer(wxVERTICAL);
  m_friendsListSizer->Add(m_onlineSizer, 0, wxEXPAND);
  m_offlineLabel =
      new wxStaticText(m_friendsListContainer, wxID_ANY, "Offline");
  m_offlineLabel->SetForegroundColour(headerColour);
  m_friendsListSizer->Add(m_offlineLabel, 0,
                          wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 8);
  m_offlineSizer = new wxBoxSizer(wxVERTICAL);
  m_friendsListSizer->Add(m_offlineSizer, 0, wxEXPAND);

  int cFriends =
      GClientContext()->SteamFriends()->GetFriendCount(k_EFriendFlagImmediate);
  for (int i = 0; i < cFriends; ++i) {
    AddFreiendPanel(GClientContext()->SteamFriends()->GetFriendByIndex(
        i, k_EFriendFlagImmediate));
  }

  m_friendsListContainer->SetSizer(m_friendsListSizer);
  ArrangeFriendPanels();
  m_friendsListContainer->Layout();
  m_friendsListSizer->Fit(m_friendsListContainer);
  frameSizer->Add(m_friendsListContainer, 1, wxEXPAND | wxALL, 0);

  SetSizer(frameSizer);
  Layout();

  Bind(BTN_PANEL_LEFT_CLICK, &FriendsListFrame::OnFriendPanelClick, this,
       rpwID_INVITE_FRIEND);
  Bind(BTN_PANEL_LEFT_CLICK, &FriendsListFrame::OnGuestPanelClick, this,
       rpwID_INVITE_GUEST);
  Bind(wxEVT_TIMER, &FriendsListFrame::OnTick, this);
  Bind(wxEVT_CLOSE_WINDOW, &FriendsListFrame::OnClose, this);
  Bind(wxEVT_ICONIZE, &FriendsListFrame::OnIconize, this);
  m_tick.Start(500);
  UpdateStatus();
}

void FriendsListFrame::OnClose(wxCloseEvent &event) {
  // Closing the main window quits the app. Take the (usually hidden) control
  // window and the tray icon with it; otherwise they keep the app running
  // invisibly. Only our own windows are destroyed here: the tray icon owns
  // and destroys its own hidden window.
  m_tick.Stop();
  std::vector<wxWindow *> controls;
  for (wxWindowList::compatibility_iterator node = wxTopLevelWindows.GetFirst();
       node; node = node->GetNext()) {
    if (dynamic_cast<ControlFrame *>(node->GetData())) {
      controls.push_back(node->GetData());
    }
  }
  for (size_t i = 0; i < controls.size(); ++i) {
    controls[i]->Destroy();
  }
  if (m_tray) {
    m_tray->RemoveIcon();
    m_tray->Destroy();
    m_tray = nullptr;
  }
  event.Skip();
}

void FriendsListFrame::OnIconize(wxIconizeEvent &event) {
  // With a tray icon, minimizing tucks the window away next to the clock.
  if (event.IsIconized() && m_tray && m_tray->IsIconInstalled()) {
    Hide();
    m_tray->ShowHiddenHintOnce();
  }
  event.Skip();
}

void FriendsListFrame::ShowFromTray() {
  // Show first, then restore: restoring a hidden window only updates wx's
  // idea of its state and leaves it minimized once it's shown.
  if (!IsShown()) {
    Show();
  }
  if (IsIconized()) {
    Iconize(false);
  }
  Raise();
#ifdef __WXMSW__
  ::SetForegroundWindow(GetHWND());
#endif
}

void FriendsListFrame::SetTray(RpwTrayIcon *tray) {
  m_tray = tray;
  if (m_tray) {
    m_tray->SetStatus(m_statusShown);
  }
}

#ifdef __WXMSW__
WXLRESULT FriendsListFrame::MSWWindowProc(WXUINT msg, WXWPARAM wParam,
                                          WXLPARAM lParam) {
  static const UINT showMessage =
      ::RegisterWindowMessageW(L"RemotePlayWhatever-Show");
  if (msg == showMessage) {
    ShowFromTray();
    return 0;
  }
  return wxFrame::MSWWindowProc(msg, wParam, lParam);
}
#endif

void FriendsListFrame::UpdateStatus() {
  wxString notice = wxString::FromUTF8(m_pRemoteInvite->TakeNotice().c_str());
  if (!notice.empty()) {
    m_notice = notice;
    m_noticeUntil = wxGetLocalTimeMillis() + 15000;
  }

  wxString text;
  wxColour colour = wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT);
  bool session = m_pRemoteInvite->IsSessionActive();

  if (!m_notice.empty() && wxGetLocalTimeMillis() < m_noticeUntil) {
    text = m_notice;
    colour = kNoticeOrange;
  } else if (!GClientContext()->SteamUser()->BLoggedOn()) {
    text = "Steam isn't signed in.";
  } else {
    m_notice.clear();
    CGameID game = GetRunningGameID();
    bool nonSteam = game.IsValid() &&
                    !(game.IsSteamApp() &&
                      game.AppID() != m_pRemoteInvite->GetNonSteamAppID());
    wxString name =
        game.IsValid()
            ? GetGameName(
                  game,
                  GClientContext()->SteamUser()->GetSteamID().GetAccountID())
            : wxString();
    if (name.empty()) {
      name = nonSteam ? "a non-Steam game" : "your game";
    }

    if (session) {
      unsigned guests = (unsigned)m_pRemoteInvite->Guests().size();
      text = "Sharing " + name + ": ";
      text += guests == 0   ? wxString("waiting for guests to join.")
              : guests == 1 ? wxString("1 guest connected.")
                            : wxString::Format("%u guests connected.", guests);
      if (m_pRemoteInvite->IsSharingDesktop()) {
        text += " Guests can see your whole screen.";
      }
      colour = kReadyGreen;
    } else if (!game.IsValid()) {
      text =
          "No game running. Start a game through Steam, then invite someone.";
    } else {
      text = "Ready to share: " + name;
      if (nonSteam) {
        text += " (non-Steam game, so your whole screen will be shared)";
      }
      colour = kReadyGreen;
    }
  }

  bool relayout = false;
  if (text != m_statusShown) {
    m_statusShown = text;
    m_status->SetLabel(text);
    m_status->SetForegroundColour(colour);
    m_status->Wrap(FromDIP(235));
    relayout = true;
    if (m_tray) {
      m_tray->SetStatus(text);
    }
  }
  if (m_endButton->IsShown() != session) {
    m_endButton->Show(session);
    relayout = true;
  }
  bool hasLink = session && !m_pRemoteInvite->GuestLink().empty();
  if (m_copyLinkButton->IsShown() != hasLink) {
    m_copyLinkButton->Show(hasLink);
    relayout = true;
  }
  if (m_copiedUntil != 0 && wxGetLocalTimeMillis() >= m_copiedUntil) {
    m_copyLinkButton->SetLabel("Copy guest link");
    m_copiedUntil = 0;
  }
  if (relayout) {
    m_status->GetParent()->Layout();
    Layout();
  }
}

void FriendsListFrame::OnCopyLink(wxCommandEvent &) { CopyGuestLink(); }

void FriendsListFrame::CopyGuestLink() {
  wxString link = wxString::FromUTF8(m_pRemoteInvite->GuestLink().c_str());
  if (!link.empty() && wxTheClipboard->Open()) {
    wxTheClipboard->SetData(new wxTextDataObject(link));
    wxTheClipboard->Flush();
    wxTheClipboard->Close();
    m_copyLinkButton->SetLabel("Copied!");
    m_copiedUntil = wxGetLocalTimeMillis() + 2000;
  }
}

void FriendsListFrame::OnEndSession(wxCommandEvent &) { ConfirmEndSession(); }

void FriendsListFrame::ConfirmEndSession() {
  int answer = wxMessageBox("Disconnect everyone and stop sharing?\n\nUnused "
                            "invites and guest links will stop working too.",
                            "Remote Play Whatever", wxYES_NO | wxICON_QUESTION,
                            IsShown() ? this : nullptr);
  if (answer == wxYES) {
    m_pRemoteInvite->EndSession();
    UpdateStatus();
  }
}

void FriendsListFrame::OnTick(wxTimerEvent &) {
  UpdateStatus();
  // Timers keep firing while a message box is open, so don't ask twice.
  if (m_pRemoteInvite->HasGroupFailure() && !m_askingGroupFallback) {
    m_askingGroupFallback = true;
    int answer =
        wxMessageBox("Steam didn't set up a Remote Play Together group for "
                     "this game, so your "
                     "guest might only be able to watch, not play.\n\n"
                     "Send the invite anyway?\n\n"
                     "If they can't play, quit the game, start it again "
                     "through Steam, and send a new invite.",
                     "Remote Play Whatever", wxYES_NO | wxICON_WARNING, this);
    m_pRemoteInvite->ResolveGroupFailure(answer == wxYES);
    m_askingGroupFallback = false;
  }
}

static wxString DescribeLaunchResult(ERemoteClientLaunchResult result) {
  switch (result) {
  case k_ERemoteClientLaunchResultTimeout:
  case k_ERemoteClientLaunchResultStreamTimeout:
    return "Steam timed out while starting the session.";
  case k_ERemoteClientLaunchResultOtherGameRunning:
    return "Another game is already being shared.";
  case k_ERemoteClientLaunchResultAccessDenied:
    return "Steam refused the request.";
  case k_ERemoteClientLaunchResultNetworkError:
    return "There was a network error.";
  case k_ERemoteClientLaunchResultScreenLocked:
    return "This PC's screen is locked.";
  case k_ERemoteClientLaunchResultUnsupported:
    return "Steam says this game can't be shared.";
  case k_ERemoteClientLaunchResultDisabledLocal:
    return "Remote Play is turned off in this PC's Steam settings.";
  case k_ERemoteClientLaunchResultDisabledRemote:
    return "Remote Play is turned off on your friend's side.";
  case k_ERemoteClientLaunchResultBroadcasting:
    return "Steam can't share the game while you're broadcasting.";
  case k_ERemoteClientLaunchResultBusy:
    return "Steam is busy with another stream.";
  case k_ERemoteClientLaunchResultDriversNotInstalled:
    return "Steam's streaming drivers aren't installed.";
  case k_ERemoteClientLaunchResultCanceled:
    return "The invite was canceled.";
  default:
    return "Steam couldn't start the session.";
  }
}

void FriendsListFrame::AddFreiendPanel(CSteamID friendID) {
  uint64 friendID64 = friendID.ConvertToUint64();
  auto it = m_friendPanels.find(friendID64);
  if (it != m_friendPanels.end()) {
    (*it).second->UpdatePersona();
    return;
  }
  m_friendPanels[friendID64] =
      new FriendPanel(m_friendsListContainer, rpwID_INVITE_FRIEND, friendID);
}

void FriendsListFrame::RemoveFriendPanel(CSteamID friendID) {
  uint64 friendID64 = friendID.ConvertToUint64();
  auto it = m_friendPanels.find(friendID64);
  if (it != m_friendPanels.end()) {
    FriendPanel *panel = (*it).second;
    m_onlineSizer->Detach(panel);
    m_offlineSizer->Detach(panel);
    panel->Destroy();
    m_friendPanels.erase(it);
  }
}

void FriendsListFrame::ArrangeFriendPanels() {
  std::vector<FriendPanel *> online, offline;
  for (auto it = m_friendPanels.begin(); it != m_friendPanels.end(); ++it) {
    (it->second->IsOnline() ? online : offline).push_back(it->second);
  }
  auto byName = [](FriendPanel *a, FriendPanel *b) {
    return a->GetDisplayPersonaName().CmpNoCase(b->GetDisplayPersonaName()) < 0;
  };
  std::sort(online.begin(), online.end(), byName);
  std::sort(offline.begin(), offline.end(), byName);

  // Detach only; the panels themselves stay.
  m_onlineSizer->Clear(false);
  m_offlineSizer->Clear(false);
  for (size_t i = 0; i < online.size(); ++i) {
    m_onlineSizer->Add(online[i], 0, wxBOTTOM | wxEXPAND, 1);
  }
  for (size_t i = 0; i < offline.size(); ++i) {
    m_offlineSizer->Add(offline[i], 0, wxBOTTOM | wxEXPAND, 1);
  }
  ApplyFilter();
}

void FriendsListFrame::OnPersonaStateChange(
    PersonaStateChange_t *personaStateCb) {
  if (personaStateCb->m_ulSteamID ==
      GClientContext()->SteamUser()->GetSteamID()) {
    return;
  }

  CSteamID steamID(personaStateCb->m_ulSteamID);
  bool known = m_friendPanels.count(steamID.ConvertToUint64()) != 0;
  if (GClientContext()->SteamFriends()->GetFriendRelationship(steamID) !=
      k_EFriendRelationshipFriend) {
    // Unfriended (or someone who isn't a friend, like a link guest).
    if (known) {
      RemoveFriendPanel(steamID);
      ApplyFilter();
    }
    return;
  }

  // Playing a different game, going away and so on don't change the list.
  const int listChanges =
      k_EPersonaChangeComeOnline | k_EPersonaChangeGoneOffline |
      k_EPersonaChangeName | k_EPersonaChangeNameFirstSet |
      k_EPersonaChangeAvatar | k_EPersonaChangeRelationshipChanged;
  if (known && !(personaStateCb->m_nChangeFlags & listChanges)) {
    return;
  }

  AddFreiendPanel(steamID);
  ArrangeFriendPanels();
}

void FriendsListFrame::ApplyFilter() {
  wxString needle =
      m_search ? m_search->GetValue().Lower().Trim().Trim(false) : wxString();
  int online = 0, offline = 0;
  for (auto it = m_friendPanels.begin(); it != m_friendPanels.end(); ++it) {
    FriendPanel *panel = it->second;
    bool match = needle.empty() ||
                 panel->GetDisplayPersonaName().Lower().Contains(needle);
    panel->Show(match);
    if (match) {
      ++(panel->IsOnline() ? online : offline);
    }
  }
  m_onlineLabel->SetLabel(wxString::Format("Online (%d)", online));
  m_onlineLabel->Show(online > 0);
  m_offlineLabel->SetLabel(wxString::Format("Offline (%d)", offline));
  m_offlineLabel->Show(offline > 0);
  m_friendsListContainer->FitInside();
  m_friendsListContainer->Layout();
}

void FriendsListFrame::OnRemotePlayInviteResult(
    RemotePlayInviteResult_t *inviteResultCb) {
  if (inviteResultCb->m_eResult != k_ERemoteClientLaunchResultOK) {
    wxMessageBox("Couldn't start Remote Play. " +
                     DescribeLaunchResult(inviteResultCb->m_eResult) +
                     wxString::Format("\n\n(Steam result %d)",
                                      (int)inviteResultCb->m_eResult),
                 "Remote Play Whatever", wxOK | wxICON_ERROR);
  } else {
    if (inviteResultCb->m_player.m_playerID.IsValid()) {
      const char *persona =
          GClientContext()->SteamFriends()->GetFriendPersonaName(
              inviteResultCb->m_player.m_playerID);
      wxMessageBox("Invite sent to " +
                       wxString::FromUTF8(persona && *persona ? persona
                                                              : "your friend") +
                       ". They'll get the link in Steam chat.",
                   "Remote Play Whatever", wxOK | wxICON_INFORMATION);
    } else {
      wxString link = wxString::FromUTF8(
          ClickableURL(inviteResultCb->m_szConnectURL).c_str());
      bool copied = false;
      // Close the clipboard before showing the message box; keeping it open
      // while the box is up blocks pasting the link anywhere else.
      if (wxTheClipboard->Open()) {
        copied = wxTheClipboard->SetData(new wxTextDataObject(link));
        wxTheClipboard->Flush();
        wxTheClipboard->Close();
      }
      wxMessageBox((copied ? "Guest invite link copied to the clipboard:\n\n"
                           : "Guest invite link:\n\n") +
                       link +
                       "\n\nSend it to whoever you want to play with. Anyone "
                       "with the link can join.",
                   "Remote Play Whatever", wxOK | wxICON_INFORMATION);
    }
  }
}

bool FriendsListFrame::CanSendInvite() {
  if (!GClientContext()->SteamUser()->BLoggedOn() ||
      !GetRunningGameID().IsValid()) {
    wxMessageBox(
        "No game is running. Start the game through Steam first, then invite.",
        "No game running", wxOK | wxICON_INFORMATION);
    return false;
  }
  return true;
}

bool FriendsListFrame::ShowNonSteamWarning() {
  AppId_t nonSteamAppID = m_pRemoteInvite->GetNonSteamAppID();
  CGameID gameRunning = GetRunningGameID();

  if (gameRunning.IsSteamApp() && gameRunning.AppID() != nonSteamAppID) {
    return true;
  }

  bool dlgResult = false;
  wxMessageDialog *dlg = new wxMessageDialog(
      this,
      "Starting a Remote Play Together session for a non-Steam game\n"
      "will expose your entire desktop to the remote player.\n"
      "The Steam client might need to be restarted to stop\n"
      "the desktop sharing session!\n\n"
      "Continue?",
      "Non-Steam Game confirmation", wxOK | wxCANCEL);
  if (dlg->ShowModal() == wxID_OK) {
    dlgResult = true;
  }
  delete dlg;
  return dlgResult;
}

void FriendsListFrame::OnFriendPanelClick(wxCommandEvent &event) {
  // Ask from the main window once this click has been handled: the clicked
  // panel can go away while a dialog is open (if the friendship ends), so
  // nothing below may touch it.
  if (m_inviteQueued) {
    return;
  }
  FriendPanel *sender = (FriendPanel *)event.GetEventObject();
  CSteamID invitee = sender->GetSteamID();
  wxString name = sender->GetDisplayPersonaName();
  bool online = sender->IsOnline();
  m_inviteQueued = true;
  CallAfter([this, invitee, name, online]() {
    ConfirmFriendInvite(invitee, name, online);
    m_inviteQueued = false;
  });
}

void FriendsListFrame::ConfirmFriendInvite(CSteamID invitee,
                                           const wxString &name, bool online) {
  if (!CanSendInvite()) {
    return;
  }

  if (!ShowNonSteamWarning()) {
    return;
  }

  wxString question = "Send remote play invite to " + name + "?";
  if (!online) {
    question = name + " appears offline (they may be invisible). "
                      "The invite link goes to them in Steam chat, and it "
                      "should work once they're on Steam, "
                      "as long as you're still sharing.\n\nSend the invite?";
  }
  wxMessageDialog *dlg = new wxMessageDialog(
      this, question, "Confirm Remote Play Invite", wxOK | wxCANCEL);
  if (dlg->ShowModal() == wxID_OK) {
    m_notice.clear(); // an old message would be about something else
    m_pRemoteInvite->SendInvite(invitee);
  }
  delete dlg;
}

void FriendsListFrame::OnGuestPanelClick(wxCommandEvent &event) {
  if (!CanSendInvite()) {
    return;
  }

  if (!ShowNonSteamWarning()) {
    return;
  }

  wxMessageDialog *dlg =
      new wxMessageDialog(this, "Create guest invite link?",
                          "Confirm Remote Play Invite", wxOK | wxCANCEL);
  if (dlg->ShowModal() == wxID_OK) {
    m_notice.clear(); // an old message would be about something else
    m_pRemoteInvite->SendInvite(
        CSteamID(0, k_EUniversePublic, k_EAccountTypeIndividual));
  }
  delete dlg;
}
