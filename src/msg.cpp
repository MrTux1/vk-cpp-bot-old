#include "common.h"
#include <iostream>
#include <mutex>

bool forwardmessages;
json botname;

mutex msgLock;
map<int, message> msgs;
unsigned long long int msgCount = 0;
unsigned long long int msgCountComplete = 0;

void msg::init()
{
}

void msg::in(json js)
{
    message inMsg;
    msg::decode(js, &inMsg);
    msgCount++;
    if (inMsg.msg == "" || (inMsg.flags & 0x02) || inMsg.user_id < 0)
        return;
    if (!msg::toMe(&inMsg))
        return;
    if (!inMsg.words.size())
        inMsg.words.push_back("help");
    thread(msg::treatment, inMsg).detach();
    //msg::treatment(inMsg);
}

void msg::change(json js)
{
    message inMsg;
    msg::decode(js, &inMsg);
    msgLock.lock();
    if (inMsg.flags & 131072 && inMsg.chat_id && msgs.find(inMsg.msg_id) != msgs.cend() && !(msgs[inMsg.msg_id].flags & 0x02)) {
        table outMsg;
        outMsg["message"] = str::replase(str::replase(str::replase(str::replase(msgs[inMsg.msg_id].msg, ". ", "@#$%&"), "&#", "-"), ".", "-"), "@#$%&", ". ");
        outMsg["peer_id"] = to_string(msgs[inMsg.msg_id].chat_id + 2000000000);
        outMsg["attachment"] = msgs[inMsg.msg_id].attach;
        msg::send(outMsg);
    }
    msgLock.unlock();
}

void msg::treatment(message inMsg)
{
    cout << other::getRealTime() + ": start(" + to_string(inMsg.msg_id) + ", " + to_string(inMsg.user_id) + "/" + to_string(inMsg.chat_id) + "): " + inMsg.words[0] << endl;
    table outMsg = {};
    string id;
    if (inMsg.chat_id)
        id = to_string(inMsg.chat_id + 2000000000);
    else
        id = to_string(inMsg.user_id);
    //msg::setTyping(id);
    long long int oldbalance;
    module::TR(&inMsg, &outMsg, &oldbalance);
    msg::func(&inMsg, &outMsg);
    module::postTR(&inMsg, &outMsg, &oldbalance);
    msgLock.lock();
    msgCountComplete++;
    msgLock.unlock();
    msg::send(outMsg);
    cout << other::getRealTime() + ": done(" + to_string(inMsg.msg_id) + "): " + inMsg.words[0] << endl;
}

void msg::decode(json js, message* inMsg)
{
    inMsg->msg_id = js[1];
    inMsg->flags = (int)js[2];
    if (!js[5].is_null())
        inMsg->msg = js[5];
    if (js[3] > 2000000000) {
        inMsg->chat_id = (int)js[3] - 2000000000;
        if (!js[6].is_null())
            inMsg->user_id = str::fromString(js[6]["from"]);
    } else {
        inMsg->chat_id = 0;
        inMsg->user_id = js[3];
    }
    inMsg->js = js;
    if (!module::user::get(inMsg))
        inMsg->msg = "";
    if (inMsg->msg == "")
        return;
    inMsg->msg = str::replase(inMsg->msg, "<br>", " \n");
    inMsg->words = str::words(inMsg->msg, ' ');
    inMsg->attach = vk::getAttach(inMsg->msg_id);
    msgLock.lock();
    msgs[inMsg->msg_id] = *inMsg;
    msgLock.unlock();
}

void msg::func(message* inMsg, table* outMsg)
{
    //outMsg["message"]=inMsg.js.dump(4);
    if (inMsg->chat_id)
        (*outMsg)["peer_id"] = to_string(inMsg->chat_id + 2000000000);
    else
        (*outMsg)["peer_id"] = to_string(inMsg->user_id);
    cmd::start(inMsg, outMsg, inMsg->words[0]);
    if (forwardmessages)
        if (inMsg->chat_id)
            (*outMsg)["forward_messages"] += to_string(inMsg->msg_id);
}

void msg::send(table outMsg)
{
    if (outMsg["peer_id"] == "")
        return;
    vk::send("messages.send", outMsg);
}

bool msg::toMe(message* inMsg)
{
    for (auto n : botname)
        if (str::low(n) == str::low(inMsg->words[0])) {
            inMsg->words.erase(inMsg->words.begin());
            return true;
        }
    if (cmd::easyGet(to_string(inMsg->chat_id) + "_" + to_string(inMsg->user_id)) != "")
        return true;
    if (!inMsg->chat_id)
        return true;
    return false;
}

void msg::setTyping(string id)
{
    vk::send("messages.setActivity", { { "peer_id", id }, { "type", "typing" } });
}

unsigned long long int msg::Count()
{
    msgLock.lock();
    unsigned long long int temp = msgCount;
    msgLock.unlock();
    return temp;
}
unsigned long long int msg::CountComplete()
{
    msgLock.lock();
    unsigned long long int temp = msgCountComplete;
    msgLock.unlock();
    return temp;
}
