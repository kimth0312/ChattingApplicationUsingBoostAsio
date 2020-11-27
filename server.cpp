#include <iostream>
#include <cstdlib>
#include <deque>
#include <memory>
#include <list>
#include <set>
#include <utility>
#include <boost/asio.hpp>
#include <string.h>
#include "message.hpp"

using boost::asio::ip::tcp;
using namespace std;

typedef deque<message> messageQueue;
// deque is an irregular acronym of double-ended queue. Double-ended queues are sequence containers with dynamic sizes that can be expanded or contracted on both ends.

class participant {
public:
    virtual ~participant() {}
    virtual void deliver(const message& messageItem) = 0;
};

typedef shared_ptr<participant> participantPointer;
// participants == set<shared_ptr<participant>>

class room {
public:
    void join(participantPointer participant) {
        participants.insert(participant);
        
        /*  for (auto messageItem : messageRecents)     // messageItem in deque<message>
            participant->deliver(messageItem);      // delivers recents messages to participants.
        */
        // I don't want it to deliver recent messages to new users.
    }
    
    // It delivers the entire messages to all participants even me myself.
    // * It even delivers the message I wrote to myself. I want it to be fixed. - solved.
    void deliver(const message& messageItem, participantPointer participant) {
        messageRecents.push_back(messageItem);
        while (messageRecents.size() > max)
            messageRecents.pop_front();
        for (auto user : participants) {
            if (user == participant)            // not echoing to myself.
                continue;
            user->deliver(messageItem);
        }
    }
    
    void leave(participantPointer participant) {
        message messageItem;
        messageItem.bodyLength(strlen(leaveMessage));
        char *total = (char *)malloc(1 + strlen(messageItem.getNickname()) + strlen(leaveMessage));
        strcat(total, messageItem.getNickname());q
        strcat(total, leaveMessage);
        memcpy(messageItem.body(), total, messageItem.bodyLength());
        messageItem.encodeHeader();
        for (auto participant : participants) {
            participant->deliver(messageItem);
        }
        participants.erase(participant);
    }
    
private:
    messageQueue messageRecents;
    enum { max = 200 };
    set<participantPointer> participants;
    // In a set, the value of an element also identifies it, and each value must be unique. The value of the elements in a set cannot be modified once in the container, but they can be inserted or removed from the container.
    char *leaveMessage = " has left the chatroom.";
};

class session : public participant, public enable_shared_from_this<session> {
public:
    session(tcp::socket socket, room& room) : socket(move(socket)), room_(room) {}      // session constructor
    
    void start() {
        room_.join(shared_from_this());     // shared_from_this() allows us to be able to use participant object in room class.
        readHeader();
    }
    
    // If there is a message, then push_back. If not, then write().
    void deliver(const message& messageItem) {
        bool write_in_progress = !Messages.empty();
        Messages.push_back(messageItem);        // Messages = messageQueue
        if (!write_in_progress)
            write();
    }
    
private:
    void readHeader() {
        auto self(shared_from_this());
        boost::asio::async_read(socket, boost::asio::buffer(messageItem.data(), message::headerLength), [this, self](boost::system::error_code ec, size_t) {
            if (!ec && messageItem.decodeHeader()) {
                readBody();
            }
            else {
                room_.leave(shared_from_this());
            }
        });
    }
    
    void readBody() {
        auto self(shared_from_this());
        boost::asio::async_read(socket, boost::asio::buffer(messageItem.body(), messageItem.bodyLength()), [this, self](boost::system::error_code ec, size_t) {
            if (!ec) {
                room_.deliver(messageItem, shared_from_this());
                readHeader();
            }
            else {
                room_.leave(shared_from_this());
            }
        });
    }
    
    void write() {
        auto self(shared_from_this());
        boost::asio::async_write(socket, boost::asio::buffer(Messages.front().data(), Messages.front().length()), [this, self](boost::system::error_code ec, size_t) {
            if (!ec) {
                Messages.pop_front();
                if (!Messages.empty()) {
                    write();
                }
            }
            else {
                room_.leave(shared_from_this());
            }
        });
    }
    
    tcp::socket socket;
    room& room_;
    message messageItem;
    messageQueue Messages;
};

class server {
public:
    server(boost::asio::io_context& io_context, const tcp::endpoint& endpoint) : acceptor(io_context, endpoint) {       // acceptor initialization
        do_accept();
    }
private:
    void do_accept() {
        acceptor.async_accept([this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec) {
                make_shared<session>(move(socket), room_)->start();     // When all the references die, the shared pointer will be destroyed.
                // when you use move() function, the variable will move and lose its value.
            }
            do_accept();        // unlimited loop
        });
    }
    tcp::acceptor acceptor;     // The server uses the I/O object 'acceptor' to accept an incoming connection from another program.
    room room_;
};

int main(int argc, const char * argv[]) {
    try {
        if (argc < 2) {
            cerr << "Usage : server <port> [<port> ...]\n";
            return 1;
        }
        boost::asio::io_context io_context;
        list<server> servers;		// list == doublely linked list.
        for (int i = 1; i < argc; ++i) {
            tcp::endpoint endpoint(tcp::v4(), atoi(argv[i]));
            servers.emplace_back(io_context, endpoint);		// emplace_back == construct and insert element at the end. Since it's a node-based STL, you must construct first.
        }
        io_context.run();
    }
    catch (exception& e) {
        cerr << "Exception : " << e.what() << '\n';
    }
    return 0;
}
