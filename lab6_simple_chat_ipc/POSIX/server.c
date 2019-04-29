#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>
#include <mqueue.h>
#include <fcntl.h>
#include "chat.h"

//signatures of available server commands
void init(int clientPID, char msg[MAX_MSG_LENGTH]);

void echo(int clientID, char msg[MAX_MSG_LENGTH]);

void stop(int clientID);

void list(int clientID);

void friends(int clientID, char msg[MAX_MSG_LENGTH]);

void add(int clientID, char msg[MAX_MSG_LENGTH]);

void delete(int clientID, char msg[MAX_MSG_LENGTH]);

void _2all(int clientID, char msg[MAX_MSG_LENGTH]);

void _2friends(int clientID, char msg[MAX_MSG_LENGTH]);

void _2one(int clientID, char msg[MAX_MSG_LENGTH]);

//utills
void executeCommands(char *msg);

void sendMessage(enum MSG_COMMAND type, char msg[MAX_MSG_LENGTH], int clientID);

typedef struct {
    int clientQueue;
    int friends[MAX_CLIENTS];
    int curr_friends_number;
    pid_t pid;
} client_t;

client_t clients[MAX_CLIENTS];
int working = 1;
mqd_t serverQueueID = -1;
int working_clients = 0;

void exitHandler(int signo) {

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].clientQueue != -1) {
            //SEND SOME STOP!
            kill(clients[i].pid, SIGINT);
        }
    }

    if (mq_close(serverQueueID) == -1) raise_error("cannot close queue \n");
    if (mq_unlink(SERVER_NAME) == -1) raise_error("cannot delete queue \n");
    else
        printf("\033[1;32mServer:\033[0m Server queue closed and deleted successfully \n");
    exit(EXIT_SUCCESS);
}

int main() {
    int i;
    for (i = 0; i < MAX_CLIENTS; i++) {
        clients[i].clientQueue = -1;
        clients[i].curr_friends_number = 0;
    }

    struct mq_attr queue_attr;
    queue_attr.mq_maxmsg = MAX_QUEUE_SIZE;
    queue_attr.mq_msgsize = MAX_MSG_LENGTH;

    //handle SIGINT -> delete queue
    struct sigaction act;
    act.sa_handler = exitHandler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGINT, &act, NULL);

    if ((serverQueueID = mq_open(SERVER_NAME, O_RDONLY | O_CREAT | O_EXCL, 0666, &queue_attr)) == -1)
        raise_error("\033[1;32mServer:\033[0m cannot create new queue \n");

    char content[MAX_MSG_LENGTH];

    while (working) {
        if (mq_receive(serverQueueID, content, MAX_MSG_LENGTH, NULL) == -1)
            raise_error("\033[1;32mServer:\033[0m cannot receive message \n");
        executeCommands(content);
        sleep(1);
    }

    if (mq_close(serverQueueID) == -1) raise_error("cannot close queue \n");
    if (mq_unlink(SERVER_NAME) == -1) raise_error("cannot delete queue \n");
    else
        printf("\033[1;32mServer:\033[0m Server queue closed and  deleted successfully \n");


    return 0;
}

void executeCommands(char *msg) {

    printf("\033[1;32mServer:\033[0m Server has just got request!\n");
    long type = convert_to_num(strtok(msg, ";"));
    long sender = convert_to_num(strtok(NULL, ";"));

    if (type == STOP) {
        stop(sender);
    } else if (type == INIT) {
        init(sender, strtok(NULL, ";"));
    } else if (type == ECHO) {
        echo(sender, strtok(NULL, ";"));
    } else if (type == FRIENDS) {
        friends(sender, strtok(NULL, ";"));
    } else if (type == LIST) {
        list(sender);
    } else if (type == _2ALL) {
        _2all(sender, strtok(NULL, ";"));
    } else if (type == _2ONE) {
        _2one(sender, strtok(NULL, ";"));
    } else if (type == _2FRIENDS) {
        _2friends(sender, strtok(NULL, ";"));
    } else if (type == ADD) {
        add(sender, strtok(NULL, ";"));
    } else if (type == DEL) {
        delete(sender, strtok(NULL, ";"));
    } else {
        raise_error("\033[1;32mServer:\033[0m unknown message type \n");
    }
}

void sendMessage(enum MSG_COMMAND type, char msg[MAX_MSG_LENGTH], int clientID) {
    if (clientID >= MAX_CLIENTS || clientID < 0 || clients[clientID].clientQueue < 0) {
        raise_error("\033[1;32mServer:\033[0m cannot send message to this client \n");
    }

    if (mq_send(clients[clientID].clientQueue, msg, MAX_MSG_LENGTH, cmdPriority(type)))
        raise_error("\033[1;32mServer:\033[0m cannot send message to client\n");
}

void init(int clientPID, char msg[MAX_MSG_LENGTH]) {
    int id;;
    for (id = 0; id < MAX_CLIENTS; id++) {
        if (clients[id].clientQueue == -1)
            break;
    }
//    printf("got msg:, %s \n", msg);
    if (id >= MAX_CLIENTS)
        raise_error("\033[1;32mServer:\033[0m too many clients \n");

    if ((clients[id].clientQueue = mq_open(msg, O_WRONLY)) == -1)
        raise_error("\033[1;32mServer:\033[0m cannot open queue \n");

    clients[id].pid = clientPID;
    clients[id].curr_friends_number = 0;

    printf("\033[1;32mServer:\033[0m New client initialized, his id: %d and pid: %d\n", id, clientPID);

    char toClient[MAX_MSG_LENGTH];
    sprintf(toClient, "%d", id);
    working_clients++;
    sendMessage(INIT, toClient, id);

}

void echo(int clientID, char msg[MAX_MSG_LENGTH]) {
    printf("\033[1;32mServer:\033[0m Echo for client: %d is %s\n", clientID, msg);
    char response[MAX_MSG_LENGTH];
    char date[64];
    FILE *f = popen("date", "r");
    int check = fread(date, sizeof(char), 31, f);
    if (check == EOF)
        raise_error("\033[1;32mServer:\033[0m cannot read date \n");
    pclose(f);
    sprintf(response, "%s date: %s", msg, date);
    //printf("what i sent back: %s \n", response);
    sendMessage(ECHO, response, clientID);
}

//TO VERIFY
void stop(int clientID) {

    if (clientID >= 0 && clientID < MAX_CLIENTS) {
        if (mq_close(clients[clientID].clientQueue) == -1)
            raise_error("\033[1;32mServer:\033[0m cannot close client's queue \n");
        clients[clientID].clientQueue = -1;
        clients[clientID].curr_friends_number = 0;
        for (int i = 0; i < MAX_CLIENTS; i++)
            clients[clientID].friends[i] = -1;
        working_clients--;
        printf("\033[1;32mServer:\033[0m Working clients: %d \n", working_clients);
        if (working_clients == 0) {
            kill(getpid(), SIGINT);
        }
    }

}

void list(int clientID) {
    printf("\033[1;32mServer:\033[0m List will be sent to client with id: %d \n", clientID);
    char response[MAX_MSG_LENGTH], buf[MAX_MSG_LENGTH];
    strcpy(response, "");
    int i = 0;
    for (; i < MAX_CLIENTS; i++) {
        if (clients[i].clientQueue >= 0) {
            sprintf(buf, "ID: %d  queueID: %d \n", i, clients[i].clientQueue);
            strcat(response, buf);
        }
    }
    sendMessage(LIST, response, clientID);
}

void makeFriendsList(int clientID, char friends[MAX_MSG_LENGTH]) {
    char *friend = strtok(friends, SPLITTER);

    while (friend && clients[clientID].curr_friends_number < MAX_CLIENTS) {
        //to improve:
        int f = convert_to_num(friend);
        if (f < 0 || f >= MAX_CLIENTS || f < 0 || clientID == f) {
            printf("\033[1;32mServer:\033[0m friend: %s cannot be added (wrong type or value)\n", friend);
            return;
        }

        int found = 0;
        int i = 0;

        //friends cannot be repeated
        for (; i < clients[clientID].curr_friends_number; i++)
            if (f == clients[clientID].friends[i]) {
                found = 1;
            }
        if (!found) {
            clients[clientID].friends[clients[clientID].curr_friends_number++] = f;
            printf("\033[1;32mServer:\033[0m \tFriend: %d has just been added \n", f);
        } else {
            printf("\033[1;32mServer:\033[0m \tFriend: %d has been already added \n", f);
        }
        friend = strtok(NULL, SPLITTER);
    }
}

void friends(int clientID, char msg[MAX_MSG_LENGTH]) {
    if (clientID >= MAX_CLIENTS || clientID < 0 || clients[clientID].clientQueue < 0) {
        raise_error("wrong client \n");
    }

    printf("\033[1;32mServer:\033[0m server will prepare new list of client's firends \n");

    char friends[MAX_MSG_LENGTH];
    int state = sscanf(msg, "%s", friends);

    if (state != 1)
        raise_error("cannot elicit friends list \n");

    //override
    clients[clientID].curr_friends_number = 0;
    int i;
    for (i = 0; i < MAX_CLIENTS; i++) {
        clients[clientID].friends[i] = -1;
    }
    if(convert_to_num(friends) < 0){
        printf("\033[1;32mServer:\033[0m Friends list clean \n");
    }else{
        makeFriendsList(clientID, friends);
    }

}

void add(int clientID, char msg[MAX_MSG_LENGTH]) {
    if (clientID >= MAX_CLIENTS || clientID < 0 || clients[clientID].clientQueue < 0) {
        raise_error("wrong client \n");
    }

    char friends[MAX_MSG_LENGTH];
    int state = sscanf(msg, "%s", friends);

    if (state != 1)
        raise_error("cannot elicit friends list \n");

    makeFriendsList(clientID, friends);
}

void delete(int clientID, char msg[MAX_MSG_LENGTH]) {
    if (clientID >= MAX_CLIENTS || clientID < 0 || clients[clientID].clientQueue < 0) {
        raise_error("wrong client \n");
    }

    char list[MAX_MSG_LENGTH];
    int num = sscanf(msg, "%s", list);
    char *elem = NULL;
    int id = -1;
    int i = 0;

    if (num == EOF || num == 0) {
        raise_error("cannot scan data \n");
    } else if (num == 1) {
        elem = strtok(list, SPLITTER);
        while (elem != NULL && (clients[clientID].curr_friends_number) > 0) {
            id = (int) strtol(elem, NULL, 10);
            i = 0;
            for (; i < (clients[clientID].curr_friends_number); i++)
                if (id == clients[clientID].friends[i]) break;
            if (i >= (clients[clientID].curr_friends_number))id = -1;
            if (id < MAX_CLIENTS && id >= 0 && id != clientID) {
                clients[clientID].friends[i] = clients[clientID].friends[(clients[clientID].curr_friends_number) - 1];
                (clients[clientID].curr_friends_number)--;
            }
            elem = strtok(NULL, SPLITTER);
        }
    }

    printf("\033[1;32mServer:\033[0m \tfriends after deleting: \n");
    int j;
    for (j = 0; j < clients[clientID].curr_friends_number; j++) {
        printf("\033[1;32mServer:\033[0m \t ID: %d\n", clients[clientID].friends[j]);
    }
}

void _2all(int clientID, char msg[MAX_MSG_LENGTH]) {
    printf("\033[1;32mServer:\033[0m Server will send messages to all clients \n");

    char response[MAX_MSG_LENGTH];
    char date[32];
    FILE *p = popen("date", "r");
    int check = fread(date, sizeof(char), 31, p);
    if (check == EOF)
        raise_error("cannot read date \n");
    pclose(p);

    sprintf(response, "%d;message: %s from: %d date: %s \n", _2ALL, msg, clientID, date);

    int i;
    for (i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].clientQueue == -1 || i == clientID)
            continue;
        sendMessage(_2ALL, response, i);
        kill(clients[i].pid, SIGRTMIN);
    }
}

void _2one(int clientID, char msg[MAX_MSG_LENGTH]) {
    printf("\033[1;32mServer:\033[0m Server will send messages to one client \n");
    char date[32];

    char content[MAX_MSG_LENGTH];
    char response[MAX_MSG_LENGTH];

    int addressee;
    FILE *f = popen("date", "r");
    int check = fread(date, sizeof(char), 31, f);
    if (check == EOF)
        raise_error("cannot read date \n");
    pclose(f);
    int x = sscanf(msg, "%i %s", &addressee, content);
    if (x != 2)
        raise_error("cannot elicit address and content of message\n");
    sprintf(response, "%d;message %s from: %d date: %s \n", _2ONE, content, clientID, date);

    if (addressee >= MAX_CLIENTS || addressee < 0 || clients[addressee].clientQueue < 0) {
        raise_error("wrong addressee \n");
    }

    if (addressee == clientID) {
        printf("\033[1;32mServer:\033[0m cannot send message to yourself \n");
        return;
    }

    sendMessage(_2ONE, response, addressee);
    kill(clients[addressee].pid, SIGRTMIN);


}

void _2friends(int clientID, char msg[MAX_MSG_LENGTH]) {
    printf("\033[1;32mServer:\033[0m Server will send messages to friends of client \n");
    char response[MAX_MSG_LENGTH];
    char date[64];
    FILE *f = popen("date", "r");
    int check = fread(date, sizeof(char), 31, f);
    if (check == EOF)
        raise_error("cannot read date \n");
    pclose(f);
    sprintf(response, "%d;message: %s from: %d date: %s\n", _2ONE, msg, clientID, date);
    int i;
    for (i = 0; i < clients[clientID].curr_friends_number; i++) {
        int addressee = clients[clientID].friends[i];
        if (clients[clientID].friends[i] == -1)
            continue;
        if (addressee >= MAX_CLIENTS || addressee < 0 || clients[addressee].clientQueue < 0) {
            raise_error("wrong addressee \n");
        }
        printf("%i\t", addressee);
        sendMessage(_2FRIENDS, response, addressee);
        kill(clients[addressee].pid, SIGRTMIN);
    }
    printf("\n");
}


