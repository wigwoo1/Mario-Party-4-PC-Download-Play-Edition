#pragma once
#ifdef __cplusplus
extern "C" {
#endif
/* Browser bootstrap must set this before starting an online session.
 * Online samples are rejected until an actual synchronized adapter is wired. */
void PartyBoardBrowserRequireOnlineInput(int required);
int PartyBoardBrowserOnlineInputRequired(void);
#ifdef __cplusplus
}
#endif
