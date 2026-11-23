#include "FriendPanel.h"
#include "wxSteamStuff.h"

FriendPanel::FriendPanel(wxWindow *parent, wxWindowID winID, CSteamID friendID)
    : ClickablePanel(parent, winID), m_personaNameLabel(nullptr),
      m_personaStateLabel(nullptr), m_steamID(friendID), m_online(true),
      m_requestedInfo(false) {
  wxBoxSizer *rootSizer = new wxBoxSizer(wxHORIZONTAL);

  SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));

  m_personaImg = new wxStaticBitmap(this, wxID_ANY, wxNullBitmap,
                                    wxDefaultPosition, wxSize(32, 32), 0);
  rootSizer->Add(m_personaImg, 0, wxALL, 10);

  wxBoxSizer *textSizer = new wxBoxSizer(wxVERTICAL);

  textSizer->Add(0, 0, 1, wxEXPAND, 5);
  m_personaNameLabel = new wxStaticText(this, wxID_ANY, wxEmptyString,
                                        wxDefaultPosition, wxDefaultSize, 0);
  textSizer->Add(m_personaNameLabel, 0, wxALL | wxEXPAND, 5);

  textSizer->Add(0, 0, 1, wxEXPAND, 5);
  rootSizer->Add(textSizer, 1, wxALL | wxEXPAND, 5);

  SetSizer(rootSizer);
  UpdatePersona();
  Layout();

  RBindBaseEvents();
}

FriendPanel::~FriendPanel() {}

void FriendPanel::UpdatePersona() {
  if (!m_steamID.IsValid()) {
    return;
  }

  ISteamFriends015 *friends = GClientContext()->SteamFriends();
  EPersonaState state = friends->GetFriendPersonaState(m_steamID);
  m_online =
      state != k_EPersonaStateOffline && state != k_EPersonaStateInvisible;

  wxString name(friends->GetFriendPersonaName(m_steamID), wxConvUTF8);
  // Until Steam has a friend's profile it reports no name, "[unknown]" or just
  // their SteamID. Ask for the profile once; it arrives in a persona update.
  wxString idText =
      wxString::Format("%llu", (unsigned long long)m_steamID.ConvertToUint64());
  if ((name.empty() || name == "[unknown]" || name == idText) &&
      !m_requestedInfo) {
    m_requestedInfo = true;
    friends->RequestUserInformation(m_steamID, false);
  }
  if (!name.empty()) {
    m_personaNameLabel->SetLabel(name);
  }

  int av = friends->GetSmallFriendAvatar(m_steamID);
  if (av > 0) {
    wxBitmap avatar = SteamImageToBitmap(av);
    if (avatar.IsOk()) {
      m_avatar = avatar;
    }
  }
  if (m_avatar.IsOk()) {
    m_personaImg->SetBitmap(m_online ? m_avatar : m_avatar.ConvertToDisabled());
  }

  m_personaNameLabel->SetForegroundColour(NormalForeground());
  m_personaNameLabel->Refresh();
}

wxColour FriendPanel::NormalForeground() const {
  return wxSystemSettings::GetColour(m_online ? wxSYS_COLOUR_WINDOWTEXT
                                              : wxSYS_COLOUR_GRAYTEXT);
}

CSteamID FriendPanel::GetSteamID() { return m_steamID; }

wxString FriendPanel::GetDisplayPersonaName() {
  return m_personaNameLabel->GetLabel();
}

wxString FriendPanel::GetDisplayPersonaState() {
  return m_personaStateLabel->GetLabel();
}

void FriendPanel::SetDisplayPersonaName(const wxString &name) {
  m_personaNameLabel->SetLabel(name);
}

void FriendPanel::SetDisaplayPersonaState(const wxString &state) {
  m_personaStateLabel->SetLabel(state);
}
