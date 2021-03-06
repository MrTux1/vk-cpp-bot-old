#include "common.h"
#include <ctime>
#include <iostream>
#include <mutex>

extern json botname;
extern bool friendsadd;
extern bool forwardmessages;

#define config "config.json"

#define vk_version "5.69"

string vk_token = "";

/* Add stats and check token */
void vk::init()
{
    if (vk_token != "")
        return;
    json data_temp;
    if (fs::exists(config)) {
        data_temp = json::parse(fs::readData(config));
        vk_token = data_temp["token"];
    }
    auto data = vk::send("stats.trackVisitor");
    if (!data["error"].is_null()) {
        cout << "invalid vk_token" << endl;
        data_temp["token"] = "put vk tocken here (vkhost.github.io)";
    }
    if (data_temp["names"].is_null())
        data_temp["names"] = { "кот", "!", "пуся", "бот" };
    botname = data_temp["names"];

    if (data_temp["friendsadd"].is_null())
        data_temp["friendsadd"] = true;
    friendsadd = data_temp["friendsadd"];

    if (data_temp["forwardmessages"].is_null())
        data_temp["forwardmessages"] = true;
    forwardmessages = data_temp["forwardmessages"];

    fs::writeData(config, data_temp.dump(4));
}

mutex l;

/* Send vk request and get response */
nlohmann::json vk::send(string method, table params, bool sendtoken)
{
    l.lock();
    if (sendtoken) {
        params["access_token"] = vk_token;
    }
    if (params.find("v") == params.cend())
        params["v"] = vk_version;
    nlohmann::json request = nlohmann::json::parse(net::send("https://api.vk.com/method/" + method, params));
    unsigned int sleept = 0;
    while (!request["error"].is_null() && (request["error"]["error_code"] == 14 || request["error"]["error_code"] == 10)) {
        if (method == "docs.save") {
            cout << other::getRealTime() + ": VK ERROR: docs not uploaded" << endl;
            request = {};
            break;
        }
        sleept += 60000;
        cout << other::getRealTime() + ": VK ERROR: capcha! wait " << sleept << "ms" << endl;
        other::sleep(sleept);
        request = nlohmann::json::parse(net::send("https://api.vk.com/method/" + method, params));
    }
    while (!request["error"].is_null() && request["error"]["error_code"] == 6) {
        other::sleep(500);
        request = nlohmann::json::parse(net::send("https://api.vk.com/method/" + method, params));
    }
    if (!request["error"].is_null())
        cout << other::getRealTime() + ": VK ERROR:" << endl
             << request /*.dump(4)*/ << endl;
    l.unlock();
    return request;
}

#include <gd.h>
string vk::upload(string path, string peer_id, string type)
{
    string out = "";
    if (!other::getFileSize(path.c_str()))
        return "";
    if (type == "photo") {
        gdImagePtr im = gdImageCreateFromFile(path.c_str());
        if (im) {
            bool f = im->sx > 2000 || im->sy > 2000;
            gdImageDestroy(im);
            if (f) {
                out = vk::upload(path, peer_id, "doc") + ",";
                if (out == ",")
                    out = "";
            }
        }
    }
    json res;
    table params = {
        { "peer_id", peer_id },
        { "type", type }
    };
    if (type != "audio_message")
        res = vk::send(type + "s.getMessagesUploadServer", params)["response"];
    else
        res = vk::send("docs.getMessagesUploadServer", params)["response"];
    string tmp = net::upload(res["upload_url"], path);
    if (tmp == "" || str::at(tmp, "504 Gateway Time-out")) {
        return out;
    }
    res = json::parse(tmp);
    params = {};
    if (type == "photo") {
        params["server"] = to_string(res["server"].get<int>());
        params["photo"] = res["photo"];
        params["hash"] = res["hash"];
        res = vk::send("photos.saveMessagesPhoto", params);
    } else if (type == "doc" || type == "audio_message") {
        params["file"] = res["file"];
        res = vk::send("docs.save", params) /*["response"]*/;
    }
    if (res["response"].is_null()) {
        return out;
    }
    if (type == "audio_message")
        type = "doc";
    return out + type + to_string((int)res["response"][0]["owner_id"]) + "_" + to_string((int)res["response"][0]["id"]);
}

string vk::getAttach(int msgid)
{
    string out;
    json res = vk::send("messages.getById", { { "message_ids", to_string(msgid) }, { "extended", "1" } });
    if (res["response"]["items"][0].is_null())
        return out;
    for (auto doc : res["response"]["items"][0]["attachments"]) {
        string type = doc["type"];
        string t = to_string((int)doc[type]["owner_id"]) + "_" + to_string((int)doc[type]["id"]);
        if (!doc[type]["access_key"].is_null())
            t += "_" + (string)doc[type]["access_key"];
        if (type == "graffity" || type == "audio_message")
            type = "doc";
        out += type + t + ",";
    }
    return out;
}

void vk::friends()
{
    while (true) {
        for (int i = 0; i < 10; i++) {
            vk::send("account.setOnline");
            other::sleep(60000);
        }
        if (!friendsadd)
            continue;
        json list = vk::send("friends.getRequests", { { "need_viewed", "1" } })["response"]["items"];
        for (unsigned int i = 0; i < list.size(); i++) {
            vk::send("friends.add", { { "user_id", to_string((int)list[i]) } });
        }
    }
}
