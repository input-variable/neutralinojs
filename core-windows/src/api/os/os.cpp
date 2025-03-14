#include <iostream>
#include <fstream>
#include "lib/json.hpp"
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <stdlib.h>
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>
#include "helpers.h"
#include "../../platform/windows.h"

#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "Shell32.lib")

using namespace std;
using json = nlohmann::json;

namespace os {
    string execCommand(json input) {
        json output;
        string command = "cmd /c " + input["command"].get<std::string>();
        output["output"] = windows::execCommand(command);
        output["success"] = true;
        return output.dump();
    }

    string getEnvar(json input) {
        json output;
        string varKey = input["key"];
        char *varValue;
        varValue = getenv(varKey.c_str());
        if(varValue == NULL) {
            output["error"] =  varKey + " is not defined";
        }
        else {
            output["value"] = varValue;
            output["success"] = true;
        }
        return output.dump();
    }


    string dialogOpen(json input) {
        json output;
        string title = input["title"];
        if(!input["isDirectoryMode"].is_null() && input["isDirectoryMode"].get<bool>()) {
            TCHAR szDir[MAX_PATH];
            BROWSEINFO bInfo;
            ZeroMemory(&bInfo, sizeof(bInfo));
            bInfo.hwndOwner = NULL;
            bInfo.pidlRoot = NULL;
            bInfo.pszDisplayName = szDir;
            bInfo.lpszTitle = const_cast<char *>(title.c_str());
            bInfo.ulFlags = 0 ;
            bInfo.lpfn = NULL;
            bInfo.lParam = 0;
            bInfo.iImage = -1;

            LPITEMIDLIST lpItem = SHBrowseForFolder( &bInfo);
            if( lpItem != NULL ) {
                SHGetPathFromIDList(lpItem, szDir );
                output["selectedEntry"] = szDir;
            }
            else {
                output["selectedEntry"] = nullptr;
            }
        }
        else {
            OPENFILENAME ofn;
            TCHAR szFile[MAX_PATH] = { 0 };
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lpstrTitle = const_cast<char *>(title.c_str());
            ofn.lStructSize = sizeof(ofn);
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile);
            ofn.nFilterIndex = 1;
            ofn.lpstrFileTitle = NULL;
            ofn.nMaxFileTitle = 0;
            ofn.lpstrInitialDir = NULL;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

            if (GetOpenFileName(&ofn)) {
                output["selectedEntry"] = ofn.lpstrFile;
            }
            else {
                output["selectedEntry"] = nullptr;
            }
        }
        output["success"] = true;
        return output.dump();
    }


    string dialogSave(json input) {
        json output;
        string title = input["title"];
        OPENFILENAME ofn;
        TCHAR szFile[260] = { 0 };

        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lpstrTitle = const_cast<char *>(title.c_str());
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = NULL;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = NULL;
        ofn.Flags = OFN_PATHMUSTEXIST;

        if (GetSaveFileName(&ofn)) {
            output["selectedEntry"] = ofn.lpstrFile;
            output["success"] = true;
        }
        else {
            output["selectedEntry"] = NULL;
        }
        return output.dump();
    }

    string showNotification(json input) {
        json output;
        string command = "powershell -Command \"& {Add-Type -AssemblyName System.Windows.Forms;"
                        "Add-Type -AssemblyName System.Drawing;"
                        "$notify = New-Object System.Windows.Forms.NotifyIcon;"
                        "$notify.Icon = [System.Drawing.SystemIcons]::Information;"
                        "$notify.Visible = $true;"
                        "$notify.ShowBalloonTip(0 ,'"+ input["summary"].get<string>() + "','" + input["body"].get<string>() + "',[System.Windows.Forms.TooltipIcon]::None)}\"";

        string commandOutput = windows::execCommand(command);

        if(commandOutput.find("'powershell'") == string::npos) {
            output["success"] = true;
            output["message"] = "Notification was sent to the system";
        }
        else
            output["error"] = "An error thrown while sending the notification";
        return output.dump();
    }

    string showMessageBox(json input) {
        json output;
        map <string, string> messageTypes = {
            {"INFO", "[System.Windows.Forms.MessageBoxButtons]::OK, [System.Windows.Forms.MessageBoxIcon]::Information"},
            {"WARN", "[System.Windows.Forms.MessageBoxButtons]::OK, [System.Windows.Forms.MessageBoxIcon]::Warning"},
            {"ERROR", "[System.Windows.Forms.MessageBoxButtons]::OK, [System.Windows.Forms.MessageBoxIcon]::Error"},
            {"QUESTION", "[System.Windows.Forms.MessageBoxButtons]::YesNo, [System.Windows.Forms.MessageBoxIcon]::Question"}
        };
        string messageType;
        messageType = input["type"].get<string>();
        if(messageTypes.find(messageType) == messageTypes.end()) {
            output["error"] = "Invalid message type: '" + messageType + "' provided";
            return output.dump();
        }
        string command = "powershell -Command \"& {Add-Type -AssemblyName System.Windows.Forms;"
                        "[System.Windows.Forms.MessageBox]::Show('" + input["content"].get<string>() +
                        "', '" + input["title"].get<string>() + "', " +
                        messageTypes[messageType] + ");}\"";

        string commandOutput = windows::execCommand(command);
        if(commandOutput.find("'powershell'") == string::npos) {
            output["success"] = true;
            if(messageType == "QUESTION")
                output["yesButtonClicked"] = commandOutput.find("Yes") != std::string::npos;
        }
        else
            output["error"] = "An error thrown while sending the notification";
        return output.dump();
    }
}
