/* Copyright (C) 2019 Mr Goldberg
   This file is part of the Goldberg Emulator

   The Goldberg Emulator is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   The Goldberg Emulator is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the Goldberg Emulator; if not, see
   <http://www.gnu.org/licenses/>.  */

/*
*/

#include "sdk_includes/steam_api.h"
#include "dll/common_includes.h"

#include <iostream>
#include <chrono>
#include <thread>
#include <string>
#include <vector>

/*uint32_t uWaitForAppID = 673950;
std::string sLaunchExecutable = "C:\\test\\Farm Together\\FarmTogether.exe";
std::string sLaunchParams = "-gpu 1";*/
uint32_t uWaitForAppID = 0;
std::string sLaunchExecutable;
std::string sLaunchParams;
bool bRunOnce = true;

#ifdef _WIN32
#include <windows.h>
#else

#endif
int main(int args, const char *argv[]) {
    if (SteamAPI_Init()) {
        //Set appid to: LOBBY_CONNECT_APPID
        SteamAPI_RestartAppIfNecessary(LOBBY_CONNECT_APPID);
        std::cout << "This is a program to find lobbies and run the game with lobby connect parameters" << std::endl;
        std::cout << "Api initialized, ";

        for (const char **p=argv+1, **end=argv+args;p!=end;++p) {
            if (**p=='-') {
                std::string cmd(*p+1);
                std::transform(cmd.begin(), cmd.end(), cmd.begin(), [](unsigned char c){ return std::tolower(c); });

                if (++p==end) {
                    break;
                }

                if (cmd=="mode") {
                    std::string param(*p+1);
                    std::transform(param.begin(), param.end(), param.begin(), [](unsigned char c){ return std::tolower(c); });

                    if (param=="once") {
                        bRunOnce = true;
                    }
                    else if (param=="loop") {
                        bRunOnce = false;
                    }
                }
                else if (cmd=="appid") {
                    uWaitForAppID = std::strtoul(*p, nullptr, 10);
                }
                else if (cmd=="run") {
                    sLaunchExecutable = *p;
                    while (++p != end) {
                        bool needs_quotes = false;
                        std::string param;

                        for (const char *p_param=*p;*p_param != 0;++p_param) {
                            switch (*p_param) {
                                case ' ' :
                                    needs_quotes = true;
                                    break;
                                case '"' :
                                    param+='\\';
                                    break;
                                default :
                                    break;
                            }
                            param+=*p_param;
                        }

                        if (!sLaunchParams.empty()) {
                            sLaunchParams += ' ';
                        }
                        if (needs_quotes) {
                            sLaunchParams += '"';
                        }
                        sLaunchParams += param;
                        if (needs_quotes) {
                            sLaunchParams += '"';
                        }
                    }
                    break;
                }
            }
        }

        if (uWaitForAppID && !sLaunchExecutable.empty()) {
            std::string connect_exist;
            bool log_initial = true;
            bool log_known = true;

            for (;;) {
                if (log_initial) {
                    log_initial = false;
                    std::cout << "Wait for new instance with AppID " << uWaitForAppID;
                }

                std::string connect_new;

                SteamAPI_RunCallbacks();

                int friend_count = SteamFriends()->GetFriendCount(k_EFriendFlagAll);

                for (int i = 0; connect_new.empty() && i < friend_count; ++i) {
                    CSteamID id = SteamFriends()->GetFriendByIndex(i, k_EFriendFlagAll);

                    FriendGameInfo_t friend_info = {};
                    SteamFriends()->GetFriendGamePlayed(id, &friend_info);

                    if (uWaitForAppID == friend_info.m_gameID.AppID()) {
                        const char *name = SteamFriends()->GetFriendPersonaName(id);
                        const char *connect = SteamFriends()->GetFriendRichPresence( id, "connect");

                        if (strlen(connect) > 0) {
                            connect_new = connect;
                        } else if (friend_info.m_steamIDLobby != k_steamIDNil) {
                            connect_new = "+connect_lobby " + std::to_string(friend_info.m_steamIDLobby.ConvertToUint64());
                        } else {
                            continue;
                        }

                        if (connect_new != connect_exist) {
                            log_known = true;
                            std::cout << std::endl << "NEW   instance by " << name << " found, trying to join." << std::endl;
                        }
                        else {
                            if (log_known) {
                                log_known = false;
                                std::cout << std::endl << "KNOWN instance by " << name << " found, ignoring." << std::endl;
                            }
                            connect_new.clear();
                        }
                    }
                }

                if (connect_new.empty())
                {
                    std::cout << '.';
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    continue;
                }

                log_initial = true;
                connect_exist = connect_new;

                std::string commandline = "\"" + sLaunchExecutable + "\" " + connect_new;
                if (!sLaunchParams.empty()) {
                    commandline += ' ';
                    commandline += sLaunchParams;
                }
                std::cout << "Executing: \"" + commandline + "\"...";

                STARTUPINFOA lpStartupInfo;
                PROCESS_INFORMATION lpProcessInfo;

                ZeroMemory( &lpStartupInfo, sizeof( lpStartupInfo ) );
                lpStartupInfo.cb = sizeof( lpStartupInfo );
                ZeroMemory( &lpProcessInfo, sizeof( lpProcessInfo ) );

                if (!CreateProcessA( NULL,
                            const_cast<char *>(commandline.c_str()), NULL, NULL,
                            NULL, NULL, NULL, NULL,
                            &lpStartupInfo,
                            &lpProcessInfo
                            )) {
                    std::cout << std::endl <<"ERROR: CreateProcess returned " << GetLastError() << std::endl;
                    exit(1);
                }

                std::cout << "pid " << lpProcessInfo.dwProcessId << std::endl;
                std::cout << "Waiting for process to exit." << std::endl;

                CloseHandle(lpProcessInfo.hThread);

                WaitForSingleObject(lpProcessInfo.hProcess, INFINITE);

                DWORD dwExitCode = -1;
                GetExitCodeProcess(lpProcessInfo.hProcess, &dwExitCode);
                CloseHandle(lpProcessInfo.hProcess);

                std::cout << "Process exited with code " << dwExitCode << "." << std::endl << "---" << std::endl;
            }

            /* Will lopp until process termination. */
            if (bRunOnce) {
                return 0;
            }
        }
top:
        std::cout << "waiting a few seconds for connections:" << std::endl;
        for (int i = 0; i < 10; ++i) {
            SteamAPI_RunCallbacks();
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        int friend_count = SteamFriends()->GetFriendCount(k_EFriendFlagAll);
        std::cout << "People on the network: " << friend_count << std::endl;
        for (int i = 0; i < friend_count; ++i) {
            CSteamID id = SteamFriends()->GetFriendByIndex(i, k_EFriendFlagAll);
            const char *name = SteamFriends()->GetFriendPersonaName(id);

            FriendGameInfo_t friend_info = {};
            SteamFriends()->GetFriendGamePlayed(id, &friend_info);
            std::cout << name << " is playing: " << friend_info.m_gameID.AppID() << std::endl;
        }

        std::cout << std::endl << "--------------Menu-------------" << std::endl << "\tappid\tname\tcommand line" << std::endl;

        std::vector<std::string> arguments;
        for (int i = 0; i < friend_count; ++i) {
            CSteamID id = SteamFriends()->GetFriendByIndex(i, k_EFriendFlagAll);
            const char *name = SteamFriends()->GetFriendPersonaName(id);
            const char *connect = SteamFriends()->GetFriendRichPresence( id, "connect");
            FriendGameInfo_t friend_info = {};
            SteamFriends()->GetFriendGamePlayed(id, &friend_info);

            if (strlen(connect) > 0) {
                std::cout << arguments.size() << "\t" << friend_info.m_gameID.AppID() << "\t" << name << "\t" << connect << std::endl;
                arguments.push_back(connect);
            } else {
                if (friend_info.m_steamIDLobby != k_steamIDNil) {
                    std::string connect = "+connect_lobby " + std::to_string(friend_info.m_steamIDLobby.ConvertToUint64());
                    std::cout << arguments.size() << "\t" << friend_info.m_gameID.AppID() << "\t" << name << "\t" << connect << std::endl;
                    arguments.push_back(connect);
                }
            }
        }

        std::cout << arguments.size() << ": Retry." << std::endl;
        std::cout << std::endl << "Enter the number corresponding to your choice then press Enter." << std::endl;
        unsigned int choice;
        std::cin >> choice;

        if (choice >= arguments.size()) goto top;

#ifdef _WIN32
        std::cout << "starting the game with: " << arguments[choice] << std::endl << "Please select the game exe" << std::endl;

        OPENFILENAMEA ofn;
        char szFileName[MAX_PATH] = "";
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = 0;
        ofn.lpstrFilter = "Exe Files (*.exe)\0*.exe\0All Files (*.*)\0*.*\0";
        ofn.lpstrFile = szFileName;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
        ofn.lpstrDefExt = "txt";
        if(GetOpenFileNameA(&ofn))
        {
            std::string filename = szFileName;
            filename = "\"" + filename + "\" " + arguments[choice];
            std::cout << filename << std::endl;
            STARTUPINFOA lpStartupInfo;
            PROCESS_INFORMATION lpProcessInfo;

            ZeroMemory( &lpStartupInfo, sizeof( lpStartupInfo ) );
            lpStartupInfo.cb = sizeof( lpStartupInfo );
            ZeroMemory( &lpProcessInfo, sizeof( lpProcessInfo ) );

            CreateProcessA( NULL,
                        const_cast<char *>(filename.c_str()), NULL, NULL,
                        NULL, NULL, NULL, NULL,
                        &lpStartupInfo,
                        &lpProcessInfo
                        );
        }
#else
        std::cout << "Please launch the game with these arguments: " << arguments[choice] << std::endl;
#endif
    }
}
